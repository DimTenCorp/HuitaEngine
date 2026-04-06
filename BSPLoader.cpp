#define _CRT_SECURE_NO_WARNINGS
#include "BSPLoader.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cctype>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include "WADLoader.h"

static const float BSP_SCALE = 0.025f;

static glm::vec3 convertPosition(const glm::vec3& bspPos) {
    return glm::vec3(-bspPos.x * BSP_SCALE, bspPos.z * BSP_SCALE, bspPos.y * BSP_SCALE);
}

static glm::vec3 convertNormal(const glm::vec3& bspNormal) {
    return glm::vec3(-bspNormal.x, bspNormal.z, bspNormal.y);
}

static std::string toUpper(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\0", 0);
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r\0");
    return str.substr(start, end - start + 1);
}

// ============================================================================
// BSP RENDERING FUNCTIONS ADAPTED FROM QUAKE r_bsp.c
// ============================================================================

// Global rendering state (similar to Quake's r_bsp.c globals)
static bool insubmodel = false;
static glm::vec3 modelorg;           // viewpoint relative to currently rendering entity
static glm::vec3 r_entorigin;        // currently rendering entity in world coordinates
static float entity_rotation[3][3];  // rotation matrix for current entity
static int r_currentkey = 0;         // sequence number for ordering surfaces
static int r_visframecount = 0;      // visibility frame counter

// Clipping edge buffers for bmodel rendering
static BSPVertexData bverts[MAX_BMODEL_VERTS];
static BSPEdgeData bedges[MAX_BMODEL_EDGES];
static int numbverts = 0, numbedges = 0;

static BSPVertexData* pfrontenter = nullptr;
static BSPVertexData* pfrontexit = nullptr;
static bool makeclippededge = false;

// Frustum clipping planes
struct FrustumPlane {
    glm::vec3 normal;
    float dist;
};
static FrustumPlane view_clipplanes[4];
static int r_clipflags = 15;  // all 4 planes active

// Back-to-front polygon buffer for transparent sorting
struct BtoFPoly {
    int clipflags;
    int surfIndex;
};
static BtoFPoly pbtofpolys[256];
static int numbtofpolys = 0;

/*
================
R_EntityRotate
Rotate a vector by the entity's rotation matrix
================
*/
static void R_EntityRotate(glm::vec3& vec) {
    glm::vec3 tvec = vec;
    vec.x = glm::dot(glm::vec3(entity_rotation[0][0], entity_rotation[0][1], entity_rotation[0][2]), tvec);
    vec.y = glm::dot(glm::vec3(entity_rotation[1][0], entity_rotation[1][1], entity_rotation[1][2]), tvec);
    vec.z = glm::dot(glm::vec3(entity_rotation[2][0], entity_rotation[2][1], entity_rotation[2][2]), tvec);
}

/*
================
R_RotateBmodel
Set up rotation matrix for current entity and rotate viewing vectors
================
*/
static void R_RotateBmodel(const glm::vec3& angles) {
    float angle;
    float sr, sp, sy, cr, cp, cy;

    // Convert angles to radians and compute rotation matrix
    angle = glm::radians(angles.x);
    sr = sin(angle);
    cr = cos(angle);
    
    angle = glm::radians(angles.y);
    sp = sin(angle);
    cp = cos(angle);
    
    angle = glm::radians(angles.z);
    sy = sin(angle);
    cy = cos(angle);

    // Build rotation matrix (ZYX order)
    entity_rotation[0][0] = cp * cr;
    entity_rotation[0][1] = cr * sp;
    entity_rotation[0][2] = -sr;

    entity_rotation[1][0] = cp * sr * sy - cy * sp;
    entity_rotation[1][1] = sp * sr * sy + cp * cy;
    entity_rotation[1][2] = sy * cr;

    entity_rotation[2][0] = sp * sy + cp * cy * sr;
    entity_rotation[2][1] = sr * cy * sp - sy * cp;
    entity_rotation[2][2] = cr * cy;
}

