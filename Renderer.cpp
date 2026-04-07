#include "Renderer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
    lightingShader->unbind();

    if (!createGBuffer(width, height)) {
        std::cerr << "Failed to create G-Buffer!" << std::endl;
        return false;
    }

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

void Renderer::setFlashlight(const glm::vec3& pos, const glm::vec3& dir, bool enabled) {
    flashlight.enabled = enabled;
    if (enabled) {
        flashlight.position = pos;
        flashlight.direction = dir;
    }
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    stats.drawCalls = 0;
    stats.triangles = 0;
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
}

void Renderer::geometryPass(const glm::mat4& view, const glm::mat4& proj) {
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);

    geometryShader->bind();
    GLint modelLoc = geometryShader->getLocation("model");
    GLint viewLoc = geometryShader->getLocation("view");
    GLint projLoc = geometryShader->getLocation("projection");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(bspVAO);
    for (const auto& dc : drawCalls) {
        glBindTexture(GL_TEXTURE_2D, dc.texID);
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    lightingShader->bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.position);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gBuffer.normal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gBuffer.albedo);

    lightingShader->setVec3("uViewPos", viewPos);
    lightingShader->setVec3("uSunDir", sunDir);
    lightingShader->setVec3("uSunColor", glm::vec3(1.0f, 0.95f, 0.8f));
    lightingShader->setFloat("uSunIntensity", 1.0f);

    static GLuint quadVAO = 0;
    static GLuint quadVBO = 0;
    if (quadVAO == 0) {
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

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    lightingShader->unbind();
}

void Renderer::renderHitbox(const glm::mat4& view, const glm::mat4& projection,
    const glm::vec3& position, bool visible) {
    if (!visible || !showHitbox) return;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
    model = model * scaleMat;

    // Простая отрисовка хитбокса (можно доработать)
}

void Renderer::createHitboxMesh() {
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Renderer::cleanup() {
    destroyGBuffer();

    if (geometryShader) { delete geometryShader; geometryShader = nullptr; }
    if (lightingShader) { delete lightingShader; lightingShader = nullptr; }
    if (forwardShader) { delete forwardShader; forwardShader = nullptr; }

    if (bspVAO) glDeleteVertexArrays(1, &bspVAO);
    if (bspVBO) glDeleteBuffers(1, &bspVBO);
    if (bspEBO) glDeleteBuffers(1, &bspEBO);

    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
    if (cubeEBO) glDeleteBuffers(1, &cubeEBO);

    if (flashlight.spotFBO) glDeleteFramebuffers(1, &flashlight.spotFBO);
    if (flashlight.spotShadowMap) glDeleteTextures(1, &flashlight.spotShadowMap);
}

void Renderer::destroyGBuffer() {
    if (gBuffer.fbo) {
        glDeleteFramebuffers(1, &gBuffer.fbo);
        gBuffer.fbo = 0;
    }
    if (gBuffer.position) {
        glDeleteTextures(1, &gBuffer.position);
        gBuffer.position = 0;
    }
    if (gBuffer.normal) {
        glDeleteTextures(1, &gBuffer.normal);
        gBuffer.normal = 0;
    }
    if (gBuffer.albedo) {
        glDeleteTextures(1, &gBuffer.albedo);
        gBuffer.albedo = 0;
    }
    if (gBuffer.depth) {
        glDeleteTextures(1, &gBuffer.depth);
        gBuffer.depth = 0;
    }
}

void Renderer::setViewport(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    destroyGBuffer();
    createGBuffer(width, height);
}
