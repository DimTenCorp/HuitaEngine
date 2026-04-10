#include "Player.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "TriangleCollider.h"

AABB Player::getPlayerAABB(const glm::vec3& pos) const {
    return AABB{
        pos + m_vecHullMin,
        pos + m_vecHullMax
    };
}

Player::Player() {
    position = glm::vec3(0.0f, 200.0f, 0.0f);
    velocity = glm::vec3(0.0f);

    // Начинаем со стоячего hull
    m_vecHullMin = VEC_HULL_MIN;
    m_vecHullMax = VEC_HULL_MAX;

    onGround = false;
    speed = 200.0f;
    jumpForce = 270.0f;
    gravity = 800.0f;
    friction = 4.0f;

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
}

// === НЕЛИНЕЙНАЯ ИНТЕРПОЛЯЦИЯ КАК В HL1 ===
// S-curve для более быстрого начала и замедления в конце
float Player::UTIL_SplineFraction(float value, float scale) const {
    value = value * scale;
    float valueSquared = value * value;
    // 3x^2 - 2x^3 - кубический сплайн
    return 3.0f * valueSquared - 2.0f * valueSquared * value;
}

// === СИСТЕМА ПРИСЯДА ===

void Player::StartDuck() {
    if (!(m_afPhysicsFlags & PFLAG_DUCKING)) {
        m_afPhysicsFlags |= PFLAG_DUCKING;
        m_flDuckTime = (float)glfwGetTime();
    }
}

void Player::StopDuck() {
    if (m_afPhysicsFlags & PFLAG_DUCKING) {
        m_afPhysicsFlags &= ~PFLAG_DUCKING;
        m_flDuckTime = (float)glfwGetTime();
    }
}

bool Player::IsDucking() const {
    return (m_afPhysicsFlags & PFLAG_DUCKING) != 0;
}

bool Player::IsFullyDucked() const {
    float currentTime = (float)glfwGetTime();
    float elapsed = currentTime - m_flDuckTime;

    // Полностью сидим если:
    // 1. Мы в процессе присяда и прошло TIME_TO_DUCK
    // 2. ИЛИ мы не в процессе присяда, но hull еще сидячий (вставаем)
    if (IsDucking()) {
        return elapsed >= TIME_TO_DUCK;
    }
    else {
        // При вставании - считаем "сидячим" пока не прошло TIME_TO_DUCK
        return elapsed < TIME_TO_DUCK;
    }
}

float Player::GetDuckFraction() const {
    float currentTime = (float)glfwGetTime();
    float elapsed = currentTime - m_flDuckTime;

    if (IsDucking()) {
        // Присядаем: 0 -> 1
        if (elapsed >= TIME_TO_DUCK) return 1.0f;
        // Нелинейная интерполяция для более "резкого" ощущения
        return UTIL_SplineFraction(elapsed, 1.0f / TIME_TO_DUCK);
    }
    else {
        // Встаем: 1 -> 0
        if (elapsed >= TIME_TO_DUCK) return 0.0f;
        return 1.0f - UTIL_SplineFraction(elapsed, 1.0f / TIME_TO_DUCK);
    }
}

glm::vec3 Player::GetCurrentViewOffset() const {
    float frac = GetDuckFraction();
    // Интерполяция между стоячим и сидячим view offset
    // VEC_VIEW = (0, 28, 0), VEC_DUCK_VIEW = (0, 12, 0)
    return glm::mix(VEC_VIEW, VEC_DUCK_VIEW, frac);
}