/*
================
R_RecursiveClipBPoly
Recursively clip polygon edges against BSP tree planes
================
*/
static void R_RecursiveClipBPoly(
    BSPEdgeData* pedges, 
    const std::vector<BSPNode>& nodes,
    int nodeIndex,
    const std::vector<BSPFace>& faces,
    const std::vector<BSPPlane>& planes,
    const std::vector<glm::vec3>& vertices,
    const std::vector<BSPEdge>& edges,
    const std::vector<int>& surfEdges,
    int faceIndex
) {
    BSPEdgeData* psideedges[2] = { nullptr, nullptr };
    BSPEdgeData* pnextedge, * ptedge;
    int side, lastside;
    float dist, frac, lastdist;
    BSPVertexData* pvert, * plastvert, * ptvert;

    makeclippededge = false;

    const BSPNode& node = nodes[nodeIndex];
    const BSPPlane& splitplane = planes[node.planeNum];

    // Transform BSP plane into model space
    glm::vec3 transformedNormal;
    transformedNormal.x = glm::dot(glm::vec3(entity_rotation[0][0], entity_rotation[0][1], entity_rotation[0][2]), splitplane.normal);
    transformedNormal.y = glm::dot(glm::vec3(entity_rotation[1][0], entity_rotation[1][1], entity_rotation[1][2]), splitplane.normal);
    transformedNormal.z = glm::dot(glm::vec3(entity_rotation[2][0], entity_rotation[2][1], entity_rotation[2][2]), splitplane.normal);
    
    float transformedDist = splitplane.dist - glm::dot(r_entorigin, splitplane.normal);

    // Clip edges to BSP plane
    for (; pedges; pedges = pnextedge) {
        pnextedge = pedges->pnext;

        plastvert = pedges->v[0];
        lastdist = glm::dot(plastvert->position, transformedNormal) - transformedDist;
        lastside = (lastdist > 0) ? 0 : 1;

        pvert = pedges->v[1];
        dist = glm::dot(pvert->position, transformedNormal) - transformedDist;
        side = (dist > 0) ? 0 : 1;

        if (side != lastside) {
            // Edge is clipped - generate intersection vertex
            if (numbverts >= MAX_BMODEL_VERTS) {
                std::cerr << "Out of verts for bmodel\n";
                return;
            }

            frac = lastdist / (lastdist - dist);
            ptvert = &bverts[numbverts++];
            ptvert->position = plastvert->position + frac * (pvert->position - plastvert->position);

            if (numbedges >= (MAX_BMODEL_EDGES - 1)) {
                std::cerr << "Out of edges for bmodel\n";
                return;
            }

            // Create two edges, one on each side of the plane
            ptedge = &bedges[numbedges];
            ptedge->pnext = psideedges[lastside];
            psideedges[lastside] = ptedge;
            ptedge->v[0] = plastvert;
            ptedge->v[1] = ptvert;

            ptedge = &bedges[numbedges + 1];
            ptedge->pnext = psideedges[side];
            psideedges[side] = ptedge;
            ptedge->v[0] = ptvert;
            ptedge->v[1] = pvert;

            numbedges += 2;

            if (side == 0) {
                pfrontenter = ptvert;
                makeclippededge = true;
            } else {
                pfrontexit = ptvert;
                makeclippededge = true;
            }
        } else {
            // Edge entirely on one side
            pedges->pnext = psideedges[side];
            psideedges[side] = pedges;
        }
    }

    // Add clip plane edges if anything was clipped
    if (makeclippededge) {
        if (numbedges >= (MAX_BMODEL_EDGES - 2)) {
            std::cerr << "Out of edges for bmodel\n";
            return;
        }

        ptedge = &bedges[numbedges];
        ptedge->pnext = psideedges[0];
        psideedges[0] = ptedge;
        ptedge->v[0] = pfrontexit;
        ptedge->v[1] = pfrontenter;

        ptedge = &bedges[numbedges + 1];
        ptedge->pnext = psideedges[1];
        psideedges[1] = ptedge;
        ptedge->v[0] = pfrontenter;
        ptedge->v[1] = pfrontexit;

        numbedges += 2;
    }

    // Recurse or draw
    for (int i = 0; i < 2; i++) {
        if (psideedges[i]) {
            int child = (i == 0) ? node.frontChild : node.backChild;

            if (child < 0) {
                // Leaf node
                int leafIndex = -child - 1;
                if (leafIndex < (int)nodes.size()) {
                    const BSPLeaf& leaf = *(BSPLeaf*)&nodes[leafIndex];  // Simplified
                    if (leaf.contents != CONTENTS_SOLID) {
                        r_currentkey = leaf.key;
                        // Render the clipped polygon here
                    }
                }
            } else if (child < (int)nodes.size()) {
                // Internal node - recurse
                R_RecursiveClipBPoly(psideedges[i], nodes, child, faces, planes, vertices, edges, surfEdges, faceIndex);
            }
        }
    }
}

