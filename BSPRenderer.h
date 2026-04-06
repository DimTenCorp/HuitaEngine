#pragma once

#include "gl_includes.h"  // glad + glfw + glm с нужными расширениями
#include <vector>
#include "BSPLoader.h"
#include "TriangleCollider.h"
#include "ShaderManager.h"

class BSPRenderer {
public:
    static bool initialize(BSPLoader& loader, MeshCollider& collider);
    static void cleanup();

    static void render(BSPLoader& loader,
        const glm::mat4& projection,
        const glm::mat4& view,
        const glm::vec3& viewPos);
    
    // Shadow pass
    static void renderShadowPass(BSPLoader& loader, const glm::mat4& lightSpaceMatrix);
    static GLuint getShadowMapTexture() { return shadowMapTexture; }

private:
    static unsigned int vao, vbo, ebo;
    static size_t indexCount;
    static std::vector<FaceDrawCall> sortedDrawCalls;
    static GLuint defaultTex;
    
    // Shadow map
    static const unsigned int SHADOW_WIDTH = 2048;
    static const unsigned int SHADOW_HEIGHT = 2048;
    static unsigned int shadowMapFBO;
    static unsigned int shadowMapTexture;

    static void setupBuffers(const std::vector<BSPVertex>& vertices,
        const std::vector<unsigned int>& indices);
    static void setupCollider(BSPLoader& loader, MeshCollider& collider);
    static void setupShadowMap();
};