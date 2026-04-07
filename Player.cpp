#include "Player.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include "TriangleCollider.h"

Player::Player() {
    position = glm::vec3(0.0f, 2.0f, 0.0f);
    velocity = glm::vec3(0.0f);
    size = glm::vec3(0.3f, 0.9f, 0.3f);

    onGround = false;
    speed = 6.0f;
    jumpForce = 8.0f;
    gravity = 25.0f;
    friction = 0.5f;

    noclipMode = false;
    noclipSpeed = 12.0f;

    yaw = -90.0f;
    pitch = 0.0f;
}

void Player::handleInput(float deltaTime) {
    if (noclipMode) return;

    glm::vec3 wishDir(0.0f);

    float yawRad = glm::radians(yaw);
    glm::vec3 forward(cos(yawRad), 0.0f, sin(yawRad));
    glm::vec3 right(sin(yawRad), 0.0f, -cos(yawRad));

    GLFWwindow* window = glfwGetCurrentContext();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir += right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir -= right;

    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir);
        velocity.x = wishDir.x * speed;
        velocity.z = wishDir.z * speed;
    }
    else {
        velocity.x = 0.0f;
        velocity.z = 0.0f;
    }

    // Прыжок
    GLFWwindow* win = glfwGetCurrentContext();
    bool spacePressed = (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS);

    if (spacePressed) {
        if (!jumpKeyWasHeld && onGround && velocity.y <= 0.01f) {
            velocity.y = jumpForce;
            onGround = false;
            position.y += 0.01f;
        }
        jumpKeyWasHeld = true;
    }
    else {
        jumpKeyWasHeld = false;
    }
}

void Player::moveNoclip(float deltaTime) {
    glm::vec3 wishDir(0.0f);

    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);

    glm::vec3 forward(cos(yawRad) * cos(pitchRad), sin(pitchRad), sin(yawRad) * cos(pitchRad));
    glm::vec3 right(sin(yawRad), 0.0f, -cos(yawRad));
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    GLFWwindow* window = glfwGetCurrentContext();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir += right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir -= right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) wishDir += up;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) wishDir -= up;

    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir);
        position += wishDir * noclipSpeed * deltaTime;
    }

    velocity = glm::vec3(0.0f);
}

AABB Player::getPlayerAABB(const glm::vec3& pos) const {
    AABB box;
    box.min = pos - size;
    box.max = pos + size;
    return box;
}

bool Player::checkCollision(const glm::vec3& pos, const AABB& box) {
    glm::vec3 pMin = getMin(pos);
    glm::vec3 pMax = getMax(pos);

    return (pMin.x < box.max.x && pMax.x > box.min.x &&
        pMin.y < box.max.y && pMax.y > box.min.y &&
        pMin.z < box.max.z && pMax.z > box.min.z);
}

bool Player::checkCollisionMesh(const glm::vec3& pos) const {
    if (!meshCollider) return false;
    AABB box = getPlayerAABB(pos);
    return meshCollider->intersectAABB(box);
}

bool Player::checkOnGround() const {
    if (!meshCollider) return false;
    glm::vec3 testPos = position;
    testPos.y -= 0.01f;
    return checkCollisionMesh(testPos);
}

// Пробуем подняться на ступеньку/склон
bool Player::tryStepUp(const glm::vec3& fromPos, const glm::vec3& moveDir, float maxStepHeight) {
    if (!meshCollider) return false;

    // 1. Проверяем, есть ли вообще препятствие перед нами
    glm::vec3 horizontalTestPos = fromPos;
    horizontalTestPos.x += moveDir.x;
    horizontalTestPos.z += moveDir.z;

    if (!checkCollisionMesh(horizontalTestPos)) {
        return false;
    }

    // 2. Ищем максимально высокую точку (пол) в направлении движения
    // Мы сканируем сверху вниз от (текущая высота + maxStepHeight)
    glm::vec3 scanPos = fromPos;
    scanPos.x += moveDir.x;
    scanPos.z += moveDir.z;
    scanPos.y += maxStepHeight;

    float foundHeight = -9999.0f;
    bool foundFloor = false;

    // Спускаемся вниз, чтобы найти поверхность ступеньки
    for (float h = maxStepHeight; h >= -0.01f; h -= 0.01f) {
        glm::vec3 checkFloorPos = fromPos + glm::vec3(moveDir.x, h, moveDir.z);
        if (checkCollisionMesh(checkFloorPos)) {
            foundHeight = fromPos.y + h + 0.005f;
            foundFloor = true;
            break;
        }
    }

    if (!foundFloor) return false;

    float heightDiff = foundHeight - fromPos.y;

    // Проверяем, не слишком ли высокая ступенька
    if (heightDiff > maxStepHeight || heightDiff <= 0.001f) {
        return false;
    }

    // 3. КРИТИЧЕСКАЯ ПРОВЕРКА: Пролезет ли игрок по росту?
    // Проверяем коллизию в целевой позиции с учетом новой высоты foundHeight
    glm::vec3 finalPosCandidate = fromPos;
    finalPosCandidate.x += moveDir.x;
    finalPosCandidate.z += moveDir.z;
    finalPosCandidate.y = foundHeight;

    if (checkCollisionMesh(finalPosCandidate)) {
        // Если здесь коллизия, значит места для роста 0.9 (size.y * 2) не хватает.
        // Потолок слишком низко над этой ступенькой.
        return false;
    }

    // 4. Если всё чисто, выполняем подъем
    float maxStepThisFrame = stepUpSpeed * 0.016f;
    float actualRise = std::min(heightDiff, maxStepThisFrame);

    position.y += actualRise;
    position.x = fromPos.x + moveDir.x;
    position.z = fromPos.z + moveDir.z;

    if (actualRise >= heightDiff - 0.001f) {
        position.y = foundHeight;
        onGround = true;
        velocity.y = 0;
    }
    else {
        onGround = false;
        velocity.y = stepUpSpeed * 0.5f;
    }

    return true;
}

