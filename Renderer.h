#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "BSPLoader.h"
#include "Shader.h"

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

    const char* geometryVert = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vFragPos;
void main() {
    vTexCoord = aTexCoord;
    vNormal = aNormal;
    vFragPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

    const char* geometryFrag = R"(
#version 330 core
in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;
layout (location = 0) out vec4 FragPosition;
layout (location = 1) out vec4 FragNormal;
layout (location = 2) out vec4 FragAlbedo;
uniform sampler2D uTexture;
void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.5) discard;
    FragPosition = vec4(vFragPos, 1.0);
    FragNormal = vec4(normalize(vNormal), 1.0);
    FragAlbedo = texColor;
}
)";

    const char* lightingVert = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
)";

    const char* lightingFrag = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform vec3 uViewPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunIntensity;
void main() {
    vec3 fragPos = texture(gPosition, vTexCoord).xyz;
    vec3 normal = normalize(texture(gNormal, vTexCoord).xyz);
    vec4 albedo = texture(gAlbedo, vTexCoord);
    
    // Ambient
    vec3 ambient = 0.1 * albedo.rgb;
    
    // Sun directional light
    float diff = max(dot(normal, -uSunDir), 0.0);
    vec3 diffuse = diff * uSunColor * uSunIntensity;
    
    // View direction for specular
    vec3 viewDir = normalize(uViewPos - fragPos);
    vec3 reflectDir = reflect(uSunDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * uSunColor * uSunIntensity * 0.5;
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result * albedo.rgb, albedo.a);
}
)";
};