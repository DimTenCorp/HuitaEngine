#include "pch.h"
#include "WaterEntity.h"
#include <iostream>

CFuncWater::CFuncWater() 
    : bounds(AABB(glm::vec3(-64.0f, 0.0f, -64.0f), glm::vec3(64.0f, 128.0f, 64.0f))),
      topHeight(128.0f) {
    // По умолчанию вода имеет размер 128x128x128 единиц
    // и поверхность на высоте Y=128
}

void CFuncWater::initFromProperties(const std::unordered_map<std::string, std::string>& props) {
    // Читаем параметры из BSP сущности
    // В Half-Life func_water обычно задается через brush entity с ключевыми точками
    
    // mins/maxs определяют границы зоны воды
    glm::vec3 mins(-64.0f, 0.0f, -64.0f);
    glm::vec3 maxs(64.0f, 128.0f, 64.0f);
    
    auto it = props.find("mins");
    if (it != props.end()) {
        sscanf(it->second.c_str(), "%f %f %f", &mins.x, &mins.y, &mins.z);
    }
    
    it = props.find("maxs");
    if (it != props.end()) {
        sscanf(it->second.c_str(), "%f %f %f", &maxs.x, &maxs.y, &maxs.z);
    }
    
    // Если есть origin, смещаем границы относительно него
    glm::vec3 origin(0.0f, 0.0f, 0.0f);
    it = props.find("origin");
    if (it != props.end()) {
        sscanf(it->second.c_str(), "%f %f %f", &origin.x, &origin.y, &origin.z);
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
    
    // Верхняя граница воды - это максимальная Y координата
    topHeight = bounds.max.y;
    
    setPosition(origin);
    
    std::cout << "[func_water] Created water zone: mins(" 
              << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z 
              << ") maxs(" << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z 
              << ") surface at Y=" << topHeight << std::endl;
}

bool CFuncWater::isPointInWater(const glm::vec3& point) const {
    // Простая проверка точки внутри AABB
    return point.x >= bounds.min.x && point.x <= bounds.max.x &&
           point.y >= bounds.min.y && point.y <= bounds.max.y &&
           point.z >= bounds.min.z && point.z <= bounds.max.z;
}

bool CFuncWater::intersectsCapsule(const Capsule& capsule) const {
    // Проверка пересечения капсулы с AABB воды
    // Находим ближайшую точку AABB к центру капсулы
    
    AABB capsuleBounds = capsule.getBounds();
    
    // Проверяем пересечение AABB капсулы и AABB воды
    if (capsuleBounds.max.x < bounds.min.x || capsuleBounds.min.x > bounds.max.x) return false;
    if (capsuleBounds.max.y < bounds.min.y || capsuleBounds.min.y > bounds.max.y) return false;
    if (capsuleBounds.max.z < bounds.min.z || capsuleBounds.min.z > bounds.max.z) return false;
    
    // Более точная проверка - проверяем обе сферы капсулы
    auto pointInAABB = [this](const glm::vec3& p) -> bool {
        return p.x >= bounds.min.x && p.x <= bounds.max.x &&
               p.y >= bounds.min.y && p.y <= bounds.max.y &&
               p.z >= bounds.min.z && p.z <= bounds.max.z;
    };
    
    // Если хотя бы одна сфера капсулы в воде - считаем что игрок в воде
    if (pointInAABB(capsule.a)) return true;
    if (pointInAABB(capsule.b)) return true;
    
    // Проверяем пересечение линии между сферами с водой
    // Упрощенно: если капсула пересекает любую грань воды
    return true;
}

void CFuncWater::Update(float deltaTime) {
    // Вода не обновляется динамически, это статичная зона
    // Можно добавить анимацию поверхности или другие эффекты
}

void CFuncWater::Render() {
    // Вода не рендерится отдельно в этом движке
    // Это триггер-зона для определения нахождения игрока в воде
    // Визуализация воды должна быть частью BSP карты (текстуры воды)
}
