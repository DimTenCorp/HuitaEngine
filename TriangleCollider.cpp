#include "pch.h"
#include "TriangleCollider.h"
#include "BSPLoader.h"
#include <algorithm>
#include <cmath>
#include <limits>

MeshCollider::MeshCollider() : cellSize(50.0f), gridBuilt(false) {}

void MeshCollider::buildFromBSP(const std::vector<BSPVertex>& vertices,
    const std::vector<unsigned int>& indices) {
    staticTriangles.clear();
    spatialGrid.clear();
    gridBuilt = false;

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Triangle tri;
        tri.vertices[0] = vertices[indices[i]].position;
        tri.vertices[1] = vertices[indices[i + 1]].position;
        tri.vertices[2] = vertices[indices[i + 2]].position;

        tri.normal = glm::normalize(glm::cross(
            tri.vertices[1] - tri.vertices[0],
            tri.vertices[2] - tri.vertices[0]
        ));

        staticTriangles.push_back(tri);
    }
    
    // Строим spatial grid после загрузки всех треугольников
    buildSpatialGrid();
}

AABB MeshCollider::getTriangleBounds(const Triangle& tri) const {
    AABB bounds;
    bounds.min = glm::min(tri.vertices[0], glm::min(tri.vertices[1], tri.vertices[2]));
    bounds.max = glm::max(tri.vertices[0], glm::max(tri.vertices[1], tri.vertices[2]));
    return bounds;
}

GridKey MeshCollider::getGridKey(const glm::vec3& pos) const {
    GridKey key;
    key.x = (int)floor(pos.x / cellSize);
    key.y = (int)floor(pos.y / cellSize);
    key.z = (int)floor(pos.z / cellSize);
    return key;
}

void MeshCollider::buildSpatialGrid() {
    spatialGrid.clear();
    
    // Добавляем статические треугольники в grid
    for (size_t i = 0; i < staticTriangles.size(); i++) {
        const auto& tri = staticTriangles[i];
        AABB bounds = getTriangleBounds(tri);
        
        // Находим ячейки, которые покрывает этот треугольник
        GridKey minKey = getGridKey(bounds.min);
        GridKey maxKey = getGridKey(bounds.max);
        
        // Добавляем треугольник во все ячейки, которые он пересекает
        for (int x = minKey.x; x <= maxKey.x; x++) {
            for (int y = minKey.y; y <= maxKey.y; y++) {
                for (int z = minKey.z; z <= maxKey.z; z++) {
                    GridKey key = {x, y, z};
                    spatialGrid[key].push_back(i);
                }
            }
        }
    }
    
    gridBuilt = true;
}

void MeshCollider::getTrianglesInCapsule(const Capsule& capsule, 
    std::vector<size_t>& outIndices) const {
    if (!gridBuilt) return;
    
    AABB bounds = capsule.getBounds();
    GridKey minKey = getGridKey(bounds.min - glm::vec3(capsule.radius));
    GridKey maxKey = getGridKey(bounds.max + glm::vec3(capsule.radius));
    
    std::unordered_set<size_t> uniqueIndices;
    
    // Собираем все треугольники из ячеек, которые пересекает капсула
    for (int x = minKey.x; x <= maxKey.x; x++) {
        for (int y = minKey.y; y <= maxKey.y; y++) {
            for (int z = minKey.z; z <= maxKey.z; z++) {
                GridKey key = {x, y, z};
                auto it = spatialGrid.find(key);
                if (it != spatialGrid.end()) {
                    for (size_t idx : it->second) {
                        uniqueIndices.insert(idx);
                    }
                }
            }
        }
    }
    
    outIndices.assign(uniqueIndices.begin(), uniqueIndices.end());
}

void MeshCollider::updateDynamicTriangles(const std::vector<Triangle>& triangles) {
    dynamicTriangles = triangles;
}

float MeshCollider::distPointLineSegment(const glm::vec3& p, const glm::vec3& a,
    const glm::vec3& b) const {
    glm::vec3 ab = b - a;
    float abLenSq = glm::dot(ab, ab);

    if (abLenSq < 0.0001f) {
        return glm::length(p - a);
    }

    glm::vec3 ap = p - a;
    float t = glm::clamp(glm::dot(ap, ab) / abLenSq, 0.0f, 1.0f);
    glm::vec3 closest = a + ab * t;
    return glm::length(p - closest);
}

bool MeshCollider::pointInTriangle(const glm::vec3& p, const Triangle& tri) const {
    glm::vec3 v0 = tri.vertices[2] - tri.vertices[0];
    glm::vec3 v1 = tri.vertices[1] - tri.vertices[0];
    glm::vec3 v2 = p - tri.vertices[0];

    float dot00 = glm::dot(v0, v0);
    float dot01 = glm::dot(v0, v1);
    float dot02 = glm::dot(v0, v2);
    float dot11 = glm::dot(v1, v1);
    float dot12 = glm::dot(v1, v2);

    float denom = dot00 * dot11 - dot01 * dot01;
    if (std::abs(denom) < 0.0001f) return false;

    float invDenom = 1.0f / denom;
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= -0.001f) && (v >= -0.001f) && (u + v <= 1.001f);
}

