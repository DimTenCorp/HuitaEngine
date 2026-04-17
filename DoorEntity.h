#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <sstream>  // Добавлено для парсинга movedir
#include "BSPLoader.h"
#include "AABB.h"

class BSPLoader;
class MeshCollider;

enum class DoorMoveType {
    Linear,
    Rotating
};

enum class DoorState {
    Closed,
    Opening,
    Open,
    Closing
};

struct DoorEntity {
    int modelIndex = -1;
    std::string targetname;
    std::string classname;
    std::unordered_map<std::string, std::string> entityProperties;

    glm::vec3 origin;
    glm::vec3 startOrigin;
    glm::vec3 endOrigin;
    glm::vec3 angles;
    glm::vec3 startAngles;
    glm::vec3 endAngles;

    glm::vec3 modelOrigin;
    glm::vec3 modelMins;
    glm::vec3 modelMaxs;

    DoorMoveType moveType = DoorMoveType::Linear;
    float speed = 100.0f;
    float waitTime = 3.0f;
    float lip = 8.0f;
    float damage = 0.0f;

    glm::vec3 moveDir;
    float moveDistance = 0.0f;

    DoorState state = DoorState::Closed;
    float stateTimer = 0.0f;
    float moveProgress = 0.0f;

    bool startOpen = false;
    bool passable = false;
    bool useOnly = false;
    bool noAutoReturn = false;
    bool oneWay = false;
    bool silent = false;

    bool rotationDirectionFixed = false;
    float rotationAngle = 90.0f;

    // НОВОЕ: Ось вращения (из movedir или Z по умолчанию)
    glm::vec3 rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);

    AABB bounds;
    std::vector<BSPVertex> vertices;
    std::vector<unsigned int> indices;
    GLuint textureID = 0;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    bool buffersDirty = true;

    int moveSound = 0;
    int stopSound = 0;
    int lockedSound = 0;
    int unlockedSound = 0;

    void initFromEntity(const BSPEntity& entity, const BSPLoader& bsp);
    void update(float deltaTime, MeshCollider* worldCollider);

    void activate(const glm::vec3& playerPos);
    void open(const glm::vec3& playerPos);
    void close();

    bool intersectsPlayer(const glm::vec3& playerPos, const Capsule& playerCapsule) const;
    AABB getCurrentBounds() const;

    glm::mat4 getTransform() const;

    void buildBuffers();
    void updateBuffers();
    void cleanupBuffers();

    bool isMoving() const { return state == DoorState::Opening || state == DoorState::Closing; }

private:
    void calculateEndPosition(const BSPLoader& bsp);
    void buildGeometry(const BSPLoader& bsp);
    void updateBounds();
    void determineRotationDirection(const glm::vec3& playerPos);
};