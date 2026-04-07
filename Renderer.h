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
    void renderHitbox(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& position, bool visible);

    void setShowHitbox(bool show) { showHitbox = show; }
    bool getShowHitbox() const { return showHitbox; }
    const RenderStats& getStats() const { return stats; }

private:
    GLuint bspVAO = 0, bspVBO = 0, bspEBO = 0;
    size_t bspIndexCount = 0;
    std::vector<FaceDrawCall> drawCalls;
    bool worldLoaded = false;

    GLuint cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;
    bool showHitbox = true;

    Shader* forwardShader = nullptr;

    RenderStats stats;
    int screenWidth = 1280, screenHeight = 720;

    void createHitboxMesh();
    void cleanup();

    const char* forwardVert = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 mvp;
uniform mat4 model;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = mvp * vec4(aPos, 1.0);
}
)";

    const char* forwardFrag = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec3 color;
void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.5) discard;
    FragColor = vec4(texColor.rgb * color, texColor.a);
}
)";
};