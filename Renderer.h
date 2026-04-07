#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
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

    void setFlashlight(const glm::vec3& pos, const glm::vec3& dir, bool enabled);
    bool hasFlashlight() const { return flashlight.enabled; }

    void beginFrame(const glm::vec3& clearColor);

    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos);
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir);

    void renderHitbox(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& position, bool visible);

    void setShowHitbox(bool show) { showHitbox = show; }
    bool getShowHitbox() const { return showHitbox; }
    const RenderStats& getStats() const { return stats; }

private:
    struct GBuffer {
        GLuint fbo = 0;
        GLuint position = 0;
        GLuint normal = 0;
        GLuint albedo = 0;
        GLuint depth = 0;
        int width = 0, height = 0;
    } gBuffer;

    struct Flashlight {
        bool enabled = false;
        glm::vec3 position = glm::vec3(0);
        glm::vec3 direction = glm::vec3(0, 0, -1);
        float radius = 30.0f;
        float angle = 30.0f;
        GLuint spotShadowMap = 0;
        GLuint spotFBO = 0;
        glm::mat4 viewProj = glm::mat4(1.0f);
    } flashlight;

    bool createGBuffer(int w, int h);
    void destroyGBuffer();

    void geometryPass(const glm::mat4& view, const glm::mat4& proj);
    void lightingPass(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir);

    Shader* geometryShader = nullptr;
    Shader* lightingShader = nullptr;
    Shader* forwardShader = nullptr;

    GLuint bspVAO = 0, bspVBO = 0, bspEBO = 0;
    size_t bspIndexCount = 0;
    std::vector<FaceDrawCall> drawCalls;
    bool worldLoaded = false;

    GLuint cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;
    bool showHitbox = true;

    RenderStats stats;
    int screenWidth = 1280, screenHeight = 720;

    void createHitboxMesh();
    void cleanup();

    const char* geometryVert = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vTexCoord = aTexCoord;
    gl_Position = projection * view * worldPos;
}
)";

    const char* geometryFrag = R"(
#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;

uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if(texColor.a < 0.1) discard;
    
    gPosition = vWorldPos;
    gNormal = normalize(vNormal);
    gAlbedo.rgb = texColor.rgb;
    gAlbedo.a = 1.0;
}
)";

    const char* lightingVert = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

    const char* lightingFrag = R"(
#version 330 core
out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;

uniform vec3 uViewPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunIntensity;

void main() {
    vec3 worldPos = texture(gPosition, vTexCoord).rgb;
    vec3 normal = normalize(texture(gNormal, vTexCoord).rgb);
    vec3 albedo = texture(gAlbedo, vTexCoord).rgb;
    
    vec3 result = albedo * 0.05;
    
    vec3 sunDirNorm = normalize(-uSunDir);
    float sunDiff = max(dot(normal, sunDirNorm), 0.0);
    result += albedo * uSunColor * sunDiff * uSunIntensity;
    
    float fogDist = length(uViewPos - worldPos);
    float fog = exp(-fogDist * 0.008);
    result = mix(vec3(0.02, 0.02, 0.04), result, clamp(fog, 0.0, 1.0));
    
    result = pow(result, vec3(1.0/2.2));
    
    FragColor = vec4(result, 1.0);
}
)";
};
