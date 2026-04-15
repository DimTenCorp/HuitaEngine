#pragma once
#include <glm/glm.hpp>

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    // Добавить конструкторы:
    AABB() : min(0), max(0) {}
    AABB(const glm::vec3& min_, const glm::vec3& max_) : min(min_), max(max_) {}

    bool contains(const glm::vec3& p) const {
        return p.x >= min.x && p.x <= max.x &&
            p.y >= min.y && p.y <= max.y &&
            p.z >= min.z && p.z <= max.z;
    }

    bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
            (min.y <= other.max.y && max.y >= other.min.y) &&
            (min.z <= other.max.z && max.z >= other.min.z);
    }
};

struct Capsule {
    glm::vec3 start;
    glm::vec3 end;
    float radius;

    Capsule() : start(0), end(0), radius(0) {}
    Capsule(const glm::vec3& s, const glm::vec3& e, float r)
        : start(s), end(e), radius(r) {
    }

    glm::vec3 getCenter() const { return (start + end) * 0.5f; }
    float getHeight() const { return glm::length(end - start); }

    AABB getBounds() const {
        AABB bounds;
        bounds.min = glm::min(start, end) - glm::vec3(radius);
        bounds.max = glm::max(start, end) + glm::vec3(radius);
        return bounds;
    }
};