// === ОБРАБОТКА ВВОДА ===

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

    // Autohop
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

    // === ПРИСЯД ===
    if (currentButtons & 32) {
        // Зажали Ctrl - начинаем присяд
        if ((m_afButtonPressed & 32) && !IsDucking()) {
            StartDuck();
        }
    }
    else {
        // Отпустили Ctrl - пытаемся встать
        if (IsDucking() || IsFullyDucked()) {
            // Проверяем, можем ли встать (нет ли препятствия сверху)
            // Разница в высоте hull: 36 - 18 = 18 юнитов
            float hullDiff = VEC_HULL_MAX.y - VEC_DUCK_HULL_MAX.y; // 18

            glm::vec3 testPos = position;
            testPos.y += hullDiff; // Проверяем на высоте стоячего hull

            // Временно ставим стоячий hull для проверки
            glm::vec3 oldMin = m_vecHullMin;
            glm::vec3 oldMax = m_vecHullMax;
            m_vecHullMin = VEC_HULL_MIN;
            m_vecHullMax = VEC_HULL_MAX;

            bool canStand = !checkCollisionMesh(testPos);

            // Восстанавливаем hull
            m_vecHullMin = oldMin;
            m_vecHullMax = oldMax;

            if (canStand) {
                StopDuck();
            }
            // Иначе остаемся сидеть (hull не меняется)
        }
    }
}

// === ОСНОВНАЯ ЛОГИКА ПРИСЯДА ===

void Player::Duck(float deltaTime) {
    float currentTime = (float)glfwGetTime();
    float elapsed = currentTime - m_flDuckTime;
    float frac = GetDuckFraction();

    if (IsDucking()) {
        // === ПРИСЕДАЕМ ===

        // Меняем hull ТОЛЬКО когда полностью сели
        if (elapsed >= TIME_TO_DUCK && m_vecHullMax.y > VEC_DUCK_HULL_MAX.y) {
            // Меняем hull на сидячий
            m_vecHullMin = VEC_DUCK_HULL_MIN;
            m_vecHullMax = VEC_DUCK_HULL_MAX;

            // Опускаем origin на разницу в hull min
            // VEC_DUCK_HULL_MIN.y - VEC_HULL_MIN.y = -18 - (-36) = 18
            if (onGround) {
                float originShift = VEC_DUCK_HULL_MIN.y - VEC_HULL_MIN.y; // 18
                position.y -= originShift;
            }
        }
    }
    else {
        // === ВСТАЕМ ===

        // Меняем hull ТОЛЬКО когда полностью встали
        if (elapsed >= TIME_TO_DUCK && m_vecHullMax.y < VEC_HULL_MAX.y) {
            // Меняем hull на стоячий
            m_vecHullMin = VEC_HULL_MIN;
            m_vecHullMax = VEC_HULL_MAX;

            // Поднимаем origin обратно
            if (onGround) {
                float originShift = VEC_HULL_MIN.y - VEC_DUCK_HULL_MIN.y; // 18
                position.y += originShift;
            }
        }
    }
}

