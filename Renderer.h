#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "BSPLoader.h"
#include "Shader.h"

class Renderer {
public:
    struct RenderStats {
        int drawCalls = 0;
        int triangles = 0;
    };

    Renderer();
    ~Renderer();

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

private:
    struct GBuffer {
        GLuint fbo = 0;
        GLuint position = 0;
        GLuint normal = 0;
        GLuint albedo = 0;
        GLuint depth = 0;
        int width = 1280;
        int height = 720;
    };

    struct Flashlight {
        bool enabled = false;
        glm::vec3 position;
        glm::vec3 direction;
        GLuint spotFBO = 0;
        GLuint spotShadowMap = 0;
    };

    GLuint bspVAO = 0, bspVBO = 0, bspEBO = 0;
    size_t bspIndexCount = 0;
    std::vector<FaceDrawCall> drawCalls;
    bool worldLoaded = false;

    GLuint cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;
    bool showHitbox = true;

    Shader* geometryShader = nullptr;
    Shader* lightingShader = nullptr;
    Shader* forwardShader = nullptr;

    GBuffer gBuffer;
    Flashlight flashlight;

    RenderStats stats;
    int screenWidth = 1280, screenHeight = 720;

    void createHitboxMesh();
    void cleanup();
    bool createGBuffer(int w, int h);
    void destroyGBuffer();
    void geometryPass(const glm::mat4& view, const glm::mat4& proj);
    void lightingPass(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir);

    static const char* getGeometryVert();
    static const char* getGeometryFrag();
    static const char* getLightingVert();
    static const char* getLightingFrag();
};