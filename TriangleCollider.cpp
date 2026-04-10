#include "TriangleCollider.h"
#include <glm/glm.hpp>
#include <cmath>
#include <limits>
#include "BSPLoader.h"

void MeshCollider::buildFromBSP(const std::vector<BSPVertex>& vertices,
    const std::vector<unsigned int>& indices,
    const std::vector<int>& faceTextureIndices,
    const BSPLoader* bspLoader) {
    triangles.clear();

    if (indices.empty()) return;

    for (size_t i = 0; i < indices.size(); i += 3) {
        if (i + 2 >= indices.size()) break;

        unsigned int i0 = indices[i];
        unsigned int i1 = indices[i + 1];
        unsigned int i2 = indices[i + 2];

        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            continue;

        const glm::vec3& v0 = vertices[i0].position;
        const glm::vec3& v1 = vertices[i1].position;
        const glm::vec3& v2 = vertices[i2].position;

        Triangle tri(v0, v1, v2);
        if (glm::length(tri.normal) < 0.001f) continue;

        // Определяем, является ли треугольник жидкостью
        tri.isLiquid = false;
        if (bspLoader && i < faceTextureIndices.size()) {
            int texIndex = faceTextureIndices[i / 3];
            if (texIndex >= 0) {
                tri.isLiquid = bspLoader->isTextureLiquid(texIndex);
            }
        }

        triangles.push_back(tri);
    }

    if (!triangles.empty()) {
        worldBounds = triangles[0].bounds;
        for (const auto& tri : triangles) {
            worldBounds.min = glm::min(worldBounds.min, tri.bounds.min);
            worldBounds.max = glm::max(worldBounds.max, tri.bounds.max);
        }
    }
}

float MeshCollider::pointSegmentDistance(const glm::vec3& point, const glm::vec3& a,
    const glm::vec3& b, glm::vec3& closest) const {
    glm::vec3 ab = b - a;
    float t = glm::dot(point - a, ab) / glm::dot(ab, ab);
    t = glm::clamp(t, 0.0f, 1.0f);
    closest = a + ab * t;
    return glm::distance(point, closest);
}

float MeshCollider::segmentSegmentDistance(const glm::vec3& p1, const glm::vec3& p2,
    const glm::vec3& p3, const glm::vec3& p4,
    glm::vec3& closest1, glm::vec3& closest2) const {
    glm::vec3 u = p2 - p1;
    glm::vec3 v = p4 - p3;
    glm::vec3 w = p1 - p3;

    float a = glm::dot(u, u);
    float b = glm::dot(u, v);
    float c = glm::dot(v, v);
    float d = glm::dot(u, w);
    float e = glm::dot(v, w);

    float D = a * c - b * b;
    float sc, sN, sD = D;
    float tc, tN, tD = D;

    if (D < 0.0001f) {
        sN = 0.0f;
        sD = 1.0f;
        tN = e;
        tD = c;
    }
    else {
        sN = (b * e - c * d);
        tN = (a * e - b * d);
        if (sN < 0.0f) {
            sN = 0.0f;
            tN = e;
            tD = c;
        }
        else if (sN > sD) {
            sN = sD;
            tN = e + b;
            tD = c;
        }
    }

    if (tN < 0.0f) {
        tN = 0.0f;
        if (-d < 0.0f) sN = 0.0f;
        else if (-d > a) sN = sD;
        else {
            sN = -d;
            sD = a;
        }
    }
    else if (tN > tD) {
        tN = tD;
        if ((-d + b) < 0.0f) sN = 0;
        else if ((-d + b) > a) sN = sD;
        else {
            sN = (-d + b);
            sD = a;
        }
    }

    sc = (std::abs(sN) < 0.0001f ? 0.0f : sN / sD);
    tc = (std::abs(tN) < 0.0001f ? 0.0f : tN / tD);

    closest1 = p1 + sc * u;
    closest2 = p3 + tc * v;

    return glm::distance(closest1, closest2);
}

