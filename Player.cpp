#include "pch.h"
#include "Player.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "TriangleCollider.h"
#include "WaterEntity.h"
#include "Engine.h"

AABB Player::getPlayerAABB(const glm::vec3& pos) const {
    return getPlayerCapsule(pos).getBounds();
}

Player::Player() {
    position = glm::vec3(0.0f, 200.0f, 0.0f);
    velocity = glm::vec3(0.0f);

    m_fHullRadius = VEC_HULL_RADIUS;
    m_fHullHeight = VEC_HULL_HEIGHT;
    m_fDuckHullHeight = VEC_DUCK_HULL_HEIGHT;

    m_vecHullMin = glm::vec3(-m_fHullRadius, -m_fHullHeight * 0.5f, -m_fHullRadius);
    m_vecHullMax = glm::vec3(m_fHullRadius, m_fHullHeight * 0.5f, m_fHullRadius);

    onGround = false;
    speed = 200.0f;
    jumpForce = 270.0f;
    gravity = 600.0f;  // УМЕНЬШЕНО с 800
    friction = 4.0f;   // УМЕНЬШЕНО с 7.0 для меньшего скольжения

    yaw = -90.0f;
    pitch = 0.0f;

    noclipMode = false;
    noclipSpeed = 300.0f;

    m_afPhysicsFlags = 0;
    m_flFallVelocity = 0.0f;
    m_flDuckTime = 0.0f;
    m_flTimeStepSound = 0.0f;
    m_iStepLeft = 0;
    m_fLongJump = false;
    m_flWallJumpTime = 0.0f;
    m_afButtonLast = 0;
    m_afButtonPressed = 0;
    m_afButtonReleased = 0;
    m_chTextureType = 0;

    m_bAutoJump = false;
    m_bDidAutoJump = false;
    m_nBunnyHopFrames = 0;

    stepHeight = 18.0f;

    m_flWaterLevel = 0.0f;
    m_flSwimTime = 0.0f;
    m_bWasInWater = false;

    m_physicsAccumulator = 0.0f;
    
    // Инициализация интерполяции
    m_previousPosition = position;
    m_renderPosition = position;
    m_interpolationAlpha = 0.0f;
}

Capsule Player::getPlayerCapsule(const glm::vec3& pos) const {
    float halfHeight = m_fHullHeight * 0.5f - m_fHullRadius;
    if (halfHeight < 0) halfHeight = 0;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 offset = up * halfHeight;

    return Capsule(
        pos - offset,
        pos + offset,
        m_fHullRadius
    );
}

float Player::UTIL_SplineFraction(float value, float scale) const {
    value = value * scale;
    float valueSquared = value * value;
    return 3.0f * valueSquared - 2.0f * valueSquared * value;
}

void Player::StartDuck() {
    if (!(m_afPhysicsFlags & PFLAG_DUCKING)) {
        m_afPhysicsFlags |= PFLAG_DUCKING;
        m_flDuckTime = (float)glfwGetTime();
    }
}

void Player::StopDuck() {
    if (m_afPhysicsFlags & PFLAG_DUCKING) {
        float newHeight = VEC_HULL_HEIGHT;
        float heightDiff = newHeight - m_fHullHeight;
        float shift = heightDiff * 0.5f;

        glm::vec3 testPos = position;
        testPos.y += shift;

        float savedHeight = m_fHullHeight;
        m_fHullHeight = newHeight;
        bool canStand = !checkCollisionMesh(testPos);
        m_fHullHeight = savedHeight;

        if (canStand) {
            m_afPhysicsFlags &= ~PFLAG_DUCKING;
            m_flDuckTime = (float)glfwGetTime();
        }
    }
}

bool Player::IsDucking() const {
    return (m_afPhysicsFlags & PFLAG_DUCKING) != 0;
}

