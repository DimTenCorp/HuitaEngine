#include "pch.h"
#include "TriangleCollider.h"
#include "BSPLoader.h"
#include <algorithm>
#include <cmath>

// УВЕЛИЧИВАЕМ cellSize для меньшего числа переключений ячеек
MeshCollider::MeshCollider() : cellSize(128.0f), gridBuilt(false), lastRadius(-1.0f) {}

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

    for (size_t i = 0; i < staticTriangles.size(); i++) {
        const auto& tri = staticTriangles[i];
        AABB bounds = getTriangleBounds(tri);

        GridKey minKey = getGridKey(bounds.min);
        GridKey maxKey = getGridKey(bounds.max);

        for (int x = minKey.x; x <= maxKey.x; x++) {
            for (int y = minKey.y; y <= maxKey.y; y++) {
                for (int z = minKey.z; z <= maxKey.z; z++) {
                    GridKey key = { x, y, z };
                    spatialGrid[key].push_back(i);
                }
            }
        }
    }

    gridBuilt = true;
}

// ОПТИМИЗИРОВАННЫЙ метод - без unordered_set, с простым массивом + сортировка/уник
void MeshCollider::getTrianglesInCapsuleFast(const Capsule& capsule,
    std::vector<size_t>& outIndices) const {

    if (!gridBuilt) return;

    AABB bounds = capsule.getBounds();
    GridKey minKey = getGridKey(bounds.min - glm::vec3(capsule.radius));
    GridKey maxKey = getGridKey(bounds.max + glm::vec3(capsule.radius));

    outIndices.clear();

    // Простой вектор без проверки уникальности на этапе добавления
    for (int x = minKey.x; x <= maxKey.x; x++) {
        for (int y = minKey.y; y <= maxKey.y; y++) {
            for (int z = minKey.z; z <= maxKey.z; z++) {
                GridKey key = { x, y, z };
                auto it = spatialGrid.find(key);
                if (it != spatialGrid.end()) {
                    outIndices.insert(outIndices.end(),
                        it->second.begin(), it->second.end());
                }
            }
        }
    }

    // Убираем дубликаты если есть (редко при большом cellSize)
    if (outIndices.size() > 100) {  // Только если много
        std::sort(outIndices.begin(), outIndices.end());
        outIndices.erase(std::unique(outIndices.begin(), outIndices.end()),
            outIndices.end());
    }
}

// БЫСТРАЯ проверка - только AABB vs AABB для раннего выхода
bool MeshCollider::intersectCapsuleQuick(const Capsule& capsule) const {
    std::vector<size_t> nearby;
    getTrianglesInCapsuleFast(capsule, nearby);

    // Только AABB проверка для статичных
    AABB capBounds = capsule.getBounds();

    for (size_t idx : nearby) {
        if (idx >= staticTriangles.size()) continue;

        const auto& tri = staticTriangles[idx];
        AABB triBounds;
        triBounds.min = glm::min(tri.vertices[0], glm::min(tri.vertices[1], tri.vertices[2]));
        triBounds.max = glm::max(tri.vertices[0], glm::max(tri.vertices[1], tri.vertices[2]));

        if (capBounds.intersects(triBounds)) {
            // Быстрая проверка расстояния до плоскости
            glm::vec3 center = (capsule.start + capsule.end) * 0.5f;
            float dist = glm::dot(center - tri.vertices[0], tri.normal);
            if (std::abs(dist) < capsule.radius + 16.0f) {  // + margin
                return true;  // Вероятно пересекается
            }
        }
    }

    // Динамические всегда полная проверка (их мало)
    for (const auto& tri : dynamicTriangles) {
        if (intersectCapsuleTriangle(capsule, tri)) return true;
    }

    return false;
}

// ПОЛНАЯ проверка когда нужна точность
bool MeshCollider::intersectCapsule(const Capsule& capsule) const {
    std::vector<size_t> nearby;
    getTrianglesInCapsuleFast(capsule, nearby);

    for (size_t idx : nearby) {
        if (idx < staticTriangles.size()) {
            if (intersectCapsuleTriangle(capsule, staticTriangles[idx])) return true;
        }
    }

    for (const auto& tri : dynamicTriangles) {
        if (intersectCapsuleTriangle(capsule, tri)) return true;
    }

    return false;
}

void MeshCollider::updateDynamicTriangles(const std::vector<Triangle>& triangles) {
    dynamicTriangles = triangles;
}

float MeshCollider::distPointLineSegment(const glm::vec3& p, const glm::vec3& a,
    const glm::vec3& b) const {
    glm::vec3 ab = b - a;
    float abLenSq = glm::dot(ab, ab);
    if (abLenSq < 0.0001f) return glm::length(p - a);

    float t = glm::clamp(glm::dot(p - a, ab) / abLenSq, 0.0f, 1.0f);
    return glm::length(p - (a + ab * t));
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

// Упрощённая проверка пересечения - меньше ветвлений
bool MeshCollider::intersectCapsuleTriangle(const Capsule& capsule,
    const Triangle& tri) const {

    // Быстрая AABB проверка
    AABB triBounds;
    triBounds.min = glm::min(tri.vertices[0], glm::min(tri.vertices[1], tri.vertices[2]));
    triBounds.max = glm::max(tri.vertices[0], glm::max(tri.vertices[1], tri.vertices[2]));

    AABB capBounds = capsule.getBounds();
    if (!triBounds.intersects(capBounds)) return false;

    // Расширяем bounds треугольника на радиус для быстрой проверки
    AABB expandedTri = triBounds;
    expandedTri.min -= glm::vec3(capsule.radius);
    expandedTri.max += glm::vec3(capsule.radius);

    glm::vec3 capCenter = capsule.getCenter();
    if (!expandedTri.contains(capCenter)) {
        // Проверяем расстояние до bounds
        glm::vec3 closest = glm::clamp(capCenter, expandedTri.min, expandedTri.max);
        if (glm::dot(capCenter - closest, capCenter - closest) > capsule.radius * capsule.radius) {
            return false;
        }
    }

    // Проверяем концы капсулы
    for (int i = 0; i < 2; i++) {
        const glm::vec3& p = (i == 0) ? capsule.start : capsule.end;

        float dist = glm::dot(p - tri.vertices[0], tri.normal);
        if (std::abs(dist) < capsule.radius) {
            glm::vec3 proj = p - tri.normal * dist;
            if (pointInTriangle(proj, tri)) return true;
        }

        // Рёбра
        for (int e = 0; e < 3; e++) {
            if (distPointLineSegment(p, tri.vertices[e], tri.vertices[(e + 1) % 3]) < capsule.radius)
                return true;
        }
    }

    // Проверяем пересечение отрезка с треугольником
    glm::vec3 ab = capsule.end - capsule.start;
    float denom = glm::dot(ab, tri.normal);
    if (std::abs(denom) > 0.0001f) {
        float t = glm::dot(tri.vertices[0] - capsule.start, tri.normal) / denom;
        if (t >= 0.0f && t <= 1.0f) {
            glm::vec3 hit = capsule.start + ab * t;
            if (pointInTriangle(hit, tri)) return true;
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