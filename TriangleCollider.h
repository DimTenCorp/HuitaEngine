#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include "AABB.h"
#include "BSPLoader.h"  // <-- Добавляем include вместо forward declaration

// УДАЛИТЬ этот блок (было строки 8-16):
// struct BSPVertex {
//     glm::vec3 position;
//     glm::vec2 texCoord;
//     glm::vec2 lightmapCoord;
//     glm::vec3 normal;
// };

struct Triangle {
    glm::vec3 vertices[3];
    glm::vec3 normal;
};

struct GridKey {
    int x, y, z;
    bool operator==(const GridKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct GridKeyHash {
    size_t operator()(const GridKey& key) const {
        return ((std::hash<int>{}(key.x) * 73856093) ^
            (std::hash<int>{}(key.y) * 19349663) ^
            (std::hash<int>{}(key.z) * 83492791));
    }
};

class MeshCollider {
public:
    MeshCollider();

    void buildFromBSP(const std::vector<BSPVertex>& vertices,
        const std::vector<unsigned int>& indices);

    bool intersectCapsuleQuick(const Capsule& capsule) const;
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

    std::unordered_map<GridKey, std::vector<size_t>, GridKeyHash> spatialGrid;
    float cellSize;
    bool gridBuilt;

    mutable GridKey lastKey;
    mutable std::vector<size_t> lastTriangles;
    mutable float lastRadius;

    bool intersectCapsuleTriangle(const Capsule& capsule, const Triangle& tri) const;
    float distPointLineSegment(const glm::vec3& p, const glm::vec3& a,
        const glm::vec3& b) const;
    bool pointInTriangle(const glm::vec3& p, const Triangle& tri) const;

    void buildSpatialGrid();
    GridKey getGridKey(const glm::vec3& pos) const;

    void getTrianglesInCapsuleFast(const Capsule& capsule,
        std::vector<size_t>& outIndices) const;

    AABB getTriangleBounds(const Triangle& tri) const;
};