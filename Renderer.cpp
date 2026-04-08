#include "Renderer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ============================================================================
// GBuffer Implementation
// ============================================================================

GBuffer::GBuffer(GBuffer&& other) noexcept 
    : fbo(other.fbo), positionTex(other.positionTex), normalTex(other.normalTex),
      albedoTex(other.albedoTex), depthTex(other.depthTex), 
      width(other.width), height(other.height) {
    other.fbo = 0; other.positionTex = 0; other.normalTex = 0;
    other.albedoTex = 0; other.depthTex = 0; other.width = 0; other.height = 0;
}

GBuffer& GBuffer::operator=(GBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        fbo = other.fbo; positionTex = other.positionTex; normalTex = other.normalTex;
        albedoTex = other.albedoTex; depthTex = other.depthTex;
        width = other.width; height = other.height;
        other.fbo = 0; other.positionTex = 0; other.normalTex = 0;
        other.albedoTex = 0; other.depthTex = 0; other.width = 0; other.height = 0;
    }
    return *this;
}

GBuffer::~GBuffer() { destroy(); }

bool GBuffer::create(int w, int h) {
    destroy();
    width = w; height = h;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Position texture
    glGenTextures(1, &positionTex);
    glBindTexture(GL_TEXTURE_2D, positionTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, positionTex, 0);

    // Normal texture
    glGenTextures(1, &normalTex);
    glBindTexture(GL_TEXTURE_2D, normalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex, 0);

    // Albedo texture
    glGenTextures(1, &albedoTex);
    glBindTexture(GL_TEXTURE_2D, albedoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, albedoTex, 0);

    // Depth texture
    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);

    // Setup draw buffers
    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!complete) std::cerr << "G-Buffer incomplete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return complete;
}

void GBuffer::destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (positionTex) { glDeleteTextures(1, &positionTex); positionTex = 0; }
    if (normalTex) { glDeleteTextures(1, &normalTex); normalTex = 0; }
    if (albedoTex) { glDeleteTextures(1, &albedoTex); albedoTex = 0; }
    if (depthTex) { glDeleteTextures(1, &depthTex); depthTex = 0; }
}

void GBuffer::bindForWriting() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void GBuffer::bindForReading(GLuint texUnitPosition, GLuint texUnitNormal, GLuint texUnitAlbedo) const {
    glActiveTexture(texUnitPosition);
    glBindTexture(GL_TEXTURE_2D, positionTex);
    glActiveTexture(texUnitNormal);
    glBindTexture(GL_TEXTURE_2D, normalTex);
    glActiveTexture(texUnitAlbedo);
    glBindTexture(GL_TEXTURE_2D, albedoTex);
}

void GBuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

// ============================================================================
// BspMesh Implementation
// ============================================================================

BspMesh::BspMesh(BspMesh&& other) noexcept
    : vao(other.vao), vbo(other.vbo), ebo(other.ebo), indexCount(other.indexCount) {
    other.vao = 0; other.vbo = 0; other.ebo = 0; other.indexCount = 0;
}

BspMesh& BspMesh::operator=(BspMesh&& other) noexcept {
    if (this != &other) {
        destroy();
        vao = other.vao; vbo = other.vbo; ebo = other.ebo; indexCount = other.indexCount;
        other.vao = 0; other.vbo = 0; other.ebo = 0; other.indexCount = 0;
    }
    return *this;
}

BspMesh::~BspMesh() { destroy(); }

bool BspMesh::buildFromBSP(const std::vector<BSPVertex>& vertices, 
                           const std::vector<unsigned int>& indices) {
    destroy();
    if (vertices.empty() || indices.empty()) return false;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BSPVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), 
                          (void*)offsetof(BSPVertex, position));
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), 
                          (void*)offsetof(BSPVertex, normal));
    glEnableVertexAttribArray(1);
    // TexCoord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), 
                          (void*)offsetof(BSPVertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    indexCount = indices.size();
    return true;
}

void BspMesh::destroy() {
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    indexCount = 0;
}

void BspMesh::bind() const { glBindVertexArray(vao); }
void BspMesh::unbind() { glBindVertexArray(0); }

// ============================================================================
// ShadowFBO Implementation
// ============================================================================

bool Renderer::ShadowFBO::create(int sz) {
    destroy();
    size = sz;

    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &depthMap);

    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, sz, sz, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return complete;
}

void Renderer::ShadowFBO::destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (depthMap) { glDeleteTextures(1, &depthMap); depthMap = 0; }
}

// ============================================================================
// Shader Sources - вынесены в отдельные статические строки
// ============================================================================

static const char* s_geometryVert = R"glsl(
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
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vNormal = normalMatrix * aNormal;
    vFragPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)glsl";

static const char* s_geometryFrag = R"glsl(
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
)glsl";

static const char* s_lightingVert = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
)glsl";

static const char* s_lightingFrag = R"glsl(
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
)glsl";

static const char* s_flashlightVert = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vFragPos;
out vec4 vFragPosLightSpace;

void main() {
    vTexCoord = aTexCoord;
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vNormal = normalMatrix * aNormal;
    vFragPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vFragPosLightSpace = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
)glsl";

