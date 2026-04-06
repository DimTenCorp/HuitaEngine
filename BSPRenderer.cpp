#include "BSPRenderer.h"
#include "ShaderManager.h"
#include <algorithm>
#include <iostream>

unsigned int BSPRenderer::vao = 0, BSPRenderer::vbo = 0, BSPRenderer::ebo = 0;
size_t BSPRenderer::indexCount = 0;
std::vector<FaceDrawCall> BSPRenderer::sortedDrawCalls;
GLuint BSPRenderer::defaultTex = 0;
unsigned int BSPRenderer::shadowMapFBO = 0;
unsigned int BSPRenderer::shadowMapTexture = 0;

bool BSPRenderer::initialize(BSPLoader& loader, MeshCollider& collider) {
    if (!loader.isLoaded()) return false;

    const auto& vertices = loader.getMeshVertices();
    const auto& indices = loader.getMeshIndices();

    if (vertices.empty() || indices.empty()) return false;

    setupCollider(loader, collider);
    setupBuffers(vertices, indices);
    setupShadowMap();

    // Sort draw calls by texture for batching
    sortedDrawCalls = loader.getDrawCalls();
    std::sort(sortedDrawCalls.begin(), sortedDrawCalls.end(),
        [](const FaceDrawCall& a, const FaceDrawCall& b) { return a.texID < b.texID; });

    defaultTex = loader.getDefaultTextureID();

    return true;
}

void BSPRenderer::setupShadowMap() {
    // Create depth texture
    glGenTextures(1, &shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    
    // Create FBO
    glGenFramebuffers(1, &shadowMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow map framebuffer incomplete!\n";
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "Shadow map initialized: " << SHADOW_WIDTH << "x" << SHADOW_HEIGHT << "\n";
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

void BSPRenderer::renderShadowPass(BSPLoader& loader, const glm::mat4& lightSpaceMatrix) {
    if (vao == 0) return;
    
    // Render to shadow map FBO
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    
    ShaderManager::useShadowShader();
    ShaderManager::setShadowMVP(lightSpaceMatrix);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    
    const glm::mat4 model(1.0f);
    
    if (!sortedDrawCalls.empty()) {
        for (const auto& dc : sortedDrawCalls) {
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(dc.indexCount),
                GL_UNSIGNED_INT,
                (void*)(dc.indexOffset * sizeof(unsigned int)));
        }
    }
    else {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount),
            GL_UNSIGNED_INT, 0);
    }
    
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
    
    if (shadowMapFBO) glDeleteFramebuffers(1, &shadowMapFBO);
    if (shadowMapTexture) glDeleteTextures(1, &shadowMapTexture);
    shadowMapFBO = shadowMapTexture = 0;
}