#include "pch.h"
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
// Shader Sources
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
uniform float uAlpha = 1.0;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    
    // HL1 color key: синий цвет (0,0,1) = прозрачность
    if (texColor.a < 0.5 || (texColor.r < 0.01 && texColor.g < 0.01 && texColor.b > 0.9)) {
        discard;
    }
    
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
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    vec3 ambient = vec3(1.00) * albedo.rgb;
    
    FragColor = vec4(ambient, 1.0);
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
    : worldMesh(std::move(other.worldMesh)),
    meshVertices(std::move(other.meshVertices)),
    meshIndices(std::move(other.meshIndices)),
    opaqueDrawCalls(std::move(other.opaqueDrawCalls)),
    transparentDrawCalls(std::move(other.transparentDrawCalls)),
    hitboxMesh(std::move(other.hitboxMesh)),
    geometryShader(std::move(other.geometryShader)),
    lightingShader(std::move(other.lightingShader)),
    transparentShader(std::move(other.transparentShader)),
    gBuffer(std::move(other.gBuffer)), stats(other.stats),
    screenWidth(other.screenWidth), screenHeight(other.screenHeight),
    quadVAO(other.quadVAO), quadVBO(other.quadVBO),
    showHitbox(other.showHitbox), worldLoaded(other.worldLoaded) {
    other.quadVAO = 0; other.quadVBO = 0;
    other.screenWidth = 1280; other.screenHeight = 720;
    other.worldLoaded = false;
    other.showHitbox = false;
}

Renderer& Renderer::operator=(Renderer&& other) noexcept {
    if (this != &other) {
        cleanup();
        worldMesh = std::move(other.worldMesh);
        meshVertices = std::move(other.meshVertices);
        meshIndices = std::move(other.meshIndices);
        opaqueDrawCalls = std::move(other.opaqueDrawCalls);
        transparentDrawCalls = std::move(other.transparentDrawCalls);
        hitboxMesh = std::move(other.hitboxMesh);
        geometryShader = std::move(other.geometryShader);
        lightingShader = std::move(other.lightingShader);
        transparentShader = std::move(other.transparentShader);
        gBuffer = std::move(other.gBuffer);
        stats = other.stats;
        screenWidth = other.screenWidth;
        screenHeight = other.screenHeight;
        quadVAO = other.quadVAO;
        quadVBO = other.quadVBO;
        showHitbox = other.showHitbox;
        worldLoaded = other.worldLoaded;
        other.quadVAO = 0; other.quadVBO = 0;
        other.screenWidth = 1280; other.screenHeight = 720;
        other.worldLoaded = false;
        other.showHitbox = false;
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
    initTransparentShader();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthRange(0.0f, 1.0f);
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

void Renderer::initTransparentShader() {
    const char* vert = R"glsl(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoord;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        
        out vec2 vTexCoord;
        
        void main() {
            vTexCoord = aTexCoord;
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )glsl";

    const char* frag = R"glsl(
        #version 330 core
        in vec2 vTexCoord;
        out vec4 FragColor;
        
        uniform sampler2D uTexture;
        uniform float uAlpha;
        
        void main() {
            vec4 texColor = texture(uTexture, vTexCoord);
            
            // HL1 color key (синий цвет = прозрачность)
            if (texColor.r < 0.01 && texColor.g < 0.01 && texColor.b > 0.9) {
                discard;
            }
            
            FragColor = vec4(texColor.rgb, texColor.a * uAlpha);
        }
    )glsl";

    transparentShader = std::make_unique<Shader>();
    if (!transparentShader->compile(vert, frag)) {
        std::cerr << "Transparent shader fail: " << transparentShader->getLastError() << std::endl;
        transparentShader.reset();
    }
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

    meshVertices = bsp.getMeshVertices();
    meshIndices = bsp.getMeshIndices();

    std::cout << "Renderer: Loading world with " << meshVertices.size() << " vertices" << std::endl;

    if (!worldMesh.buildFromBSP(meshVertices, meshIndices)) {
        std::cerr << "Renderer: Failed to build mesh" << std::endl;
        return false;
    }

    // Разделяем draw calls на непрозрачные и прозрачные
    auto allDrawCalls = bsp.getDrawCalls();
    opaqueDrawCalls.clear();
    transparentDrawCalls.clear();

    for (const auto& dc : allDrawCalls) {
        if (dc.isTransparent) {
            transparentDrawCalls.push_back(dc);
        }
        else {
            opaqueDrawCalls.push_back(dc);
        }
    }

    std::cout << "Renderer: " << opaqueDrawCalls.size() << " opaque, "
        << transparentDrawCalls.size() << " transparent draw calls" << std::endl;

    worldLoaded = true;
    return true;
}

void Renderer::unloadWorld() {
    worldMesh.destroy();
    meshVertices.clear();
    meshIndices.clear();
    opaqueDrawCalls.clear();
    transparentDrawCalls.clear();
    worldLoaded = false;
}

void Renderer::beginFrame(const glm::vec3& clearColor) {
    stats.reset();
    gBuffer.bindForWriting();
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos) {
    if (!worldLoaded) return;

    glm::mat4 projection = glm::perspective(glm::radians(75.0f),
        (float)screenWidth / (float)screenHeight, 0.1f, 10000.0f);

    // ============================================
    // ПРОХОД 1: Только НЕПРОЗРАЧНАЯ геометрия
    // ============================================
    gBuffer.bindForWriting();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Рисуем НЕПРОЗРАЧНЫЕ draw calls
    geometryPass(view, projection, false); // ← параметр false = только opaque

    // ============================================
    // ПРОХОД 2: Освещение
    // ============================================
    lightingPass(viewPos);

    // ============================================
    // ПРОХОД 3: Прозрачная геометрия (поверх всего)
    // ============================================
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);  // ← КЛЮЧЕВОЙ МОМЕНТ!
    glDepthFunc(GL_LEQUAL);

    // Рисуем ПРОЗРАЧНЫЕ draw calls (отсортированные)
    renderTransparentFacesForward(view, projection, viewPos);

    // Восстанавливаем состояние
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
}

