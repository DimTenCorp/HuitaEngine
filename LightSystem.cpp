#include "LightSystem.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>

// Global instance
LightSystem g_lightSystem;

LightSystem::LightSystem() {
    // Initialize light styles to default value (256 = full brightness)
    for (int i = 0; i < MAX_LIGHTSTYLES; ++i) {
        d_lightstylevalue[i] = 256;
    }
}

LightSystem::~LightSystem() {
}

void LightSystem::setLightStyle(int index, const std::string& pattern) {
    if (index < 0 || index >= MAX_LIGHTSTYLES) {
        return;
    }
    
    LightStyle& style = cl_lightstyle[index];
    style.map = pattern;
    style.length = static_cast<int>(pattern.length());
    
    // Calculate average and peak brightness
    if (style.length > 0) {
        int total = 0;
        style.peak = 'a';
        for (char c : style.map) {
            total += c - 'a';
            if (c > style.peak) {
                style.peak = c;
            }
        }
        style.average = static_cast<char>(total / style.length + 'a');
    } else {
        style.average = style.peak = 'm';
    }
}

int LightSystem::parseLightStyleValue(const LightStyle& style, float time) const {
    if (style.length == 0) {
        return 256;  // Default full brightness
    }
    
    int k;
    if (r_flatlightstyles == 2) {
        // Use peak brightness
        k = style.peak - 'a';
    } else if (r_flatlightstyles == 1) {
        // Use average brightness
        k = style.average - 'a';
    } else {
        // Animate through the pattern
        int i = static_cast<int>(time * 10.0f);
        k = i % style.length;
        k = style.map[k] - 'a';
    }
    
    return k * 22;  // Scale: 'a'=0, 'm'=286, 'z'=550
}

void LightSystem::animateLightStyles(float time) {
    for (int j = 0; j < MAX_LIGHTSTYLES; ++j) {
        const LightStyle& style = cl_lightstyle[j];
        d_lightstylevalue[j] = parseLightStyleValue(style, time);
    }
}

DLight* LightSystem::allocDLight(int key) {
    double currentTime = 0.0;  // Should be passed as parameter in real implementation
    
    // Find existing dlight with same key or free slot
    for (int i = 0; i < MAX_DLIGHTS; ++i) {
        DLight& dl = cl_dlights[i];
        if (dl.key == key || !dl.isActive(currentTime)) {
            dl.key = key;
            dl.spawn = static_cast<float>(currentTime);
            return &dl;
        }
    }
    
    // No free slots
    return nullptr;
}

void LightSystem::updateDLights(double currentTime, float deltaTime) {
    for (int i = 0; i < MAX_DLIGHTS; ++i) {
        DLight& dl = cl_dlights[i];
        if (dl.isActive(currentTime)) {
            // Decay radius over time
            if (dl.decay > 0.0f) {
                dl.radius -= dl.decay * deltaTime;
                if (dl.radius <= dl.minlight) {
                    dl.die = currentTime;  // Mark as expired
                }
            }
        }
    }
}

