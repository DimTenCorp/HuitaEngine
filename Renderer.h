#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "BSPLoader.h"
#include "Shader.h"
#include "Light.h"
#include "ShadowSystem.h"
#include "WorldRenderer.h"
#include "BrushRenderer.h"
#include "BSPLightmap.h"

struct GBuffer {
    GLuint fbo = 0;
    GLuint positionTex = 0;
    GLuint normalTex = 0;
    GLuint albedoTex = 0;
    GLuint lightingTex = 0;  // Текстура для освещения из lightmap
    GLuint depthTex = 0;
    int width = 1280;
    int height = 720;

    GBuffer() = default;
    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    GBuffer(GBuffer&& other) noexcept;
    GBuffer& operator=(GBuffer&& other) noexcept;
    ~GBuffer();

    bool create(int w, int h);
    void destroy();
    void bindForWriting() const;
    void bindForReading(GLuint texUnitPosition, GLuint texUnitNormal, GLuint texUnitAlbedo, GLuint texUnitLighting) const;
    static void unbind();
};

struct Flashlight {
    bool enabled = false;
    glm::vec3 position{ 0.0f };
    glm::vec3 direction{ 0.0f, 0.0f, -1.0f };
    float cutoffInner = glm::radians(12.5f);
    float cutoffOuter = glm::radians(17.5f);
    float intensity = 3.0f;
};

struct BspMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    size_t indexCount = 0;

    BspMesh() = default;
    BspMesh(const BspMesh&) = delete;
    BspMesh& operator=(const BspMesh&) = delete;
    BspMesh(BspMesh&& other) noexcept;
    BspMesh& operator=(BspMesh&& other) noexcept;
    ~BspMesh();

    bool buildFromBSP(const std::vector<BSPVertex>& vertices,
        const std::vector<unsigned int>& indices);
    void destroy();
    void bind() const;
    static void unbind();
};

struct ShadowMap {
    GLuint fbo = 0;
    GLuint depthMap = 0;
    int size = 512;
    glm::mat4 lightSpaceMatrix;
    
    bool create(int sz);
    void destroy();
    void bindForWriting();
    static void unbind();
};

struct RenderLight {
    // Структура больше не используется - динамическое освещение отключено
    // Оставлена для обратной совместимости
};

class Renderer {
public:
    struct RenderStats {
        int drawCalls = 0;
        int triangles = 0;
        void reset() { drawCalls = 0; triangles = 0; }
    };

    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&& other) noexcept;
    Renderer& operator=(Renderer&& other) noexcept;

    bool init(int width, int height);
    void setViewport(int width, int height);
    bool loadWorld(BSPLoader& bsp);
    void unloadWorld();

    void beginFrame(const glm::vec3& clearColor);
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos, ShadowSystem* shadowSystem);
    void renderHitbox(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& position, bool visible);

    void setShowHitbox(bool show) { showHitbox = show; }
    bool getShowHitbox() const { return showHitbox; }
    const RenderStats& getStats() const { return stats; }

    void setFlashlight(const glm::vec3& pos, const glm::vec3& dir, bool enabled);
    void addLight(const Light& light);
    void clearLights();

    void setSunDirection(const glm::vec3& dir) { sunDirection = glm::normalize(dir); }
    void setSunColor(const glm::vec3& color) { sunColor = color; }
    void setSunIntensity(float intensity) { sunIntensity = glm::max(0.0f, intensity); }
    void setAmbientStrength(float strength) { ambientStrength = glm::clamp(strength, 0.0f, 1.0f); }
    
    void setLightmapTexture(GLuint tex, int size) { lightmapTexture = tex; lightmapSize = size; }

private:
    // Старая система рендеринга (для обратной совместимости)
    BspMesh worldMesh;
    std::vector<FaceDrawCall> drawCalls;
    bool worldLoaded = false;

    // Новая система рендеринга на основе BSPRenderer
    std::unique_ptr<WorldRenderer> worldRenderer;
    std::unique_ptr<BrushRenderer> brushRenderer;
    std::unique_ptr<BSPLightmap> bspLightmap;

    BspMesh hitboxMesh;
    bool showHitbox = true;

    std::unique_ptr<Shader> geometryShader;
    std::unique_ptr<Shader> lightingShader;
    std::unique_ptr<Shader> flashlightShader;

    GBuffer gBuffer;

    struct ShadowFBO {
        GLuint fbo = 0;
        GLuint depthMap = 0;
        int size = 1024;
        bool create(int sz);
        void destroy();
    } shadowFBO;

    RenderStats stats;
    int screenWidth = 1280, screenHeight = 720;
    Flashlight flashlight;

    std::vector<RenderLight> lights;  // Не используется - динамическое освещение отключено

    glm::vec3 sunDirection{ 0.5f, -1.0f, 0.3f };
    glm::vec3 sunColor{ 1.0f, 0.95f, 0.8f };
    float sunIntensity{ 1.0f };
    float ambientStrength{ 0.1f };

    GLuint lightmapTexture = 0;
    int lightmapSize = 128;

    static constexpr int MAX_LIGHTS = 32;

    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    void createQuadMesh();
    void cleanup();
    bool createGBuffer(int w, int h);
    void destroyGBuffer();
    void geometryPass(const glm::mat4& view, const glm::mat4& proj);
    void lightingPass(const glm::mat4& view, const glm::vec3& viewPos, ShadowSystem* shadowSystem);
    void renderShadowPass(const glm::mat4& lightSpaceMatrix);

    static const char* getGeometryVert();
    static const char* getGeometryFrag();
    static const char* getLightingVert();
    static const char* getLightingFrag();
};