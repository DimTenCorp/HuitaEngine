#include "WaterRenderer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ============================================================================
// WaterSurface Implementation
// ============================================================================

WaterSurface::WaterSurface(WaterSurface&& other) noexcept
    : vao(other.vao), vbo(other.vbo), ebo(other.ebo), 
      indexCount(other.indexCount), type(other.type), textureIndex(other.textureIndex) {
    other.vao = 0; other.vbo = 0; other.ebo = 0; 
    other.indexCount = 0; other.textureIndex = -1;
}

WaterSurface& WaterSurface::operator=(WaterSurface&& other) noexcept {
    if (this != &other) {
        destroy();
        vao = other.vao; vbo = other.vbo; ebo = other.ebo;
        indexCount = other.indexCount; type = other.type; textureIndex = other.textureIndex;
        other.vao = 0; other.vbo = 0; other.ebo = 0;
        other.indexCount = 0; other.textureIndex = -1;
    }
    return *this;
}

WaterSurface::~WaterSurface() { destroy(); }

bool WaterSurface::build(const std::vector<WaterPoly>& polys) {
    destroy();
    
    if (polys.empty()) return false;
    
    // Собираем все вершины из полигонов
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<unsigned int> indices;
    
    unsigned int vertexOffset = 0;
    for (const auto& poly : polys) {
        for (size_t i = 0; i < poly.vertices.size(); i++) {
            vertices.push_back(poly.vertices[i]);
            texCoords.push_back(poly.texCoords[i]);
        }
        
        // Создаем индексы для triangle fan
        if (poly.vertices.size() >= 3) {
            for (size_t i = 1; i < poly.vertices.size() - 1; i++) {
                indices.push_back(vertexOffset);
                indices.push_back(vertexOffset + i);
                indices.push_back(vertexOffset + i + 1);
            }
        }
        
        vertexOffset += poly.vertices.size();
    }
    
    if (vertices.empty() || indices.empty()) return false;
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    // Упаковываем вершины и текстурные координаты в один буфер
    struct Vertex {
        float pos[3];
        float tc[2];
    };
    
    std::vector<Vertex> packedVertices(vertices.size());
    for (size_t i = 0; i < vertices.size(); i++) {
        packedVertices[i].pos[0] = vertices[i].x;
        packedVertices[i].pos[1] = vertices[i].y;
        packedVertices[i].pos[2] = vertices[i].z;
        packedVertices[i].tc[0] = texCoords[i].x;
        packedVertices[i].tc[1] = texCoords[i].y;
    }
    
    glBufferData(GL_ARRAY_BUFFER, packedVertices.size() * sizeof(Vertex), 
                 packedVertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), 
                 indices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    indexCount = indices.size();
    
    return true;
}

void WaterSurface::destroy() {
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    indexCount = 0;
}

void WaterSurface::bind() const { glBindVertexArray(vao); }
void WaterSurface::unbind() { glBindVertexArray(0); }

// ============================================================================
// Shader Sources
// ============================================================================

static const char* s_waterVert = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;
out vec3 vFragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform vec3 vertexOffset;

void main() {
    vTexCoord = aTexCoord;
    vFragPos = vec3(model * vec4(aPos, 1.0));
    
    // Warp эффект для воды (как в HL1 GL_WARP.C)
    float warp1 = sin(time * 160.0 + aPos.x + aPos.y) * 8.0 + 8.0;
    float warp2 = sin(time * 171.0 + aPos.x * 5.0 - aPos.y) * 8.0 + 8.0;
    float warpOffset = (warp1 + warp2 * 0.8) * 0.01;
    
    vec3 warpedPos = aPos + vertexOffset * warpOffset;
    gl_Position = projection * view * (model * vec4(warpedPos, 1.0));
}
)glsl";

static const char* s_waterFrag = R"glsl(
#version 330 core
in vec2 vTexCoord;
in vec3 vFragPos;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec3 waterColor;
uniform float alpha;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    
    // Проверяем прозрачность (HL1 color key: синий = прозрачный)
    if (texColor.a < 0.5 || (texColor.r < 0.01 && texColor.g < 0.01 && texColor.b > 0.9)) {
        discard;
    }
    
    // Смешиваем с цветом воды
    FragColor = vec4(mix(texColor.rgb, waterColor, 0.3), alpha);
}
)glsl";

static const char* s_underwaterVert = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)glsl";

static const char* s_underwaterFrag = R"glsl(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSceneTexture;
uniform vec3 underwaterColor;
uniform float fogDensity;
uniform float time;

