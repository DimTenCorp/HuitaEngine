#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "TriangleCollider.h"

// Quake-style movement constants
constexpr float MOVE_EPSILON = 0.01f;
constexpr float STOP_EPSILON = 0.1f;
constexpr int MAX_CLIP_PLANES = 5;
constexpr float STEPSIZE = 0.45f; // 18 units in Quake (~0.45m)
constexpr int NUM_MOVE_BUMPS = 4;

class MeshCollider;

class Player {
private:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 wishDir;        // направление желания движения
    glm::vec3 size;

    bool onGround;
    float speed;
    float maxSpeed;
    float jumpForce;
    float gravity;
    float friction;
    float stopSpeed;
    float accelRate;       // скорость ускорения (sv_accelerate)
    float airAccelRate;    // ускорение в воздухе

    float yaw;
    float pitch;

    bool noclipMode;
    float noclipSpeed;

    const MeshCollider* meshCollider = nullptr;
    bool jumpKeyWasHeld = false;

    float stepHeight;
    
    // Для обработки движения
    glm::vec3 baseVelocity;   // для лифтов и движущихся платформ
    int groundEntity = -1;

    // Вспомогательные функции для Quake-style движения
    void clipVelocity(const glm::vec3& in, const glm::vec3& normal, glm::vec3& out, float overbounce);
    int flyMove(float deltaTime);
    void walkMove(float deltaTime);
    void applyAcceleration(float wishspeed, const glm::vec3& wishdir);
    void applyAirAcceleration(float wishspeed, const glm::vec3& wishveloc);
    void userFriction();

public:
    Player();

    void update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider);

    void handleInput(float deltaTime);
    void moveWithCollision(float deltaTime);
    void moveNoclip(float deltaTime);

    bool checkCollision(const glm::vec3& pos, const AABB& box);
    bool checkCollisionMesh(const glm::vec3& pos) const;
    bool checkOnGround() const;
    
    void toggleNoclip() { noclipMode = !noclipMode; }
    bool isNoclip() const { return noclipMode; }

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getMin(const glm::vec3& pos) const { return pos - size; }
    glm::vec3 getMax(const glm::vec3& pos) const { return pos + size; }
    glm::vec3 getMin() const { return position - size; }
    glm::vec3 getMax() const { return position + size; }

    AABB getPlayerAABB(const glm::vec3& pos) const;

    void setPosition(const glm::vec3& pos) { position = pos; }
    bool isOnGround() const { return onGround; }

    void getViewMatrix(float* matrix) const;
    glm::vec3 getEyePosition() const;

    void setYaw(float y) { yaw = y; }
    void setPitch(float p) { pitch = p; }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }
    
    // Геттеры для физических параметров
    float getSpeed() const { return speed; }
    float getMaxSpeed() const { return maxSpeed; }
    float getGravity() const { return gravity; }
};