bool MeshCollider::intersectCapsuleTriangle(const Capsule& capsule,
    const Triangle& tri) const {
    // Быстрая проверка AABB
    AABB triBounds;
    triBounds.min = glm::min(tri.vertices[0], glm::min(tri.vertices[1], tri.vertices[2]));
    triBounds.max = glm::max(tri.vertices[0], glm::max(tri.vertices[1], tri.vertices[2]));

    // Расширяем bounds треугольника на радиус капсулы для быстрой проверки
    AABB expandedTriBounds = triBounds;
    expandedTriBounds.min -= glm::vec3(capsule.radius);
    expandedTriBounds.max += glm::vec3(capsule.radius);

    AABB capsuleBounds = capsule.getBounds();

    if (!expandedTriBounds.contains(capsuleBounds.min) && !expandedTriBounds.contains(capsuleBounds.max)) {
        // Проверяем пересечение AABB
        if (capsuleBounds.max.x < triBounds.min.x || capsuleBounds.min.x > triBounds.max.x ||
            capsuleBounds.max.y < triBounds.min.y || capsuleBounds.min.y > triBounds.max.y ||
            capsuleBounds.max.z < triBounds.min.z || capsuleBounds.min.z > triBounds.max.z) {
            return false;
        }
    }

    // Проверяем расстояние от отрезка капсулы до треугольника
    // 1. Проверяем концы капсулы относительно плоскости треугольника
    for (int i = 0; i < 2; i++) {
        const glm::vec3& p = (i == 0) ? capsule.start : capsule.end;

        float dist = glm::dot(p - tri.vertices[0], tri.normal);
        glm::vec3 proj = p - tri.normal * dist;

        // Если проекция внутри треугольника
        if (pointInTriangle(proj, tri)) {
            if (std::abs(dist) < capsule.radius) {
                return true;
            }
        }

        // Проверяем расстояние до рёбер треугольника
        for (int e = 0; e < 3; e++) {
            const glm::vec3& e1 = tri.vertices[e];
            const glm::vec3& e2 = tri.vertices[(e + 1) % 3];
            float d = distPointLineSegment(p, e1, e2);
            if (d < capsule.radius) {
                return true;
            }
        }
    }

    // 2. Проверяем пересечение отрезка капсулы с треугольником
    glm::vec3 ab = capsule.end - capsule.start;
    float denom = glm::dot(ab, tri.normal);

    if (std::abs(denom) > 0.0001f) {
        float t = glm::dot(tri.vertices[0] - capsule.start, tri.normal) / denom;
        if (t >= 0.0f && t <= 1.0f) {
            glm::vec3 hit = capsule.start + ab * t;
            if (pointInTriangle(hit, tri)) {
                return true;
            }
        }
    }

    // 3. Проверяем расстояние от центра капсулы
    glm::vec3 center = (capsule.start + capsule.end) * 0.5f;
    float dist = glm::dot(center - tri.vertices[0], tri.normal);
    glm::vec3 proj = center - tri.normal * dist;

    if (pointInTriangle(proj, tri) && std::abs(dist) < capsule.radius) {
        return true;
    }

    return false;
}

bool MeshCollider::intersectCapsule(const Capsule& capsule) const {
    // Получаем только треугольники, которые находятся рядом с капсулой
    std::vector<size_t> nearbyTriangles;
    getTrianglesInCapsule(capsule, nearbyTriangles);
    
    // Проверяем только близкие статичные треугольники
    for (size_t idx : nearbyTriangles) {
        if (idx < staticTriangles.size()) {
            if (intersectCapsuleTriangle(capsule, staticTriangles[idx])) {
                return true;
            }
        }
    }

    // Проверяем динамические треугольники (двери) - их мало, проверяем все
    for (const auto& tri : dynamicTriangles) {
        if (intersectCapsuleTriangle(capsule, tri)) {
            return true;
        }
    }

    return false;
}

bool MeshCollider::intersectRay(const glm::vec3& origin, const glm::vec3& dir,
    float& outDist, glm::vec3& outNormal) const {
    bool hit = false;
    outDist = 999999.0f;
    outNormal = glm::vec3(0, 1, 0);

    auto checkTriangles = [&](const std::vector<Triangle>& triangles) {
        for (const auto& tri : triangles) {
            float denom = glm::dot(dir, tri.normal);
            if (std::abs(denom) < 0.0001f) continue;

            float t = glm::dot(tri.vertices[0] - origin, tri.normal) / denom;
            if (t < 0.001f || t > outDist) continue;

            glm::vec3 hitPoint = origin + dir * t;
            if (pointInTriangle(hitPoint, tri)) {
                outDist = t;
                outNormal = (denom < 0) ? tri.normal : -tri.normal;
                hit = true;
            }
        }
        };

    checkTriangles(staticTriangles);
    checkTriangles(dynamicTriangles);

    return hit;
}