void main() {
    vec4 sceneColor = texture(uSceneTexture, vTexCoord);
    
    // Эффект подводного тумана (как D_SetFadeColor в HL1)
    vec3 fogColor = underwaterColor;
    float fogFactor = fogDensity * 0.5;
    
    // Небольшая анимация для эффекта волн
    float wave = sin(time * 2.0) * 0.02;
    
    vec3 finalColor = mix(sceneColor.rgb, fogColor, fogFactor + wave);
    
    FragColor = vec4(finalColor, sceneColor.a);
}
)glsl";

const char* WaterRenderer::getWaterVert() { return s_waterVert; }
const char* WaterRenderer::getWaterFrag() { return s_waterFrag; }
const char* WaterRenderer::getUnderwaterVert() { return s_underwaterVert; }
const char* WaterRenderer::getUnderwaterFrag() { return s_underwaterFrag; }

// ============================================================================
// WaterRenderer Implementation
// ============================================================================

WaterRenderer::WaterRenderer() = default;

WaterRenderer::WaterRenderer(WaterRenderer&& other) noexcept
    : waterManager(std::move(other.waterManager)),
      waterSurfaces(std::move(other.waterSurfaces)),
      waterTextures(std::move(other.waterTextures)),
      waterShader(std::move(other.waterShader)),
      underwaterShader(std::move(other.underwaterShader)),
      useWireframe(other.useWireframe),
      initialized(other.initialized),
      quadVAO(other.quadVAO),
      quadVBO(other.quadVBO),
      totalWaterPolys(other.totalWaterPolys),
      totalWaterVertices(other.totalWaterVertices) {
    other.quadVAO = 0;
    other.quadVBO = 0;
    other.initialized = false;
}

WaterRenderer& WaterRenderer::operator=(WaterRenderer&& other) noexcept {
    if (this != &other) {
        unload();
        destroyQuadMesh();
        waterManager = std::move(other.waterManager);
        waterSurfaces = std::move(other.waterSurfaces);
        waterTextures = std::move(other.waterTextures);
        waterShader = std::move(other.waterShader);
        underwaterShader = std::move(other.underwaterShader);
        useWireframe = other.useWireframe;
        initialized = other.initialized;
        quadVAO = other.quadVAO;
        quadVBO = other.quadVBO;
        totalWaterPolys = other.totalWaterPolys;
        totalWaterVertices = other.totalWaterVertices;
        other.quadVAO = 0;
        other.quadVBO = 0;
        other.initialized = false;
    }
    return *this;
}

WaterRenderer::~WaterRenderer() {
    unload();
    destroyQuadMesh();
}

bool WaterRenderer::init() {
    if (initialized) return true;
    
    waterManager.init();
    
    // Компилируем шейдеры для воды
    waterShader = std::make_unique<Shader>();
    if (!waterShader->compile(getWaterVert(), getWaterFrag())) {
        std::cerr << "Water shader fail: " << waterShader->getLastError() << std::endl;
        waterShader.reset();
        return false;
    }
    
    // Компилируем шейдеры для underwater эффекта
    underwaterShader = std::make_unique<Shader>();
    if (!underwaterShader->compile(getUnderwaterVert(), getUnderwaterFrag())) {
        std::cerr << "Underwater shader fail: " << underwaterShader->getLastError() << std::endl;
        waterShader.reset();
        underwaterShader.reset();
        return false;
    }
    
    createQuadMesh();
    
    initialized = true;
    return true;
}

