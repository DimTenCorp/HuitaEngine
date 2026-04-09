#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <glad/glad.h>
#include "Light.h"

// Forward declaration only for WADLoader
class WADLoader;
// Forward declaration for LightManager to avoid circular dependency
class LightManager;

// Include AABB definition - moved to separate header to avoid circular dependency
#include "AABB.h"

#pragma pack(push, 1)
struct BSPLump { int offset; int length; };
struct BSPHeader { int version; BSPLump lumps[15]; };

#define LUMP_ENTITIES      0
#define LUMP_PLANES        1
#define LUMP_TEXTURES      2
#define LUMP_VERTICES      3
#define LUMP_VISIBILITY    4
#define LUMP_NODES         5
#define LUMP_TEXINFO       6
#define LUMP_FACES         7
#define LUMP_LIGHTING      8
#define LUMP_CLIPNODES     9
#define LUMP_LEAVES       10
#define LUMP_MARKSURFACES 11
#define LUMP_EDGES        12
#define LUMP_SURFEDGES    13
#define LUMP_MODELS       14

struct BSPPlane { glm::vec3 normal; float dist; int type; };
struct BSPFace { unsigned short planeNum; unsigned short side; int firstEdge; unsigned short numEdges; unsigned short texInfo; unsigned char styles[4]; int lightOffset; };
struct BSPEdge { unsigned short v[2]; };
struct BSPModel { float min[3]; float max[3]; float origin[3]; int headNode[4]; int visLeafs; int firstFace; int numFaces; };
struct BSPTexInfo { float s[4]; float t[4]; int textureIndex; int flags; };
#pragma pack(pop)

// Ключевые классы сущностей для быстрого поиска
static const std::unordered_set<std::string> CRITICAL_ENTITY_CLASSES = {
    "info_player_start",
    "info_player_deathmatch", 
    "info_player_coop",
    "info_player_team1",
    "info_player_team2",
    "info_player_axis",
    "info_player_allies",
    "player_start",
    "light_environment"
};

struct BSPVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 lightmapUV;  // UV координаты для световой карты
};

struct BSPEntity {
    std::string classname;
    std::string model;
    glm::vec3 origin{ 0 };
    glm::vec3 angles{ 0 };
    std::unordered_map<std::string, std::string> properties;
};

struct FaceDrawCall {
    GLuint texID;
    unsigned int indexOffset;
    unsigned int indexCount;
};

class BSPLoader {
private:
    std::vector<glm::vec3> vertices;
    std::vector<BSPEdge> edges;
    std::vector<int> surfEdges;
    std::vector<BSPFace> faces;
    std::vector<BSPPlane> planes;
    std::vector<BSPModel> models;
    std::vector<BSPTexInfo> texInfos;

    std::vector<BSPVertex> meshVertices;
    std::vector<unsigned int> meshIndices;
    std::vector<AABB> faceBoundingBoxes;
    AABB worldBounds;

    std::vector<GLuint> glTextureIds;
    std::vector<glm::uvec2> textureDimensions;
    GLuint defaultTextureId = 0;
    std::vector<FaceDrawCall> drawCalls;

    std::vector<BSPEntity> entities;
    std::vector<std::string> requiredWADs;

    // Lightmap data
    std::vector<unsigned char> lightmapData;  // Сырые данные освещения из BSP
    GLuint lightmapTexture = 0;
    int lightmapSize = 128;  // Стандартный размер lightmap страницы в GoldSrc

    bool loadVertices(FILE* file, const BSPHeader& header);
    bool loadEdges(FILE* file, const BSPHeader& header);
    bool loadSurfEdges(FILE* file, const BSPHeader& header);
    bool loadFaces(FILE* file, const BSPHeader& header);
    bool loadPlanes(FILE* file, const BSPHeader& header);
    bool loadModels(FILE* file, const BSPHeader& header);
    bool loadTexInfo(FILE* file, const BSPHeader& header);
    bool loadTextures(FILE* file, const BSPHeader& header, WADLoader& wadLoader);
    bool parseEntities(FILE* file, const BSPHeader& header);
    bool loadRequiredWADsFromEntities();
    void buildMesh();
    void buildSubmodelMesh(const BSPModel& subModel);

public:
    BSPLoader();
    ~BSPLoader();
    bool load(const std::string& filename, WADLoader& wadLoader);

    const std::vector<BSPVertex>& getMeshVertices() const { return meshVertices; }
    const std::vector<unsigned int>& getMeshIndices() const { return meshIndices; }
    const std::vector<AABB>& getFaceBounds() const { return faceBoundingBoxes; }
    const AABB& getWorldBounds() const { return worldBounds; }
    const std::vector<BSPEntity>& getEntities() const { return entities; }
    const std::vector<FaceDrawCall>& getDrawCalls() const { return drawCalls; }
    const std::vector<std::string>& getRequiredWADs() const { return requiredWADs; }

    GLuint getDefaultTextureID() const { return defaultTextureId; }
    void cleanupTextures();

    void printStats() const;
    void debugPrintEntities() const;
    bool isLoaded() const { return !meshVertices.empty(); }

    bool findPlayerStart(glm::vec3& outPosition, glm::vec3& outAngles) const;
    std::vector<BSPEntity> getEntitiesByClass(const std::string& classname) const;
    
    // Setup lighting from light_environment entity
    void setupLightEnvironment(LightManager& lightManager) const;

    std::vector<Light> extractLights() const;

    // Lightmap data
    GLuint getLightmapTexture() const { return lightmapTexture; }
    int getLightmapSize() const { return lightmapSize; }
    
private:
    bool loadLighting(FILE* file, const BSPHeader& header);
    void buildLightmapUVs();
};