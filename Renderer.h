#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include "BSPLoader.h"
#include "PointLight.h"
#include "Shader.h"

class ShadowSystem;

enum class LightType {
    BAKED,
    DYNAMIC,
    HYBRID
};

struct LightInstance {
    PointLight* light;
    LightType type;
    int bakedShadowID = -1;
    float lastUpdateTime = 0.0f;
    bool shadowDirty = true;
    std::vector<int> affectingBrushes;
};

class Renderer {
public:
    static constexpr int MAX_LIGHTS = 256;
    static constexpr int MAX_REALTIME_SHADOWS = 4;
    static constexpr int SHADOW_ATLAS_SIZE = 2048;

    struct RenderStats {
        int drawCalls = 0;
        int triangles = 0;
        int shadowUpdates = 0;
    };

    Renderer();
    ~Renderer();

    bool init(int width, int height);
    void setViewport(int width, int height);
    bool loadWorld(BSPLoader& bsp);
    void unloadWorld();

    void registerLight(PointLight* light, LightType type);
    void setLightDirty(PointLight* light);
    void updateShadows(ShadowSystem* shadowSystem, const glm::vec3& cameraPos, float deltaTime);

    void setFlashlight(const glm::vec3& pos, const glm::vec3& dir, bool enabled);
    bool hasFlashlight() const { return flashlight.enabled; }

    void beginFrame(const glm::vec3& clearColor);

    // ИСПРАВЛЕНО: добавил перегрузку с 2 параметрами
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos);
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir);

    void renderHitbox(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& position, bool visible);
    void renderFaceToAtlas(PointLight* light, int face, GLuint atlasFBO,
        int faceX, int faceY, int tileSize,
        GLuint bspVAO, size_t bspIndexCount);

    void setShowHitbox(bool show) { showHitbox = show; }
    bool getShowHitbox() const { return showHitbox; }
    const RenderStats& getStats() const { return stats; }

    void bakeStaticShadows(ShadowSystem* shadowSystem, BSPLoader& bsp);

private:
    struct GBuffer {
        GLuint fbo = 0;
        GLuint position = 0;
        GLuint normal = 0;
        GLuint albedo = 0;
        GLuint depth = 0;
        int width = 0, height = 0;
    } gBuffer;

    struct ShadowAtlas {
        GLuint fbo = 0;
        GLuint texture = 0;
        int size = 2048;
        int tileSize = 768;  // 3x2 грани по 256 = 768x512, но квадрат 768x768
        int maxLights = 0;
        int facesPerRow = 0;

        struct Slot {
            bool occupied = false;
            PointLight* owner = nullptr;
            int x, y;
        };
        std::vector<Slot> slots;
    } shadowAtlas;

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
    bool createShadowAtlas(int size, int tileSize);
    void destroyGBuffer();

    int allocateShadowSlot(PointLight* light);
    void freeShadowSlot(int slot);
    void updateShadowAtlas(ShadowSystem* system, const std::vector<LightInstance*>& lightsToUpdate);

    void geometryPass(const glm::mat4& view, const glm::mat4& proj);
    void lightingPass(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir);
    void renderFlashlightShadow(ShadowSystem* shadowSystem);

    Shader* geometryShader = nullptr;
    Shader* lightingShader = nullptr;
    Shader* forwardShader = nullptr;

    GLuint bspVAO = 0, bspVBO = 0, bspEBO = 0;
    size_t bspIndexCount = 0;
    std::vector<FaceDrawCall> drawCalls;
    bool worldLoaded = false;

    GLuint cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;
    bool showHitbox = true;

    std::vector<LightInstance> lightInstances;
    std::unordered_map<PointLight*, int> lightToIndex;

    GLuint lightsUBO = 0;
    void updateLightsBuffer();

    GLuint depthProgram = 0;
    GLuint spotProgram = 0;

    std::vector<LightInstance*> getLightsNeedingShadows(const glm::vec3& cameraPos, int maxCount);

    RenderStats stats;
    int screenWidth = 1280, screenHeight = 720;
    float currentTime = 0.0f;

    void createHitboxMesh();
    void cleanup();

    // Шейдеры
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
uniform sampler2D shadowAtlas;
uniform sampler2D flashlightShadow;

uniform vec3 uViewPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunIntensity;

uniform int uFlashlightEnabled;
uniform vec3 uFlashlightPos;
uniform vec3 uFlashlightDir;
uniform float uFlashlightRadius;
uniform float uFlashlightAngle;
uniform mat4 uFlashlightMatrix;

uniform int uAtlasTilesPerRow;
uniform float uAtlasTileSize;
uniform int shadowAtlasSize;

struct Light {
    vec4 position_radius;
    vec4 color_intensity;
    ivec4 shadowSlot_type;
};

layout (std140) uniform LightsBlock {
    Light lights[256];
    int uNumLights;
};