void Player::Jump() {
    if (m_afPhysicsFlags & PFLAG_ONLADDER) return;
    if (!onGround) return;

    // Longjump только если полностью сидим
    if (IsFullyDucked() && m_fLongJump && (m_afButtonLast & 64)) {
        float yawRad = glm::radians(yaw);
        velocity.x = cos(yawRad) * PLAYER_LONGJUMP_SPEED;
        velocity.z = sin(yawRad) * PLAYER_LONGJUMP_SPEED;
        velocity.y = sqrt(2.0f * gravity * 56.0f);

        // После longjump встаем
        StopDuck();
    }
    else {
        velocity.y = sqrt(2.0f * gravity * 45.0f);
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

    // Friction
    if (onGround) {
        float currentspeed = glm::length(glm::vec2(velocity.x, velocity.z));
        if (currentspeed > 0.001f) {
            float drop = currentspeed * friction * deltaTime;
            float newspeed = currentspeed - drop;
            if (newspeed < 0) newspeed = 0;
            if (newspeed > 0) {
                newspeed /= currentspeed;
                velocity.x *= newspeed;
                velocity.z *= newspeed;
            }
        }
    }

    // Acceleration
    float accel = onGround ? 10.0f : 0.3f;
    float maxSpeed = speed;

    // Сидя - 1/3 скорости
    if (IsFullyDucked()) {
        maxSpeed = speed * 0.333f;
    }
    // Бег
    else if (m_afButtonLast & 64) {
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

    // Air acceleration
    float airAccel = 10.0f;
    float currentspeed = glm::dot(glm::vec2(velocity.x, velocity.z), glm::vec2(wishdir.x, wishdir.z));
    float addspeed = wishspeed - currentspeed;

    if (addspeed > 0) {
        float accelspeed = airAccel * wishspeed * deltaTime;
        if (accelspeed > addspeed) accelspeed = addspeed;
        velocity.x += accelspeed * wishdir.x;
        velocity.z += accelspeed * wishdir.z;
    }

    velocity.y -= gravity * deltaTime;
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
    if (!meshCollider) return false;
    AABB box = getPlayerAABB(pos);
    return meshCollider->intersectAABB(box);
}

bool Player::checkOnGround() const {
    if (!meshCollider) return false;
    glm::vec3 testPos = position;
    testPos.y -= 0.5f;
    return checkCollisionMesh(testPos);
}

bool Player::stepSlideMove(float deltaTime, int axis) {
    float moveAmount = (axis == 0) ? velocity.x * deltaTime : velocity.z * deltaTime;
    if (std::abs(moveAmount) < 0.0001f) return true;

    glm::vec3 startPos = position;
    glm::vec3 moveDir(0.0f);
    if (axis == 0) moveDir.x = moveAmount;
    else moveDir.z = moveAmount;

    // Try normal move
    glm::vec3 newPos = startPos + moveDir;
    if (!checkCollisionMesh(newPos)) {
        position = newPos;
        return true;
    }

    if (!onGround) return false;

    // Try step up
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

        if (checkCollisionMesh(newPos)) {
            if (velocity.y > 0) {
                velocity.y = 0;
            }
            else if (velocity.y < 0) {
                onGround = true;
                velocity.y = 0;
            }
        }
        else {
            position.y = newPos.y;
        }
        return;
    }

    if (!stepSlideMove(deltaTime, axis)) {
        if (axis == 0) velocity.x = 0;
        else velocity.z = 0;
    }
}

void Player::CategorizePosition() {
    onGround = checkOnGround();
}

void Player::moveWithCollision(float deltaTime) {
    PreThink(deltaTime);
    CategorizePosition();

    if (onGround) {
        if (m_nBunnyHopFrames < 3) {
            m_nBunnyHopFrames++;
        }
    }
    else {
        m_nBunnyHopFrames = 0;
    }

    // Обрабатываем присяд ДО движения
    Duck(deltaTime);

    if (onGround) {
        WalkMove(deltaTime);
    }
    else {
        FlyMove(deltaTime);
    }

    bool groundBelow = checkOnGround();
    if (onGround && !groundBelow && velocity.y > 0) {
        onGround = false;
    }

    resolveCollisionAxis(deltaTime, 0);
    resolveCollisionAxis(deltaTime, 2);
    resolveCollisionAxis(deltaTime, 1);

    if (!onGround && velocity.y <= 0) {
        if (checkOnGround()) {
            onGround = true;
            velocity.y = 0;
        }
    }

    // Ground snap
    if (onGround && std::abs(velocity.y) < 1.0f) {
        glm::vec3 testPos = position;
        for (int i = 0; i < 20; i++) {
            testPos.y -= 1.0f;
            if (checkCollisionMesh(testPos)) {
                float groundHeight = testPos.y + 1.0f + 0.5f;
                float diff = position.y - groundHeight;
                if (diff > 0.01f && diff < 10.0f) {
                    position.y = groundHeight;
                }
                break;
            }
        }
    }

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

void Player::update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider) {
    yaw = cameraYaw;
    pitch = cameraPitch;
    meshCollider = collider;

    handleInput(deltaTime);

    if (noclipMode) {
        moveNoclip(deltaTime);
    }
    else {
        moveWithCollision(deltaTime);
    }
}

glm::vec3 Player::getEyePosition() const {
    return position + GetCurrentViewOffset();
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