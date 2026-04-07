#pragma once
#include <glm/glm.hpp>

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    
    AABB() : min(0.0f), max(0.0f) {}
    AABB(const glm::vec3& mn, const glm::vec3& mx) : min(mn), max(mx) {}
};
