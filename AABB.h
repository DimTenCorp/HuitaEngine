#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    AABB() : min(0.0f), max(0.0f) {}
    AABB(const glm::vec3& mn, const glm::vec3& mx)
        : min(mn),
        max(glm::max(mx, mn)) {
    }

    void validate() {
        for (int i = 0; i < 3; ++i) {
            if (min[i] > max[i]) {
                std::swap(min[i], max[i]);
            }
        }
    }

    bool isValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
};

// === НОВОЕ: Капсульный коллайдер ===
struct Capsule {
    glm::vec3 a;        // Нижняя точка (центр нижней сферы)
    glm::vec3 b;        // Верхняя точка (центр верхней сферы)
    float radius;

    Capsule() : radius(0.0f) {}
    Capsule(const glm::vec3& start, const glm::vec3& end, float r)
        : a(start), b(end), radius(r) {
    }

    // Получить центр капсулы
    glm::vec3 getCenter() const { return (a + b) * 0.5f; }

    // Получить высоту (расстояние между сферами)
    float getHeight() const { return glm::length(b - a); }

    // Получить AABB для broad-phase
    AABB getBounds() const {
        AABB bounds;
        bounds.min = glm::min(a, b) - glm::vec3(radius);
        bounds.max = glm::max(a, b) + glm::vec3(radius);
        return bounds;
    }
};