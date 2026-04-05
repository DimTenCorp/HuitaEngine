#pragma once

#include "gl_includes.h"  // glad + glfw + glm с нужными расширениями
#include <vector>
#include "BSPLoader.h"
#include "TriangleCollider.h"

class BSPRenderer {
public:
    static bool initialize(BSPLoader& loader, MeshCollider& collider);
    static void cleanup();

    static void render(BSPLoader& loader,
        const glm::mat4& projection,
        const glm::mat4& view,
        const glm::vec3& viewPos);

private:
    static unsigned int vao, vbo, ebo;
    static size_t indexCount;
    static std::vector<FaceDrawCall> sortedDrawCalls;
    static GLuint defaultTex;

    static void setupBuffers(const std::vector<BSPVertex>& vertices,
        const std::vector<unsigned int>& indices);
    static void setupCollider(BSPLoader& loader, MeshCollider& collider);
};