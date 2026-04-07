#pragma once
#include <glm/glm.hpp>
#include <algorithm>

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    
    AABB() : min(0.0f), max(0.0f) {}
    AABB(const glm::vec3& mn, const glm::vec3& mx) 
        : min(mn), 
          max(glm::max(mx, mn)) {} // Ensure min <= max invariant
    
    // Validate and fix the AABB invariant: min <= max
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