void Player::resolveCollisionAxis(float deltaTime, int axis) {
    glm::vec3 newPos = position;

    if (axis == 0) newPos.x += velocity.x * deltaTime;
    else if (axis == 1) newPos.y += velocity.y * deltaTime;
    else newPos.z += velocity.z * deltaTime;

    if (checkCollisionMesh(newPos)) {
        if (axis == 0) {
            // X коллизия - пробуем подняться на ступеньку
            glm::vec3 moveDir(velocity.x * deltaTime, 0.0f, 0.0f);
            if (!tryStepUp(position, moveDir, stepHeight)) {
                velocity.x = 0;
            }
            else {
                // Если поднялись - очищаем горизонтальную скорость чтобы не застрять
                velocity.x *= 0.5f;
            }
        }
        else if (axis == 1) {
            if (velocity.y > 0) {
                velocity.y = 0;
            }
            else if (velocity.y < 0) {
                onGround = true;
                velocity.y = 0;
            }
        }
        else {
            // Z коллизия - пробуем подняться на ступеньку
            glm::vec3 moveDir(0.0f, 0.0f, velocity.z * deltaTime);
            if (!tryStepUp(position, moveDir, stepHeight)) {
                velocity.z = 0;
            }
            else {
                velocity.z *= 0.5f;
            }
        }
        return;
    }

    if (axis == 0) position.x = newPos.x;
    else if (axis == 1) position.y = newPos.y;
    else position.z = newPos.z;
}

void Player::moveWithCollision(float deltaTime) {
    bool groundBelow = checkOnGround();

    if (onGround && !groundBelow && velocity.y >= 0) {
        onGround = false;
    }

    // Горизонтальное движение с подъемом на ступеньки
    resolveCollisionAxis(deltaTime, 0);
    resolveCollisionAxis(deltaTime, 2);

    // Вертикальное
    resolveCollisionAxis(deltaTime, 1);

    // Проверка земли после движения
    if (!onGround && velocity.y < 0) {
        if (checkOnGround()) {
            onGround = true;
            velocity.y = 0;
        }
    }

    // === ИСПРАВЛЕНИЕ: Финальный снаппинг к полу ===
    if (onGround && velocity.y == 0) {
        // Находим точную высоту пола под нами
        glm::vec3 testPos = position;
        float groundHeight = -9999.0f;

        // Спускаемся вниз пока не найдем пол
        for (int i = 0; i < 60; i++) { // Максимум 0.3 юнита вниз
            testPos.y -= 0.005f;
            if (checkCollisionMesh(testPos)) {
                groundHeight = testPos.y + 0.005f + 0.001f; // +0.001f чтобы не задеть пол
                break;
            }
        }

        // Если нашли пол и мы выше него - прижимаем
        if (groundHeight > -9000.0f) {
            float diff = position.y - groundHeight;
            if (diff > 0.001f && diff < 0.1f) {
                // Мы парим над полом - прижимаем
                position.y = groundHeight;
            }
        }
    }
}

void Player::update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider) {
    yaw = cameraYaw;
    pitch = cameraPitch;
    meshCollider = collider;

    handleInput(deltaTime);

    if (noclipMode) {
        moveNoclip(deltaTime);
    }
    else {
        if (!onGround) {
            velocity.y -= gravity * deltaTime;
        }
        moveWithCollision(deltaTime);
    }
}

glm::vec3 Player::getEyePosition() const {
    return glm::vec3(position.x, position.y + 0.8f, position.z);
}

void Player::getViewMatrix(float* matrix) const {
    glm::vec3 eye = getEyePosition();
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    glm::mat4 view = glm::lookAt(eye, eye + front, glm::vec3(0.0f, 1.0f, 0.0f));
    memcpy(matrix, glm::value_ptr(view), sizeof(glm::mat4));
}