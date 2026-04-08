#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <optional>
#include "BSPLoader.h"
#include "Shader.h"

// ============================================================================
// G-Buffer Structure - хранит текстуры и FBO для deferred rendering
// ============================================================================
struct GBuffer {
    GLuint fbo = 0;
    GLuint positionTex = 0;
    GLuint normalTex = 0;
    GLuint albedoTex = 0;
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
    void bindForReading(GLuint texUnitPosition, GLuint texUnitNormal, GLuint texUnitAlbedo) const;
    static void unbind();
};

// ============================================================================
// PointLight Structure - точечный источник света
// ============================================================================
struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
    bool enabled = true;
};

// ============================================================================
// Flashlight Structure - параметры прожектора игрока
// ============================================================================
struct Flashlight {
    bool enabled = false;
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    float cutoffInner = glm::radians(12.5f);
    float cutoffOuter = glm::radians(17.5f);
    float intensity = 3.0f;
};

// ============================================================================
// Mesh Resource - управляет VAO/VBO/EBO для BSP мира
// ============================================================================
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

// ============================================================================
// Renderer Class - основной класс рендеринга
// ============================================================================
class Renderer {
public:
    struct RenderStats {
        int drawCalls = 0;
        int triangles = 0;
        void reset() { drawCalls = 0; triangles = 0; }
    };

    Renderer();
    ~Renderer();

    // Non-copyable, movable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&& other) noexcept;
    Renderer& operator=(Renderer&& other) noexcept;

    bool init(int width, int height);
    void setViewport(int width, int height);
    bool loadWorld(BSPLoader& bsp);
    void unloadWorld();

    void beginFrame(const glm::vec3& clearColor);
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos);
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir);
    void renderHitbox(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& position, bool visible);

    void setShowHitbox(bool show) { showHitbox = show; }
    bool getShowHitbox() const { return showHitbox; }
    const RenderStats& getStats() const { return stats; }

    void setFlashlight(const glm::vec3& pos, const glm::vec3& dir, bool enabled);
    
    // Point light management
    void addPointLight(const PointLight& light);
    void clearPointLights();
    void setAmbientStrength(float strength) { ambientStrength = strength; }

private:
    // Mesh resources
    BspMesh worldMesh;
    std::vector<FaceDrawCall> drawCalls;
    bool worldLoaded = false;

    // Hitbox mesh
    BspMesh hitboxMesh;
    bool showHitbox = true;

    // Shaders
    std::unique_ptr<Shader> geometryShader;
    std::unique_ptr<Shader> lightingShader;
    std::unique_ptr<Shader> flashlightShader;

    // G-Buffer
    GBuffer gBuffer;

    // Flashlight shadow map
    struct ShadowFBO {
        GLuint fbo = 0;
        GLuint depthMap = 0;
        int size = 1024;
        bool create(int sz);
        void destroy();
    } shadowFBO;

    // State
    RenderStats stats;
    int screenWidth = 1280, screenHeight = 720;
    Flashlight flashlight;
    float ambientStrength = 0.1f;
    std::vector<PointLight> pointLights;
    static constexpr int MAX_POINT_LIGHTS = 8;

    // Quad for fullscreen passes
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    void createQuadMesh();
    void cleanup();
    bool createGBuffer(int w, int h);
    void destroyGBuffer();
    void geometryPass(const glm::mat4& view, const glm::mat4& proj);
    void lightingPass(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir);
    void renderFlashlight(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos);

    // Shader sources
    static const char* getGeometryVert();
    static const char* getGeometryFrag();
    static const char* getLightingVert();
    static const char* getLightingFrag();
    static const char* getFlashlightVert();
    static const char* getFlashlightFrag();
};