void LightSystem::loadLightGrid(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        worldLightGrid.clear();
        return;
    }
    
    // Parse BSPX lightgrid format
    // This is a simplified parser - full implementation would match Quake's format exactly
    struct Reader {
        const uint8_t* data;
        size_t offset;
        size_t size;
        
        uint8_t readByte() {
            if (offset >= size) {
                offset++;
                return 0;
            }
            return data[offset++];
        }
        
        int32_t readInt() {
            int32_t r = readByte() << 0;
            r |= readByte() << 8;
            r |= readByte() << 16;
            r |= readByte() << 24;
            return r;
        }
        
        float readFloat() {
            union { float f; int32_t i; } u;
            u.i = readInt();
            return u.f;
        }
    };
    
    Reader ctx{data, 0, size};
    
    // Read grid scale
    glm::vec3 step;
    for (int j = 0; j < 3; ++j) {
        step[j] = ctx.readFloat();
    }
    
    // Read grid dimensions
    int gridSize[3];
    for (int j = 0; j < 3; ++j) {
        gridSize[j] = ctx.readInt();
    }
    
    // Read minimum bounds
    glm::vec3 mins;
    for (int j = 0; j < 3; ++j) {
        mins[j] = ctx.readFloat();
    }
    
    // Setup grid scale (prefer multiply over divide)
    worldLightGrid.gridScale = glm::vec3(
        step.x != 0 ? 1.0f / step.x : 0,
        step.y != 0 ? 1.0f / step.y : 0,
        step.z != 0 ? 1.0f / step.z : 0
    );
    worldLightGrid.count[0] = gridSize[0];
    worldLightGrid.count[1] = gridSize[1];
    worldLightGrid.count[2] = gridSize[2];
    worldLightGrid.mins = mins;
    
    // Read number of styles
    worldLightGrid.numStyles = ctx.readByte();
    
    // Read root node index
    worldLightGrid.rootNode = ctx.readInt();
    
    // Read number of nodes
    unsigned int numNodes = ctx.readInt();
    unsigned int nodeStart = ctx.offset;
    ctx.offset += (3 + 8) * 4 * numNodes;  // Skip node data for now
    
    // Read number of leafs
    unsigned int numLeafs = ctx.readInt();
    
    // Count total samples needed
    unsigned int totalSamples = 0;
    for (unsigned int i = 0; i < numLeafs; ++i) {
        int lsz[3];
        for (int j = 0; j < 3; ++j) {
            lsz[j] = ctx.readInt();
        }
        unsigned int leafSamples = lsz[0] * lsz[1] * lsz[2];
        totalSamples += leafSamples;
        
        // Skip sample data for counting
        for (unsigned int s = 0; s < leafSamples; ++s) {
            uint8_t styleCount = ctx.readByte();
            if (styleCount == 255) continue;
            ctx.offset += styleCount * 4;
        }
    }
    
    // Allocate nodes and leafs
    worldLightGrid.nodes.resize(numNodes);
    worldLightGrid.leafs.resize(numLeafs);
    
    // Rewind to read nodes
    ctx.offset = nodeStart;
    for (unsigned int i = 0; i < numNodes; ++i) {
        for (int j = 0; j < 3; ++j) {
            worldLightGrid.nodes[i].mid[j] = ctx.readInt();
        }
        for (int j = 0; j < 8; ++j) {
            worldLightGrid.nodes[i].child[j] = ctx.readInt();
        }
    }
    
    ctx.offset += 4;  // Skip padding
    
    // Read leafs
    unsigned int sampleIndex = 0;
    for (unsigned int i = 0; i < numLeafs; ++i) {
        for (int j = 0; j < 3; ++j) {
            worldLightGrid.leafs[i].mins[j] = ctx.readInt();
            worldLightGrid.leafs[i].size[j] = ctx.readInt();
        }
        
        unsigned int leafSamples = worldLightGrid.leafs[i].size[0] * 
                                   worldLightGrid.leafs[i].size[1] * 
                                   worldLightGrid.leafs[i].size[2];
        
        worldLightGrid.leafs[i].samples.resize(leafSamples);
        
        for (unsigned int s = 0; s < leafSamples; ++s) {
            uint8_t styleCount = ctx.readByte();
            
            if (styleCount == 255) {
                // All styles invalid
                for (int k = 0; k < 4; ++k) {
                    worldLightGrid.leafs[i].samples[s].map[k].style = 255;
                }
            } else {
                // Read styleCount styles
                for (uint8_t k = 0; k < styleCount; ++k) {
                    if (k >= 4) {
                        ctx.offset += 4;  // Skip extra styles
                    } else {
                        worldLightGrid.leafs[i].samples[s].map[k].style = ctx.readByte();
                        worldLightGrid.leafs[i].samples[s].map[k].rgb[0] = ctx.readByte();
                        worldLightGrid.leafs[i].samples[s].map[k].rgb[1] = ctx.readByte();
                        worldLightGrid.leafs[i].samples[s].map[k].rgb[2] = ctx.readByte();
                    }
                }
                
                // Fill remaining slots with invalid
                for (uint8_t k = styleCount; k < 4; ++k) {
                    worldLightGrid.leafs[i].samples[s].map[k].style = 255;
                    worldLightGrid.leafs[i].samples[s].map[k].rgb[0] = 0;
                    worldLightGrid.leafs[i].samples[s].map[k].rgb[1] = 0;
                    worldLightGrid.leafs[i].samples[s].map[k].rgb[2] = 0;
                }
            }
        }
    }
    
    std::cout << "[LightSystem] Loaded lightgrid: " << numNodes << " nodes, " 
              << numLeafs << " leafs, " << totalSamples << " samples\n";
}