static const char* s_flashlightFrag = R"glsl(
#version 330 core
in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform sampler2D shadowMap;
uniform vec3 uLightPos;
uniform vec3 uLightDir;
uniform float uCutOffInner;
uniform float uCutOffOuter;
uniform float uIntensity;

float calcShadow() {
    vec3 projCoords = vFragPosLightSpace.xyz / vFragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.005;
    float shadow = currentDepth - bias > closestDepth ? 0.5 : 0.0;
    return shadow;
}

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.5) discard;
    
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float theta = dot(lightDir, normalize(-uLightDir));
    float epsilon = cos(uCutOffOuter) - cos(uCutOffInner);
    float intensity = clamp((theta - cos(uCutOffOuter)) / epsilon, 0.0, 1.0);
    
    float diff = max(dot(normal, lightDir), 0.0);
    float shadow = calcShadow();
    vec3 lighting = (1.0 - shadow) * diff * intensity * uIntensity;
    
    FragColor = vec4(lighting * texColor.rgb, texColor.a);
}
)glsl";

const char* Renderer::getGeometryVert() { return s_geometryVert; }
const char* Renderer::getGeometryFrag() { return s_geometryFrag; }
const char* Renderer::getLightingVert() { return s_lightingVert; }
const char* Renderer::getLightingFrag() { return s_lightingFrag; }
const char* Renderer::getFlashlightVert() { return s_flashlightVert; }
const char* Renderer::getFlashlightFrag() { return s_flashlightFrag; }

// ============================================================================
// Renderer Implementation
// ============================================================================

Renderer::Renderer() = default;

Renderer::Renderer(Renderer&& other) noexcept
    : worldMesh(std::move(other.worldMesh)), hitboxMesh(std::move(other.hitboxMesh)),
      geometryShader(std::move(other.geometryShader)), lightingShader(std::move(other.lightingShader)),
      flashlightShader(std::move(other.flashlightShader)), gBuffer(std::move(other.gBuffer)),
      shadowFBO(std::move(other.shadowFBO)), stats(other.stats),
      screenWidth(other.screenWidth), screenHeight(other.screenHeight),
      flashlight(other.flashlight), quadVAO(other.quadVAO), quadVBO(other.quadVBO) {
    other.quadVAO = 0; other.quadVBO = 0;
    other.screenWidth = 1280; other.screenHeight = 720;
}

Renderer& Renderer::operator=(Renderer&& other) noexcept {
    if (this != &other) {
        cleanup();
        worldMesh = std::move(other.worldMesh);
        hitboxMesh = std::move(other.hitboxMesh);
        geometryShader = std::move(other.geometryShader);
        lightingShader = std::move(other.lightingShader);
        flashlightShader = std::move(other.flashlightShader);
        gBuffer = std::move(other.gBuffer);
        shadowFBO = std::move(other.shadowFBO);
        stats = other.stats;
        screenWidth = other.screenWidth;
        screenHeight = other.screenHeight;
        flashlight = other.flashlight;
        quadVAO = other.quadVAO;
        quadVBO = other.quadVBO;
        other.quadVAO = 0; other.quadVBO = 0;
        other.screenWidth = 1280; other.screenHeight = 720;
    }
    return *this;
}

Renderer::~Renderer() { cleanup(); }

bool Renderer::init(int width, int height) {
    screenWidth = width;
    screenHeight = height;

    // Create geometry shader
    geometryShader = std::make_unique<Shader>();
    if (!geometryShader->compile(getGeometryVert(), getGeometryFrag())) {
        std::cerr << "Geometry shader fail: " << geometryShader->getLastError() << std::endl;
        geometryShader.reset();
        return false;
    }

    // Create lighting shader
    lightingShader = std::make_unique<Shader>();
    if (!lightingShader->compile(getLightingVert(), getLightingFrag())) {
        std::cerr << "Lighting shader fail: " << lightingShader->getLastError() << std::endl;
        geometryShader.reset();
        lightingShader.reset();
        return false;
    }

    // Setup lighting shader uniforms
    lightingShader->bind();
    lightingShader->setInt("gPosition", 0);
    lightingShader->setInt("gNormal", 1);
    lightingShader->setInt("gAlbedo", 2);
    lightingShader->unbind();

    // Create flashlight shader
    flashlightShader = std::make_unique<Shader>();
    if (!flashlightShader->compile(getFlashlightVert(), getFlashlightFrag())) {
        std::cerr << "Flashlight shader fail: " << flashlightShader->getLastError() << std::endl;
        flashlightShader.reset();
    } else {
        flashlightShader->bind();
        flashlightShader->setInt("uTexture", 0);
        flashlightShader->setInt("shadowMap", 1);
        flashlightShader->unbind();
        
        // Create shadow map FBO
        if (!shadowFBO.create(1024)) {
            std::cerr << "Failed to create shadow FBO" << std::endl;
        }
    }

    // Create G-Buffer
    if (!createGBuffer(width, height)) {
        std::cerr << "Failed to create G-Buffer!" << std::endl;
        geometryShader.reset();
        lightingShader.reset();
        flashlightShader.reset();
        return false;
    }

    // Create fullscreen quad
    createQuadMesh();

    // Enable OpenGL states
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    // Create hitbox mesh
    hitboxMesh.buildFromBSP(
        std::vector<BSPVertex>{
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}
        },
        {}
    );

    return true;
}

