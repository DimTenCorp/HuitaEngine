#include "ShadowSystem.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <unordered_set>

// ============================================================================
// StaticShadow
// ============================================================================

bool StaticShadow::isVisible(const glm::vec3& point) const {
    if (point.x < bounds.min.x || point.x > bounds.max.x ||
        point.y < bounds.min.y || point.y > bounds.max.y ||
        point.z < bounds.min.z || point.z > bounds.max.z) {
        return false;
    }

    glm::vec3 local = (point - bounds.min) / (bounds.max - bounds.min);
    local = glm::clamp(local, 0.0f, 0.999f);

    int x = static_cast<int>(local.x * gridResolution);
    int y = static_cast<int>(local.y * gridResolution);
    int z = static_cast<int>(local.z * gridResolution);

    int idx = x + y * gridResolution + z * gridResolution * gridResolution;
    if (idx < 0 || idx >= (int)visibilityGrid.size()) return false;

    return visibilityGrid[idx];
}

// ============================================================================
// DynamicShadow
// ============================================================================

bool DynamicShadow::create(int res) {
    destroy();
    resolution = res;

    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &depthMap);

    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, res, res, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}

void DynamicShadow::destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (depthMap) { glDeleteTextures(1, &depthMap); depthMap = 0; }
}

void DynamicShadow::updateMatrix() {
    float aspect = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = radius;
    float fov = glm::radians(60.0f);

    glm::mat4 proj = glm::perspective(fov, aspect, nearPlane, farPlane);
    glm::mat4 view = glm::lookAt(position, position + direction, glm::vec3(0, 1, 0));
    lightSpaceMatrix = proj * view;
}

// ============================================================================
// ShadowSystem
// ============================================================================

ShadowSystem::ShadowSystem() = default;
ShadowSystem::~ShadowSystem() {
    clearStaticLights();
    dynamicShadow.destroy();
}

void ShadowSystem::init(const MeshCollider* worldCollider) {
    world = worldCollider;
}

bool ShadowSystem::rayBlocked(const glm::vec3& from, const glm::vec3& to) const {
    if (!world) return false;

    glm::vec3 dir = to - from;
    float len = glm::length(dir);
    if (len < 0.001f) return false;

    dir /= len;

    AABB rayBox;
    rayBox.min = glm::min(from, to) - glm::vec3(0.01f);
    rayBox.max = glm::max(from, to) + glm::vec3(0.01f);

    auto candidates = world->getCandidateTriangles(rayBox);

    for (size_t triIdx : candidates) {
        const Triangle& tri = world->getTriangles()[triIdx];

        const float EPSILON = 0.0001f;
        glm::vec3 edge1 = tri.v1 - tri.v0;
        glm::vec3 edge2 = tri.v2 - tri.v0;
        glm::vec3 h = glm::cross(dir, edge2);
        float a = glm::dot(edge1, h);

        if (a > -EPSILON && a < EPSILON) continue;

        float f = 1.0f / a;
        glm::vec3 s = from - tri.v0;
        float u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f) continue;

        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(dir, q);
        if (v < 0.0f || u + v > 1.0f) continue;

        float t = f * glm::dot(edge2, q);

        if (t > 0.01f && t < len - 0.01f) {
            return true;
        }
    }

    return false;
}

int ShadowSystem::bakeStaticLight(const glm::vec3& pos, float radius, const AABB& worldBounds) {
    StaticShadow shadow;
    shadow.position = pos;
    shadow.radius = radius;

    glm::vec3 extent = glm::vec3(radius * 1.2f);
    shadow.bounds.min = glm::max(worldBounds.min, pos - extent);
    shadow.bounds.max = glm::min(worldBounds.max, pos + extent);
    shadow.bounds.validate();

    int gridSize = shadow.gridResolution * shadow.gridResolution * shadow.gridResolution;
    shadow.visibilityGrid.resize(gridSize);

    std::cout << "[Shadow] Baking light at (" << pos.x << "," << pos.y << "," << pos.z
        << ") radius=" << radius << std::endl;

    int visibleCount = 0;

    for (int z = 0; z < shadow.gridResolution; z++) {
        for (int y = 0; y < shadow.gridResolution; y++) {
            for (int x = 0; x < shadow.gridResolution; x++) {
                float fx = (x + 0.5f) / shadow.gridResolution;
                float fy = (y + 0.5f) / shadow.gridResolution;
                float fz = (z + 0.5f) / shadow.gridResolution;

                glm::vec3 point(
                    shadow.bounds.min.x + fx * (shadow.bounds.max.x - shadow.bounds.min.x),
                    shadow.bounds.min.y + fy * (shadow.bounds.max.y - shadow.bounds.min.y),
                    shadow.bounds.min.z + fz * (shadow.bounds.max.z - shadow.bounds.min.z)
                );

                float dist = glm::distance(pos, point);
                bool visible = (dist <= radius) && !rayBlocked(pos, point);

                int idx = x + y * shadow.gridResolution + z * shadow.gridResolution * shadow.gridResolution;
                shadow.visibilityGrid[idx] = visible;
                if (visible) visibleCount++;
            }
        }
    }

    std::cout << "[Shadow] Visible: " << visibleCount << "/" << gridSize << std::endl;

    staticLights.push_back(std::move(shadow));
    return (int)staticLights.size() - 1;
}

void ShadowSystem::clearStaticLights() {
    staticLights.clear();
}

bool ShadowSystem::canSeeLight(int lightID, const glm::vec3& point) const {
    if (lightID < 0 || lightID >= (int)staticLights.size()) return false;
    return staticLights[lightID].isVisible(point);
}

bool ShadowSystem::createDynamicShadow(const glm::vec3& pos, const glm::vec3& dir,
    float fov, float range) {
    if (!dynamicShadow.create(1024)) return false;

    dynamicShadow.position = pos;
    dynamicShadow.direction = glm::normalize(dir);
    dynamicShadow.radius = range;
    dynamicShadow.updateMatrix();

    return true;
}

void ShadowSystem::updateDynamicShadow(const glm::vec3& pos, const glm::vec3& dir) {
    dynamicShadow.position = pos;
    dynamicShadow.direction = glm::normalize(dir);
    dynamicShadow.updateMatrix();
}

void ShadowSystem::bindDynamicShadowForWriting() {
    glBindFramebuffer(GL_FRAMEBUFFER, dynamicShadow.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, dynamicShadow.resolution, dynamicShadow.resolution);
}

void ShadowSystem::unbindDynamicShadow(int screenWidth, int screenHeight) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth, screenHeight);
}

const glm::mat4& ShadowSystem::getDynamicLightSpaceMatrix() const {
    return dynamicShadow.lightSpaceMatrix;
}

GLuint ShadowSystem::getDynamicDepthMap() const {
    return dynamicShadow.depthMap;
}