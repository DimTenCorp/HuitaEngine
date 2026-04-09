#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "BSPLoader.h"
#include "Shader.h"

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
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos);
    void renderHitbox(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& position, bool visible);

    void setShowHitbox(bool show) { showHitbox = show; }
    bool getShowHitbox() const { return showHitbox; }
    const RenderStats& getStats() const { return stats; }

private:
    BspMesh worldMesh;
    std::vector<FaceDrawCall> drawCalls;
    bool worldLoaded = false;

    BspMesh hitboxMesh;
    bool showHitbox = true;

    std::unique_ptr<Shader> geometryShader;
    std::unique_ptr<Shader> lightingShader;

    GBuffer gBuffer;

    RenderStats stats;
    int screenWidth = 1280, screenHeight = 720;

    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    void createQuadMesh();
    void cleanup();
    bool createGBuffer(int w, int h);
    void destroyGBuffer();
    void geometryPass(const glm::mat4& view, const glm::mat4& proj);
    void lightingPass(const glm::vec3& viewPos);

    static const char* getGeometryVert();
    static const char* getGeometryFrag();
    static const char* getLightingVert();
    static const char* getLightingFrag();
};