BSPLoader::BSPLoader() {
    worldBounds.min = glm::vec3(0);
    worldBounds.max = glm::vec3(0);
}

BSPLoader::~BSPLoader() {
    cleanupTextures();
}

std::vector<glm::vec3> BSPLoader::getLightPositions() const {
    std::vector<glm::vec3> lights;

    std::cout << "[BSP] Scanning " << entities.size() << " entities for lights...\n";

    for (const auto& entity : entities) {
        // Расширенный список классов света
        if (entity.classname == "light" ||
            entity.classname == "light_spot" ||
            entity.classname == "light_environment" ||
            entity.classname == "light_point" ||
            entity.classname.find("light") != std::string::npos) {

            lights.push_back(entity.origin);
            std::cout << "[BSP] Found light: " << entity.classname
                << " at (" << entity.origin.x << ", "
                << entity.origin.y << ", " << entity.origin.z << ")\n";
        }
    }

    std::cout << "[BSP] Total lights found: " << lights.size() << "\n";
    return lights;
}

std::vector<BSPLightInfo> BSPLoader::getLightInfos() const {
    std::vector<BSPLightInfo> lights;

    std::cout << "[BSP] Scanning " << entities.size() << " entities for light info...\n";

    for (const auto& entity : entities) {
        // Расширенный список классов света
        if (entity.classname == "light" ||
            entity.classname == "light_spot" ||
            entity.classname == "light_environment" ||
            entity.classname == "light_point" ||
            entity.classname.find("light") != std::string::npos) {

            BSPLightInfo info;
            info.position = entity.origin;
            
            // Парсим цвет из свойства "_light" или "color" (формат "R G B" 0-255)
            auto lightIt = entity.properties.find("_light");
            if (lightIt == entity.properties.end()) {
                lightIt = entity.properties.find("color");
            }
            
            if (lightIt != entity.properties.end()) {
                int r = 255, g = 255, b = 255;
                sscanf(lightIt->second.c_str(), "%d %d %d", &r, &g, &b);
                info.color = glm::vec3(
                    r / 255.0f,
                    g / 255.0f,
                    b / 255.0f
                );
            }
            
            // Парсим интенсивность (может быть в "_light" после цвета или отдельно)
            auto intensityIt = entity.properties.find("intensity");
            if (intensityIt != entity.properties.end()) {
                info.intensity = std::stof(intensityIt->second);
            } else if (lightIt != entity.properties.end()) {
                // Пробуем найти четвёртое число в строке "_light" как интенсивность
                int dummy1, dummy2, dummy3, intensityInt;
                if (sscanf(lightIt->second.c_str(), "%d %d %d %d", &dummy1, &dummy2, &dummy3, &intensityInt) == 4) {
                    info.intensity = intensityInt / 100.0f;
                }
            }
            
            // Парсим радиус из свойства "radius" или "_radius"
            auto radiusIt = entity.properties.find("radius");
            if (radiusIt == entity.properties.end()) {
                radiusIt = entity.properties.find("_radius");
            }
            if (radiusIt != entity.properties.end()) {
                info.radius = std::stof(radiusIt->second);
            } else {
                // Дефолтный радиус на основе интенсивности
                info.radius = 10.0f * info.intensity;
            }
            
            // Парсим стиль мерцания
            auto styleIt = entity.properties.find("style");
            if (styleIt != entity.properties.end()) {
                info.style = std::stoi(styleIt->second);
            }
            
            // Парсим targetname и target для связанных ламп
            auto targetnameIt = entity.properties.find("targetname");
            if (targetnameIt != entity.properties.end()) {
                info.targetname = targetnameIt->second;
            }
            
            auto targetIt = entity.properties.find("target");
            if (targetIt != entity.properties.end()) {
                info.target = targetIt->second;
            }
            
            lights.push_back(info);
            std::cout << "[BSP] Found light: " << entity.classname
                << " pos=(" << info.position.x << ", " << info.position.y << ", " << info.position.z << ")"
                << " color=(" << info.color.r << ", " << info.color.g << ", " << info.color.b << ")"
                << " intensity=" << info.intensity
                << " radius=" << info.radius
                << " style=" << info.style << "\n";
        }
    }

    std::cout << "[BSP] Total lights with info found: " << lights.size() << "\n";
    return lights;
}

