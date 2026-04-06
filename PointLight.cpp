#include "PointLight.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

PointLight::PointLight() = default;
PointLight::~PointLight() { cleanup(); }

void PointLight::init(const glm::vec3& pos, float rad, int shSize) {
    position = pos;
    radius = rad;
    shadowSize = shSize < 64 ? 64 : shSize;  // Минимум 64, но лучше 256

    std::cout << "[PointLight] Init at (" << pos.x << "," << pos.y << "," << pos.z
        << ") r=" << rad << " map=" << shadowSize << std::endl;

    createShadowCubemap();
}

void PointLight::cleanup() {
    if (shadowFBO) {
        glDeleteFramebuffers(1, &shadowFBO);
        shadowFBO = 0;
    }
    if (shadowCubemap) {
        glDeleteTextures(1, &shadowCubemap);
        shadowCubemap = 0;
    }
}

void PointLight::createShadowCubemap() {
    glGenTextures(1, &shadowCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowCubemap);

    for (int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
            shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glGenFramebuffers(1, &shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowCubemap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[PointLight] FBO incomplete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PointLight::getLightSpaceMatrices(glm::mat4 matrices[6]) const {
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, radius);

    matrices[0] = proj * glm::lookAt(position, position + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0));
    matrices[1] = proj * glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0));
    matrices[2] = proj * glm::lookAt(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
    matrices[3] = proj * glm::lookAt(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
    matrices[4] = proj * glm::lookAt(position, position + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
    matrices[5] = proj * glm::lookAt(position, position + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
}