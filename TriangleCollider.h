#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "AABB.h"  // Здесь уже определён Capsule

// Предварительное объявление
struct BSPVertex;

struct Triangle {
    glm::vec3 vertices[3];
    glm::vec3 normal;
};

class MeshCollider {
public:
    void buildFromBSP(const std::vector<BSPVertex>& vertices,
        const std::vector<unsigned int>& indices);

    bool intersectCapsule(const Capsule& capsule) const;
    bool intersectRay(const glm::vec3& origin, const glm::vec3& dir,
        float& outDist, glm::vec3& outNormal) const;

    void updateDynamicTriangles(const std::vector<Triangle>& triangles);

    size_t getTriangleCount() const {
        return staticTriangles.size() + dynamicTriangles.size();
    }

private:
    std::vector<Triangle> staticTriangles;
    std::vector<Triangle> dynamicTriangles;

    // Вспомогательные методы
    bool intersectCapsuleTriangle(const Capsule& capsule, const Triangle& tri) const;
    float distPointLineSegment(const glm::vec3& p, const glm::vec3& a,
        const glm::vec3& b) const;
    bool pointInTriangle(const glm::vec3& p, const Triangle& tri) const;
};