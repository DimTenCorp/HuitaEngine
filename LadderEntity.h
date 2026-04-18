#pragma once
#include "Entity.h"
#include "AABB.h"
#include <unordered_map>
#include <string>

// Сущность func_ladder - зона лестницы в карте
class CFuncLadder : public CBaseEntity {
private:
    AABB bounds;      // Границы зоны лестницы

public:
    CFuncLadder();
    virtual ~CFuncLadder() {}

    // Инициализация из AABB модели браша
    void initFromBounds(const AABB& modelBounds);

    // Инициализация из параметров BSP сущности
    void initFromProperties(const std::unordered_map<std::string, std::string>& props);

    // Проверка нахождения точки внутри лестницы
    bool isPointInLadder(const glm::vec3& point) const;

    // Проверка пересечения капсулы игрока с лестницей
    bool intersectsCapsule(const Capsule& capsule) const;

    // Получить AABB зоны
    const AABB& getBounds() const { return bounds; }

    // Обновление (переопределяется из базового класса)
    void Update(float deltaTime) override;

    // Рендеринг (лестница не рендерится отдельно, это триггер зона)
    void Render() override;

    glm::vec3 GetClimbNormal(const glm::vec3& playerPos) const {
        // Направление от игрока к центру лестницы (примерно)
        glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
        glm::vec3 dir = playerPos - center;
        dir.y = 0; // Только горизонтальная составляющая
        if (glm::length(dir) > 0.001f) {
            return glm::normalize(dir);
        }
        return glm::vec3(1.0f, 0.0f, 0.0f); // По умолчанию
    }
};