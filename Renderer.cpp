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

    glGenTextures(1, &positionTex);
    glBindTexture(GL_TEXTURE_2D, positionTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, positionTex, 0);

    glGenTextures(1, &normalTex);
    glBindTexture(GL_TEXTURE_2D, normalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex, 0);

    glGenTextures(1, &albedoTex);
    glBindTexture(GL_TEXTURE_2D, albedoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, albedoTex, 0);

    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);

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
    if (vertices.empty() || indices.empty()) {
        std::cerr << "BspMesh: Empty vertices or indices" << std::endl;
        return false;
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BSPVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    indexCount = indices.size();

    std::cout << "BspMesh: Built with " << indexCount << " indices" << std::endl;

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
// Shader Sources - SIMPLIFIED (no lighting)
// ============================================================================

static const char* s_geometryVert = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoord;

uniform mat4 model, view, projection;

void main() {
    vFragPos = vec3(model * vec4(aPos, 1.0));
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vTexCoord = aTexCoord;
    gl_Position = projection * view * vec4(vFragPos, 1.0);
}
)glsl";

static const char* s_geometryFrag = R"glsl(
#version 330 core
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedo;

uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.5) discard;
    
    gPosition = vec4(vFragPos, 1.0);
    gNormal = vec4(normalize(vNormal), 1.0);
    gAlbedo = texColor;
}
)glsl";

static const char* s_lightingVert = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)glsl";

static const char* s_lightingFrag = R"glsl(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;

void main() {
    vec3 fragPos = texture(gPosition, vTexCoord).xyz;
    vec3 normal = normalize(texture(gNormal, vTexCoord).xyz);
    vec4 albedo = texture(gAlbedo, vTexCoord);
    
    if (length(fragPos) < 0.001) {
        FragColor = vec4(0.05, 0.05, 0.05, 1.0);
        return;
    }
    
    // Simple ambient lighting only - no dynamic lights
    vec3 ambient = vec3(0.3) * albedo.rgb;
    
    FragColor = vec4(ambient, albedo.a);
}
)glsl";

const char* Renderer::getGeometryVert() { return s_geometryVert; }
const char* Renderer::getGeometryFrag() { return s_geometryFrag; }
const char* Renderer::getLightingVert() { return s_lightingVert; }
const char* Renderer::getLightingFrag() { return s_lightingFrag; }

// ============================================================================
// Renderer Implementation
// ============================================================================

Renderer::Renderer() = default;

Renderer::Renderer(Renderer&& other) noexcept
    : worldMesh(std::move(other.worldMesh)), hitboxMesh(std::move(other.hitboxMesh)),
    geometryShader(std::move(other.geometryShader)), lightingShader(std::move(other.lightingShader)),
    gBuffer(std::move(other.gBuffer)), stats(other.stats),
    screenWidth(other.screenWidth), screenHeight(other.screenHeight),
    quadVAO(other.quadVAO), quadVBO(other.quadVBO) {
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
        gBuffer = std::move(other.gBuffer);
        stats = other.stats;
        screenWidth = other.screenWidth;
        screenHeight = other.screenHeight;
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

    glViewport(0, 0, width, height);

    geometryShader = std::make_unique<Shader>();
    if (!geometryShader->compile(getGeometryVert(), getGeometryFrag())) {
        std::cerr << "Geometry shader fail: " << geometryShader->getLastError() << std::endl;
        geometryShader.reset();
        return false;
    }

    lightingShader = std::make_unique<Shader>();
    if (!lightingShader->compile(getLightingVert(), getLightingFrag())) {
        std::cerr << "Lighting shader fail: " << lightingShader->getLastError() << std::endl;
        geometryShader.reset();
        lightingShader.reset();
        return false;
    }

    lightingShader->bind();
    lightingShader->setInt("gPosition", 0);
    lightingShader->setInt("gNormal", 1);
    lightingShader->setInt("gAlbedo", 2);
    lightingShader->unbind();

    if (!createGBuffer(width, height)) {
        std::cerr << "Failed to create G-Buffer!" << std::endl;
        cleanup();
        return false;
    }

    createQuadMesh();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    hitboxMesh.buildFromBSP(
        std::vector<BSPVertex>{
            {{-0.5f, -0.5f, -0.5f}, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }},
            { { 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f} },
            { { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f} },
            { { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f} },
            { {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f} },
            { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f} }
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
    if (!bsp.isLoaded()) {
        std::cerr << "Renderer: BSP not loaded" << std::endl;
        return false;
    }

    const auto& vertices = bsp.getMeshVertices();
    const auto& indices = bsp.getMeshIndices();

    std::cout << "Renderer: Loading world with " << vertices.size() << " vertices" << std::endl;

    if (!worldMesh.buildFromBSP(vertices, indices)) {
        std::cerr << "Renderer: Failed to build mesh" << std::endl;
        return false;
    }

    drawCalls = bsp.getDrawCalls();
    std::sort(drawCalls.begin(), drawCalls.end(),
        [](const FaceDrawCall& a, const FaceDrawCall& b) { return a.texID < b.texID; });

    std::cout << "Renderer: World loaded, " << drawCalls.size() << " draw calls" << std::endl;

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
    if (!worldLoaded) return;

    glm::mat4 projection = glm::perspective(glm::radians(75.0f),
        (float)screenWidth / (float)screenHeight, 0.1f, 1000.0f);

    geometryPass(view, projection);
    lightingPass(viewPos);
}

void Renderer::geometryPass(const glm::mat4& view, const glm::mat4& proj) {
    gBuffer.bindForWriting();
    glClear(GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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

void Renderer::lightingPass(const glm::vec3& viewPos) {
    (void)viewPos; // Unused in simplified version

    GBuffer::unbind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    lightingShader->bind();
    gBuffer.bindForReading(GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    lightingShader->unbind();
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

void Renderer::setViewport(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    destroyGBuffer();
    createGBuffer(width, height);
}

void Renderer::cleanup() {
    destroyGBuffer();
    worldMesh.destroy();
    hitboxMesh.destroy();

    geometryShader.reset();
    lightingShader.reset();

    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
}