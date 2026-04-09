#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <array>
#include <cstdint>

// Constants from Quake
constexpr int MAX_LIGHTSTYLES = 1024;  // Spike - increased from 64
constexpr int MAX_DLIGHTS = 64;        // johnfitz - increased from 32
constexpr int MAXLIGHTMAPS = 4;
constexpr unsigned int INVALID_LIGHTSTYLE = 0xFFFFu;

// Light style structure - stores animated light patterns
struct LightStyle {
    int length = 0;
    std::string map;  // Pattern string (e.g., "abcdefghijklm")
    char average = 'm';  // Average brightness for flat styles
    char peak = 'm';     // Peak brightness for flat styles
    
    void clear() {
        length = 0;
        map.clear();
        average = 'm';
        peak = 'm';
    }
};

// Dynamic light structure - temporary lights from explosions, rockets, etc.
struct DLight {
    glm::vec3 origin{0.0f};
    float radius = 0.0f;
    float spawn = 0.0f;      // Time when light was spawned (for demo playback)
    double die = 0.0;        // Stop lighting after this time
    float decay = 0.0f;      // Drop this each second
    float minlight = 0.0f;   // Don't add when contributing less
    int key = 0;             // Entity key for network sync
    glm::vec3 color{1.0f};   // Colored lighting support
    
    bool isActive(double currentTime) const {
        return die > currentTime && radius > 0.0f;
    }
};

// Light grid sample - stored in BSP lightgrid
struct LightGridSample {
    struct StyleMap {
        uint8_t style;       // Light style index (0-255, 255 = invalid)
        uint8_t rgb[3];      // RGB color values (0-255)
        
        StyleMap() : style(255), rgb{0, 0, 0} {}
    };
    
    StyleMap map[4];  // Up to 4 light styles per sample
    
    LightGridSample() {
        for (int i = 0; i < 4; ++i) {
            map[i].style = 255;
            map[i].rgb[0] = map[i].rgb[1] = map[i].rgb[2] = 0;
        }
    }
};

// Light grid leaf node
struct LightGridLeaf {
    int mins[3]{0, 0, 0};
    int size[3]{0, 0, 0};
    std::vector<LightGridSample> samples;
};

// Light grid octree node
struct LightGridNode {
    int mid[3]{0, 0, 0};
    uint32_t child[8]{0, 0, 0, 0, 0, 0, 0, 0};
    
    static constexpr uint32_t LGNODE_LEAF = (1u << 31);
    static constexpr uint32_t LGNODE_MISSING = (1u << 30);
    
    bool isLeaf() const { return child[0] & LGNODE_LEAF; }
    bool isMissing() const { return child[0] & LGNODE_MISSING; }
};

// Light grid structure for fast lighting queries
struct LightGrid {
    glm::vec3 gridScale{1.0f};  // 1/step size for each axis
    unsigned int count[3]{0, 0, 0};  // Grid dimensions
    glm::vec3 mins{0.0f};       // Minimum bounds
    unsigned int numStyles = 4;
    
    unsigned int rootNode = 0;
    std::vector<LightGridNode> nodes;
    std::vector<LightGridLeaf> leafs;
    
    bool isValid() const { return !nodes.empty() && !leafs.empty(); }
    void clear() {
        nodes.clear();
        leafs.clear();
        rootNode = 0;
    }
};

// Main lighting system manager
class LightSystem {
public:
    LightSystem();
    ~LightSystem();
    
    // Light styles management
    void setLightStyle(int index, const std::string& pattern);
    void animateLightStyles(float time);
    const std::array<int, MAX_LIGHTSTYLES>& getLightStyleValues() const { return d_lightstylevalue; }
    int getLightStyleValue(int index) const { return d_lightstylevalue[index]; }
    
    // Dynamic lights management
    DLight* allocDLight(int key);
    void updateDLights(double currentTime, float deltaTime);
    const std::array<DLight, MAX_DLIGHTS>& getDLights() const { return cl_dlights; }
    
    // Light grid loading and sampling
    void loadLightGrid(const uint8_t* data, size_t size);
    glm::vec3 sampleLightGrid(const LightGrid& grid, const glm::vec3& point) const;
    glm::vec3 sampleLightAtPoint(const glm::vec3& point) const;
    
    // Legacy lightmap sampling (for surfaces without lightgrid)
    void setWorldLightData(const uint8_t* lightData, size_t dataSize);
    
    // Configuration
    void setFlatLightStyles(int mode);  // 0=animated, 1=average, 2=peak
    void setColoredPowerupGlow(bool enabled);
    void setRocketLightIntensity(float intensity);
    void setExplosionLightIntensity(float intensity);
    void setCShiftPercent(float percent);
    
private:
    // Light styles
    std::array<LightStyle, MAX_LIGHTSTYLES> cl_lightstyle;
    std::array<int, MAX_LIGHTSTYLES> d_lightstylevalue;  // Current brightness values (0-256)
    int r_flatlightstyles = 0;
    
    // Dynamic lights
    std::array<DLight, MAX_DLIGHTS> cl_dlights;
    int nextDLightKey = 1;
    
    // Light grid
    LightGrid worldLightGrid;
    const uint8_t* worldLightData = nullptr;
    size_t worldLightDataSize = 0;
    
    // Configuration cvars
    bool r_coloredpowerupglow = false;
    float r_rocketlight = 1.0f;
    float r_explosionlight = 1.0f;
    float gl_cshiftpercent = 100.0f;
    
    // Internal helpers
    int parseLightStyleValue(const LightStyle& style, float time) const;
    glm::vec3 recursiveLightPoint(const glm::vec3& point, float maxDist) const;
    bool sampleLightGridSingleValue(const LightGrid& grid, int x, int y, int z, glm::vec3& result) const;
};

// Global light system instance
extern LightSystem g_lightSystem;
