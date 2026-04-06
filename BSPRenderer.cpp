#include "BSPRenderer.h"
#include "ShaderManager.h"
#include <algorithm>
#include <iostream>

unsigned int BSPRenderer::vao = 0, BSPRenderer::vbo = 0, BSPRenderer::ebo = 0;
size_t BSPRenderer::indexCount = 0;
std::vector<FaceDrawCall> BSPRenderer::sortedDrawCalls;
GLuint BSPRenderer::defaultTex = 0;

bool BSPRenderer::initialize(BSPLoader& loader, MeshCollider& collider) {
    if (!loader.isLoaded()) return false;

    const auto& vertices = loader.getMeshVertices();
    const auto& indices = loader.getMeshIndices();

    if (vertices.empty() || indices.empty()) return false;

    setupCollider(loader, collider);
    setupBuffers(vertices, indices);

    // Sort draw calls by texture for batching
    sortedDrawCalls = loader.getDrawCalls();
    std::sort(sortedDrawCalls.begin(), sortedDrawCalls.end(),
        [](const FaceDrawCall& a, const FaceDrawCall& b) { return a.texID < b.texID; });

    defaultTex = loader.getDefaultTextureID();

    return true;
}

void BSPRenderer::setupCollider(BSPLoader& loader, MeshCollider& collider) {
    collider.buildFromBSP(loader.getMeshVertices(), loader.getMeshIndices());
    std::cout << "Mesh collider built with " << collider.getTriangleCount() << " triangles\n";
}

void BSPRenderer::setupBuffers(const std::vector<BSPVertex>& vertices,
    const std::vector<unsigned int>& indices) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BSPVertex),
        vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, position));
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, normal));
    glEnableVertexAttribArray(1);

    // TexCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    indexCount = indices.size();
}

void BSPRenderer::render(BSPLoader& loader,
    const glm::mat4& projection,
    const glm::mat4& view,
    const glm::vec3& viewPos) {
    if (vao == 0) return;

    ShaderManager::use();
    ShaderManager::setView(view);
    ShaderManager::setProjection(projection);
    ShaderManager::setViewPos(viewPos);

    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    const glm::mat4 model(1.0f);
    ShaderManager::setModel(model);

    if (!sortedDrawCalls.empty()) {
        // Batched rendering by texture
        GLuint lastTex = 0;
        for (const auto& dc : sortedDrawCalls) {
            if (dc.texID != lastTex) {
                glBindTexture(GL_TEXTURE_2D, dc.texID);
                lastTex = dc.texID;
            }

            glm::mat4 mvp = projection * view * model;
            ShaderManager::setMVP(mvp);
            ShaderManager::setColor(glm::vec3(1.0f));

            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(dc.indexCount),
                GL_UNSIGNED_INT, (void*)(dc.indexOffset * sizeof(unsigned int)));
        }
    }
    else {
        // Fallback: render all with default texture
        glBindTexture(GL_TEXTURE_2D, defaultTex);
        glm::mat4 mvp = projection * view * model;
        ShaderManager::setMVP(mvp);
        ShaderManager::setColor(glm::vec3(0.8f));
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount),
            GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
}

void BSPRenderer::cleanup() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;
    indexCount = 0;
    sortedDrawCalls.clear();
}