bool Player::IsFullyDucked() const {
    float currentTime = (float)glfwGetTime();
    float elapsed = currentTime - m_flDuckTime;

    if (IsDucking()) {
        return elapsed >= TIME_TO_DUCK;
    }
    else {
        return elapsed < TIME_TO_DUCK;
    }
}

float Player::GetDuckFraction() const {
    float currentTime = (float)glfwGetTime();
    float elapsed = currentTime - m_flDuckTime;

    if (IsDucking()) {
        if (elapsed >= TIME_TO_DUCK) return 1.0f;
        return UTIL_SplineFraction(elapsed, 1.0f / TIME_TO_DUCK);
    }
    else {
        if (elapsed >= TIME_TO_DUCK) return 0.0f;
        return 1.0f - UTIL_SplineFraction(elapsed, 1.0f / TIME_TO_DUCK);
    }
}

glm::vec3 Player::GetCurrentViewOffset() const {
    float frac = GetDuckFraction();
    return glm::mix(VEC_VIEW, VEC_DUCK_VIEW, frac);
}

void Player::handleInput(float deltaTime) {
    if (noclipMode) return;

    GLFWwindow* window = glfwGetCurrentContext();

    int currentButtons = 0;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) currentButtons |= 1;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) currentButtons |= 2;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) currentButtons |= 4;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) currentButtons |= 8;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) currentButtons |= 16;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) currentButtons |= 32;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) currentButtons |= 64;

    int buttonsChanged = m_afButtonLast ^ currentButtons;
    m_afButtonPressed = buttonsChanged & currentButtons;
    m_afButtonReleased = buttonsChanged & (~currentButtons);
    m_afButtonLast = currentButtons;

    if (m_bAutoJump && (currentButtons & 16)) {
        if (onGround && !m_bDidAutoJump) {
            Jump();
            m_bDidAutoJump = true;
        }
    }
    else if (m_afButtonPressed & 16) {
        Jump();
    }

    if (!onGround) {
        m_bDidAutoJump = false;
    }

    if (currentButtons & 32) {
        if ((m_afButtonPressed & 32) && !IsDucking()) {
            StartDuck();
        }
    }
    else {
        if (IsDucking()) {
            StopDuck();
        }
    }
}

void Player::Duck(float deltaTime) {
    float currentTime = (float)glfwGetTime();
    float elapsed = currentTime - m_flDuckTime;

    if (IsDucking()) {
        if (elapsed >= TIME_TO_DUCK && m_fHullHeight > m_fDuckHullHeight) {
            float oldHeight = m_fHullHeight;
            m_fHullHeight = m_fDuckHullHeight;

            if (onGround) {
                float shift = (oldHeight - m_fHullHeight) * 0.5f;
                position.y -= shift;
            }
        }
    }
    else {
        if (elapsed >= TIME_TO_DUCK && m_fHullHeight < VEC_HULL_HEIGHT) {
            float newHeight = VEC_HULL_HEIGHT;
            float heightDiff = newHeight - m_fHullHeight;
            float shift = heightDiff * 0.5f;

            if (onGround) {
                glm::vec3 testPos = position;
                testPos.y += shift;

                float savedHeight = m_fHullHeight;
                m_fHullHeight = newHeight;
                bool canStand = !checkCollisionMesh(testPos);
                m_fHullHeight = savedHeight;

                if (!canStand) {
                    m_afPhysicsFlags |= PFLAG_DUCKING;
                    return;
                }

                position.y += shift;
            }
            else if (IsInWater()) {
                glm::vec3 testPos = position;
                testPos.y += shift;

                float savedHeight = m_fHullHeight;
                m_fHullHeight = newHeight;
                bool canStand = !checkCollisionMesh(testPos);
                m_fHullHeight = savedHeight;

                if (!canStand) {
                    m_afPhysicsFlags |= PFLAG_DUCKING;
                    return;
                }

                position.y += shift;
            }

            m_fHullHeight = newHeight;
        }
    }

    float halfHeight = m_fHullHeight * 0.5f;
    // ИСПРАВЛЕНО: -m_fHullRadius для Z
    m_vecHullMin = glm::vec3(-m_fHullRadius, -halfHeight, -m_fHullRadius);
    m_vecHullMax = glm::vec3(m_fHullRadius, halfHeight, m_fHullRadius);
}

