#pragma once
#include <glm/glm.hpp>
#include "AABB.h"

class Frustum {
public:
    enum PlaneIndex {
        NEAR = 0,
        FAR = 1,
        LEFT = 2,
        RIGHT = 3,
        TOP = 4,
        BOTTOM = 5,
        PLANE_COUNT = 6
    };

    struct Plane {
        glm::vec3 normal;
        float d;

        Plane() : normal(0.0f), d(0.0f) {}
        Plane(const glm::vec3& n, float dist) : normal(n), d(dist) {}

        float distanceToPoint(const glm::vec3& p) const {
            return glm::dot(normal, p) + d;
        }
    };

    Frustum() = default;

    // Обновить плоскости фрустума из матрицы view-projection
    void update(const glm::mat4& vpMatrix) {
        // Извлекаем строки комбинированной матрицы
        glm::vec4 col0 = vpMatrix[0];
        glm::vec4 col1 = vpMatrix[1];
        glm::vec4 col2 = vpMatrix[2];
        glm::vec4 col3 = vpMatrix[3];

        // Левая плоскость: row3 + row0
        planes[LEFT] = Plane(
            glm::vec3(col3.w + col0.w, col3.x + col0.x, col3.y + col0.y),
            col3.z + col0.z
        );

        // Правая плоскость: row3 - row0
        planes[RIGHT] = Plane(
            glm::vec3(col3.w - col0.w, col3.x - col0.x, col3.y - col0.y),
            col3.z - col0.z
        );

        // Нижняя плоскость: row3 + row1
        planes[BOTTOM] = Plane(
            glm::vec3(col3.w + col1.w, col3.x + col1.x, col3.y + col1.y),
            col3.z + col1.z
        );

        // Верхняя плоскость: row3 - row1
        planes[TOP] = Plane(
            glm::vec3(col3.w - col1.w, col3.x - col1.x, col3.y - col1.y),
            col3.z - col1.z
        );

        // Ближняя плоскость: row3 + row2 (для OpenGL)
        planes[NEAR] = Plane(
            glm::vec3(col3.w + col2.w, col3.x + col2.x, col3.y + col2.y),
            col3.z + col2.z
        );

        // Дальняя плоскость: row3 - row2 (для OpenGL)
        planes[FAR] = Plane(
            glm::vec3(col3.w - col2.w, col3.x - col2.x, col3.y - col2.y),
            col3.z - col2.z
        );

        // Нормализуем все плоскости
        for (int i = 0; i < PLANE_COUNT; ++i) {
            float len = glm::length(planes[i].normal);
            if (len > 0.0001f) {
                planes[i].normal /= len;
                planes[i].d /= len;
            }
        }
    }

    // Проверка видимости AABB
    // Возвращает true если бокс хотя бы частично виден
    bool testAABB(const AABB& box) const {
        for (int i = 0; i < PLANE_COUNT; ++i) {
            // Находим точку бокса наиболее удаленную от плоскости в направлении нормали
            glm::vec3 testPoint;
            
            // Для каждой оси выбираем мин или макс в зависимости от направления нормали
            testPoint.x = planes[i].normal.x >= 0 ? box.max.x : box.min.x;
            testPoint.y = planes[i].normal.y >= 0 ? box.max.y : box.min.y;
            testPoint.z = planes[i].normal.z >= 0 ? box.max.z : box.min.z;

            // Если тестовая точка за плоскостью - бокс полностью невидим
            if (planes[i].distanceToPoint(testPoint) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    // Проверка точки
    bool testPoint(const glm::vec3& p) const {
        for (int i = 0; i < PLANE_COUNT; ++i) {
            if (planes[i].distanceToPoint(p) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    const Plane& getPlane(int index) const { return planes[index]; }

private:
    Plane planes[PLANE_COUNT];
};