bool BSPLoader::loadVertices(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_VERTICES];
    if (lump.length == 0) return false;
    size_t count = lump.length / (3 * sizeof(float));
    vertices.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    for (size_t i = 0; i < count; i++) {
        float x, y, z;
        fread(&x, sizeof(float), 1, file);
        fread(&y, sizeof(float), 1, file);
        fread(&z, sizeof(float), 1, file);
        vertices[i] = convertPosition(glm::vec3(x, y, z));
    }
    return true;
}

bool BSPLoader::loadEdges(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_EDGES];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(BSPEdge);
    edges.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(edges.data(), sizeof(BSPEdge), count, file);
    return true;
}

bool BSPLoader::loadSurfEdges(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_SURFEDGES];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(int);
    surfEdges.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(surfEdges.data(), sizeof(int), count, file);
    return true;
}

bool BSPLoader::loadFaces(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_FACES];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(BSPFace);
    faces.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(faces.data(), sizeof(BSPFace), count, file);
    return true;
}

bool BSPLoader::loadPlanes(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_PLANES];
    if (lump.length == 0) return false;
    size_t count = lump.length / (4 * sizeof(float) + sizeof(int));
    planes.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    for (size_t i = 0; i < count; i++) {
        float nx, ny, nz, dist;
        int type;
        fread(&nx, sizeof(float), 1, file);
        fread(&ny, sizeof(float), 1, file);
        fread(&nz, sizeof(float), 1, file);
        fread(&dist, sizeof(float), 1, file);
        fread(&type, sizeof(int), 1, file);
        planes[i].normal = convertNormal(glm::vec3(nx, ny, nz));
        planes[i].dist = dist * BSP_SCALE;
        planes[i].type = type;
    }
    return true;
}

bool BSPLoader::loadModels(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_MODELS];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(BSPModel);
    models.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(models.data(), sizeof(BSPModel), count, file);
    for (auto& model : models) {
        glm::vec3 minBSP(model.min[0], model.min[1], model.min[2]);
        glm::vec3 maxBSP(model.max[0], model.max[1], model.max[2]);
        glm::vec3 originBSP(model.origin[0], model.origin[1], model.origin[2]);
        glm::vec3 minConv = convertPosition(minBSP);
        glm::vec3 maxConv = convertPosition(maxBSP);
        glm::vec3 originConv = convertPosition(originBSP);
        model.min[0] = minConv.x; model.min[1] = minConv.y; model.min[2] = minConv.z;
        model.max[0] = maxConv.x; model.max[1] = maxConv.y; model.max[2] = maxConv.z;
        model.origin[0] = originConv.x; model.origin[1] = originConv.y; model.origin[2] = originConv.z;
    }
    if (!models.empty()) {
        worldBounds.min = glm::vec3(models[0].min[0], models[0].min[1], models[0].min[2]);
        worldBounds.max = glm::vec3(models[0].max[0], models[0].max[1], models[0].max[2]);
    }
    return true;
}

bool BSPLoader::loadTexInfo(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_TEXINFO];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(BSPTexInfo);
    texInfos.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(texInfos.data(), sizeof(BSPTexInfo), count, file);
    return true;
}

bool BSPLoader::loadRequiredWADsFromEntities() {
    requiredWADs.clear();

    for (const auto& entity : entities) {
        auto it = entity.properties.find("wad");
        if (it != entity.properties.end()) {
            std::string wadValue = it->second;

            size_t start = 0;
            size_t end;

            while (start < wadValue.length()) {
                while (start < wadValue.length() &&
                    (wadValue[start] == ';' || wadValue[start] == ' ' || wadValue[start] == '|')) {
                    start++;
                }
                if (start >= wadValue.length()) break;

                end = start;
                while (end < wadValue.length() &&
                    wadValue[end] != ';' && wadValue[end] != ' ' && wadValue[end] != '|') {
                    end++;
                }

                std::string wadName = wadValue.substr(start, end - start);

                if (!wadName.empty() && wadName.front() == '"') {
                    wadName = wadName.substr(1);
                }
                if (!wadName.empty() && wadName.back() == '"') {
                    wadName.pop_back();
                }

                size_t slashPos = wadName.find_last_of("/\\");
                if (slashPos != std::string::npos) {
                    wadName = wadName.substr(slashPos + 1);
                }

                if (!wadName.empty() && wadName != " " && wadName.length() > 2) {
                    requiredWADs.push_back(wadName);
                    std::cout << "[BSP] Required WAD from BSP: " << wadName << std::endl;
                }

                start = end;
            }
        }
    }

    std::sort(requiredWADs.begin(), requiredWADs.end());
    requiredWADs.erase(std::unique(requiredWADs.begin(), requiredWADs.end()), requiredWADs.end());

    return !requiredWADs.empty();
}