void Renderer::createQuadMesh() {
    if (quadVAO != 0) return;

    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

bool Renderer::createGBuffer(int w, int h) {
    return gBuffer.create(w, h);
}

void Renderer::destroyGBuffer() {
    gBuffer.destroy();
}

bool Renderer::loadWorld(BSPLoader& bsp) {
    unloadWorld();
    if (!bsp.isLoaded()) return false;

    if (!worldMesh.buildFromBSP(bsp.getMeshVertices(), bsp.getMeshIndices())) {
        return false;
    }

    drawCalls = bsp.getDrawCalls();
    std::sort(drawCalls.begin(), drawCalls.end(),
        [](const FaceDrawCall& a, const FaceDrawCall& b) { return a.texID < b.texID; });

    worldLoaded = true;
    return true;
}

void Renderer::unloadWorld() {
    worldMesh.destroy();
    drawCalls.clear();
    worldLoaded = false;
}

void Renderer::beginFrame(const glm::vec3& clearColor) {
    stats.reset();
    gBuffer.bindForWriting();
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos) {
    renderWorld(view, viewPos, glm::vec3(0.5f, -1.0f, 0.3f));
}

void Renderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir) {
    if (!worldLoaded) return;

    glm::mat4 projection = glm::perspective(glm::radians(75.0f),
        (float)screenWidth / screenHeight, 0.1f, 1000.0f);

    geometryPass(view, projection);
    lightingPass(view, viewPos, sunDir);
    
    if (flashlight.enabled) {
        renderFlashlight(view, projection, viewPos);
    }
}

void Renderer::geometryPass(const glm::mat4& view, const glm::mat4& proj) {
    gBuffer.bindForWriting();
    glClear(GL_DEPTH_BUFFER_BIT);

    geometryShader->bind();
    geometryShader->setMat4("model", glm::mat4(1.0f));
    geometryShader->setMat4("view", view);
    geometryShader->setMat4("projection", proj);

    glActiveTexture(GL_TEXTURE0);
    worldMesh.bind();

    for (const auto& dc : drawCalls) {
        glBindTexture(GL_TEXTURE_2D, dc.texID);
        glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
            (void*)(dc.indexOffset * sizeof(unsigned int)));
        stats.drawCalls++;
        stats.triangles += dc.indexCount / 3;
    }
    
    BspMesh::unbind();
    geometryShader->unbind();
}

void Renderer::lightingPass(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir) {
    GBuffer::unbind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    lightingShader->bind();
    gBuffer.bindForReading(GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2);

    lightingShader->setVec3("uViewPos", viewPos);
    lightingShader->setVec3("uSunDir", sunDir);
    lightingShader->setVec3("uSunColor", glm::vec3(1.0f, 0.95f, 0.8f));
    lightingShader->setFloat("uSunIntensity", 1.0f);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    lightingShader->unbind();
}

void Renderer::renderFlashlight(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos) {
    if (!flashlightShader || !flashlight.enabled) return;

    // Render shadow map from light perspective
    float aspect = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 50.0f;
    float fov = glm::radians(45.0f);
    
    glm::mat4 lightProjection = glm::perspective(fov, aspect, nearPlane, farPlane);
    glm::mat4 lightView = glm::lookAt(flashlight.position, 
                                       flashlight.position + flashlight.direction, 
                                       glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    // Shadow pass
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);

    flashlightShader->bind();
    flashlightShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowFBO.depthMap);

    glActiveTexture(GL_TEXTURE0);
    worldMesh.bind();

    for (const auto& dc : drawCalls) {
        glBindTexture(GL_TEXTURE_2D, dc.texID);
        glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
            (void*)(dc.indexOffset * sizeof(unsigned int)));
    }
    BspMesh::unbind();

    // Note: Flashlight rendering would be done as a screen-space overlay
    // For now we just render the shadow pass - full implementation would add
    // a second pass to blend the flashlight illumination onto the scene
    
    GBuffer::unbind();
    flashlightShader->unbind();
}

void Renderer::renderHitbox(const glm::mat4& view, const glm::mat4& projection,
    const glm::vec3& position, bool visible) {
    if (!visible || !showHitbox) return;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);

    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    hitboxMesh.bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    BspMesh::unbind();
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
}

void Renderer::setFlashlight(const glm::vec3& pos, const glm::vec3& dir, bool enabled) {
    flashlight.enabled = enabled;
    if (enabled) {
        flashlight.position = pos;
        flashlight.direction = dir;
    }
}

void Renderer::setViewport(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    destroyGBuffer();
    createGBuffer(width, height);
}

void Renderer::cleanup() {
    destroyGBuffer();
    shadowFBO.destroy();
    worldMesh.destroy();
    hitboxMesh.destroy();

    geometryShader.reset();
    lightingShader.reset();
    flashlightShader.reset();

    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
}