void Player::Jump() {
    if (m_afPhysicsFlags & PFLAG_ONLADDER) return;
    if (!onGround) return;

    if (IsFullyDucked() && m_fLongJump && !(m_afButtonLast & 64)) {
        float yawRad = glm::radians(yaw);
        velocity.x = cos(yawRad) * PLAYER_LONGJUMP_SPEED;
        velocity.z = sin(yawRad) * PLAYER_LONGJUMP_SPEED;
        velocity.y = sqrt(2.0f * gravity * 30.0f);  // ← ВЫСОТА LONGJUMP
        StopDuck();
    }
    else {
        velocity.y = sqrt(2.0f * gravity * 40.0f);  // ← ВЫСОТА ОБЫЧНОГО ПРЫЖКА
    }
    onGround = false;
}

void Player::WalkMove(float deltaTime) {
    float yawRad = glm::radians(yaw);
    glm::vec3 forward(cos(yawRad), 0.0f, sin(yawRad));
    glm::vec3 right(cos(yawRad + glm::half_pi<float>()), 0.0f, sin(yawRad + glm::half_pi<float>()));

    glm::vec3 wishvel(0.0f);
    GLFWwindow* window = glfwGetCurrentContext();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishvel += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishvel -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishvel -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishvel += right;

    float wishspeed = glm::length(wishvel);
    glm::vec3 wishdir = (wishspeed > 0) ? wishvel / wishspeed : glm::vec3(0.0f);

    // Friction - как в старом коде с уменьшенным коэффициентом
    if (onGround) {
        float speed = glm::length(glm::vec2(velocity.x, velocity.z));
        if (speed > 0.001f) {
            float drop = speed * friction * deltaTime;
            float newspeed = std::max(0.0f, speed - drop);
            velocity.x *= newspeed / speed;
            velocity.z *= newspeed / speed;
        }
    }

    float accel = onGround ? 10.0f : 0.3f;
    float maxSpeed = speed;

    if (IsFullyDucked()) {
        maxSpeed = speed * 0.333f;
    }
    else if (!(m_afButtonLast & 64)) {
        maxSpeed = speed * 1.5f;
    }

    float currentspeed = glm::dot(glm::vec2(velocity.x, velocity.z), glm::vec2(wishdir.x, wishdir.z));
    float addspeed = maxSpeed - currentspeed;
    if (addspeed > 0) {
        float accelspeed = accel * deltaTime * maxSpeed;
        if (accelspeed > addspeed) accelspeed = addspeed;
        velocity.x += accelspeed * wishdir.x;
        velocity.z += accelspeed * wishdir.z;
    }

    // Полная остановка при отсутствии ввода
    float horizSpeed = glm::length(glm::vec2(velocity.x, velocity.z));
    if (horizSpeed < 0.5f && onGround) {
        GLFWwindow* window = glfwGetCurrentContext();
        bool hasInput = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

        if (!hasInput) {
            velocity.x = 0.0f;
            velocity.z = 0.0f;
        }
    }
}

