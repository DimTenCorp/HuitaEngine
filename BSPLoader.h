#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <string>
#include <glad/glad.h>

class WADLoader;
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

// Структура узла BSP дерева
struct BSPNode {
    int planeNum;
    short children[2];  // Если > 0 - индекс узла, если < 0 - ~(индекс листа)
    short min[3];
    short max[3];
    unsigned short firstFace;
    unsigned short numFaces;
    short area;
    short pad;
};

// Структура листа BSP дерева
struct BSPLeaf {
    int contents;
    int cluster;          // Кластер для VIS (потенциально видимые множества)
    short min[3];
    short max[3];
    unsigned short firstMarkSurface;
    unsigned short numMarkSurfaces;
    unsigned char area;
    unsigned char numPortals;
};

// Данные VIS для кластера
struct VISCluster {
    std::vector<uint8_t> visibilityBits;  // Битовая маска видимых кластеров
    int numVisibleClusters;               // Количество видимых кластеров
    std::vector<int> visibleClusters;     // Список видимых кластеров (кэшированный)
};

struct BSPFace {
    unsigned short planeNum;
    unsigned short side;
    int firstEdge;
    unsigned short numEdges;
    unsigned short texInfo;
    unsigned char styles[4];
    int lightofs;
};

struct BSPEdge { unsigned short v[2]; };
struct BSPModel { float min[3]; float max[3]; float origin[3]; int headNode[4]; int visLeafs; int firstFace; int numFaces; };
struct BSPTexInfo { float s[4]; float t[4]; int textureIndex; int flags; };
#pragma pack(pop)

struct BSPVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
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
    int faceIndex;

    unsigned char rendermode = 0;
    unsigned char renderamt = 255;
    bool isTransparent = false;
    bool isWater = false;
    bool isSky = false;
    bool isLadder = false;  // <-- ДОБАВЛЕНО: флаг лестницы (нет коллизии)
    
    AABB bounds;  // Границы фейса для frustum culling
};

class BSPLoader {
private:
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> originalVertices;
    std::vector<BSPEdge> edges;
    std::vector<int> surfEdges;
    std::vector<BSPFace> faces;
    std::vector<BSPPlane> planes;
    std::vector<BSPModel> models;
    std::vector<BSPTexInfo> texInfos;
    
    // Данные для VIS
    std::vector<BSPNode> nodes;
    std::vector<BSPLeaf> leaves;
    std::vector<VISCluster> visClusters;
    int numVisClusters = 0;
    std::vector<uint8_t> rawVisData;
    
    // Маппинг: face index -> cluster index
    std::vector<int> faceToCluster;

    std::vector<BSPVertex> meshVertices;
    std::vector<unsigned int> meshIndices;
    std::vector<AABB> faceBoundingBoxes;
    AABB worldBounds;

    std::vector<GLuint> glTextureIds;
    std::vector<glm::uvec2> textureDimensions;
    std::vector<std::string> textureNames;
    GLuint defaultTextureId = 0;
    std::vector<FaceDrawCall> drawCalls;

    std::vector<BSPEntity> entities;
    std::vector<std::string> requiredWADs;

    std::string skyName;

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
    void buildSubmodelMesh(const BSPModel& subModel, int rendermode, int renderamt, bool isWaterModel = false, bool isLadderModel = false);
    bool loadLighting(FILE* file, const BSPHeader& header);
    
    // Методы для загрузки и парсинга VIS
    bool loadNodes(FILE* file, const BSPHeader& header);
    bool loadLeaves(FILE* file, const BSPHeader& header);
    bool loadVisibility(FILE* file, const BSPHeader& header);
    void parseVISData();
    void buildFaceToClusterMapping();
    int getLeafFromPosition(const glm::vec3& pos) const;
    int getClusterFromLeaf(int leafIndex) const;

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
    const std::vector<BSPModel>& getModels() const { return models; }

    GLuint getDefaultTextureID() const { return defaultTextureId; }
    void cleanupTextures();

    void printStats() const;
    void debugPrintEntities() const;
    bool isLoaded() const { return !meshVertices.empty(); }

    bool findPlayerStart(glm::vec3& outPosition, glm::vec3& outAngles) const;
    std::vector<BSPEntity> getEntitiesByClass(const std::string& classname) const;

    // Методы для работы с VIS
    bool isFaceVisible(int faceIndex, int clusterIndex) const;
    int getClusterForCamera(const glm::vec3& cameraPos) const;
    const std::vector<int>& getVisibleClusters(int clusterIndex) const;
    int getNumVisClusters() const { return numVisClusters; }
    bool hasVISData() const { return !visClusters.empty(); }

    std::vector<uint8_t> lightingData;

    int getFaceLightOffset(int faceIndex) const {
        if (faceIndex >= 0 && faceIndex < static_cast<int>(faces.size())) {
            return faces[faceIndex].lightofs;
        }
        return -1;
    }

    void getFaceLightStyles(int faceIndex, unsigned char styles[4]) const {
        if (faceIndex >= 0 && faceIndex < static_cast<int>(faces.size())) {
            memcpy(styles, faces[faceIndex].styles, 4);
        }
    }

    const std::vector<BSPFace>& getFaces() const { return faces; }
    const std::vector<BSPTexInfo>& getTexInfos() const { return texInfos; }
    const std::vector<int>& getSurfEdges() const { return surfEdges; }
    const std::vector<BSPEdge>& getEdges() const { return edges; }

    const std::vector<glm::vec3>& getOriginalVertices() const { return originalVertices; }
    const std::vector<glm::vec3>& getVertices() const { return vertices; }
    const std::vector<BSPPlane>& getPlanes() const { return planes; }

    void getFaceLightmapDims(int faceIndex, int& width, int& height, float& minS, float& minT) const;

    bool isTriggerTexture(int textureIndex) const;

    GLuint getTextureID(int index) const {
        if (index >= 0 && index < static_cast<int>(glTextureIds.size())) {
            return glTextureIds[index];
        }
        return defaultTextureId;
    }

    glm::uvec2 getTextureDimensions(int index) const {
        if (index >= 0 && index < static_cast<int>(textureDimensions.size())) {
            return textureDimensions[index];
        }
        return glm::uvec2(256, 256);
    }

    const std::string& getTextureName(int index) const {
        static std::string empty;
        if (index < 0 || index >= (int)textureNames.size()) return empty;
        return textureNames[index];
    }

    bool isSkyTexture(int textureIndex) const {
        if (textureIndex < 0 || textureIndex >= (int)textureNames.size()) return false;
        const std::string& name = textureNames[textureIndex];
        return (name == "sky" || name == "SKY" ||
            name.find("sky") == 0 || name.find("SKY") == 0);
    }

    bool isModelDoor(int modelIndex) const;

    const std::string& getSkyName() const { return skyName; }

    int getTextureCount() const { return (int)glTextureIds.size(); }
};