bool BSPLoader::loadTextures(FILE* file, const BSPHeader& header, WADLoader& wadLoader) {
    const BSPLump& lump = header.lumps[LUMP_TEXTURES];
    if (lump.length == 0) return false;

    std::vector<uint8_t> lumpData(lump.length);
    fseek(file, lump.offset, SEEK_SET);
    if (fread(lumpData.data(), 1, lump.length, file) != lump.length) {
        std::cerr << "[BSP] Failed to read texture lump data.\n";
        return false;
    }

    int numTextures = *(int*)(lumpData.data());
    if (numTextures <= 0) return false;

    const int* offsets = (const int*)(lumpData.data() + sizeof(int));

    defaultTextureId = wadLoader.getDefaultTexture();
    glTextureIds.reserve(numTextures);
    textureDimensions.reserve(numTextures);

    int foundCount = 0;
    int notFoundCount = 0;

    for (int i = 0; i < numTextures; ++i) {
        if (offsets[i] == -1 || offsets[i] < 0 || offsets[i] >= (int)lump.length) {
            glTextureIds.push_back(defaultTextureId);
            textureDimensions.push_back({ 16, 16 });
            notFoundCount++;
            continue;
        }

        const uint8_t* texPtr = lumpData.data() + offsets[i];

        char rawName[17] = { 0 };
        memcpy(rawName, texPtr, 16);

        std::string name;
        for (int j = 0; j < 16; j++) {
            if (rawName[j] == '\0' || rawName[j] == ' ') {
                break;
            }
            name += rawName[j];
        }

        if (name.empty()) {
            glTextureIds.push_back(defaultTextureId);
            textureDimensions.push_back({ 16, 16 });
            notFoundCount++;
            continue;
        }

        uint32_t width = *(const uint32_t*)(texPtr + 16);
        uint32_t height = *(const uint32_t*)(texPtr + 20);

        if (width == 0 || height == 0 || width > 4096 || height > 4096) {
            width = 256;
            height = 256;
        }

        GLuint texId = wadLoader.getTexture(name);

        if (texId != defaultTextureId) {
            foundCount++;
        }
        else {
            notFoundCount++;
            static int missingPrinted = 0;
            if (missingPrinted < 30) {
                std::cout << "[BSP] Texture not found in WADs: \"" << name << "\"" << std::endl;
                missingPrinted++;
            }
        }

        glTextureIds.push_back(texId);
        textureDimensions.push_back({ width, height });
    }

    std::cout << "[BSP] Textures mapped: " << glTextureIds.size()
        << " (found: " << foundCount << ", missing: " << notFoundCount << ")" << std::endl;

    return true;
}

bool BSPLoader::findPlayerStart(glm::vec3& outPosition, glm::vec3& outAngles) const {
    // Список возможных имён спавн точек (Quake / Half-Life)
    std::vector<std::string> spawnClasses = {
        "info_player_start",      // Quake / HL singleplayer
        "info_player_deathmatch", // Quake / HL deathmatch
        "info_player_coop",       // Quake coop
        "info_player_team1",      // Team spawn
        "info_player_team2",
        "info_player_axis",       // Some mods
        "info_player_allies",
        "player_start"            // Alternative name
    };

    for (const auto& className : spawnClasses) {
        for (const auto& entity : entities) {
            if (entity.classname == className) {
                outPosition = entity.origin;
                outAngles = entity.angles;

                std::cout << "[BSP] Found player spawn: " << className
                    << " at (" << outPosition.x << ", " << outPosition.y << ", " << outPosition.z << ")"
                    << " angles (" << outAngles.x << ", " << outAngles.y << ", " << outAngles.z << ")"
                    << std::endl;
                return true;
            }
        }
    }

    // Если не нашли спавн точку, используем центр карты (fallback)
    outPosition = glm::vec3(0.0f, 100.0f, 0.0f);
    outAngles = glm::vec3(0.0f, 0.0f, 0.0f);
    std::cout << "[BSP] No player start found, using fallback position" << std::endl;
    return false;
}

std::vector<BSPEntity> BSPLoader::getEntitiesByClass(const std::string& classname) const {
    std::vector<BSPEntity> result;
    for (const auto& entity : entities) {
        if (entity.classname == classname) {
            result.push_back(entity);
        }
    }
    return result;
}

