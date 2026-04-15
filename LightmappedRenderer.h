#pragma once
#include "Renderer.h"
#include "LightmapManager.h"
#include <memory>
#include <unordered_set>

// Forward declaration
struct DoorEntity;

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

    void renderDoors(const std::vector<std::unique_ptr<DoorEntity>>& doors,
        const glm::mat4& view, const glm::mat4& projection);

private:
    struct SortedTransparentFace {
        const LMFaceDrawCall* dc = nullptr;
        float distance = 0.0f;
    };

    std::unique_ptr<Shader> lightmappedShader;
    GLuint worldVAO = 0, worldVBO = 0, worldEBO = 0;
    GLuint doorVAO = 0, doorVBO = 0, doorEBO = 0;
    size_t doorVertexCapacity = 0;
    size_t doorIndexCapacity = 0;
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

    std::vector<LMRenderVertex> meshVertices;
    std::vector<unsigned int> meshIndices;
    std::vector<SortedTransparentFace> sortedTransparentCache;
    glm::mat4 cachedProjection = glm::mat4(1.0f);
    bool projectionDirty = true;
    bool blendEnabled = false;
    bool depthWriteEnabled = true;
    GLenum depthFunc = GL_LESS;

    bool initShaders();
    bool buildLightmappedMesh(BSPLoader& bsp, LightmapManager& lmManager);
    void ensureDoorBuffers(size_t vertexCount, size_t indexCount);
    void setDepthState(GLenum func, bool depthWrite);
    void setBlendEnabled(bool enabled);
    void cleanup();
    static const char* getVertexShaderSource();
    static const char* getFragmentShaderSource();
};
