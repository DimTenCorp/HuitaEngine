#pragma once
#include "Entity.h"
#include "AABB.h"
#include <unordered_map>
#include <string>

// Сущность func_door - дверь как в Half-Life
class CFuncDoor : public CBaseEntity {
private:
    AABB bounds;              // Границы двери
    AABB originalBounds;      // Оригинальные границы для сброса
    glm::vec3 moveDirection;  // Направление движения двери
    float moveDistance;       // Расстояние открытия двери
    float speed;              // Скорость открытия (units/sec)
    float lip;                // Lip - расстояние которое дверь остается закрытой
    float openWaitTime;       // Время ожидания перед закрытием
    float currentPos;         // Текущая позиция вдоль направления движения (0 = закрыто, 1 = открыто)
    
    enum DoorState {
        DOOR_CLOSED,
        DOOR_OPENING,
        DOOR_OPEN,
        DOOR_CLOSING
    };
    
    DoorState state;
    float stateTimer;         // Таймер для состояния
    bool touchTriggered;      // Флаг触发ления от игрока

public:
    CFuncDoor();
    virtual ~CFuncDoor() {}

    // Инициализация из параметров BSP сущности
    void initFromProperties(const std::unordered_map<std::string, std::string>& props, const AABB& modelBounds);
    
    // Обновление состояния двери
    void Update(float deltaTime) override;
    
    // Рендеринг двери
    void Render() override;
    
    // Проверка пересечения с капсулой игрока
    bool intersectsCapsule(const Capsule& capsule) const;
    
    // Получить текущие границы двери (с учетом открытия)
    AABB getCurrentBounds() const;
    
    // Триггер открытия двери
    void triggerOpen();
    
    // Получить состояние двери
    DoorState getState() const { return state; }
    
    // Получить скорость
    float getSpeed() const { return speed; }
    
    // Получить направление движения
    glm::vec3 getMoveDirection() const { return moveDirection; }
};
