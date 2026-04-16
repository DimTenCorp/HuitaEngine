#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "AABB.h"  // Здесь уже определён Capsule

// Предварительное объявление
struct BSPVertex;

struct Triangle {
    glm::vec3 vertices[3];
    glm::vec3 normal;
};

// Ключ для spatial hash
struct GridKey {
    int x, y, z;
    
    bool operator==(const GridKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Хеш-функция для GridKey
struct GridKeyHash {
    size_t operator()(const GridKey& key) const {
        size_t h1 = std::hash<int>{}(key.x);
        size_t h2 = std::hash<int>{}(key.y);
        size_t h3 = std::hash<int>{}(key.z);
        
        // Комбинируем хеши
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class MeshCollider {
public:
    MeshCollider();
    
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
    
    // Spatial hash grid для оптимизации коллизий
    mutable std::unordered_map<GridKey, std::vector<size_t>, GridKeyHash> spatialGrid;
    float cellSize;
    bool gridBuilt;
    
    // Вспомогательные методы
    bool intersectCapsuleTriangle(const Capsule& capsule, const Triangle& tri) const;
    float distPointLineSegment(const glm::vec3& p, const glm::vec3& a,
        const glm::vec3& b) const;
    bool pointInTriangle(const glm::vec3& p, const Triangle& tri) const;
    
    // Методы spatial hash
    void buildSpatialGrid();
    GridKey getGridKey(const glm::vec3& pos) const;
    void getTrianglesInCapsule(const Capsule& capsule, 
        std::vector<size_t>& outIndices) const;
    AABB getTriangleBounds(const Triangle& tri) const;
};