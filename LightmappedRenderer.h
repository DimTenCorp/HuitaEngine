#pragma once
#include "Renderer.h"
#include "LightmapManager.h"
#include <memory>

struct BSPVertexLightmapped {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec2 lightmapCoord;
    int lightmapIndex;
};

struct LMFaceDrawCall {
    GLuint texID;
    unsigned int indexOffset;
    unsigned int indexCount;
    int faceIndex;
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

    void beginFrame(const glm::vec3& clearColor);
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos,
        BSPLoader& bsp,
        const glm::vec3& ambientColor = glm::vec3(0.05f));

    void setLightmapIntensity(float intensity) { lightmapIntensity = intensity; }
    float getLightmapIntensity() const { return lightmapIntensity; }

    void setShowLightmapsOnly(bool show) { showLightmapsOnly = show; }
    bool getShowLightmapsOnly() const { return showLightmapsOnly; }

    const Renderer::RenderStats& getStats() const { return stats; }

private:
    std::unique_ptr<Shader> lightmappedShader;
    GLuint worldVAO = 0, worldVBO = 0, worldEBO = 0;
    size_t indexCount = 0;
    LightmapManager* lmManager = nullptr;
    float lightmapIntensity = 1.0f;
    bool showLightmapsOnly = false;
    int screenWidth = 1280, screenHeight = 720;
    Renderer::RenderStats stats;
    GBuffer gBuffer;
    std::vector<LMFaceDrawCall> faceDrawCalls;

    bool initShaders();
    bool buildLightmappedMesh(BSPLoader& bsp, LightmapManager& lmManager);
    void geometryPass(const glm::mat4& view, const glm::mat4& proj);
    void lightingPass(const glm::vec3& viewPos, const glm::vec3& ambient);
    void cleanup();
    static const char* getVertexShaderSource();
    static const char* getFragmentShaderSource();
};