glm::vec3 LightSystem::sampleLightGrid(const LightGrid& grid, const glm::vec3& point) const {
    glm::vec3 result(0.0f);
    
    if (!grid.isValid()) {
        return result;
    }
    
    // Calculate tile indices and fractional positions
    int tile[3];
    float frac[3];
    
    for (int i = 0; i < 3; ++i) {
        float pos = (point[i] - grid.mins[i]) * grid.gridScale[i];
        tile[i] = static_cast<int>(std::floor(pos));
        frac[i] = pos - tile[i];
    }
    
    // Trilinear interpolation
    float totalWeight = 0.0f;
    
    for (int i = 0; i < 8; ++i) {
        // Calculate weight for this corner
        float weight = ((i & 1) ? frac[0] : (1.0f - frac[0])) *
                       ((i & 2) ? frac[1] : (1.0f - frac[1])) *
                       ((i & 4) ? frac[2] : (1.0f - frac[2]));
        
        // Get sample at this corner
        int x = tile[0] + ((i & 1) ? 1 : 0);
        int y = tile[1] + ((i & 2) ? 1 : 0);
        int z = tile[2] + ((i & 4) ? 1 : 0);
        
        glm::vec3 sampleValue(0.0f);
        if (sampleLightGridSingleValue(grid, x, y, z, sampleValue)) {
            result += sampleValue * weight;
            totalWeight += weight;
        }
    }
    
    if (totalWeight > 0.0f) {
        result /= totalWeight;
    }
    
    return result;
}

bool LightSystem::sampleLightGridSingleValue(const LightGrid& grid, int x, int y, int z, glm::vec3& result) const {
    result = glm::vec3(0.0f);
    
    // Traverse octree to find leaf
    unsigned int node = grid.rootNode;
    
    while (!(node & LightGridNode::LGNODE_LEAF)) {
        if (node & LightGridNode::LGNODE_MISSING) {
            return false;  // Missing data
        }
        
        const LightGridNode& n = grid.nodes[node];
        int childIndex = ((x >= n.mid[0]) << 2) | ((y >= n.mid[1]) << 1) | ((z >= n.mid[2]) << 0);
        node = n.child[childIndex];
    }
    
    // Found leaf
    const LightGridLeaf& leaf = grid.leafs[node & ~LightGridNode::LGNODE_LEAF];
    
    // Check bounds
    int lx = x - leaf.mins[0];
    int ly = y - leaf.mins[1];
    int lz = z - leaf.mins[2];
    
    if (lx < 0 || lx >= leaf.size[0] ||
        ly < 0 || ly >= leaf.size[1] ||
        lz < 0 || lz >= leaf.size[2]) {
        return false;
    }
    
    // Get sample
    int index = lx + leaf.size[0] * (ly + leaf.size[1] * lz);
    if (index >= static_cast<int>(leaf.samples.size())) {
        return false;
    }
    
    const LightGridSample& samp = leaf.samples[index];
    
    // Accumulate light from all styles
    for (int i = 0; i < 4; ++i) {
        if (samp.map[i].style == 255) break;
        
        float styleValue = d_lightstylevalue[samp.map[i].style] / 256.0f;
        result.x += samp.map[i].rgb[0] * styleValue;
        result.y += samp.map[i].rgb[1] * styleValue;
        result.z += samp.map[i].rgb[2] * styleValue;
    }
    
    // Normalize to 0-1 range
    result /= 255.0f;
    
    return true;
}

glm::vec3 LightSystem::sampleLightAtPoint(const glm::vec3& point) const {
    if (worldLightGrid.isValid()) {
        return sampleLightGrid(worldLightGrid, point);
    }
    
    // Fallback to legacy lightmap sampling
    return recursiveLightPoint(point, 8192.0f);
}

void LightSystem::setWorldLightData(const uint8_t* lightData, size_t dataSize) {
    worldLightData = lightData;
    worldLightDataSize = dataSize;
}

glm::vec3 LightSystem::recursiveLightPoint(const glm::vec3& point, float maxDist) const {
    // Simplified fallback - returns ambient light
    // Full implementation would trace ray through BSP and sample lightmaps
    return glm::vec3(0.3f);  // Default ambient
}

void LightSystem::setFlatLightStyles(int mode) {
    r_flatlightstyles = mode;
}

void LightSystem::setColoredPowerupGlow(bool enabled) {
    r_coloredpowerupglow = enabled;
}

void LightSystem::setRocketLightIntensity(float intensity) {
    r_rocketlight = intensity;
}

void LightSystem::setExplosionLightIntensity(float intensity) {
    r_explosionlight = intensity;
}

void LightSystem::setCShiftPercent(float percent) {
    gl_cshiftpercent = percent;
}