void WaterRenderer::createQuadMesh() {
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

void WaterRenderer::destroyQuadMesh() {
    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
}

bool WaterRenderer::isWaterTexture(const std::string& name) const {
    // В HL1 водные текстуры имеют определенные имена или флаги
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    return lowerName.find("water") != std::string::npos ||
           lowerName.find("slime") != std::string::npos ||
           lowerName.find("lava") != std::string::npos ||
           name[0] == '!';  // В HL1 анимированные текстуры начинаются с !
}

SurfaceType WaterRenderer::getSurfaceTypeFromTexture(const std::string& name) const {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    if (lowerName.find("slime") != std::string::npos || 
        lowerName.find("lava") != std::string::npos) {
        return SurfaceType::Slime;
    }
    
    return SurfaceType::Water;
}

void WaterRenderer::extractWaterSurfaces(BSPLoader& bsp, WADLoader& wad) {
    (void)wad;
    
    const auto& faces = bsp.getFaces();
    const auto& texInfos = bsp.getTexInfos();
    const auto& surfEdges = bsp.getSurfEdges();
    const auto& edges = bsp.getEdges();
    const auto& vertices = bsp.getOriginalVertices();
    
    totalWaterPolys = 0;
    totalWaterVertices = 0;
    
    for (size_t faceIdx = 0; faceIdx < faces.size(); faceIdx++) {
        const auto& face = faces[faceIdx];
        const auto& texInfo = texInfos[face.texInfo];
        
        // Проверяем флаг SURF_DRAWTURB (0x04 в HL1)
        if (texInfo.flags & 0x04) {
            SurfaceType type = getSurfaceTypeFromTexture("water");
            
            std::vector<glm::vec3> faceVerts;
            std::vector<glm::vec2> faceTexCoords;
            
            for (int i = 0; i < face.numEdges; i++) {
                int edgeIdx = surfEdges[face.firstEdge + i];
                int vertIdx;
                
                if (edgeIdx >= 0) {
                    vertIdx = edges[edgeIdx].v[0];
                } else {
                    vertIdx = edges[-edgeIdx].v[1];
                }
                
                if (vertIdx >= 0 && vertIdx < static_cast<int>(vertices.size())) {
                    faceVerts.push_back(vertices[vertIdx]);
                    
                    const glm::vec3& vert = vertices[vertIdx];
                    float s = DotProduct(vert, glm::vec3(texInfo.s[0], texInfo.s[1], texInfo.s[2])) + texInfo.s[3];
                    float t = DotProduct(vert, glm::vec3(texInfo.t[0], texInfo.t[1], texInfo.t[2])) + texInfo.t[3];
                    faceTexCoords.push_back(glm::vec2(s, t));
                }
            }
            
            if (faceVerts.size() >= 3) {
                std::vector<WaterPoly> waterPolys;
                waterManager.subdividePolygon(faceVerts, faceTexCoords, type, waterPolys);
                
                if (!waterPolys.empty()) {
                    WaterSurface surface;
                    if (surface.build(waterPolys)) {
                        surface.type = type;
                        surface.textureIndex = texInfo.textureIndex;
                        waterSurfaces.push_back(std::move(surface));
                        
                        totalWaterPolys += waterPolys.size();
                        for (const auto& poly : waterPolys) {
                            totalWaterVertices += poly.vertices.size();
                        }
                    }
                }
            }
        }
    }
    
    std::cout << "WaterRenderer: Found " << waterSurfaces.size() 
              << " water surfaces (" << totalWaterPolys << " polys, "
              << totalWaterVertices << " vertices)" << std::endl;
}

bool WaterRenderer::loadFromBSP(BSPLoader& bsp, WADLoader& wad) {
    unload();
    
    if (!bsp.isLoaded()) {
        std::cerr << "WaterRenderer: BSP not loaded" << std::endl;
        return false;
    }
    
    extractWaterSurfaces(bsp, wad);
    
    return true;
}

void WaterRenderer::unload() {
    for (auto& surface : waterSurfaces) {
        surface.destroy();
    }
    waterSurfaces.clear();
    
    for (GLuint tex : waterTextures) {
        if (tex) glDeleteTextures(1, &tex);
    }
    waterTextures.clear();
    
    totalWaterPolys = 0;
    totalWaterVertices = 0;
}

void WaterRenderer::update(float deltaTime, float currentTime) {
    waterManager.update(deltaTime, currentTime);
}

void WaterRenderer::render(const glm::mat4& view, const glm::mat4& projection,
                           const glm::vec3& viewPos, float time) {
    if (waterSurfaces.empty()) return;
    
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    
    if (useWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    
    waterShader->bind();
    waterShader->setMat4("view", view);
    waterShader->setMat4("projection", projection);
    waterShader->setFloat("time", time);
    waterShader->setVec3("vertexOffset", glm::vec3(0.0f, 0.0f, 1.0f));
    waterShader->setVec3("waterColor", glm::vec3(0.2f, 0.4f, 0.6f));
    waterShader->setFloat("alpha", 0.7f);
    
    glActiveTexture(GL_TEXTURE0);
    
    for (const auto& surface : waterSurfaces) {
        surface.bind();
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(surface.indexCount), 
                      GL_UNSIGNED_INT, 0);
        WaterSurface::unbind();
    }
    
    waterShader->unbind();
    
    if (useWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void WaterRenderer::renderUnderwaterEffect(int screenWidth, int screenHeight) {
    (void)screenWidth; (void)screenHeight;
    
    if (!isCameraUnderwater(glm::vec3(0))) return;
    
    glDisable(GL_DEPTH_TEST);
    
    underwaterShader->bind();
    underwaterShader->setVec3("underwaterColor", waterManager.getUnderwaterColor());
    underwaterShader->setFloat("fogDensity", waterManager.getUnderwaterFogDensity());
    underwaterShader->setFloat("time", waterManager.currentTime);
    
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    underwaterShader->unbind();
    
    glEnable(GL_DEPTH_TEST);
}

bool WaterRenderer::isCameraUnderwater(const glm::vec3& cameraPos) const {
    return cameraPos.z < 0;
}
