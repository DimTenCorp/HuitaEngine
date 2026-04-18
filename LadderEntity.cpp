#include "pch.h"
#include "LadderEntity.h"
#include <iostream>

CFuncLadder::CFuncLadder()
    : bounds(AABB(glm::vec3(-16.0f, 0.0f, -16.0f), glm::vec3(16.0f, 128.0f, 16.0f))) {
    // По умолчанию лестница имеет размер 32x128x32 единиц
}

void CFuncLadder::initFromBounds(const AABB& modelBounds) {
    // Инициализируем зону лестницы напрямую из границ модели браша
    bounds = modelBounds;

    // Вычисляем центр для позиции сущности
    glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
    setPosition(center);

    std::cout << "[func_ladder] Created ladder zone from brush bounds: mins("
        << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z
        << ") maxs(" << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z
        << ")" << std::endl;
}

void CFuncLadder::initFromProperties(const std::unordered_map<std::string, std::string>& props) {
    // Читаем параметры из BSP сущности
    glm::vec3 mins(-16.0f, 0.0f, -16.0f);
    glm::vec3 maxs(16.0f, 128.0f, 16.0f);

    auto it = props.find("mins");
    if (it != props.end()) {
        sscanf_s(it->second.c_str(), "%f %f %f", &mins.x, &mins.y, &mins.z);
    }

    it = props.find("maxs");
    if (it != props.end()) {
        sscanf_s(it->second.c_str(), "%f %f %f", &maxs.x, &maxs.y, &maxs.z);
    }

    // Если есть origin, смещаем границы относительно него
    glm::vec3 origin(0.0f, 0.0f, 0.0f);
    it = props.find("origin");
    if (it != props.end()) {
        sscanf_s(it->second.c_str(), "%f %f %f", &origin.x, &origin.y, &origin.z);
        // Конвертация координат как в BSPLoader
        origin = glm::vec3(-origin.x, origin.z, origin.y);
    }

    // Устанавливаем границы с учетом конвертации осей (X -> -X, Y -> Z, Z -> Y)
    bounds.min = glm::vec3(-mins.x, mins.z, mins.y) + origin;
    bounds.max = glm::vec3(-maxs.x, maxs.z, maxs.y) + origin;

    // Проверяем валидность AABB
    if (bounds.min.x > bounds.max.x) std::swap(bounds.min.x, bounds.max.x);
    if (bounds.min.y > bounds.max.y) std::swap(bounds.min.y, bounds.max.y);
    if (bounds.min.z > bounds.max.z) std::swap(bounds.min.z, bounds.max.z);

    setPosition(origin);

    std::cout << "[func_ladder] Created ladder zone from properties: mins("
        << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z
        << ") maxs(" << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z
        << ")" << std::endl;
}

bool CFuncLadder::isPointInLadder(const glm::vec3& point) const {
    // Простая проверка точки внутри AABB
    return point.x >= bounds.min.x && point.x <= bounds.max.x &&
        point.y >= bounds.min.y && point.y <= bounds.max.y &&
        point.z >= bounds.min.z && point.z <= bounds.max.z;
}

bool CFuncLadder::intersectsCapsule(const Capsule& capsule) const {
    // Проверка пересечения капсулы с AABB лестницы

    AABB capsuleBounds = capsule.getBounds();

    // Проверяем пересечение AABB капсулы и AABB лестницы
    if (capsuleBounds.max.x < bounds.min.x || capsuleBounds.min.x > bounds.max.x) return false;
    if (capsuleBounds.max.y < bounds.min.y || capsuleBounds.min.y > bounds.max.y) return false;
    if (capsuleBounds.max.z < bounds.min.z || capsuleBounds.min.z > bounds.max.z) return false;

    // Более точная проверка - проверяем обе сферы капсулы
    auto pointInAABB = [this](const glm::vec3& p) -> bool {
        return p.x >= bounds.min.x && p.x <= bounds.max.x &&
            p.y >= bounds.min.y && p.y <= bounds.max.y &&
            p.z >= bounds.min.z && p.z <= bounds.max.z;
        };

    // Если хотя бы одна сфера капсулы в лестнице - считаем что игрок на лестнице
    if (pointInAABB(capsule.start)) return true;
    if (pointInAABB(capsule.end)) return true;

    return true;
}

void CFuncLadder::Update(float deltaTime) {
    // Лестница не обновляется динамически, это статичная зона
}

void CFuncLadder::Render() {
    // Лестница не рендерится отдельно в этом движке
    // Это триггер-зона для определения нахождения игрока на лестнице
}