bool BSPLoader::parseEntities(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_ENTITIES];
    if (lump.length == 0) return false;
    std::string buffer;
    buffer.resize(lump.length);
    fseek(file, lump.offset, SEEK_SET);
    fread(&buffer[0], 1, lump.length, file);

    size_t pos = 0;
    while (pos < buffer.size()) {
        while (pos < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[pos]))) pos++;
        if (pos >= buffer.size() || buffer[pos] != '{') break;
        pos++;

        BSPEntity entity;
        while (pos < buffer.size()) {
            while (pos < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[pos]))) pos++;
            if (buffer[pos] == '}') { pos++; break; }
            if (buffer[pos] != '"') { pos++; continue; }
            pos++;
            size_t keyStart = pos;
            while (pos < buffer.size() && buffer[pos] != '"') pos++;
            std::string key = buffer.substr(keyStart, pos - keyStart);
            pos++;

            while (pos < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[pos]))) pos++;
            if (buffer[pos] != '"') { pos++; continue; }
            pos++;
            size_t valStart = pos;
            while (pos < buffer.size() && buffer[pos] != '"') pos++;
            std::string value = buffer.substr(valStart, pos - valStart);
            pos++;

            entity.properties[key] = value;
            if (key == "classname") entity.classname = value;
            if (key == "model") entity.model = value;

            // Парсим origin (позиция)
            if (key == "origin") {
                sscanf(value.c_str(), "%f %f %f", &entity.origin.x, &entity.origin.y, &entity.origin.z);
                // Конвертируем координаты так же, как и вершины BSP
                glm::vec3 original(entity.origin.x, entity.origin.y, entity.origin.z);
                entity.origin = convertPosition(original);
            }

            // Парсим angles (поворот)
            if (key == "angles") {
                sscanf(value.c_str(), "%f %f %f", &entity.angles.x, &entity.angles.y, &entity.angles.z);
            }
        }
        entities.push_back(entity);
    }
    return true;
}

void BSPLoader::buildSubmodelMesh(const BSPModel& subModel) {
    unsigned int baseVertex = (unsigned int)meshVertices.size();

    for (int i = 0; i < subModel.numFaces; i++) {
        const BSPFace& face = faces[subModel.firstFace + i];
        if (face.numEdges < 3) continue;

        if (face.planeNum >= planes.size()) continue;
        if (face.texInfo >= texInfos.size()) continue;

        glm::vec3 normal = planes[face.planeNum].normal;
        if (face.side) normal = -normal;

        std::vector<glm::vec3> faceVerts;
        faceVerts.reserve(face.numEdges);

        for (int j = 0; j < face.numEdges; j++) {
            int edgeIdx = face.firstEdge + j;
            if (edgeIdx >= (int)surfEdges.size()) break;

            int surfEdge = surfEdges[edgeIdx];
            unsigned short vindex;

            if (surfEdge >= 0) {
                if (surfEdge >= (int)edges.size()) break;
                vindex = edges[surfEdge].v[0];
            }
            else {
                int negEdge = -surfEdge;
                if (negEdge >= (int)edges.size()) break;
                vindex = edges[negEdge].v[1];
            }

            if (vindex < vertices.size()) {
                faceVerts.push_back(vertices[vindex]);
            }
        }

        if (faceVerts.size() < 3) continue;

        AABB faceBounds;
        faceBounds.min = faceBounds.max = faceVerts[0];
        for (const auto& v : faceVerts) {
            faceBounds.min = glm::min(faceBounds.min, v);
            faceBounds.max = glm::max(faceBounds.max, v);
        }
        float epsilon = 0.1f * BSP_SCALE;
        faceBounds.min -= glm::vec3(epsilon);
        faceBounds.max += glm::vec3(epsilon);
        faceBoundingBoxes.push_back(faceBounds);

        const BSPTexInfo& texInfo = texInfos[face.texInfo];
        int texIdx = texInfo.textureIndex;

        if (texIdx < 0 || texIdx >= (int)glTextureIds.size()) {
            texIdx = 0;
        }

        GLuint textureID = glTextureIds[texIdx];

        int texWidth = textureDimensions[texIdx].x;
        int texHeight = textureDimensions[texIdx].y;

        if (texWidth <= 0 || texHeight <= 0) {
            texWidth = 256;
            texHeight = 256;
        }

        glm::vec3 s_vec(texInfo.s[0], texInfo.s[1], texInfo.s[2]);
        glm::vec3 t_vec(texInfo.t[0], texInfo.t[1], texInfo.t[2]);

        glm::vec3 s_converted(-s_vec.x, s_vec.z, s_vec.y);
        glm::vec3 t_converted(-t_vec.x, t_vec.z, t_vec.y);

        float s_offset = texInfo.s[3];
        float t_offset = texInfo.t[3];

        std::vector<glm::vec2> texCoords;
        texCoords.reserve(faceVerts.size());

        for (const auto& pos : faceVerts) {
            glm::vec3 originalBSPPos(
                -pos.x / BSP_SCALE,
                pos.z / BSP_SCALE,
                pos.y / BSP_SCALE
            );

            float u = glm::dot(originalBSPPos, s_vec) + s_offset;
            float v = glm::dot(originalBSPPos, t_vec) + t_offset;

            u = u / (float)texWidth;
            v = v / (float)texHeight;

            texCoords.push_back(glm::vec2(u, v));
        }

        unsigned int faceStartVertex = baseVertex;

        for (size_t j = 0; j < faceVerts.size(); j++) {
            BSPVertex v;
            v.position = faceVerts[j];
            v.normal = normal;
            v.texCoord = texCoords[j];
            meshVertices.push_back(v);
        }

        unsigned int startIdxOffset = (unsigned int)meshIndices.size();
        for (size_t j = 1; j + 1 < faceVerts.size(); j++) {
            meshIndices.push_back(faceStartVertex);
            meshIndices.push_back(faceStartVertex + (unsigned int)j);
            meshIndices.push_back(faceStartVertex + (unsigned int)j + 1);
        }

        drawCalls.push_back({ textureID, startIdxOffset, (unsigned int)((faceVerts.size() - 2) * 3) });
        baseVertex += (unsigned int)faceVerts.size();
    }
}

