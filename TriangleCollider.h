#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
#include "AABB.h"

struct Triangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
    AABB bounds;
    bool isLiquid;  // Флаг жидкости для треугольника

    Triangle() : isLiquid(false) {}
    Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        v0 = a; v1 = b; v2 = c;
        normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        bounds.min = glm::min(glm::min(v0, v1), v2);
        bounds.max = glm::max(glm::max(v0, v1), v2);
        isLiquid = false;
    }
};

struct SweepResult {
    bool hit = false;
    float t = 1.0f;
    glm::vec3 point;
    glm::vec3 normal;
    float distance = 0.0f;
};

// Forward declaration
struct BSPVertex;

class MeshCollider {
private:
    std::vector<Triangle> triangles;
    AABB worldBounds;

    bool rayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir,
        const Triangle& tri, float& t, float& u, float& v) const;
    bool aabbTriangleIntersect(const AABB& box, const Triangle& tri) const;
    float pointTriangleDistance(const glm::vec3& p, const Triangle& tri, glm::vec3& closest) const;

    // === НОВОЕ: Помощники для капсулы ===
    float segmentSegmentDistance(const glm::vec3& p1, const glm::vec3& p2,
        const glm::vec3& p3, const glm::vec3& p4,
        glm::vec3& closest1, glm::vec3& closest2) const;
    float pointSegmentDistance(const glm::vec3& point, const glm::vec3& a,
        const glm::vec3& b, glm::vec3& closest) const;

public:
    MeshCollider() = default;

    void buildFromBSP(const std::vector<BSPVertex>& vertices,
        const std::vector<unsigned int>& indices,
        const std::vector<int>& faceTextureIndices,
        const BSPLoader* bspLoader);

    // Старые AABB методы (оставим для совместимости)
    bool intersectAABB(const AABB& box) const;
    SweepResult sweepAABB(const AABB& playerBox, const glm::vec3& start,
        const glm::vec3& end) const;

    // === НОВОЕ: Методы для капсулы ===
    bool intersectCapsule(const Capsule& capsule) const;
    SweepResult sweepCapsule(const Capsule& playerCapsule, const glm::vec3& start,
        const glm::vec3& end) const;

    bool findClosestPoint(const glm::vec3& point, float radius,
        glm::vec3& closest, glm::vec3& normal) const;

    const std::vector<Triangle>& getTriangles() const { return triangles; }
    size_t getTriangleCount() const { return triangles.size(); }
};