#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <glad/glad.h>
#include "TriangleCollider.h"

struct StaticShadow {
    glm::vec3 position;
    float radius;
    std::vector<bool> visibilityGrid;
    AABB bounds;
    int gridResolution = 16;

    bool isVisible(const glm::vec3& point) const;
};

struct DynamicShadow {
    GLuint fbo = 0;
    GLuint depthMap = 0;
    int resolution = 1024;
    glm::mat4 lightSpaceMatrix;
    glm::vec3 position;
    glm::vec3 direction;
    float radius;

    bool create(int res);
    void destroy();
    void updateMatrix();
};

class ShadowSystem {
public:
    ShadowSystem();
    ~ShadowSystem();

    void init(const MeshCollider* worldCollider);

    // Запекание света отключено - используется только lightmap из BSP
    void clearStaticLights();
    bool canSeeLight(int lightID, const glm::vec3& point) const;

    // Динамические тени отключены
    bool createDynamicShadow(const glm::vec3& pos, const glm::vec3& dir, float fov, float range);
    void updateDynamicShadow(const glm::vec3& pos, const glm::vec3& dir);
    void bindDynamicShadowForWriting();
    void unbindDynamicShadow(int screenWidth, int screenHeight);

    const glm::mat4& getDynamicLightSpaceMatrix() const;
    GLuint getDynamicDepthMap() const;

    // Проверка блокировки луча - используется только для внутреннего тестирования
    bool rayBlocked(const glm::vec3& from, const glm::vec3& to) const;

    const std::vector<StaticShadow>& getStaticLights() const { return staticLights; }

private:
    const MeshCollider* world = nullptr;
    std::vector<StaticShadow> staticLights;
    DynamicShadow dynamicShadow;
};