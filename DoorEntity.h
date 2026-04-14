#pragma once
#include "pch.h"
#include "AABB.h"
#include <string>
#include <vector>
#include <unordered_map>

// Forward declarations
struct BSPEntity;
struct BSPVertex;
class BSPLoader;
class MeshCollider;
struct Capsule;

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

// Класс двери
class DoorEntity {
public:
    DoorEntity() = default;
    ~DoorEntity();

    // Инициализация из BSP энтити
    void initFromEntity(const BSPEntity& entity, const BSPLoader& bsp);

    // Обновление состояния двери
    void update(float deltaTime, MeshCollider* worldCollider);

    // Активация двери (открытие/закрытие)
    void activate();

    // Открытие двери
    void open();

    // Закрытие двери
    void close();

    // Проверка пересечения с игроком
    bool intersectsPlayer(const glm::vec3& playerPos, const Capsule& playerCapsule) const;

    // Получить текущие bounds
    AABB getCurrentBounds() const;

    // Получить матрицу трансформации
    glm::mat4 getTransform() const;

    // Геттеры
    inline const std::string& getClassname() const { return classname; }
    inline const std::string& getTargetname() const { return targetname; }
    inline DoorState getState() const { return state; }
    inline DoorMoveType getMoveType() const { return moveType; }
    inline const glm::vec3& getOrigin() const { return origin; }
    inline const glm::vec3& getAngles() const { return angles; }
    inline float getSpeed() const { return speed; }
    inline float getLip() const { return lip; }
    inline float getWaitTime() const { return waitTime; }
    inline int getModelIndex() const { return modelIndex; }

    // Рендеринг двери
    void render(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection);

private:
    // Расчет конечной позиции
    void calculateEndPosition(const BSPLoader& bsp);

    // Построение геометрии
    void buildGeometry(const BSPLoader& bsp);

    // Обновление bounds
    void updateBounds();

    // Очистка OpenGL ресурсов
    void cleanupGeometry();

    // Свойства энтити
    std::string classname;
    std::string targetname;
    std::unordered_map<std::string, std::string> entityProperties;

    // Тип и состояние
    DoorMoveType moveType = DoorMoveType::Linear;
    DoorState state = DoorState::Closed;

    // Позиция и углы
    glm::vec3 origin{ 0.0f };
    glm::vec3 angles{ 0.0f };
    glm::vec3 startOrigin{ 0.0f };
    glm::vec3 endOrigin{ 0.0f };
    glm::vec3 startAngles{ 0.0f };
    glm::vec3 endAngles{ 0.0f };

    // Модель
    int modelIndex = 0;
    glm::vec3 modelMins{ 0.0f };
    glm::vec3 modelMaxs{ 0.0f };
    glm::vec3 modelOrigin{ 0.0f };

    // Параметры движения
    float speed = 100.0f;
    float lip = 0.0f;
    float waitTime = 2.0f;
    float damage = 0.0f;
    float moveDistance = 0.0f;
    float moveProgress = 0.0f;
    float stateTimer = 0.0f;

    // Направление движения (для линейных дверей)
    glm::vec3 moveDir{ 0.0f, 0.0f, 1.0f };

    // Флаги
    bool startOpen = false;
    bool passable = false;
    bool oneWay = false;
    bool noAutoReturn = false;
    bool useOnly = false;
    bool silent = false;

    // Звуки
    int moveSound = 0;
    int stopSound = 0;

    // Геометрия
    std::vector<BSPVertex> vertices;
    std::vector<unsigned int> indices;
    GLuint textureID = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    bool geometryBuilt = false;

    // Bounds
    AABB bounds;
};
