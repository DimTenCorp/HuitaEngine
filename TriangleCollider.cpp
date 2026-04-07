#include "TriangleCollider.h"
#include <cmath>
#include <limits>
#include <unordered_set>
#include "BSPLoader.h"

void MeshCollider::buildSpatialGrid() {
    if (triangles.empty()) {
        gridBuilt = false;
        return;
    }
    
    // Determine grid size based on world bounds
    glm::vec3 extent = worldBounds.max - worldBounds.min;
    float maxExtent = std::max({extent.x, extent.y, extent.z});
    
    // Target ~10 cells along the largest axis for reasonable performance
    cellSize = maxExtent / 10.0f;
    if (cellSize < 1.0f) cellSize = 1.0f;
    
    gridSize.x = static_cast<int>(std::ceil(extent.x / cellSize)) + 1;
    gridSize.y = static_cast<int>(std::ceil(extent.y / cellSize)) + 1;
    gridSize.z = static_cast<int>(std::ceil(extent.z / cellSize)) + 1;
    
    // Limit grid dimensions to prevent excessive memory usage
    const int maxGridDim = 50;
    if (gridSize.x > maxGridDim) gridSize.x = maxGridDim;
    if (gridSize.y > maxGridDim) gridSize.y = maxGridDim;
    if (gridSize.z > maxGridDim) gridSize.z = maxGridDim;
    
    spatialGrid.clear();
    spatialGrid.resize(gridSize.x * gridSize.y * gridSize.z);
    
    // Initialize cell bounds
    for (int z = 0; z < gridSize.z; ++z) {
        for (int y = 0; y < gridSize.y; ++y) {
            for (int x = 0; x < gridSize.x; ++x) {
                GridCell& cell = spatialGrid[x + y * gridSize.x + z * gridSize.x * gridSize.y];
                cell.bounds.min = worldBounds.min + glm::vec3(x * cellSize, y * cellSize, z * cellSize);
                cell.bounds.max = cell.bounds.min + glm::vec3(cellSize, cellSize, cellSize);
            }
        }
    }
    
    // Assign triangles to cells
    for (size_t i = 0; i < triangles.size(); ++i) {
        const Triangle& tri = triangles[i];
        
        // Find all cells that this triangle's bounds overlaps
        glm::ivec3 minCell(
            static_cast<int>((tri.bounds.min.x - worldBounds.min.x) / cellSize),
            static_cast<int>((tri.bounds.min.y - worldBounds.min.y) / cellSize),
            static_cast<int>((tri.bounds.min.z - worldBounds.min.z) / cellSize)
        );
        glm::ivec3 maxCell(
            static_cast<int>((tri.bounds.max.x - worldBounds.min.x) / cellSize),
            static_cast<int>((tri.bounds.max.y - worldBounds.min.y) / cellSize),
            static_cast<int>((tri.bounds.max.z - worldBounds.min.z) / cellSize)
        );
        
        // Clamp to grid bounds
        minCell = glm::clamp(minCell, glm::ivec3(0), gridSize - 1);
        maxCell = glm::clamp(maxCell, glm::ivec3(0), gridSize - 1);
        
        for (int z = minCell.z; z <= maxCell.z; ++z) {
            for (int y = minCell.y; y <= maxCell.y; ++y) {
                for (int x = minCell.x; x <= maxCell.x; ++x) {
                    size_t cellIdx = x + y * gridSize.x + z * gridSize.x * gridSize.y;
                    spatialGrid[cellIdx].triangleIndices.push_back(i);
                }
            }
        }
    }
    
    gridBuilt = true;
}

std::vector<size_t> MeshCollider::getCandidateTriangles(const AABB& box) const {
    std::unordered_set<size_t> candidates;
    
    if (!gridBuilt || triangles.empty()) {
        // Fallback: return all triangles
        candidates.reserve(triangles.size());
        for (size_t i = 0; i < triangles.size(); ++i) {
            candidates.insert(i);
        }
        return std::vector<size_t>(candidates.begin(), candidates.end());
    }
    
    // Find overlapping cells
    glm::ivec3 minCell(
        static_cast<int>((box.min.x - worldBounds.min.x) / cellSize),
        static_cast<int>((box.min.y - worldBounds.min.y) / cellSize),
        static_cast<int>((box.min.z - worldBounds.min.z) / cellSize)
    );
    glm::ivec3 maxCell(
        static_cast<int>((box.max.x - worldBounds.min.x) / cellSize),
        static_cast<int>((box.max.y - worldBounds.min.y) / cellSize),
        static_cast<int>((box.max.z - worldBounds.min.z) / cellSize)
    );
    
    // Clamp to grid bounds
    minCell = glm::clamp(minCell, glm::ivec3(0), gridSize - 1);
    maxCell = glm::clamp(maxCell, glm::ivec3(0), gridSize - 1);
    
    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                size_t cellIdx = x + y * gridSize.x + z * gridSize.x * gridSize.y;
                for (size_t triIdx : spatialGrid[cellIdx].triangleIndices) {
                    candidates.insert(triIdx);
                }
            }
        }
    }
    
    return std::vector<size_t>(candidates.begin(), candidates.end());
}

void MeshCollider::buildFromBSP(const std::vector<BSPVertex>& vertices,
    const std::vector<unsigned int>& indices) {
    triangles.clear();
    spatialGrid.clear();
    gridBuilt = false;

    if (indices.empty()) return;

    // Строим треугольники из индексов
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

        // Отбрасываем слишком маленькие треугольники (degenerate)
        if (glm::length(tri.normal) < 0.001f) continue;

        triangles.push_back(tri);
    }

    // Считаем общие bounds
    if (!triangles.empty()) {
        worldBounds = triangles[0].bounds;
        for (const auto& tri : triangles) {
            worldBounds.min = glm::min(worldBounds.min, tri.bounds.min);
            worldBounds.max = glm::max(worldBounds.max, tri.bounds.max);
        }
        
        // Build spatial partitioning grid
        buildSpatialGrid();
    }
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
    // Broad phase: проверяем world bounds
    if (box.max.x < worldBounds.min.x || box.min.x > worldBounds.max.x ||
        box.max.y < worldBounds.min.y || box.min.y > worldBounds.max.y ||
        box.max.z < worldBounds.min.z || box.min.z > worldBounds.max.z) {
        return false;
    }

    // Get candidate triangles using spatial partitioning
    std::vector<size_t> candidates = getCandidateTriangles(box);

    // Narrow phase: проверяем только кандидаты
    for (size_t triIdx : candidates) {
        const Triangle& tri = triangles[triIdx];
        if (aabbTriangleIntersect(box, tri)) {
            // Дополнительная проверка: расстояние от центра AABB до треугольника
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

    // Get candidate triangles using spatial partitioning
    AABB sweepBox;
    sweepBox.min = glm::min(start, end) - glm::vec3(playerBox.max - playerBox.min);
    sweepBox.max = glm::max(start, end) + glm::vec3(playerBox.max - playerBox.min);
    
    std::vector<size_t> candidates = getCandidateTriangles(sweepBox);

    // Проверяем только кандидаты
    for (size_t triIdx : candidates) {
        const Triangle& tri = triangles[triIdx];
        
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

    // Get candidate triangles using spatial partitioning
    AABB searchBox;
    searchBox.min = point - glm::vec3(radius);
    searchBox.max = point + glm::vec3(radius);
    
    std::vector<size_t> candidates = getCandidateTriangles(searchBox);

    for (size_t triIdx : candidates) {
        const Triangle& tri = triangles[triIdx];
        
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