#pragma once
#include "gl_includes.h"

class DebugRenderer {
public:
    static void initialize();
    static void cleanup();
    static void renderHitbox(const glm::mat4& projection,
        const glm::mat4& view,
        const glm::vec3& position);
private:
    static unsigned int cubeVAO, cubeVBO, cubeEBO;
    static void setupCubeBuffers();
};