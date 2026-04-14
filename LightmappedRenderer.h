#pragma once
#include "Renderer.h"
#include "LightmapManager.h"
#include <memory>
#include <unordered_set>

struct BSPVertexLightmapped {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec2 lightmapCoord;
    int lightmapIndex;
};

struct LMRenderVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec2 lightmapCoord;
};

struct LMFaceDrawCall {
    GLuint texID;
    unsigned int indexOffset;
    unsigned int indexCount;
    int faceIndex;

    unsigned char rendermode = 0;
    unsigned char renderamt = 255;
    bool isTransparent = false;
    bool isSky = false;
};

class LightmappedRenderer {
public:
    LightmappedRenderer();
    ~LightmappedRenderer();

    LightmappedRenderer(const LightmappedRenderer&) = delete;
    LightmappedRenderer& operator=(const LightmappedRenderer&) = delete;

    LightmappedRenderer(LightmappedRenderer&& other) noexcept;
    LightmappedRenderer& operator=(LightmappedRenderer&& other) noexcept;

    bool init(int width, int height);
    bool loadWorld(BSPLoader& bsp, LightmapManager& lightmapManager);
    void unloadWorld();

    void beginFrame(const glm::vec3& clearColor, bool clearColorBuffer = true);
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos,
        BSPLoader& bsp,
        const glm::vec3& ambientColor = glm::vec3(0.05f));

    void setLightmapIntensity(float intensity) { lightmapIntensity = intensity; }
    float getLightmapIntensity() const { return lightmapIntensity; }

    void setShowLightmapsOnly(bool show) { showLightmapsOnly = show; }
    bool getShowLightmapsOnly() const { return showLightmapsOnly; }

    void setUseLighting(bool use) { useLighting = use; }
    bool getUseLighting() const { return useLighting; }

    const Renderer::RenderStats& getStats() const { return stats; }

    void setViewport(int width, int height);

    void setSkipSkyFaces(bool skip) { skipSkyFaces = skip; }
    bool getSkipSkyFaces() const { return skipSkyFaces; }
    
    // Получить информацию о дверях (face -> model index)
    const std::unordered_map<int, int>& getDoorFaceToModelIndex() const { return doorFaceToModelIndex; }

private:
    std::unique_ptr<Shader> lightmappedShader;
    GLuint worldVAO = 0, worldVBO = 0, worldEBO = 0;
    size_t indexCount = 0;
    LightmapManager* lmManager = nullptr;
    float lightmapIntensity = 1.0f;
    bool showLightmapsOnly = false;
    bool useLighting = true;
    int screenWidth = 1280, screenHeight = 720;
    Renderer::RenderStats stats;
    std::vector<LMFaceDrawCall> faceDrawCalls;
    bool hasTransparentFaces = false;
    bool skipSkyFaces = false;
    
    // Информация о дверях для исключения из статического меша
    std::unordered_map<int, int> doorFaceToModelIndex;

    std::vector<LMRenderVertex> meshVertices;
    std::vector<unsigned int> meshIndices;

    bool initShaders();
    bool buildLightmappedMesh(BSPLoader& bsp, LightmapManager& lmManager);
    void cleanup();
    static const char* getVertexShaderSource();
    static const char* getFragmentShaderSource();
};