void BSPLoader::buildMesh() {
    meshVertices.clear();
    meshIndices.clear();
    faceBoundingBoxes.clear();
    drawCalls.clear();
    if (models.empty()) return;

    buildSubmodelMesh(models[0]);

    for (const auto& entity : entities) {
        if (entity.model.empty() || entity.model == "*0") continue;
        if (entity.classname.find("func_") == std::string::npos &&
            entity.classname.find("brush") == std::string::npos) continue;

        int modelIndex = std::stoi(entity.model.substr(1));
        if (modelIndex <= 0 || modelIndex >= (int)models.size()) continue;
        buildSubmodelMesh(models[modelIndex]);
    }

    std::cout << "Built " << meshVertices.size() << " mesh vertices, "
        << meshIndices.size() << " indices, "
        << drawCalls.size() << " draw calls\n";
}

bool BSPLoader::load(const std::string& filename, WADLoader& wadLoader) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) { std::cerr << "Cannot open BSP file: " << filename << "\n"; return false; }

    BSPHeader header;
    if (fread(&header, sizeof(BSPHeader), 1, file) != 1) {
        std::cerr << "Failed to read BSP header\n"; fclose(file); return false;
    }
    if (header.version != 29 && header.version != 30) {
        std::cerr << "Unsupported BSP version: " << header.version << "\n"; fclose(file); return false;
    }

    loadVertices(file, header);
    loadEdges(file, header);
    loadSurfEdges(file, header);
    loadFaces(file, header);
    loadPlanes(file, header);
    loadModels(file, header);
    loadTexInfo(file, header);
    parseEntities(file, header);
    
    // Load BSP tree structures for runtime rendering (adapted from r_bsp.c)
    loadNodes(file, header);
    loadLeafs(file, header);
    loadMarkSurfaces(file, header);

    // Загружаем WAD файлы из ENTITY секции
    loadRequiredWADsFromEntities();

    // Загружаем все WAD файлы из папки
    wadLoader.init();
    wadLoader.loadAllWADs();

    // Теперь загружаем текстуры
    loadTextures(file, header, wadLoader);

    fclose(file);
    buildMesh();
    printStats();
    return !meshVertices.empty();
}

void BSPLoader::cleanupTextures() {
    if (defaultTextureId) glDeleteTextures(1, &defaultTextureId);
    if (!glTextureIds.empty()) {
        for (GLuint tex : glTextureIds) {
            if (tex != defaultTextureId && tex != 0) {
                glDeleteTextures(1, &tex);
            }
        }
    }
    glTextureIds.clear();
    textureDimensions.clear();
    drawCalls.clear();
    defaultTextureId = 0;
}

