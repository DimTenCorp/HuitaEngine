#pragma once
#include "Entity.h"
#include "AABB.h"
#include <unordered_map>
#include <string>
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>

struct DoorVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

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
    
    // Данные для рендеринга
    std::vector<DoorVertex> vertices;
    std::vector<unsigned int> indices;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint textureID = 0;
    bool hasGeometry = false;

public:
    CFuncDoor();
    virtual ~CFuncDoor();

    // Инициализация из параметров BSP сущности
    void initFromProperties(const std::unordered_map<std::string, std::string>& props, const AABB& modelBounds);
    
    // Инициализация геометрии двери из BSP
    void initGeometry(const std::vector<glm::vec3>& bspVertices, 
                      const std::vector<glm::vec3>& bspNormals,
                      const std::vector<glm::vec2>& bspTexCoords,
                      const std::vector<unsigned int>& bspIndices,
                      GLuint texId);
    
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
    
    // Есть ли геометрия
    bool hasGeom() const { return hasGeometry; }
};