void Renderer::geometryPass(const glm::mat4& view, const glm::mat4& proj, bool onlyTransparent) {
    gBuffer.bindForWriting();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    geometryShader->bind();
    geometryShader->setMat4("model", glm::mat4(1.0f));
    geometryShader->setMat4("view", view);
    geometryShader->setMat4("projection", proj);
    geometryShader->setFloat("uAlpha", 1.0f);

    glActiveTexture(GL_TEXTURE0);
    worldMesh.bind();

    GLuint currentTex = 0;

    // Выбираем какие draw calls рисовать
    const auto& drawCalls = onlyTransparent ? transparentDrawCalls : opaqueDrawCalls;

    for (const auto& dc : drawCalls) {
        if (dc.texID != currentTex) {
            glBindTexture(GL_TEXTURE_2D, dc.texID);
            currentTex = dc.texID;
        }

        glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
            (void*)(dc.indexOffset * sizeof(unsigned int)));

        stats.drawCalls++;
        stats.triangles += dc.indexCount / 3;
    }

    BspMesh::unbind();
    geometryShader->unbind();
}

void Renderer::lightingPass(const glm::vec3& viewPos) {
    (void)viewPos;

    GBuffer::unbind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    lightingShader->bind();
    gBuffer.bindForReading(GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    lightingShader->unbind();

    // ВАЖНО: Восстанавливаем состояние для прозрачных объектов
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderTransparentFacesForward(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos) {
    if (transparentDrawCalls.empty() || !transparentShader) return;

    // Сортируем прозрачные объекты от дальних к ближним
    struct SortedDrawCall {
        FaceDrawCall dc;
        float distance;
    };

    std::vector<SortedDrawCall> sorted;
    sorted.reserve(transparentDrawCalls.size());

    for (const auto& dc : transparentDrawCalls) {
        float dist = 0.0f;
        if (dc.indexCount > 0 && !meshIndices.empty() && !meshVertices.empty()) {
            unsigned int firstIdx = dc.indexOffset;
            if (firstIdx < meshIndices.size()) {
                unsigned int vertIdx = meshIndices[firstIdx];
                if (vertIdx < meshVertices.size()) {
                    glm::vec3 worldPos = meshVertices[vertIdx].position;
                    dist = glm::distance(viewPos, worldPos);
                }
            }
        }
        sorted.push_back({ dc, dist });
    }

    // Сортируем от дальних к ближним
    std::sort(sorted.begin(), sorted.end(), [](const SortedDrawCall& a, const SortedDrawCall& b) {
        return a.distance > b.distance;
        });

    transparentShader->bind();
    transparentShader->setMat4("view", view);
    transparentShader->setMat4("projection", proj);
    transparentShader->setMat4("model", glm::mat4(1.0f));

    worldMesh.bind();

    GLuint currentTex = 0;
    for (const auto& item : sorted) {
        const auto& dc = item.dc;

        if (dc.texID != currentTex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, dc.texID);
            currentTex = dc.texID;
        }

        float alpha = dc.renderamt / 255.0f;
        alpha = std::max(0.05f, std::min(1.0f, alpha));
        transparentShader->setFloat("uAlpha", alpha);

        if (dc.indexCount > 0) {
            glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
                (void*)(dc.indexOffset * sizeof(unsigned int)));

            stats.drawCalls++;
            stats.triangles += dc.indexCount / 3;
        }
    }

    BspMesh::unbind();
    transparentShader->unbind();
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
    glViewport(0, 0, width, height);

    destroyGBuffer();
    createGBuffer(width, height);
}

void Renderer::cleanup() {
    destroyGBuffer();
    worldMesh.destroy();
    hitboxMesh.destroy();

    geometryShader.reset();
    lightingShader.reset();
    transparentShader.reset();

    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
}