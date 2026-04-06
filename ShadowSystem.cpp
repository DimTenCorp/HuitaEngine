#include "ShadowSystem.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

ShadowSystem::ShadowSystem() = default;
ShadowSystem::~ShadowSystem() { cleanup(); }

bool ShadowSystem::init() {
    return compileDepthShader() && compileSpotShader();
}

void ShadowSystem::cleanup() {
    if (depthProgram) {
        glDeleteProgram(depthProgram);
        depthProgram = 0;
    }
    if (spotProgram) {
        glDeleteProgram(spotProgram);
        spotProgram = 0;
    }
}

// ИСПРАВЛЕННЫЙ depth шейдер для правильной записи глубины
bool ShadowSystem::compileDepthShader() {
    const char* vert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
void main() {
    gl_Position = proj * view * model * vec4(aPos, 1.0);
}
)";

    const char* frag = R"(
#version 330 core
void main() {
    // gl_FragDepth автоматически записывается из gl_Position.z
}
)";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vert, nullptr);
    glCompileShader(vs);

    GLint success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vs, 512, nullptr, log);
        std::cerr << "Vertex shader error: " << log << std::endl;
        return false;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &frag, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fs, 512, nullptr, log);
        std::cerr << "Fragment shader error: " << log << std::endl;
        glDeleteShader(vs);
        return false;
    }

    depthProgram = glCreateProgram();
    glAttachShader(depthProgram, vs);
    glAttachShader(depthProgram, fs);
    glLinkProgram(depthProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    glGetProgramiv(depthProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(depthProgram, 512, nullptr, log);
        std::cerr << "Depth shader link fail: " << log << std::endl;
        return false;
    }

    return true;
}

bool ShadowSystem::compileSpotShader() {
    const char* vert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 viewProj;
void main() {
    gl_Position = viewProj * model * vec4(aPos, 1.0);
}
)";

    const char* frag = R"(
#version 330 core
void main() {
}
)";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vert, nullptr);
    glCompileShader(vs);

    GLint success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vs, 512, nullptr, log);
        std::cerr << "Spot vertex shader error: " << log << std::endl;
        return false;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &frag, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fs, 512, nullptr, log);
        std::cerr << "Spot fragment shader error: " << log << std::endl;
        glDeleteShader(vs);
        return false;
    }

    spotProgram = glCreateProgram();
    glAttachShader(spotProgram, vs);
    glAttachShader(spotProgram, fs);
    glLinkProgram(spotProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    glGetProgramiv(spotProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(spotProgram, 512, nullptr, log);
        std::cerr << "Spot shader link fail: " << log << std::endl;
        return false;
    }

    return true;
}

