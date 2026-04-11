#pragma once
#include "Entity.h"
#include "AABB.h"
#include <unordered_map>
#include <string>

// Сущность func_water - зона воды в карте
class CFuncWater : public CBaseEntity {
private:
    AABB bounds;      // Границы водной зоны
    float topHeight;  // Верхняя граница воды (для определения поверхности)

public:
    CFuncWater();
    virtual ~CFuncWater() {}

    // Инициализация из AABB модели браша
    void initFromBounds(const AABB& modelBounds);
    
    // Инициализация из параметров BSP сущности (устаревший метод)
    void initFromProperties(const std::unordered_map<std::string, std::string>& props);

    // Проверка нахождения точки внутри воды
    bool isPointInWater(const glm::vec3& point) const;

    // Проверка пересечения капсулы игрока с водой
    bool intersectsCapsule(const Capsule& capsule) const;

    // Получить верхнюю границу воды (поверхность)
    float getTopHeight() const { return topHeight; }

    // Получить AABB зоны
    const AABB& getBounds() const { return bounds; }

    // Обновление (переопределяется из базового класса)
    void Update(float deltaTime) override;

    // Рендеринг (вода не рендерится отдельно, это триггер зона)
    void Render() override;
};
