#include "Renderer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include "ShadowSystem.h"
#include "ShadowSystem.h"

Renderer::Renderer() = default;
Renderer::~Renderer() { cleanup(); }

bool Renderer::init(int width, int height) {
    screenWidth = width;
    screenHeight = height;

    geometryShader = new Shader();
    if (!geometryShader->loadFromStrings(geometryVert, geometryFrag)) {
        std::cerr << "Geometry shader fail: " << geometryShader->getError() << std::endl;
        return false;
    }

    lightingShader = new Shader();
    if (!lightingShader->loadFromStrings(lightingVert, lightingFrag)) {
        std::cerr << "Lighting shader fail: " << lightingShader->getError() << std::endl;
        return false;
    }

    lightingShader->bind();
    lightingShader->setInt("gPosition", 0);
    lightingShader->setInt("gNormal", 1);
    lightingShader->setInt("gAlbedo", 2);
    lightingShader->setInt("shadowAtlas", 3);
    lightingShader->setInt("flashlightShadow", 4);
    lightingShader->unbind();

    if (!createGBuffer(width, height)) {
        std::cerr << "Failed to create G-Buffer!" << std::endl;
        return false;
    }

    // Исправлено: tileSize 768 для cubemap 3x2 по 256 каждая грань
    if (!createShadowAtlas(SHADOW_ATLAS_SIZE, 768)) {
        std::cerr << "Failed to create shadow atlas!" << std::endl;
        return false;
    }

    glGenBuffers(1, &lightsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
    glBufferData(GL_UNIFORM_BUFFER, 256 * 48 + 16, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    GLuint blockIndex = glGetUniformBlockIndex(lightingShader->getID(), "LightsBlock");
    if (blockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(lightingShader->getID(), blockIndex, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, lightsUBO);
    }

    // Создаём FBO для фонарика
    glGenFramebuffers(1, &flashlight.spotFBO);
    glGenTextures(1, &flashlight.spotShadowMap);
    glBindTexture(GL_TEXTURE_2D, flashlight.spotShadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, flashlight.spotFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
        flashlight.spotShadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    createHitboxMesh();
    return true;
}

bool Renderer::createGBuffer(int w, int h) {
    gBuffer.width = w;
    gBuffer.height = h;

    glGenFramebuffers(1, &gBuffer.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);

    glGenTextures(1, &gBuffer.position);
    glBindTexture(GL_TEXTURE_2D, gBuffer.position);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gBuffer.position, 0);

    glGenTextures(1, &gBuffer.normal);
    glBindTexture(GL_TEXTURE_2D, gBuffer.normal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gBuffer.normal, 0);

    glGenTextures(1, &gBuffer.albedo);
    glBindTexture(GL_TEXTURE_2D, gBuffer.albedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gBuffer.albedo, 0);

    glGenTextures(1, &gBuffer.depth);
    glBindTexture(GL_TEXTURE_2D, gBuffer.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gBuffer.depth, 0);

    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "G-Buffer incomplete!" << std::endl;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool Renderer::createShadowAtlas(int size, int tileSize) {
    shadowAtlas.size = size;
    shadowAtlas.tileSize = tileSize;  // 768

    // Каждому источнику нужно 3x2 тайла (для 6 граней куба)
    int lightsPerRow = size / tileSize;
    int lightsPerCol = size / tileSize;
    shadowAtlas.maxLights = lightsPerRow * lightsPerCol;
    shadowAtlas.facesPerRow = size / (tileSize / 3);  // 2048 / 256 = 8

    if (shadowAtlas.maxLights <= 0) {
        std::cerr << "Shadow Atlas size too small for this tileSize!" << std::endl;
        return false;
    }

    shadowAtlas.slots.clear();
    shadowAtlas.slots.resize(shadowAtlas.maxLights);

    glGenFramebuffers(1, &shadowAtlas.fbo);
    glGenTextures(1, &shadowAtlas.texture);
    glBindTexture(GL_TEXTURE_2D, shadowAtlas.texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowAtlas.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowAtlas.texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

int Renderer::allocateShadowSlot(PointLight* light) {
    for (int i = 0; i < shadowAtlas.slots.size(); i++) {
        if (!shadowAtlas.slots[i].occupied) {
            shadowAtlas.slots[i].occupied = true;
            shadowAtlas.slots[i].owner = light;

            int cubemapsPerRow = shadowAtlas.size / shadowAtlas.tileSize;
            shadowAtlas.slots[i].x = (i % cubemapsPerRow) * shadowAtlas.tileSize;
            shadowAtlas.slots[i].y = (i / cubemapsPerRow) * shadowAtlas.tileSize;

            return i;
        }
    }
    return -1;
}

void Renderer::freeShadowSlot(int slot) {
    if (slot >= 0 && slot < shadowAtlas.slots.size()) {
        shadowAtlas.slots[slot].occupied = false;
        shadowAtlas.slots[slot].owner = nullptr;
    }
}

void Renderer::registerLight(PointLight* light, LightType type) {
    auto it = lightToIndex.find(light);
    if (it != lightToIndex.end()) {
        lightInstances[it->second].type = type;
        return;
    }

    LightInstance inst;
    inst.light = light;
    inst.type = type;

    if (type != LightType::BAKED) {
        inst.bakedShadowID = allocateShadowSlot(light);
    }

    lightInstances.push_back(inst);
    lightToIndex[light] = lightInstances.size() - 1;
}

void Renderer::setLightDirty(PointLight* light) {
    auto it = lightToIndex.find(light);
    if (it != lightToIndex.end()) {
        lightInstances[it->second].shadowDirty = true;
    }
}

std::vector<LightInstance*> Renderer::getLightsNeedingShadows(const glm::vec3& cameraPos, int maxCount) {
    std::vector<std::pair<float, LightInstance*>> prioritized;

    for (auto& inst : lightInstances) {
        if (inst.type == LightType::DYNAMIC) {
            float dist = glm::length(inst.light->getPosition() - cameraPos);
            prioritized.push_back({ dist, &inst });
        }
        else if (inst.type == LightType::HYBRID && inst.shadowDirty) {
            float dist = glm::length(inst.light->getPosition() - cameraPos);
            if (dist < inst.light->radius + 50.0f) {
                prioritized.push_back({ dist, &inst });
            }
        }
    }

    std::sort(prioritized.begin(), prioritized.end());

    std::vector<LightInstance*> result;
    for (int i = 0; i < std::min((int)prioritized.size(), maxCount); i++) {
        result.push_back(prioritized[i].second);
    }

    return result;
}

void Renderer::updateShadows(ShadowSystem* shadowSystem, const glm::vec3& cameraPos, float deltaTime) {
    if (!shadowSystem || !worldLoaded) return;

    currentTime += deltaTime;
    stats.shadowUpdates = 0;

    auto lightsToUpdate = getLightsNeedingShadows(cameraPos, MAX_REALTIME_SHADOWS);

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    // Обновляем тени для динамических источников
    for (auto* inst : lightsToUpdate) {
        if (inst->bakedShadowID < 0) continue;

        const auto& slot = shadowAtlas.slots[inst->bakedShadowID];

        // ПРАВИЛЬНЫЙ ВЫЗОВ: передаём tileSize = 768 (cubemapTileSize), а не faceSize
        shadowSystem->renderCubemapToAtlas(
            inst->light,
            shadowAtlas.fbo,
            slot.x, slot.y,
            shadowAtlas.tileSize,  // Это 768, а не 256!
            bspVAO, bspIndexCount,
            drawCalls
        );

        inst->shadowDirty = false;
        inst->lastUpdateTime = currentTime;
        stats.shadowUpdates++;
    }

    // Обновляем тень фонарика
    if (flashlight.enabled) {
        renderFlashlightShadow(shadowSystem);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

void Renderer::renderFlashlightShadow(ShadowSystem* shadowSystem) {
    if (!shadowSystem || !worldLoaded) return;

    glm::vec3 up = glm::abs(flashlight.direction.y) > 0.99f ?
        glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 view = glm::lookAt(flashlight.position,
        flashlight.position + flashlight.direction, up);
    glm::mat4 proj = glm::perspective(glm::radians(flashlight.angle * 2.0f),
        1.0f, 0.1f, flashlight.radius);
    flashlight.viewProj = proj * view;

    // Рендерим сцену в карту теней фонарика
    shadowSystem->renderSpotShadow(
        flashlight.position, flashlight.direction,
        flashlight.radius, flashlight.angle,
        flashlight.spotFBO, 1024, 1024,
        bspVAO, bspIndexCount, drawCalls
    );
}

void Renderer::bakeStaticShadows(ShadowSystem* shadowSystem, BSPLoader& bsp) {
    if (!shadowSystem || !worldLoaded) return;

    std::cout << "[Renderer] Baking static shadows..." << std::endl;

    int bakedCount = 0;

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    for (auto& inst : lightInstances) {
        if (inst.type == LightType::BAKED && inst.bakedShadowID < 0) {
            int slot = allocateShadowSlot(inst.light);
            if (slot >= 0) {
                inst.bakedShadowID = slot;
                const auto& slotInfo = shadowAtlas.slots[slot];

                // Рендерим тень один раз в слот атласа
                shadowSystem->renderCubemapToAtlas(
                    inst.light,
                    shadowAtlas.fbo,
                    slotInfo.x, slotInfo.y,
                    shadowAtlas.tileSize,
                    bspVAO, bspIndexCount, drawCalls
                );

                bakedCount++;
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    std::cout << "[Renderer] Baked " << bakedCount << " static shadows" << std::endl;
}

void Renderer::setFlashlight(const glm::vec3& pos, const glm::vec3& dir, bool enabled) {
    flashlight.enabled = enabled;
    if (enabled) {
        flashlight.position = pos;
        flashlight.direction = dir;
    }
}

void Renderer::updateLightsBuffer() {
    if (!lightsUBO) return;

    struct LightData {
        glm::vec4 position_radius;
        glm::vec4 color_intensity;
        glm::ivec2 shadowSlot_type;
        glm::vec2 padding;
    };

    std::vector<LightData> data;
    int count = 0;

    // Сначала динамические и гибридные (с тенями)
    for (auto& inst : lightInstances) {
        if (count >= 256) break;
        if (inst.type == LightType::BAKED) continue;

        LightData ld;
        ld.position_radius = glm::vec4(inst.light->getPosition(), inst.light->radius);
        ld.color_intensity = glm::vec4(inst.light->color, inst.light->intensity);
        ld.shadowSlot_type.x = inst.bakedShadowID; // -1 если нет слота
        ld.shadowSlot_type.y = (int)inst.type;
        data.push_back(ld);
        count++;
    }

    // Затем запеченные (без теней в реальном времени)
    for (auto& inst : lightInstances) {
        if (count >= 256) break;
        if (inst.type != LightType::BAKED) continue;

        LightData ld;
        ld.position_radius = glm::vec4(inst.light->getPosition(), inst.light->radius);
        ld.color_intensity = glm::vec4(inst.light->color, inst.light->intensity);
        ld.shadowSlot_type.x = inst.bakedShadowID; // Может быть -1 или слот для статической тени
        ld.shadowSlot_type.y = 0; // BAKED
        data.push_back(ld);
        count++;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
    if (!data.empty()) {
        glBufferSubData(GL_UNIFORM_BUFFER, 0, data.size() * sizeof(LightData), data.data());
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 256 * sizeof(LightData), sizeof(int), &count);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

bool Renderer::loadWorld(BSPLoader& bsp) {
    unloadWorld();
    if (!bsp.isLoaded()) return false;

    const auto& vertices = bsp.getMeshVertices();
    const auto& indices = bsp.getMeshIndices();
    if (vertices.empty() || indices.empty()) return false;

    glGenVertexArrays(1, &bspVAO);
    glGenBuffers(1, &bspVBO);
    glGenBuffers(1, &bspEBO);

    glBindVertexArray(bspVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bspVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BSPVertex),
        vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bspEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW);

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

    bspIndexCount = indices.size();
    drawCalls = bsp.getDrawCalls();

    std::sort(drawCalls.begin(), drawCalls.end(),
        [](const FaceDrawCall& a, const FaceDrawCall& b) { return a.texID < b.texID; });

    worldLoaded = true;
    return true;
}

void Renderer::unloadWorld() {
    if (bspVAO) { glDeleteVertexArrays(1, &bspVAO); bspVAO = 0; }
    if (bspVBO) { glDeleteBuffers(1, &bspVBO); bspVBO = 0; }
    if (bspEBO) { glDeleteBuffers(1, &bspEBO); bspEBO = 0; }
    drawCalls.clear();
    worldLoaded = false;
}

void Renderer::beginFrame(const glm::vec3& clearColor) {
    stats = RenderStats();

    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Renderer::geometryPass(const glm::mat4& view, const glm::mat4& proj) {
    if (!worldLoaded || bspVAO == 0) return;

    geometryShader->bind();
    geometryShader->setMat4("projection", proj);
    geometryShader->setMat4("view", view);
    geometryShader->setMat4("model", glm::mat4(1.0f));

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(bspVAO);

    GLuint lastTex = 0;
    for (const auto& dc : drawCalls) {
        if (dc.texID != lastTex) {
            glBindTexture(GL_TEXTURE_2D, dc.texID);
            lastTex = dc.texID;
        }
        glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
            (void*)(dc.indexOffset * sizeof(unsigned int)));
        stats.drawCalls++;
        stats.triangles += dc.indexCount / 3;
    }

    glBindVertexArray(0);
    geometryShader->unbind();
}

void Renderer::lightingPass(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    updateLightsBuffer();

    lightingShader->bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.position);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gBuffer.normal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gBuffer.albedo);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowAtlas.texture);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, flashlight.spotShadowMap);

    lightingShader->setVec3("uViewPos", viewPos);
    lightingShader->setVec3("uSunDir", sunDir);
    lightingShader->setVec3("uSunColor", glm::vec3(1.0f, 0.95f, 0.8f));
    lightingShader->setFloat("uSunIntensity", 0.5f);

    int tilesPerRow = shadowAtlas.size / (shadowAtlas.tileSize / 3);
    lightingShader->setInt("uAtlasTilesPerRow", tilesPerRow);
    lightingShader->setFloat("uAtlasTileSize", 1.0f / tilesPerRow);
    lightingShader->setInt("shadowAtlasSize", shadowAtlas.size);

    lightingShader->setInt("uFlashlightEnabled", flashlight.enabled ? 1 : 0);
    if (flashlight.enabled) {
        lightingShader->setVec3("uFlashlightPos", flashlight.position);
        lightingShader->setVec3("uFlashlightDir", flashlight.direction);
        lightingShader->setFloat("uFlashlightRadius", flashlight.radius);
        lightingShader->setFloat("uFlashlightAngle", flashlight.angle);
        lightingShader->setMat4("uFlashlightMatrix", flashlight.viewProj);
    }

    static GLuint quadVAO = 0, quadVBO = 0;
    if (quadVAO == 0) {
        float quadVerts[] = {
            -1,  1, 0,  0, 1,
            -1, -1, 0,  0, 0,
             1,  1, 0,  1, 1,
             1, -1, 0,  1, 0,
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    lightingShader->unbind();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Renderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos,
    const glm::vec3& sunDir) {

    glm::mat4 proj = glm::perspective(glm::radians(75.0f),
        (float)screenWidth / screenHeight, 0.1f, 1000.0f);

    geometryPass(view, proj);
    lightingPass(view, viewPos, sunDir);
}

void Renderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos) {
    renderWorld(view, viewPos, glm::vec3(0.3f, -0.7f, 0.5f));
}

void Renderer::renderHitbox(const glm::mat4& view, const glm::mat4& projection,
    const glm::vec3& position, bool visible) {
    if (!visible || !showHitbox) return;

    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
}

void Renderer::createHitboxMesh() {
    float verts[] = {
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
    };
    unsigned int inds[] = {
        0,1,2,2,3,0, 4,6,5,6,4,7, 4,0,3,3,7,4,
        1,5,6,6,2,1, 3,2,6,6,7,3, 4,5,1,1,0,4
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::destroyGBuffer() {
    if (gBuffer.position) glDeleteTextures(1, &gBuffer.position);
    if (gBuffer.normal) glDeleteTextures(1, &gBuffer.normal);
    if (gBuffer.albedo) glDeleteTextures(1, &gBuffer.albedo);
    if (gBuffer.depth) glDeleteTextures(1, &gBuffer.depth);
    if (gBuffer.fbo) glDeleteFramebuffers(1, &gBuffer.fbo);
    gBuffer = {};
}

void Renderer::cleanup() {
    unloadWorld();
    destroyGBuffer();

    if (shadowAtlas.fbo) glDeleteFramebuffers(1, &shadowAtlas.fbo);
    if (shadowAtlas.texture) glDeleteTextures(1, &shadowAtlas.texture);
    if (flashlight.spotShadowMap) glDeleteTextures(1, &flashlight.spotShadowMap);
    if (flashlight.spotFBO) glDeleteFramebuffers(1, &flashlight.spotFBO);
    if (lightsUBO) glDeleteBuffers(1, &lightsUBO);
    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
    if (cubeEBO) glDeleteBuffers(1, &cubeEBO);

    delete geometryShader;
    delete lightingShader;
    delete forwardShader;
}