void Player::FlyMove(float deltaTime) {
    float yawRad = glm::radians(yaw);
    glm::vec3 forward(cos(yawRad), 0.0f, sin(yawRad));
    glm::vec3 right(cos(yawRad + glm::half_pi<float>()), 0.0f, sin(yawRad + glm::half_pi<float>()));

    glm::vec3 wishvel(0.0f);
    GLFWwindow* window = glfwGetCurrentContext();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishvel += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishvel -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishvel -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishvel += right;

    float wishspeed = glm::length(wishvel);
    glm::vec3 wishdir = (wishspeed > 0) ? wishvel / wishspeed : glm::vec3(0.0f);

    float airAccel = 0.3f;  // УМЕНЬШЕНО с 10.0 для меньшего контроля в воздухе
    float currentspeed = glm::dot(glm::vec2(velocity.x, velocity.z), glm::vec2(wishdir.x, wishdir.z));
    float addspeed = wishspeed - currentspeed;

    if (addspeed > 0) {
        float accelspeed = airAccel * wishspeed * deltaTime;
        if (accelspeed > addspeed) accelspeed = addspeed;
        velocity.x += accelspeed * wishdir.x;
        velocity.z += accelspeed * wishdir.z;
    }

    velocity.y -= gravity * deltaTime;

    float maxFallSpeed = 1200.0f;
    if (velocity.y < -maxFallSpeed) {
        velocity.y = -maxFallSpeed;
    }
}

void Player::CheckFalling(float deltaTime) {
    if (!onGround) {
        if (velocity.y < 0) {
            m_flFallVelocity = -velocity.y;
        }
    }
    else {
        if (m_flFallVelocity > PLAYER_FALL_PUNCH_THRESHOLD) {
            float fallDamage = 0.0f;
            if (m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED) {
                fallDamage = (m_flFallVelocity - PLAYER_MAX_SAFE_FALL_SPEED) * DAMAGE_FOR_FALL_SPEED;
                if (fallDamage > 0) {
                    std::cout << "Fall damage: " << fallDamage << std::endl;
                }
            }
        }
        m_flFallVelocity = 0.0f;
    }
}

void Player::PreThink(float deltaTime) {
}

void Player::PostThink(float deltaTime) {
    CheckFalling(deltaTime);
}

bool Player::checkCollisionMesh(const glm::vec3& pos) const {
    // Сначала проверяем статичный мир
    if (meshCollider && meshCollider->intersectCapsule(getPlayerCapsule(pos))) {
        return true;
    }

    // Затем проверяем двери через Engine (оптимизированно)
    if (Engine* engine = Engine::getInstance()) {
        if (engine->checkDoorCollisionSimple(getPlayerCapsule(pos))) {
            return true;
        }
    }

    return false;
}

bool Player::checkCollision(const glm::vec3& pos, const AABB& box) {
    Capsule capsule = getPlayerCapsule(pos);
    glm::vec3 closest = glm::clamp(capsule.getCenter(), box.min, box.max);
    float dist = glm::length(capsule.getCenter() - closest);
    return dist <= capsule.radius;
}

glm::vec3 Player::ClipVelocity(const glm::vec3& in, const glm::vec3& normal, float overbounce) {
    float backoff = glm::dot(in, normal) * overbounce;
    return in - normal * backoff;
}

bool Player::TryMove(const glm::vec3& start, const glm::vec3& end, glm::vec3& outPos) {
    glm::vec3 delta = end - start;
    float length = glm::length(delta);

    if (length < 0.001f) {
        outPos = start;
        return true;
    }

    glm::vec3 dir = delta / length;
    int steps = (int)(length / 4.0f) + 1;

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        glm::vec3 testPos = start + dir * (length * t);

        if (checkCollisionMesh(testPos)) {
            if (i > 0) {
                t = (float)(i - 1) / (float)steps;
                outPos = start + dir * (length * t);
            }
            else {
                outPos = start;
            }
            return false;
        }
    }

    outPos = end;
    return true;
}

void Player::WalkMove_Sliding(float deltaTime) {
    WalkMove(deltaTime);

    glm::vec3 originalVel = velocity;
    glm::vec3 newPos = position + velocity * deltaTime;

    if (checkCollisionMesh(newPos)) {
        glm::vec3 testX = position + glm::vec3(velocity.x * deltaTime, 0, 0);
        if (!checkCollisionMesh(testX)) {
            position = testX;
        }
        else {
            velocity.x = 0;
        }

        glm::vec3 testZ = position + glm::vec3(0, 0, velocity.z * deltaTime);
        if (!checkCollisionMesh(testZ)) {
            position = testZ;
        }
        else {
            velocity.z = 0;
        }
    }
    else {
        position = newPos;
    }
}