bool MeshCollider::intersectCapsule(const Capsule& capsule) const {
    // Broad phase
    AABB bounds = capsule.getBounds();
    if (bounds.max.x < worldBounds.min.x || bounds.min.x > worldBounds.max.x ||
        bounds.max.y < worldBounds.min.y || bounds.min.y > worldBounds.max.y ||
        bounds.max.z < worldBounds.min.z || bounds.min.z > worldBounds.max.z) {
        return false;
    }

    // Проверяем каждый треугольник
    for (const auto& tri : triangles) {
        // Проверка AABB треугольника
        if (bounds.max.x < tri.bounds.min.x || bounds.min.x > tri.bounds.max.x ||
            bounds.max.y < tri.bounds.min.y || bounds.min.y > tri.bounds.max.y ||
            bounds.max.z < tri.bounds.min.z || bounds.min.z > tri.bounds.max.z) {
            continue;
        }

        // Находим ближайшие точки между осью капсулы и треугольником
        // Упрощенный метод: проверяем расстояние от центра капсулы до треугольника
        glm::vec3 closestOnTri;
        float distToTri = pointTriangleDistance(capsule.getCenter(), tri, closestOnTri);

        // Если центр близко - проверяем точнее
        if (distToTri < capsule.radius + capsule.getHeight() * 0.5f) {
            // Точная проверка: расстояние от отрезка капсулы до треугольника
            glm::vec3 closestOnSeg, closestOnTri2;

            // Проверяем все 3 ребра треугольника против оси капсулы
            float d1 = segmentSegmentDistance(capsule.a, capsule.b, tri.v0, tri.v1, closestOnSeg, closestOnTri2);
            if (d1 < capsule.radius) return true;

            float d2 = segmentSegmentDistance(capsule.a, capsule.b, tri.v1, tri.v2, closestOnSeg, closestOnTri2);
            if (d2 < capsule.radius) return true;

            float d3 = segmentSegmentDistance(capsule.a, capsule.b, tri.v2, tri.v0, closestOnSeg, closestOnTri2);
            if (d3 < capsule.radius) return true;

            // Проверяем расстояние от вершин капсулы до треугольника
            float da = pointTriangleDistance(capsule.a, tri, closestOnTri2);
            if (da < capsule.radius) return true;

            float db = pointTriangleDistance(capsule.b, tri, closestOnTri2);
            if (db < capsule.radius) return true;
        }
    }
    return false;
}

SweepResult MeshCollider::sweepCapsule(const Capsule& playerCapsule, const glm::vec3& start,
    const glm::vec3& end) const {
    SweepResult result;
    result.t = 1.0f;

    glm::vec3 movement = end - start;
    float moveLen = glm::length(movement);
    if (moveLen < 0.0001f) {
        Capsule testCapsule = playerCapsule;
        testCapsule.a += start;
        testCapsule.b += start;
        result.hit = intersectCapsule(testCapsule);
        return result;
    }

    glm::vec3 dir = movement / moveLen;
    float bestT = 1.0f;
    glm::vec3 bestNormal(0.0f, 1.0f, 0.0f);
    bool foundHit = false;

    // Проверяем несколько точек вдоль пути (simplified sweep)
    int steps = std::max(3, (int)(moveLen / playerCapsule.radius));
    for (int i = 0; i <= steps; ++i) {
        float t = i / (float)steps;
        glm::vec3 pos = start + movement * t;

        Capsule testCapsule = playerCapsule;
        testCapsule.a += pos;
        testCapsule.b += pos;

        if (intersectCapsule(testCapsule)) {
            // Уточняем бинарным поиском
            float left = (i > 0) ? (i - 1) / (float)steps : 0.0f;
            float right = t;
            for (int iter = 0; iter < 4; ++iter) {
                float mid = (left + right) * 0.5f;
                glm::vec3 midPos = start + movement * mid;
                Capsule midCapsule = playerCapsule;
                midCapsule.a += midPos;
                midCapsule.b += midPos;

                if (intersectCapsule(midCapsule)) {
                    right = mid;
                }
                else {
                    left = mid;
                }
            }

            bestT = left;
            foundHit = true;
            break;
        }
    }

    if (foundHit) {
        result.hit = true;
        result.t = bestT;
        result.point = start + movement * bestT;
        result.distance = bestT * moveLen;
        // Нормаль примерная - в сторону движения назад
        result.normal = -dir;
    }

    return result;
}

// Möller–Trumbore ray-triangle intersection
bool MeshCollider::rayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir,
    const Triangle& tri, float& t, float& u, float& v) const {
    const float EPSILON = 0.000001f;
    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    glm::vec3 h = glm::cross(dir, edge2);
    float a = glm::dot(edge1, h);

    if (a > -EPSILON && a < EPSILON) return false;  // Луч параллелен треугольнику

    float f = 1.0f / a;
    glm::vec3 s = orig - tri.v0;
    u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(s, edge1);
    v = f * glm::dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    t = f * glm::dot(edge2, q);
    return t > EPSILON;
}

// AABB vs Triangle (Separating Axis Theorem упрощённый)
bool MeshCollider::aabbTriangleIntersect(const AABB& box, const Triangle& tri) const {
    // Быстрая проверка по AABB
    if (box.max.x < tri.bounds.min.x || box.min.x > tri.bounds.max.x ||
        box.max.y < tri.bounds.min.y || box.min.y > tri.bounds.max.y ||
        box.max.z < tri.bounds.min.z || box.min.z > tri.bounds.max.z) {
        return false;
    }

    // Упрощённая проверка: если AABB пересекает bounds треугольника, 
    // считаем что есть коллизия (для игровых целей достаточно)
    // Точная проверка SAT сложнее, но можно добавить если будут проблемы
    return true;
}