// ИСПРАВЛЕНО: функция теперь член класса
glm::mat4 ShadowSystem::calculateViewMatrixForFace(const glm::vec3& pos, int face) {
    switch (face) {
    case 0: return glm::lookAt(pos, pos + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0));
    case 1: return glm::lookAt(pos, pos + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0));
    case 2: return glm::lookAt(pos, pos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
    case 3: return glm::lookAt(pos, pos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
    case 4: return glm::lookAt(pos, pos + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
    case 5: return glm::lookAt(pos, pos + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
    }
    return glm::mat4(1.0f);
}

void ShadowSystem::getCubemapViewMatrices(const glm::vec3& pos, glm::mat4 views[6], glm::mat4& proj, float radius) {
    proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, radius);

    views[0] = glm::lookAt(pos, pos + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0));
    views[1] = glm::lookAt(pos, pos + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0));
    views[2] = glm::lookAt(pos, pos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
    views[3] = glm::lookAt(pos, pos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
    views[4] = glm::lookAt(pos, pos + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
    views[5] = glm::lookAt(pos, pos + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
}

// ИСПРАВЛЕНА функция renderFaceToAtlas
void ShadowSystem::renderFaceToAtlas(PointLight* light, int face,
    GLuint atlasFBO, int atlasX, int atlasY, int tileSize,
    GLuint bspVAO, size_t bspIndexCount,
    const std::vector<FaceDrawCall>& drawCalls) {

    if (!light || bspVAO == 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, atlasFBO);

    // ВАЖНО: viewport должен быть tileSize x tileSize, но atlasX, atlasY - это индексы, а не пиксели!
    // atlasX и atlasY уже должны быть в пикселях!
    glViewport(atlasX, atlasY, tileSize, tileSize);  // atlasX, atlasY уже в пикселях!

    glClear(GL_DEPTH_BUFFER_BIT);

    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, light->radius);
    glm::mat4 view = calculateViewMatrixForFace(light->getPosition(), face);

    glUseProgram(depthProgram);

    GLint modelLoc = glGetUniformLocation(depthProgram, "model");
    GLint viewLoc = glGetUniformLocation(depthProgram, "view");
    GLint projLoc = glGetUniformLocation(depthProgram, "proj");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));

    glBindVertexArray(bspVAO);

    for (const auto& dc : drawCalls) {
        glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
            (void*)(dc.indexOffset * sizeof(unsigned int)));
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ИСПРАВЛЕНА функция renderCubemapToAtlas
void ShadowSystem::renderCubemapToAtlas(PointLight* light,
    GLuint atlasFBO, int slotX, int slotY, int cubemapTileSize,
    GLuint bspVAO, size_t bspIndexCount,
    const std::vector<FaceDrawCall>& drawCalls) {

    if (!light) return;

    int faceSize = cubemapTileSize / 3;  // 256

    for (int face = 0; face < 6; face++) {
        int faceX = slotX + (face % 3) * faceSize;
        int faceY = slotY + (face / 3) * faceSize;

        renderFaceToAtlas(light, face, atlasFBO, faceX, faceY, faceSize,
            bspVAO, bspIndexCount, drawCalls);
    }
}

void ShadowSystem::renderSpotShadow(const glm::vec3& pos, const glm::vec3& dir,
    float radius, float angle,
    GLuint shadowFBO, int width, int height,
    GLuint bspVAO, size_t bspIndexCount,
    const std::vector<FaceDrawCall>& drawCalls) {

    glUseProgram(spotProgram);
    glBindVertexArray(bspVAO);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glViewport(0, 0, width, height);
    glClear(GL_DEPTH_BUFFER_BIT);

    glm::vec3 up = glm::abs(dir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 view = glm::lookAt(pos, pos + dir, up);
    glm::mat4 proj = glm::perspective(glm::radians(angle * 2.0f), 1.0f, 0.1f, radius);
    glm::mat4 viewProj = proj * view;

    GLint vpLoc = glGetUniformLocation(spotProgram, "viewProj");
    GLint modelLoc = glGetUniformLocation(spotProgram, "model");

    glUniformMatrix4fv(vpLoc, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_DEPTH_TEST);

    for (const auto& dc : drawCalls) {
        glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
            (void*)(dc.indexOffset * sizeof(unsigned int)));
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindVertexArray(0);
}

void ShadowSystem::renderShadowMaps(const std::vector<PointLight*>& lights,
    GLuint bspVAO,
    size_t bspIndexCount,
    const std::vector<FaceDrawCall>& drawCalls,
    const glm::vec3& cameraPos) {

    if (bspVAO == 0 || drawCalls.empty() || lights.empty()) return;

    std::vector<PointLight*> nearbyLights;
    for (auto* light : lights) {
        if (!light) continue;
        float distToCamera = glm::length(light->getPosition() - cameraPos);
        if (distToCamera < light->radius + 30.0f) {
            nearbyLights.push_back(light);
        }
    }

    if (nearbyLights.empty()) return;

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    glUseProgram(depthProgram);
    glBindVertexArray(bspVAO);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(4.0f, 8.0f);

    for (auto* light : nearbyLights) {
        GLuint fbo = light->getShadowFBO();
        int size = light->getShadowSize();
        float radius = light->radius;
        glm::vec3 pos = light->getPosition();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, size, size);

        glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, radius);

        GLint modelLoc = glGetUniformLocation(depthProgram, "model");
        GLint viewLoc = glGetUniformLocation(depthProgram, "view");
        GLint projLoc = glGetUniformLocation(depthProgram, "proj");

        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

        for (int face = 0; face < 6; face++) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, light->getShadowCubemap(), 0);
            glClear(GL_DEPTH_BUFFER_BIT);

            glm::mat4 view = calculateViewMatrixForFace(pos, face);
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

            for (const auto& dc : drawCalls) {
                glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
                    (void*)(dc.indexOffset * sizeof(unsigned int)));
            }
        }
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}