bool Player::checkOnGround() const {
    if (!meshCollider) return false;
    glm::vec3 testPos = position;
    testPos.y -= 2.0f;
    return checkCollisionMesh(testPos);
}

bool Player::stepSlideMove(float deltaTime, int axis) {
    float moveAmount = (axis == 0) ? velocity.x * deltaTime : velocity.z * deltaTime;
    if (std::abs(moveAmount) < 0.0001f) return true;

    glm::vec3 startPos = position;
    glm::vec3 moveDir(0.0f);
    if (axis == 0) moveDir.x = moveAmount;
    else moveDir.z = moveAmount;

    glm::vec3 newPos = startPos + moveDir;
    if (!checkCollisionMesh(newPos)) {
        position = newPos;
        return true;
    }

    if (!onGround) return false;

    glm::vec3 stepUp = startPos;
    stepUp.y += stepHeight;

    if (checkCollisionMesh(stepUp)) {
        return false;
    }

    glm::vec3 stepOver = stepUp + moveDir;
    if (checkCollisionMesh(stepOver)) {
        return false;
    }

    glm::vec3 stepDown = stepOver;
    stepDown.y -= stepHeight;

    if (checkCollisionMesh(stepDown)) {
        position = stepDown;
        position.y += 0.5f;
        onGround = true;
        velocity.y = 0.0f;
        return true;
    }

    for (int i = 1; i <= 5; i++) {
        glm::vec3 testPos = stepOver;
        testPos.y -= stepHeight * (i / 5.0f);
        if (checkCollisionMesh(testPos)) {
            position = testPos;
            position.y += 0.5f;
            onGround = true;
            velocity.y = 0.0f;
            return true;
        }
    }

    return false;
}

void Player::resolveCollisionAxis(float deltaTime, int axis) {
    if (axis == 1) {
        glm::vec3 newPos = position;
        newPos.y += velocity.y * deltaTime;

        if (!checkCollisionMesh(newPos)) {
            position.y = newPos.y;
            return;
        }

        if (velocity.y > 0) {
            velocity.y = 0;
            for (int i = 1; i <= 5; i++) {
                glm::vec3 testPos = position;
                testPos.y = newPos.y - i * 0.5f;
                if (!checkCollisionMesh(testPos)) {
                    position.y = testPos.y;
                    break;
                }
            }
        }
        else if (velocity.y < 0) {
            float minY = newPos.y;
            float maxY = position.y;

            for (int i = 0; i < 5; i++) {
                float midY = (minY + maxY) * 0.5f;
                glm::vec3 testPos = position;
                testPos.y = midY;

                if (checkCollisionMesh(testPos)) {
                    minY = midY;
                }
                else {
                    maxY = midY;
                }
            }

            position.y = maxY + 0.01f;
            onGround = true;
            velocity.y = 0;
        }
        return;
    }

    float moveAmount = (axis == 0) ? velocity.x * deltaTime : velocity.z * deltaTime;
    if (std::abs(moveAmount) < 0.001f) return;

    glm::vec3 moveDir(0.0f);
    if (axis == 0) moveDir.x = moveAmount;
    else moveDir.z = moveAmount;

    glm::vec3 targetPos = position + moveDir;

    if (!checkCollisionMesh(targetPos)) {
        position = targetPos;
        return;
    }

    if (!onGround || std::abs(moveAmount) < 0.01f) {
        if (axis == 0) velocity.x = 0;
        else velocity.z = 0;
        return;
    }

    glm::vec3 stepUp = position;
    stepUp.y += stepHeight;
    stepUp += moveDir;

    if (checkCollisionMesh(stepUp)) {
        if (axis == 0) velocity.x = 0;
        else velocity.z = 0;
        return;
    }

    glm::vec3 groundTest = stepUp;
    groundTest.y -= stepHeight + 2.0f;

    if (checkCollisionMesh(groundTest)) {
        position = stepUp;
        float exactY = findGroundHeight(position, stepHeight + 2.0f);
        if (exactY > 0) position.y = exactY + 0.01f;
        velocity.y = 0;
        return;
    }

    for (float down = stepHeight * 0.5f; down <= stepHeight + 2.0f; down += 2.0f) {
        glm::vec3 slopeTest = stepUp;
        slopeTest.y -= down;
        if (checkCollisionMesh(slopeTest)) {
            position = stepUp;
            position.y -= down - 0.01f;
            velocity.y = 0;
            return;
        }
    }

    if (axis == 0) velocity.x = 0;
    else velocity.z = 0;
}

