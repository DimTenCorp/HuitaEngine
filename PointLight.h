#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>

class PointLight {
public:
    PointLight();
    ~PointLight();

    void init(const glm::vec3& pos, float radius, int shadowSize = 256);  // ← 256 по умолчанию
    void cleanup();

    void setPosition(const glm::vec3& pos) { position = pos; }
    glm::vec3 getPosition() const { return position; }

    glm::vec3 color = glm::vec3(1.0f, 0.8f, 0.5f);
    float intensity = 1.0f;
    float radius = 10.0f;

    GLuint getShadowCubemap() const { return shadowCubemap; }
    GLuint getShadowFBO() const { return shadowFBO; }
    int getShadowSize() const { return shadowSize; }

    void getLightSpaceMatrices(glm::mat4 matrices[6]) const;

private:
    glm::vec3 position;
    GLuint shadowFBO = 0;
    GLuint shadowCubemap = 0;
    int shadowSize = 256;  // ← 256 вместо 512/1024

    void createShadowCubemap();
};