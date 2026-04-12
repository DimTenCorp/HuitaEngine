#include "pch.h"
#include "LightmappedRenderer.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ============================================================================
// ШЕЙДЕРЫ (исправлены!)
// ============================================================================

const char* LightmappedRenderer::getVertexShaderSource() {
    return R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec2 aLightmapCoord;

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec2 vLightmapCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vFragPos = vec3(model * vec4(aPos, 1.0));
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vTexCoord = aTexCoord;
    vLightmapCoord = aLightmapCoord;
    
    gl_Position = projection * view * vec4(vFragPos, 1.0);
}
)glsl";
}

const char* LightmappedRenderer::getFragmentShaderSource() {
    return R"glsl(
#version 330 core
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec2 vLightmapCoord;

out vec4 FragColor;

uniform sampler2D albedoTexture;
uniform sampler2D lightmapAtlas;

uniform float lightmapIntensity;
uniform bool showLightmapsOnly;
uniform vec3 ambientColor;
uniform float uAlpha = 1.0;

void main() {
    vec4 albedo = texture(albedoTexture, vTexCoord);
    
    if (albedo.a < 0.5) discard;
    if (albedo.r < 0.01 && albedo.g < 0.01 && albedo.b > 0.9) discard;
    
    vec3 lightmap = texture(lightmapAtlas, vLightmapCoord).rgb;
    
    if (showLightmapsOnly) {
        FragColor = vec4(lightmap, 1.0);
        return;
    }
    
    vec3 lighting = lightmap * lightmapIntensity;
    vec3 finalColor = albedo.rgb * lighting;
    
    FragColor = vec4(finalColor, albedo.a * uAlpha);
}
)glsl";
}

// ============================================================================
// Implementation
// ============================================================================

LightmappedRenderer::LightmappedRenderer() = default;

LightmappedRenderer::~LightmappedRenderer() {
    cleanup();
}

LightmappedRenderer::LightmappedRenderer(LightmappedRenderer&& other) noexcept
    : lightmappedShader(std::move(other.lightmappedShader)),
    worldVAO(other.worldVAO), worldVBO(other.worldVBO), worldEBO(other.worldEBO),
    indexCount(other.indexCount), lmManager(other.lmManager),
    lightmapIntensity(other.lightmapIntensity),
    showLightmapsOnly(other.showLightmapsOnly),
    screenWidth(other.screenWidth), screenHeight(other.screenHeight),
    stats(other.stats), gBuffer(std::move(other.gBuffer)),
    faceDrawCalls(std::move(other.faceDrawCalls)) {
    other.worldVAO = 0;
    other.worldVBO = 0;
    other.worldEBO = 0;
    other.indexCount = 0;
    other.lmManager = nullptr;
}

LightmappedRenderer& LightmappedRenderer::operator=(LightmappedRenderer&& other) noexcept {
    if (this != &other) {
        cleanup();
        lightmappedShader = std::move(other.lightmappedShader);
        worldVAO = other.worldVAO;
        worldVBO = other.worldVBO;
        worldEBO = other.worldEBO;
        indexCount = other.indexCount;
        lmManager = other.lmManager;
        lightmapIntensity = other.lightmapIntensity;
        showLightmapsOnly = other.showLightmapsOnly;
        screenWidth = other.screenWidth;
        screenHeight = other.screenHeight;
        stats = other.stats;
        gBuffer = std::move(other.gBuffer);
        faceDrawCalls = std::move(other.faceDrawCalls);

        other.worldVAO = 0;
        other.worldVBO = 0;
        other.worldEBO = 0;
        other.indexCount = 0;
        other.lmManager = nullptr;
    }
    return *this;
}


