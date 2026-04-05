#include "DebugRenderer.h"
#include "gl_includes.h"
#include "ShaderManager.h"
#include "Config.h"

unsigned int DebugRenderer::cubeVAO = 0, DebugRenderer::cubeVBO = 0, DebugRenderer::cubeEBO = 0;

void DebugRenderer::setupCubeBuffers() {
    float vertices[] = {
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f
    };
    unsigned int indices[] = {
        0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,0,3, 3,7,4,
        1,5,6, 6,2,1, 3,2,6, 6,7,3, 4,5,1, 1,0,4
    };
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void DebugRenderer::initialize() { setupCubeBuffers(); }

void DebugRenderer::cleanup() {
    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
    if (cubeEBO) glDeleteBuffers(1, &cubeEBO);
}

void DebugRenderer::renderHitbox(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& position) {
    if (cubeVAO == 0) return;
    ShaderManager::use();
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::scale(model, glm::vec3(
        Config::Player::HITBOX_WIDTH,
        Config::Player::HITBOX_HEIGHT,
        Config::Player::HITBOX_WIDTH
    ));

    glm::mat4 mvp = projection * view * model;
    ShaderManager::setMVP(mvp);
    ShaderManager::setModel(model);
    ShaderManager::setColor(glm::vec3(1.0f, 1.0f, 0.0f));

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
}