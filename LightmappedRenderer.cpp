#include "pch.h"
#include "LightmappedRenderer.h"
#include <iostream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
uniform bool useLighting;
uniform vec3 ambientColor;
uniform float uAlpha = 1.0;

void main() {
    vec4 albedo = texture(albedoTexture, vTexCoord);
    
    if (albedo.a < 0.5 || (albedo.r < 0.01 && albedo.g < 0.01 && albedo.b > 0.9)) {
        discard;
    }
    
    if (showLightmapsOnly) {
        vec3 lightmap = texture(lightmapAtlas, vLightmapCoord).rgb;
        FragColor = vec4(lightmap, 1.0);
        return;
    }
    
    vec3 finalColor;
    
    if (useLighting) {
        vec3 lightmap = texture(lightmapAtlas, vLightmapCoord).rgb;
        vec3 lighting = lightmap * lightmapIntensity;
        finalColor = albedo.rgb * lighting;
    } else {
        finalColor = albedo.rgb;
    }
    
    FragColor = vec4(finalColor, albedo.a * uAlpha);
}
)glsl";
}

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
    useLighting(other.useLighting),
    skipSkyFaces(other.skipSkyFaces),
    screenWidth(other.screenWidth), screenHeight(other.screenHeight),
    stats(other.stats),
    faceDrawCalls(std::move(other.faceDrawCalls)),
    meshVertices(std::move(other.meshVertices)),
    meshIndices(std::move(other.meshIndices)) {
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
        useLighting = other.useLighting;
        skipSkyFaces = other.skipSkyFaces;
        screenWidth = other.screenWidth;
        screenHeight = other.screenHeight;
        stats = other.stats;
        faceDrawCalls = std::move(other.faceDrawCalls);
        meshVertices = std::move(other.meshVertices);
        meshIndices = std::move(other.meshIndices);

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

    if (!initShaders()) {
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    return true;
}

void LightmappedRenderer::setViewport(int width, int height) {
    if (width <= 0 || height <= 0) return;

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
    lightmappedShader->setInt("albedoTexture", 0);
    lightmappedShader->setInt("lightmapAtlas", 1);
    lightmappedShader->setBool("showLightmapsOnly", false);
    lightmappedShader->setBool("useLighting", true);
    lightmappedShader->setVec3("ambientColor", glm::vec3(0.05f));
    lightmappedShader->unbind();

    return true;
}

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

    std::unordered_set<int> triggerFaceIndices;
    std::unordered_map<int, int> doorFaceToModelIndex; // face index -> model index для дверей

    for (const auto& entity : entities) {
        if (entity.classname.find("trigger_") != 0 && entity.classname != "func_door") continue;
        if (entity.model.empty() || entity.model[0] != '*') continue;

        int modelIndex = 0;
        try {
            modelIndex = std::stoi(entity.model.substr(1));
        }
        catch (...) { continue; }

        if (modelIndex <= 0 || modelIndex >= (int)models.size()) continue;

        const BSPModel& model = models[modelIndex];
        for (int i = 0; i < model.numFaces; i++) {
            int faceIdx = model.firstFace + i;
            if (faceIdx >= 0 && faceIdx < (int)faces.size()) {
                triggerFaceIndices.insert(faceIdx);
                if (entity.classname == "func_door") {
                    doorFaceToModelIndex[faceIdx] = modelIndex;
                }
            }
        }
    }

    const auto& bspDrawCalls = bsp.getDrawCalls();
    std::unordered_map<int, std::pair<int, int>> faceTransparency;
    for (const auto& dc : bspDrawCalls) {
        if (dc.isTransparent) {
            faceTransparency[dc.faceIndex] = { dc.rendermode, dc.renderamt };
        }
    }

    std::vector<LMRenderVertex> lmVertices;
    std::vector<unsigned int> lmIndices;
    faceDrawCalls.clear();
    hasTransparentFaces = false;

    int processedFaces = 0;
    int skippedFaces = 0;
    int triggerSkipped = 0;
    int skySkipped = 0;

    for (size_t faceIdx = 0; faceIdx < faces.size(); faceIdx++) {
        if (triggerFaceIndices.count((int)faceIdx)) {
            triggerSkipped++;
            continue;
        }

        const BSPFace& face = faces[faceIdx];

        if (face.numEdges < 3) { skippedFaces++; continue; }
        if (face.texInfo < 0 || face.texInfo >= (int)texInfos.size()) { skippedFaces++; continue; }
        if (face.planeNum >= (int)planes.size()) { skippedFaces++; continue; }

        const BSPTexInfo& tex = texInfos[face.texInfo];
        const std::string& texName = bsp.getTextureName(tex.textureIndex);

        // Пропускаем SKY-фейсы если нужно
        if (texName == "sky" || texName == "SKY" ||
            texName.find("sky") == 0 || texName.find("SKY") == 0) {
            skySkipped++;
            continue;
        }

        const FaceLightmap& lm = lmManager.getFaceLightmap((int)faceIdx);

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

        std::vector<LMRenderVertex> faceVerts;
        faceVerts.reserve(bspPositions.size());

        for (const auto& bspPos : bspPositions) {
            LMRenderVertex v;

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
        dc.isSky = (texName == "sky" || texName == "SKY");

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

    if (lmVertices.empty() || lmIndices.empty()) {
        std::cerr << "LightmappedRenderer: No valid geometry built" << std::endl;
        return false;
    }

    meshVertices = lmVertices;
    meshIndices = lmIndices;

    if (worldVAO) glDeleteVertexArrays(1, &worldVAO);
    if (worldVBO) glDeleteBuffers(1, &worldVBO);
    if (worldEBO) glDeleteBuffers(1, &worldEBO);

    glGenVertexArrays(1, &worldVAO);
    glGenBuffers(1, &worldVBO);
    glGenBuffers(1, &worldEBO);

    glBindVertexArray(worldVAO);

    glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
    glBufferData(GL_ARRAY_BUFFER, lmVertices.size() * sizeof(LMRenderVertex),
        lmVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, lmIndices.size() * sizeof(unsigned int),
        lmIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LMRenderVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LMRenderVertex), (void*)offsetof(LMRenderVertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(LMRenderVertex), (void*)offsetof(LMRenderVertex, texCoord));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(LMRenderVertex), (void*)offsetof(LMRenderVertex, lightmapCoord));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    indexCount = lmIndices.size();

    std::cout << "[LMRENDERER] Built mesh: " << lmVertices.size()
        << " vertices, " << indexCount << " indices, "
        << faceDrawCalls.size() << " face draw calls"
        << " (transparent: " << (hasTransparentFaces ? "yes" : "no")
        << ", sky skipped: " << skySkipped << ")" << std::endl;

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
    meshVertices.clear();
    meshIndices.clear();
}

void LightmappedRenderer::beginFrame(const glm::vec3& clearColor, bool clearColorBuffer) {
    stats.reset();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (clearColorBuffer) {
        glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else {
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void LightmappedRenderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos,
    BSPLoader& bsp, const glm::vec3& ambientColor) {

    if (worldVAO == 0 || !lmManager) return;

    glm::mat4 projection = glm::perspective(glm::radians(75.0f),
        (float)screenWidth / (float)screenHeight, 0.1f, 10000.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    lightmappedShader->bind();
    lightmappedShader->setMat4("model", glm::mat4(1.0f));
    lightmappedShader->setMat4("view", view);
    lightmappedShader->setMat4("projection", projection);
    lightmappedShader->setFloat("lightmapIntensity", lightmapIntensity);
    lightmappedShader->setBool("showLightmapsOnly", showLightmapsOnly);
    lightmappedShader->setBool("useLighting", useLighting);
    lightmappedShader->setVec3("ambientColor", ambientColor);
    lightmappedShader->setFloat("uAlpha", 1.0f);

    glActiveTexture(GL_TEXTURE1);
    lmManager->getAtlas().bind(GL_TEXTURE1);

    glBindVertexArray(worldVAO);

    GLuint currentTex = 0;
    for (const auto& dc : faceDrawCalls) {
        if (dc.isTransparent) continue;
        if (dc.isSky) continue;

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

    lightmappedShader->unbind();

    if (hasTransparentFaces) {
        struct SortedDrawCall {
            const LMFaceDrawCall* dc;
            float distance;
        };

        std::vector<SortedDrawCall> sorted;
        sorted.reserve(faceDrawCalls.size());

        for (const auto& dc : faceDrawCalls) {
            if (!dc.isTransparent) continue;

            float distance = 0.0f;
            if (dc.indexCount > 0 && !meshIndices.empty() && !meshVertices.empty()) {
                unsigned int firstIdx = dc.indexOffset;
                if (firstIdx < meshIndices.size()) {
                    unsigned int vertIdx = meshIndices[firstIdx];
                    if (vertIdx < meshVertices.size()) {
                        distance = glm::distance(viewPos, meshVertices[vertIdx].position);
                    }
                }
            }
            sorted.push_back({ &dc, distance });
        }

        std::sort(sorted.begin(), sorted.end(),
            [](const SortedDrawCall& a, const SortedDrawCall& b) {
                return a.distance > b.distance;
            });

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        lightmappedShader->bind();
        lightmappedShader->setMat4("model", glm::mat4(1.0f));
        lightmappedShader->setMat4("view", view);
        lightmappedShader->setMat4("projection", projection);
        lightmappedShader->setFloat("lightmapIntensity", lightmapIntensity);
        lightmappedShader->setBool("showLightmapsOnly", showLightmapsOnly);
        lightmappedShader->setBool("useLighting", useLighting);
        lightmappedShader->setVec3("ambientColor", ambientColor);

        currentTex = 0;
        for (const auto& item : sorted) {
            const auto& dc = *item.dc;

            if (dc.texID != currentTex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, dc.texID);
                currentTex = dc.texID;
            }

            float alpha = dc.renderamt / 255.0f;
            alpha = std::max(0.05f, std::min(1.0f, alpha));
            lightmappedShader->setFloat("uAlpha", alpha);

            if (dc.indexCount > 0) {
                glDrawElements(GL_TRIANGLES, dc.indexCount, GL_UNSIGNED_INT,
                    (void*)(dc.indexOffset * sizeof(unsigned int)));
                stats.drawCalls++;
                stats.triangles += dc.indexCount / 3;
            }
        }

        lightmappedShader->unbind();

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
    }

    glBindVertexArray(0);
}

void LightmappedRenderer::cleanup() {
    unloadWorld();
    lightmappedShader.reset();
}