bool LightmappedRenderer::init(int width, int height) {
    screenWidth = width;
    screenHeight = height;

    glViewport(0, 0, width, height);

    if (!initShaders()) {
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    return true;
}

void LightmappedRenderer::setViewport(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    glViewport(0, 0, width, height);
}

bool LightmappedRenderer::initShaders() {
    lightmappedShader = std::make_unique<Shader>();
    if (!lightmappedShader->compile(getVertexShaderSource(), getFragmentShaderSource())) {
        std::cerr << "Lightmapped shader failed: " << lightmappedShader->getLastError() << std::endl;
        return false;
    }

    lightmappedShader->bind();
    lightmappedShader->setInt("albedoTexture", 0);  // Юнит 0 - альбедо
    lightmappedShader->setInt("lightmapAtlas", 1);  // Юнит 1 - lightmap
    lightmappedShader->setFloat("lightmapIntensity", 4.0f);
    lightmappedShader->setBool("showLightmapsOnly", false);
    lightmappedShader->setVec3("ambientColor", glm::vec3(0.05f));
    lightmappedShader->unbind();

    return true;
}


// ИСПРАВЛЕНО: правильный расчёт UV с использованием оригинальных вершин
bool LightmappedRenderer::buildLightmappedMesh(BSPLoader& bsp, LightmapManager& lmManager) {
    const auto& faces = bsp.getFaces();
    const auto& texInfos = bsp.getTexInfos();
    const auto& surfEdges = bsp.getSurfEdges();
    const auto& edges = bsp.getEdges();
    const auto& originalVertices = bsp.getOriginalVertices();
    const auto& planes = bsp.getPlanes();
    const auto& entities = bsp.getEntities();
    const auto& models = bsp.getModels();

    if (faces.empty() || originalVertices.empty()) {
        std::cerr << "LightmappedRenderer: Empty BSP data" << std::endl;
        return false;
    }

    // Собираем индексы фейсов которые принадлежат trigger_ entities
    std::unordered_set<int> triggerFaceIndices;

    for (const auto& entity : entities) {
        // Проверяем что это trigger
        if (entity.classname.find("trigger_") != 0) continue;
        if (entity.model.empty() || entity.model[0] != '*') continue;

        int modelIndex = 0;
        try {
            modelIndex = std::stoi(entity.model.substr(1));
        }
        catch (...) { continue; }

        if (modelIndex <= 0 || modelIndex >= (int)models.size()) continue;

        // Добавляем все фейсы этой модели в список исключений
        const BSPModel& model = models[modelIndex];
        for (int i = 0; i < model.numFaces; i++) {
            int faceIdx = model.firstFace + i;
            if (faceIdx >= 0 && faceIdx < (int)faces.size()) {
                triggerFaceIndices.insert(faceIdx);
            }
        }
        std::cout << "[LMRENDERER] Excluding trigger " << entity.classname
            << " model *" << modelIndex << " (" << model.numFaces << " faces)" << std::endl;
    }

    // Получаем маппинг прозрачности из draw calls BSP
    const auto& bspDrawCalls = bsp.getDrawCalls();
    std::unordered_map<int, std::pair<int, int>> faceTransparency;
    for (const auto& dc : bspDrawCalls) {
        if (dc.isTransparent) {
            faceTransparency[dc.faceIndex] = { dc.rendermode, dc.renderamt };
        }
    }

    struct LMVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec2 lightmapCoord;
    };

    std::vector<LMVertex> lmVertices;
    std::vector<unsigned int> lmIndices;
    faceDrawCalls.clear();
    hasTransparentFaces = false;

    int processedFaces = 0;
    int skippedFaces = 0;
    int triggerSkipped = 0;

    for (size_t faceIdx = 0; faceIdx < faces.size(); faceIdx++) {
        // ПРОПУСКАЕМ trigger_ фейсы!
        if (triggerFaceIndices.count((int)faceIdx)) {
            triggerSkipped++;
            continue;
        }

        const BSPFace& face = faces[faceIdx];

        if (face.numEdges < 3) { skippedFaces++; continue; }
        if (face.texInfo < 0 || face.texInfo >= (int)texInfos.size()) { skippedFaces++; continue; }
        if (face.planeNum >= (int)planes.size()) { skippedFaces++; continue; }

        const FaceLightmap& lm = lmManager.getFaceLightmap((int)faceIdx);
        const BSPTexInfo& tex = texInfos[face.texInfo];

        glm::vec3 bspNormal = planes[face.planeNum].normal;
        if (face.side != 0) bspNormal = -bspNormal;
        glm::vec3 worldNormal = glm::vec3(-bspNormal.x, bspNormal.z, bspNormal.y);
        worldNormal = glm::normalize(worldNormal);

        glm::uvec2 texDim = bsp.getTextureDimensions(tex.textureIndex);
        int texWidth = texDim.x > 0 ? texDim.x : 256;
        int texHeight = texDim.y > 0 ? texDim.y : 256;

        std::vector<glm::vec3> bspPositions;
        bspPositions.reserve(face.numEdges);

        for (int j = 0; j < face.numEdges; j++) {
            int surfEdgeIdx = face.firstEdge + j;
            if (surfEdgeIdx < 0 || surfEdgeIdx >= (int)surfEdges.size()) continue;

            int edgeIdx = surfEdges[surfEdgeIdx];
            int vIdx;

            if (edgeIdx >= 0) {
                if (edgeIdx >= (int)edges.size()) continue;
                vIdx = edges[edgeIdx].v[0];
            }
            else {
                int negIdx = -edgeIdx;
                if (negIdx < 0 || negIdx >= (int)edges.size()) continue;
                vIdx = edges[negIdx].v[1];
            }

            if (vIdx < 0 || vIdx >= (int)originalVertices.size()) continue;
            bspPositions.push_back(originalVertices[vIdx]);
        }

        if (bspPositions.size() < 3) { skippedFaces++; continue; }

        std::vector<LMVertex> faceVerts;
        faceVerts.reserve(bspPositions.size());

        for (const auto& bspPos : bspPositions) {
            LMVertex v;

            v.position = glm::vec3(-bspPos.x, bspPos.z, bspPos.y);
            v.normal = worldNormal;

            float s = bspPos.x * tex.s[0] + bspPos.y * tex.s[1] + bspPos.z * tex.s[2] + tex.s[3];
            float t = bspPos.x * tex.t[0] + bspPos.y * tex.t[1] + bspPos.z * tex.t[2] + tex.t[3];
            v.texCoord = glm::vec2(s / texWidth, t / texHeight);

            if (lm.valid && lm.width > 0 && lm.height > 0) {
                float lmU = (s / 16.0f) - std::floor(lm.minS / 16.0f);
                float lmV = (t / 16.0f) - std::floor(lm.minT / 16.0f);

                lmU = (lmU + 0.5f) / (float)lm.width;
                lmV = (lmV + 0.5f) / (float)lm.height;

                v.lightmapCoord.x = lm.uvMin.x + lmU * (lm.uvMax.x - lm.uvMin.x);
                v.lightmapCoord.y = lm.uvMin.y + lmV * (lm.uvMax.y - lm.uvMin.y);
            }
            else {
                v.lightmapCoord = glm::vec2(0.001f, 0.001f);
            }

            faceVerts.push_back(v);
        }

        unsigned int baseIdx = (unsigned int)lmVertices.size();
        unsigned int startIndex = (unsigned int)lmIndices.size();

        for (auto& fv : faceVerts) {
            lmVertices.push_back(fv);
        }

        for (size_t j = 1; j + 1 < faceVerts.size(); j++) {
            lmIndices.push_back(baseIdx);
            lmIndices.push_back(baseIdx + (unsigned int)j + 1);
            lmIndices.push_back(baseIdx + (unsigned int)j);
        }

        unsigned int indexCount = (unsigned int)(lmIndices.size() - startIndex);
        GLuint texID = bsp.getTextureID(tex.textureIndex);

        LMFaceDrawCall dc;
        dc.texID = texID;
        dc.indexOffset = startIndex;
        dc.indexCount = indexCount;
        dc.faceIndex = (int)faceIdx;

        auto it = faceTransparency.find((int)faceIdx);
        if (it != faceTransparency.end()) {
            dc.rendermode = static_cast<unsigned char>(it->second.first);
            dc.renderamt = static_cast<unsigned char>(it->second.second);
            dc.isTransparent = true;
            hasTransparentFaces = true;
        }
        else {
            dc.rendermode = 0;
            dc.renderamt = 255;
            dc.isTransparent = false;
        }

        faceDrawCalls.push_back(dc);
        processedFaces++;
    }

    std::cout << "[LMRENDERER] Processed " << processedFaces << " faces, skipped "
        << skippedFaces << ", trigger skipped " << triggerSkipped << std::endl;

    if (lmVertices.empty() || lmIndices.empty()) {
        std::cerr << "LightmappedRenderer: No valid geometry built" << std::endl;
        return false;
    }

    if (worldVAO) glDeleteVertexArrays(1, &worldVAO);
    if (worldVBO) glDeleteBuffers(1, &worldVBO);
    if (worldEBO) glDeleteBuffers(1, &worldEBO);

    glGenVertexArrays(1, &worldVAO);
    glGenBuffers(1, &worldVBO);
    glGenBuffers(1, &worldEBO);

    glBindVertexArray(worldVAO);

    glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
    glBufferData(GL_ARRAY_BUFFER, lmVertices.size() * sizeof(LMVertex),
        lmVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, lmIndices.size() * sizeof(unsigned int),
        lmIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LMVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LMVertex), (void*)offsetof(LMVertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(LMVertex), (void*)offsetof(LMVertex, texCoord));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(LMVertex), (void*)offsetof(LMVertex, lightmapCoord));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    indexCount = lmIndices.size();

    std::cout << "[LMRENDERER] Built mesh: " << lmVertices.size()
        << " vertices, " << indexCount << " indices, "
        << faceDrawCalls.size() << " face draw calls"
        << " (transparent: " << (hasTransparentFaces ? "yes" : "no") << ")" << std::endl;

    return true;
}