float sampleShadowAtlas(int slot, vec3 fragPos, vec3 lightPos, float radius) {
    if (slot < 0) return 1.0;
    
    vec3 lightToFrag = fragPos - lightPos;
    float currentDepth = length(lightToFrag);

    if (currentDepth > radius) return 0.0;

    vec3 dir = normalize(lightToFrag);
    
    vec3 absDir = abs(dir);
    int face;
    vec2 uv;
    
    if (absDir.x > absDir.y && absDir.x > absDir.z) {
        face = dir.x > 0.0 ? 0 : 1;
        uv = vec2(-dir.z / absDir.x, -dir.y / absDir.x);
    } else if (absDir.y > absDir.z) {
        face = dir.y > 0.0 ? 2 : 3;
        uv = vec2(dir.x / absDir.y, dir.z / absDir.y);
    } else {
        face = dir.z > 0.0 ? 4 : 5;
        uv = vec2(dir.x / absDir.z, -dir.y / absDir.z);
    }
    
    uv = uv * 0.5 + 0.5;
    
    int tilesPerRow = uAtlasTilesPerRow;
    int cubemapsPerRow = tilesPerRow / 3;
    
    int cubeX = (slot % cubemapsPerRow) * 3;
    int cubeY = (slot / cubemapsPerRow) * 2;
    
    int faceX = cubeX + (face % 3);
    int faceY = cubeY + (face / 3);
    
    vec2 atlasUV = vec2(
        (float(faceX) + uv.x) / float(tilesPerRow),
        (float(faceY) + uv.y) / float(tilesPerRow)
    );
    
    float closestDepth = texture(shadowAtlas, atlasUV).r;
    float bias = max(0.01 * (1.0 - dot(normalize(fragPos - lightPos), normalize(vec3(0,1,0)))), 0.005);

    float shadow = (currentDepth > closestDepth + bias) ? 0.0 : 1.0;

    return shadow;
}

float calcFlashlightShadow(vec3 worldPos) {
    vec4 lightSpace = uFlashlightMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpace.xyz / lightSpace.w * 0.5 + 0.5;
    
    if (projCoords.z > 1.0) return 0.0;
    
    float closest = texture(flashlightShadow, projCoords.xy).r;
    float current = projCoords.z;
    float bias = 0.005;
    
    return (current > closest + bias) ? 0.0 : 1.0;
}

void main() {
    vec3 worldPos = texture(gPosition, vTexCoord).rgb;
    vec3 normal = normalize(texture(gNormal, vTexCoord).rgb);
    vec3 albedo = texture(gAlbedo, vTexCoord).rgb;
    
    vec3 result = albedo * 0.05;
    
    vec3 sunDirNorm = normalize(-uSunDir);
    float sunDiff = max(dot(normal, sunDirNorm), 0.0);
    result += albedo * uSunColor * sunDiff * uSunIntensity;
    
    for(int i = 0; i < uNumLights && i < 256; i++) {
        Light light = lights[i];
        
        vec3 lightPos = light.position_radius.xyz;
        float radius = light.position_radius.w;
        vec3 lightColor = light.color_intensity.xyz;
        float intensity = light.color_intensity.w;
        int shadowSlot = light.shadowSlot_type.x;
        
        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);
        if(dist > radius) continue;
        
        vec3 lightDir = normalize(toLight);
        float diff = max(dot(normal, lightDir), 0.0);
        if(diff < 0.001) continue;
        
        float att = 1.0 / (1.0 + dist * 0.5 + dist * dist * 0.05);
        float shadow = sampleShadowAtlas(shadowSlot, worldPos, lightPos, radius);
        
        result += albedo * lightColor * diff * intensity * att * shadow;
    }
    
    if (uFlashlightEnabled > 0) {
        vec3 toLight = uFlashlightPos - worldPos;
        float dist = length(toLight);
        if (dist < uFlashlightRadius) {
            vec3 lightDir = normalize(toLight);
            float diff = max(dot(normal, lightDir), 0.0);
            
            float cosAngle = dot(-lightDir, normalize(uFlashlightDir));
            float spotAtt = smoothstep(cos(radians(uFlashlightAngle + 5.0)), 
                                       cos(radians(uFlashlightAngle)), cosAngle);
            
            float att = 1.0 / (1.0 + dist * 0.3 + dist * dist * 0.1);
            float shadow = calcFlashlightShadow(worldPos);
            
            result += albedo * vec3(1.0, 0.95, 0.8) * diff * att * spotAtt * shadow * 2.0;
        }
    }
    
    float fogDist = length(uViewPos - worldPos);
    float fog = exp(-fogDist * 0.008);
    result = mix(vec3(0.02, 0.02, 0.04), result, clamp(fog, 0.0, 1.0));
    
    result = pow(result, vec3(1.0/2.2));
    
    FragColor = vec4(result, 1.0);
}
)";
};