float Player::findGroundHeight(const glm::vec3& pos, float maxSearchDist) {
    float minY = pos.y - maxSearchDist;
    float maxY = pos.y;

    for (int i = 0; i < 4; i++) {
        float midY = (minY + maxY) * 0.5f;
        glm::vec3 testPos = pos;
        testPos.y = midY;

        if (checkCollisionMesh(testPos)) {
            minY = midY;
        }
        else {
            maxY = midY;
        }
    }

    return maxY;
}

void Player::CategorizePosition() {
    onGround = checkOnGround();
}

void Player::moveWithCollision(float deltaTime) {
    PreThink(deltaTime);
    Duck(deltaTime);

    glm::vec3 oldPos = position;
    bool wasOnGround = onGround;

    CategorizePosition();

    GLFWwindow* window = glfwGetCurrentContext();
    bool hasInput = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        !onGround);

    if (!hasInput && onGround && velocity == glm::vec3(0.0f)) {
        PostThink(deltaTime);
        return;
    }

    if (onGround) {
        if (m_nBunnyHopFrames < 3) {
            m_nBunnyHopFrames++;
        }
    }
    else {
        m_nBunnyHopFrames = 0;
    }

    if (onGround) {
        WalkMove(deltaTime);
    }
    else {
        FlyMove(deltaTime);
    }

    resolveCollisionAxis(deltaTime, 0);
    resolveCollisionAxis(deltaTime, 2);
    resolveCollisionAxis(deltaTime, 1);

    PostThink(deltaTime);
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

// === ИНТЕРПОЛЯЦИЯ ДЛЯ ПЛАВНОГО РЕНДЕРИНГА ===
void Player::updateInterpolation(float alpha) {
    m_interpolationAlpha = alpha;
    m_renderPosition = glm::mix(m_previousPosition, position, alpha);
}

// === ОСНОВНОЙ UPDATE С ФИКСИРОВАННЫМ ТАЙМШАПОМ И ИНТЕРПОЛЯЦИЕЙ ===
void Player::update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider) {
    yaw = cameraYaw;
    pitch = cameraPitch;
    meshCollider = collider;

    handleInput(deltaTime);

    if (noclipMode) {
        moveNoclip(deltaTime);
        // Для noclip тоже нужна интерполяция
        m_previousPosition = m_renderPosition;
        m_renderPosition = position;
        return;
    }

    // Сохраняем предыдущую позицию перед обновлением физики
    m_previousPosition = position;
    
    // === ФИКСИРОВАННЫЙ ТАЙМШАП как в старом коде ===
    m_physicsAccumulator += deltaTime;

    while (m_physicsAccumulator >= FIXED_TIMESTEP) {
        moveWithCollision(FIXED_TIMESTEP);

        // Водная физика внутри фиксированного шага
        if (IsInWater()) {
            ApplyWaterPhysics(FIXED_TIMESTEP);
        }

        m_physicsAccumulator -= FIXED_TIMESTEP;
    }
    
    // Вычисляем альфу для интерполяции
    float interpolationAlpha = 1.0f - (m_physicsAccumulator / FIXED_TIMESTEP);
    updateInterpolation(interpolationAlpha);
}