void BSPLoader::printStats() const {
    std::cout << "=== BSP Loaded ===\n";
    std::cout << "Vertices: " << vertices.size() << "\n";
    std::cout << "Faces: " << faces.size() << "\n";
    std::cout << "Textures: " << glTextureIds.size() << "\n";
    std::cout << "Draw Calls: " << drawCalls.size() << "\n";
    std::cout << "Required WADs: " << requiredWADs.size() << "\n";
    std::cout << "BSP Nodes: " << nodes.size() << "\n";
    std::cout << "BSP Leafs: " << leafs.size() << "\n";
    std::cout << "Mark Surfaces: " << markSurfaces.size() << "\n";
    std::cout << "==================\n";
}

// ============================================================================
// BSP TREE LOADING FUNCTIONS (adapted from Quake r_bsp.c)
// ============================================================================

bool BSPLoader::loadNodes(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_NODES];
    if (lump.length == 0) return false;
    
    // Quake BSP node format: planeNum (4), children[2] (8), mins[3] (12), maxs[3] (12), firstSurface (4), numSurfaces (4)
    // Total: 40 bytes per node
    size_t count = lump.length / 40;
    nodes.resize(count);
    
    fseek(file, lump.offset, SEEK_SET);
    for (size_t i = 0; i < count; i++) {
        int planeNum, frontChild, backChild, firstSurface, numSurfaces;
        float mins[3], maxs[3];
        
        fread(&planeNum, sizeof(int), 1, file);
        fread(&frontChild, sizeof(int), 1, file);
        fread(&backChild, sizeof(int), 1, file);
        fread(mins, sizeof(float), 3, file);
        fread(maxs, sizeof(float), 3, file);
        fread(&firstSurface, sizeof(int), 1, file);
        fread(&numSurfaces, sizeof(int), 1, file);
        
        nodes[i].planeNum = planeNum;
        nodes[i].frontChild = frontChild;
        nodes[i].backChild = backChild;
        nodes[i].firstSurface = firstSurface;
        nodes[i].numSurfaces = numSurfaces;
        nodes[i].contents = 0;  // Internal nodes have no contents
        nodes[i].mins = convertPosition(glm::vec3(mins[0], mins[1], mins[2]));
        nodes[i].maxs = convertPosition(glm::vec3(maxs[0], maxs[1], maxs[2]));
    }
    
    return true;
}

bool BSPLoader::loadLeafs(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_LEAVES];
    if (lump.length == 0) return false;
    
    // Quake BSP leaf format: contents (4), visFrame (4), mins[3] (12), maxs[3] (12), 
    //                        firstMarkSurface (4), numMarkSurfaces (4), markSurfaces (short[4]) (8)
    // Total: 52 bytes per leaf
    size_t count = lump.length / 52;
    leafs.resize(count);
    
    fseek(file, lump.offset, SEEK_SET);
    for (size_t i = 0; i < count; i++) {
        int contents, visFrame, firstMarkSurface, numMarkSurfaces;
        float mins[3], maxs[3];
        short ambient_levels[4];
        
        fread(&contents, sizeof(int), 1, file);
        fread(&visFrame, sizeof(int), 1, file);
        fread(mins, sizeof(float), 3, file);
        fread(maxs, sizeof(float), 3, file);
        fread(&firstMarkSurface, sizeof(int), 1, file);
        fread(&numMarkSurfaces, sizeof(int), 1, file);
        fread(ambient_levels, sizeof(short), 4, file);
        
        leafs[i].contents = contents;
        leafs[i].visFrame = 0;  // Will be set during rendering
        leafs[i].key = (int)i;  // Unique key for ordering
        leafs[i].firstMarkSurface = firstMarkSurface;
        leafs[i].numMarkSurfaces = numMarkSurfaces;
        leafs[i].mins = convertPosition(glm::vec3(mins[0], mins[1], mins[2]));
        leafs[i].maxs = convertPosition(glm::vec3(maxs[0], maxs[1], maxs[2]));
    }
    
    return true;
}

bool BSPLoader::loadMarkSurfaces(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_MARKSURFACES];
    if (lump.length == 0) return false;
    
    size_t count = lump.length / sizeof(unsigned short);
    markSurfaces.resize(count);
    
    fseek(file, lump.offset, SEEK_SET);
    fread(markSurfaces.data(), sizeof(unsigned short), count, file);
    
    return true;
}