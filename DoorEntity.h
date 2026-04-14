#pragma once
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include "AABB.h"
#include "TriangleCollider.h"

class BSPLoader;
struct BSPEntity;
struct BSPModel;
class MeshCollider;

// Типы движения двери
enum class DoorMoveType {
    Linear,
    Rotating
};

// Состояния двери
enum class DoorState {
    Closed,
    Opening,
    Open,
    Closing
};

class DoorEntity {
public:
    // Свойства двери из BSP
    std::unordered_map<std::string, std::string> entityProperties;
    std::string classname;
    std::string targetname;

    // Параметры движения
    DoorMoveType moveType = DoorMoveType::Linear;
    DoorState state = DoorState::Closed;
    float moveProgress = 0.0f;
    float stateTimer = 0.0f;

    // Параметры из ключей BSP
    float speed = 100.0f;       // Speed - скорость движения
    float lip = 0.0f;           // Lip - сколько оставлять торчать
    float waitTime = 3.0f;      // Wait - время ожидания перед закрытием
    float damage = 0.0f;        // Dmg - урон при защемлении
    glm::vec3 angles{ 0.0f };   // Pitch Yaw Roll углы
    glm::vec3 movedir{ 0.0f };  // Направление движения

    // Spawnflags
    bool startOpen = false;
    bool passable = false;
    bool oneWay = false;
    bool noAutoReturn = false;
    bool useOnly = false;
    bool silent = false;

    // Геометрия
    int modelIndex = -1;
    glm::vec3 modelMins{ 0.0f };
    glm::vec3 modelMaxs{ 0.0f };
    glm::vec3 modelOrigin{ 0.0f };
    glm::vec3 startOrigin{ 0.0f };
    glm::vec3 endOrigin{ 0.0f };
    glm::vec3 origin{ 0.0f };
    glm::vec3 startAngles{ 0.0f };
    glm::vec3 endAngles{ 0.0f };
    glm::vec3 angles_current{ 0.0f };
    glm::vec3 moveDir{ 0.0f };
    float moveDistance = 0.0f;

    // Рендеринг
    std::vector<BSPVertex> vertices;
    std::vector<unsigned int> indices;
    GLuint textureID = 0;
    AABB bounds;

    // Инициализация из BSP энтити
    void initFromEntity(const BSPEntity& entity, const BSPLoader& bsp);

    // Обновление состояния двери
    void update(float deltaTime, MeshCollider* worldCollider);

    // Активация двери (открытие/закрытие)
    void activate();

    // Открыть дверь
    void open();

    // Закрыть дверь
    void close();

    // Проверка пересечения с игроком
    bool intersectsPlayer(const glm::vec3& playerPos, const Capsule& playerCapsule) const;

    // Получить текущие bounds
    AABB getCurrentBounds() const;

    // Получить трансформацию для рендеринга
    glm::mat4 getTransform() const;

private:
    // Расчет конечной позиции
    void calculateEndPosition(const BSPLoader& bsp);

    // Построение геометрии
    void buildGeometry(const BSPLoader& bsp);

    // Обновление bounds
    void updateBounds();
};
