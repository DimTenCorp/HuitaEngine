#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "TriangleCollider.h"

class MeshCollider;

class Player {
private:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 size;

    bool onGround;
    float speed;
    float jumpForce;
    float gravity;
    float friction;

    float yaw;
    float pitch;

    bool noclipMode;
    float noclipSpeed;

    const MeshCollider* meshCollider = nullptr;
    bool jumpKeyWasHeld = false;

    float stepHeight = 0.45f; // Максимальная высота ступеньки/склона
    float stepUpSpeed = 1.0f;

public:
    Player();

    void update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider);

    void handleInput(float deltaTime);
    void moveWithCollision(float deltaTime);
    void moveNoclip(float deltaTime);

    bool checkCollision(const glm::vec3& pos, const AABB& box);
    bool checkCollisionMesh(const glm::vec3& pos) const;
    bool checkOnGround() const;
    bool tryStepUp(const glm::vec3& fromPos, const glm::vec3& moveDir, float maxStepHeight);

    void resolveCollisionAxis(float deltaTime, int axis);

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
};