bool LightmappedRenderer::loadWorld(BSPLoader& bsp, LightmapManager& lightmapManager) {
    unloadWorld();

    if (!bsp.isLoaded()) {
        std::cerr << "LightmappedRenderer: BSP not loaded" << std::endl;
        return false;
    }

    this->lmManager = &lightmapManager;

    if (!buildLightmappedMesh(bsp, lightmapManager)) {
        return false;
    }

    std::cout << "LightmappedRenderer: World loaded successfully" << std::endl;
    return true;
}

void LightmappedRenderer::unloadWorld() {
    if (worldVAO) {
        glDeleteVertexArrays(1, &worldVAO);
        worldVAO = 0;
    }
    if (worldVBO) {
        glDeleteBuffers(1, &worldVBO);
        worldVBO = 0;
    }
    if (worldEBO) {
        glDeleteBuffers(1, &worldEBO);
        worldEBO = 0;
    }
    indexCount = 0;
    lmManager = nullptr;
    faceDrawCalls.clear();
}

void LightmappedRenderer::beginFrame(const glm::vec3& clearColor) {
    stats.reset();
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void LightmappedRenderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos,
    BSPLoader& bsp, const glm::vec3& ambientColor) {

    if (worldVAO == 0 || !lmManager) return;

    glm::mat4 projection = glm::perspective(glm::radians(75.0f),
        (float)screenWidth / (float)screenHeight, 0.1f, 10000.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);

    lightmappedShader->bind();
    lightmappedShader->setMat4("model", glm::mat4(1.0f));
    lightmappedShader->setMat4("view", view);
    lightmappedShader->setMat4("projection", projection);
    lightmappedShader->setFloat("lightmapIntensity", lightmapIntensity);
    lightmappedShader->setBool("showLightmapsOnly", showLightmapsOnly);
    lightmappedShader->setVec3("ambientColor", ambientColor);

    glActiveTexture(GL_TEXTURE1);
    lmManager->getAtlas().bind(GL_TEXTURE1);

    glBindVertexArray(worldVAO);

    // 1. Непрозрачная геометрия
    lightmappedShader->setFloat("uAlpha", 1.0f);

    GLuint currentTex = 0;
    for (size_t i = 0; i < faceDrawCalls.size(); i++) {
        const auto& dc = faceDrawCalls[i];
        if (dc.isTransparent) continue;

        if (dc.texID != currentTex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, dc.texID);
            currentTex = dc.texID;
        }

        if (dc.indexCount > 0) {
            glDrawElements(GL_TRIANGLES, dc.indexCount, GL_UNSIGNED_INT,
                (void*)(dc.indexOffset * sizeof(unsigned int)));
            stats.drawCalls++;
            stats.triangles += dc.indexCount / 3;
        }
    }

    // 2. Прозрачная геометрия (вода и func_wall)
    if (hasTransparentFaces) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        for (size_t i = 0; i < faceDrawCalls.size(); i++) {
            const auto& dc = faceDrawCalls[i];
            if (!dc.isTransparent) continue;

            if (dc.texID != currentTex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, dc.texID);
                currentTex = dc.texID;
            }

            // Важно: минимальный alpha чтобы вода была видна
            float alpha = dc.renderamt / 255.0f;
            alpha = std::max(0.1f, alpha); // Минимум 0.1 чтобы была видна
            lightmappedShader->setFloat("uAlpha", alpha);

            if (dc.indexCount > 0) {
                glDrawElements(GL_TRIANGLES, dc.indexCount, GL_UNSIGNED_INT,
                    (void*)(dc.indexOffset * sizeof(unsigned int)));
                stats.drawCalls++;
                stats.triangles += dc.indexCount / 3;
            }
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    glBindVertexArray(0);
    lightmappedShader->unbind();
}

void LightmappedRenderer::cleanup() {
    unloadWorld();
    lightmappedShader.reset();
}