bool MeshCollider::intersectAABB(const AABB& box) const {
    if (box.max.x < worldBounds.min.x || box.min.x > worldBounds.max.x ||
        box.max.y < worldBounds.min.y || box.min.y > worldBounds.max.y ||
        box.max.z < worldBounds.min.z || box.min.z > worldBounds.max.z) {
        return false;
    }

    for (const auto& tri : triangles) {
        if (aabbTriangleIntersect(box, tri)) {
            glm::vec3 boxCenter = (box.min + box.max) * 0.5f;
            glm::vec3 closest;
            float dist = pointTriangleDistance(boxCenter, tri, closest);
            glm::vec3 boxSize = box.max - box.min;
            float boxRadius = glm::length(boxSize) * 0.5f;
            if (dist < boxRadius) return true;
        }
    }
    return false;
}

// Sweep AABB: движение из start в end
SweepResult MeshCollider::sweepAABB(const AABB& playerBox, const glm::vec3& start,
    const glm::vec3& end) const {
    SweepResult result;
    result.t = 1.0f;

    glm::vec3 movement = end - start;
    float moveLen = glm::length(movement);
    if (moveLen < 0.0001f) {
        AABB testBox = playerBox;
        testBox.min += start;
        testBox.max += start;
        result.hit = intersectAABB(testBox);
        return result;
    }

    glm::vec3 dir = movement / moveLen;
    float bestT = 1.0f;
    glm::vec3 bestNormal(0.0f);

    // Проверяем все треугольники
    for (const auto& tri : triangles) {
        // Быстрая проверка: пересекается ли расширенный AABB треугольника с лучом?
        glm::vec3 triCenter = (tri.bounds.min + tri.bounds.max) * 0.5f;
        float triRadius = glm::length(tri.bounds.max - tri.bounds.min) * 0.5f;

        // Расширяем bounds на размер игрока
        float playerRadius = glm::length(playerBox.max - playerBox.min) * 0.5f;

        if (glm::distance(start, triCenter) > moveLen + triRadius + playerRadius + 1.0f) {
            continue;
        }

        // Проверяем лучи от углов AABB игрока
        glm::vec3 extents = (playerBox.max - playerBox.min) * 0.5f;
        glm::vec3 corners[] = {
            glm::vec3(-extents.x, -extents.y, -extents.z),
            glm::vec3(extents.x, -extents.y, -extents.z),
            glm::vec3(-extents.x,  extents.y, -extents.z),
            glm::vec3(extents.x,  extents.y, -extents.z),
            glm::vec3(-extents.x, -extents.y,  extents.z),
            glm::vec3(extents.x, -extents.y,  extents.z),
            glm::vec3(-extents.x,  extents.y,  extents.z),
            glm::vec3(extents.x,  extents.y,  extents.z)
        };

        for (const auto& offset : corners) {
            glm::vec3 rayStart = start + offset;
            float t, u, v;
            if (rayTriangleIntersect(rayStart, dir, tri, t, u, v)) {
                if (t > 0.001f && t < moveLen && t / moveLen < bestT) {
                    bestT = t / moveLen;
                    bestNormal = tri.normal;
                    result.hit = true;
                }
            }
        }
    }

    if (result.hit) {
        result.t = bestT;
        result.point = start + dir * (bestT * moveLen);
        result.normal = bestNormal;
        result.distance = bestT * moveLen;
    }

    return result;
}

float MeshCollider::pointTriangleDistance(const glm::vec3& p, const Triangle& tri,
    glm::vec3& closest) const {
    glm::vec3 ab = tri.v1 - tri.v0;
    glm::vec3 ac = tri.v2 - tri.v0;
    glm::vec3 ap = p - tri.v0;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        closest = tri.v0;
        return glm::distance(p, closest);
    }

    glm::vec3 bp = p - tri.v1;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        closest = tri.v1;
        return glm::distance(p, closest);
    }

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        closest = tri.v0 + ab * v;
        return glm::distance(p, closest);
    }

    glm::vec3 cp = p - tri.v2;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        closest = tri.v2;
        return glm::distance(p, closest);
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        closest = tri.v0 + ac * w;
        return glm::distance(p, closest);
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        closest = tri.v1 + (tri.v2 - tri.v1) * w;
        return glm::distance(p, closest);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    closest = tri.v0 + ab * v + ac * w;
    return glm::distance(p, closest);
}

bool MeshCollider::findClosestPoint(const glm::vec3& point, float radius,
    glm::vec3& closest, glm::vec3& normal) const {
    bool found = false;
    float minDist = radius;

    for (const auto& tri : triangles) {
        // Broad phase
        if (glm::distance(point, (tri.bounds.min + tri.bounds.max) * 0.5f) >
            glm::length(tri.bounds.max - tri.bounds.min) * 0.5f + radius) {
            continue;
        }

        glm::vec3 triClosest;
        float dist = pointTriangleDistance(point, tri, triClosest);

        if (dist < minDist) {
            minDist = dist;
            closest = triClosest;
            normal = tri.normal;
            found = true;
        }
    }

    return found;
}