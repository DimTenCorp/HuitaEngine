#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <algorithm>

// Include shared AABB definition
#include "AABB.h"

struct Triangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
    AABB bounds;

    Triangle() = default;
    Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        v0 = a; v1 = b; v2 = c;
        normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        bounds.min = glm::min(glm::min(v0, v1), v2);
        bounds.max = glm::max(glm::max(v0, v1), v2);
    }
};

struct SweepResult {
    bool hit = false;
    float t = 1.0f;
    glm::vec3 point;
    glm::vec3 normal;
    float distance = 0.0f;
};

// Forward declaration of BSPVertex
struct BSPVertex;

class MeshCollider {
private:
    std::vector<Triangle> triangles;
    std::vector<AABB> cellBounds;
    AABB worldBounds;

    bool rayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir,
        const Triangle& tri, float& t, float& u, float& v) const;
    bool aabbTriangleIntersect(const AABB& box, const Triangle& tri) const;
    float pointTriangleDistance(const glm::vec3& p, const Triangle& tri, glm::vec3& closest) const;
    
    // Spatial partitioning for faster collision detection
    struct GridCell {
        std::vector<size_t> triangleIndices;
        AABB bounds;
    };
    std::vector<GridCell> spatialGrid;
    glm::ivec3 gridSize{1, 1, 1};
    float cellSize = 1.0f;
    bool gridBuilt = false;
    
    void buildSpatialGrid();
    std::vector<size_t> getCandidateTriangles(const AABB& box) const;

public:
    MeshCollider() = default;

    void buildFromBSP(const std::vector<BSPVertex>& vertices,
        const std::vector<unsigned int>& indices);

    SweepResult sweepAABB(const AABB& playerBox, const glm::vec3& start,
        const glm::vec3& end) const;

    bool intersectAABB(const AABB& box) const;

    bool findClosestPoint(const glm::vec3& point, float radius,
        glm::vec3& closest, glm::vec3& normal) const;

    const std::vector<Triangle>& getTriangles() const { return triangles; }
    size_t getTriangleCount() const { return triangles.size(); }
};