glm::vec3 Player::getEyePosition() const {
    // Используем интерполированную позицию для рендеринга
    return m_renderPosition + GetCurrentViewOffset();
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

void Player::SetInWater(bool inWater) {
    if (inWater) {
        m_afPhysicsFlags |= PFLAG_INWATER;
    }
    else {
        m_afPhysicsFlags &= ~PFLAG_INWATER;
    }
}

void Player::CheckWater(const std::vector<CFuncWater*>& waterZones) {
    Capsule capsule = getPlayerCapsule(position);
    bool inWater = false;
    float waterSurface = 0.0f;

    for (const auto* water : waterZones) {
        if (water->intersectsCapsule(capsule)) {
            inWater = true;
            waterSurface = water->getTopHeight();
            break;
        }
    }

    bool currentlyInWater = inWater;

    if (inWater) {
        float feetY = position.y + m_vecHullMin.y;
        float headY = position.y + m_vecHullMax.y;

        if (feetY >= waterSurface) {
            currentlyInWater = false;
        }
        else if (headY >= waterSurface) {
            SetInWater(true);
            m_flWaterLevel = (position.y >= waterSurface) ? 3.0f :
                ((position.y + m_vecHullMax.y * 0.5f) >= waterSurface) ? 2.0f : 1.0f;
        }
        else {
            SetInWater(true);
            m_flWaterLevel = 3.0f;
        }
    }
    else {
        SetInWater(false);
        m_flWaterLevel = 0;
    }

    if (currentlyInWater && !m_bWasInWater) {
        std::cout << "[WATER] Entered water! Level=" << m_flWaterLevel
            << " surface=" << waterSurface << std::endl;
    }
    else if (!currentlyInWater && m_bWasInWater) {
        std::cout << "[WATER] Exited water!" << std::endl;
    }

    m_bWasInWater = currentlyInWater;
}

void Player::ApplyWaterPhysics(float deltaTime) {
    if (!IsInWater()) return;

    onGround = false;

    float waterDrag = 0.92f;
    float dragFactor = powf(waterDrag, deltaTime * 60.0f);
    velocity *= dragFactor;

    GLFWwindow* window = glfwGetCurrentContext();
    float yawRad = glm::radians(yaw);
    glm::vec3 forward(cos(yawRad), 0.0f, sin(yawRad));
    glm::vec3 right(cos(yawRad + glm::half_pi<float>()), 0.0f, sin(yawRad + glm::half_pi<float>()));
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::vec3 wishvel(0.0f);
    float swimSpeed = speed * 0.6f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishvel += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishvel -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishvel -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishvel += right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) wishvel += up;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) wishvel -= up;

    float wishspeed = glm::length(wishvel);
    if (wishspeed > 0) {
        glm::vec3 wishdir = wishvel / wishspeed;
        float accel = 15.0f;
        float addspeed = swimSpeed * wishspeed - glm::dot(velocity, wishdir);

        if (addspeed > 0) {
            float accelspeed = accel * deltaTime * swimSpeed * wishspeed;
            if (accelspeed > addspeed) accelspeed = addspeed;
            velocity += accelspeed * wishdir;
        }
    }

    float maxSwimSpeed = swimSpeed * 1.5f;
    if (glm::length(velocity) > maxSwimSpeed) {
        velocity = glm::normalize(velocity) * maxSwimSpeed;
    }

    if (wishspeed < 0.1f && !onGround) {
        velocity.y += gravity * 0.5f * deltaTime;
    }
    else {
        if (velocity.y < 0) {
            velocity.y -= gravity * 0.3f * deltaTime;
        }
    }

    m_flSwimTime += deltaTime;
}