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
#include "DoorEntity.h"

AABB Player::getPlayerAABB(const glm::vec3& pos) const {
    return getPlayerCapsule(pos).getBounds();
}

Player::Player() {
    position = glm::vec3(0.0f, 200.0f, 0.0f);
    velocity = glm::vec3(0.0f);

    // Инициализация капсулы
    m_fHullRadius = VEC_HULL_RADIUS;
    m_fHullHeight = VEC_HULL_HEIGHT;
    m_fDuckHullHeight = VEC_DUCK_HULL_HEIGHT;

    // Начальные hull (как AABB для расчета, но используем как капсулу)
    m_vecHullMin = glm::vec3(-m_fHullRadius, -m_fHullHeight * 0.5f, -m_fHullRadius);
    m_vecHullMax = glm::vec3(m_fHullRadius, m_fHullHeight * 0.5f, m_fHullRadius);

    onGround = false;
    speed = 200.0f;
    jumpForce = 270.0f;
    gravity = 800.0f;
    friction = 7.0f;

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

    stepHeight = 12.0f;

    // Инициализация воды
    m_flWaterLevel = 0.0f;
    m_flSwimTime = 0.0f;
    m_bWasInWater = false;
}

Capsule Player::getPlayerCapsule(const glm::vec3& pos) const {
    float halfHeight = m_fHullHeight * 0.5f - m_fHullRadius; // Высота цилиндра без полусфер
    if (halfHeight < 0) halfHeight = 0;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 offset = up * halfHeight;

    return Capsule(
        pos - offset,  // Нижняя сфера
        pos + offset,  // Верхняя сфера  
        m_fHullRadius  // Радиус
    );
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
    // Пытаемся встать ТОЛЬКО если есть место
    if (m_afPhysicsFlags & PFLAG_DUCKING) {
        // Проверяем, можем ли мы встать прямо сейчас
        float newHeight = VEC_HULL_HEIGHT; // 72
        float heightDiff = newHeight - m_fHullHeight;
        float shift = heightDiff * 0.5f;

        glm::vec3 testPos = position;
        testPos.y += shift;

        // Сохраняем текущую высоту для проверки
        float savedHeight = m_fHullHeight;
        m_fHullHeight = newHeight;
        bool canStand = !checkCollisionMesh(testPos);
        m_fHullHeight = savedHeight;

        if (canStand) {
            // Можем встать - снимаем флаг и обновляем время
            m_afPhysicsFlags &= ~PFLAG_DUCKING;
            m_flDuckTime = (float)glfwGetTime();
        }
        // Если НЕ можем встать - НЕ снимаем флаг и НЕ обновляем время!
        // Остаёмся сидеть молча, без попыток встать
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

    // === ПРИСЯД (без изменений) ===
    if (currentButtons & 32) {
        // Зажали Ctrl - начинаем присяд
        if ((m_afButtonPressed & 32) && !IsDucking()) {
            StartDuck();
        }
    }
    else {
        // Отпустили Ctrl - пытаемся встать (StopDuck сам проверит можно ли)
        if (IsDucking()) {
            StopDuck();
            // Если StopDuck не смог встать (нельзя), он просто оставит флаг DUCKING
            // и игрок останется сидеть без цикла попыток
        }
    }
}

// === ОСНОВНАЯ ЛОГИКА ПРИСЯДА ===

void Player::Duck(float deltaTime) {
    float currentTime = (float)glfwGetTime();
    float elapsed = currentTime - m_flDuckTime;

    if (IsDucking()) {
        // === ПРИСЕДАЕМ или УЖЕ СИДИМ ===
        if (elapsed >= TIME_TO_DUCK && m_fHullHeight > m_fDuckHullHeight) {
            float oldHeight = m_fHullHeight;
            m_fHullHeight = m_fDuckHullHeight; // 36

            // На земле опускаем origin, чтобы ноги остались на месте
            if (onGround) {
                float shift = (oldHeight - m_fHullHeight) * 0.5f;
                position.y -= shift;
            }
        }
    }
    else {
        // === ВСТАЁМ (только если флаг снят, а он снимается только когда можно встать) ===
        if (elapsed >= TIME_TO_DUCK && m_fHullHeight < VEC_HULL_HEIGHT) {
            float newHeight = VEC_HULL_HEIGHT; // 72
            float heightDiff = newHeight - m_fHullHeight;
            float shift = heightDiff * 0.5f;

            // Здесь мы уже знаем что можно встать (StopDuck проверил)
            // Но на всякий случай проверим ещё раз
            if (onGround) {
                glm::vec3 testPos = position;
                testPos.y += shift;

                float savedHeight = m_fHullHeight;
                m_fHullHeight = newHeight;
                bool canStand = !checkCollisionMesh(testPos);
                m_fHullHeight = savedHeight;

                if (!canStand) {
                    // Опять нельзя встать (что-то изменилось) - возвращаем флаг
                    m_afPhysicsFlags |= PFLAG_DUCKING;
                    return;
                }

                position.y += shift;
            }
            // === ИСПРАВЛЕНИЕ: Под водой тоже нужно проверять и смещаться ===
            else if (IsInWater()) {
                glm::vec3 testPos = position;
                testPos.y += shift;

                float savedHeight = m_fHullHeight;
                m_fHullHeight = newHeight;
                bool canStand = !checkCollisionMesh(testPos);
                m_fHullHeight = savedHeight;

                if (!canStand) {
                    // Нельзя встать - возвращаем флаг
                    m_afPhysicsFlags |= PFLAG_DUCKING;
                    return;
                }

                // Можем встать - смещаем позицию вверх
                position.y += shift;
            }

            m_fHullHeight = newHeight;
        }
    }

    // Обновляем AABB для совместимости
    float halfHeight = m_fHullHeight * 0.5f;
    m_vecHullMin = glm::vec3(-m_fHullRadius, -halfHeight, m_fHullRadius);
    m_vecHullMax = glm::vec3(m_fHullRadius, halfHeight, m_fHullRadius);
}

void Player::Jump() {
    if (m_afPhysicsFlags & PFLAG_ONLADDER) return;
    if (!onGround) return;

    // Longjump: полностью сидим + Shift НЕ зажат (теперь "бег" по умолчанию)
    // Или если хочешь оставить логику "longjump только когда бежишь":
    if (IsFullyDucked() && m_fLongJump && !(m_afButtonLast & 64)) {
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

    // === ИНВЕРТИРОВАННАЯ ЛОГИКА СКОРОСТИ ===
    // Сидя - всё ещё 1/3 скорости (это не меняем)
    if (IsFullyDucked()) {
        maxSpeed = speed * 0.333f;
    }
    // Теперь: просто W = бег (1.5x), Shift+W = ходьба (1.0x)
    else if (!(m_afButtonLast & 64)) {  // Shift НЕ зажат - бежим
        maxSpeed = speed * 1.5f;
    }
    // Если Shift зажат - обычная скорость (speed * 1.0, ничего не меняем)
    // else { maxSpeed = speed; } - это по умолчанию

    float currentspeed = glm::dot(glm::vec2(velocity.x, velocity.z), glm::vec2(wishdir.x, wishdir.z));
    float addspeed = maxSpeed - currentspeed;
    if (addspeed > 0) {
        float accelspeed = accel * deltaTime * maxSpeed;
        if (accelspeed > addspeed) accelspeed = addspeed;
        velocity.x += accelspeed * wishdir.x;
        velocity.z += accelspeed * wishdir.z;
    }

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
    if (!meshCollider) return false;
    Capsule capsule = getPlayerCapsule(pos);
    return meshCollider->intersectCapsule(capsule);
}

// Проверка столкновений с AABB (для совместимости)
bool Player::checkCollision(const glm::vec3& pos, const AABB& box) {
    Capsule capsule = getPlayerCapsule(pos);

    // Находим ближайшую точку на AABB к капсуле
    glm::vec3 closest = glm::clamp(capsule.getCenter(), box.min, box.max);

    // Проверяем расстояние от центра капсулы до ближайшей точки
    float dist = glm::length(capsule.getCenter() - closest);

    // Также нужно проверить расстояние от линии капсулы до коробки
    // Упрощенная проверка: если центр капсулы внутри AABB или близко
    return dist <= capsule.radius;
}

// Отскок от поверхности
glm::vec3 Player::ClipVelocity(const glm::vec3& in, const glm::vec3& normal, float overbounce) {
    float backoff = glm::dot(in, normal) * overbounce;
    return in - normal * backoff;
}

// Попытка перемещения с проверкой столкновений
bool Player::TryMove(const glm::vec3& start, const glm::vec3& end, glm::vec3& outPos) {
    glm::vec3 delta = end - start;
    float length = glm::length(delta);

    if (length < 0.001f) {
        outPos = start;
        return true;
    }

    glm::vec3 dir = delta / length;

    // Step test - проверяем несколько точек вдоль пути
    int steps = (int)(length / 4.0f) + 1;

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        glm::vec3 testPos = start + dir * (length * t);

        if (checkCollisionMesh(testPos)) {
            // Столкновение - возвращаем предыдущую позицию
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

// Движение со скольжением вдоль стен (HL-style)
void Player::WalkMove_Sliding(float deltaTime) {
    WalkMove(deltaTime);

    // После обычного движения, если есть столкновение - скользим вдоль стен
    glm::vec3 originalVel = velocity;
    glm::vec3 newPos = position + velocity * deltaTime;

    if (checkCollisionMesh(newPos)) {
        // Столкновение - пытаемся скользить
        // В HL это делается через ClipVelocity

        // Проверяем оси по отдельности
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

    // Проверяем несколько точек вниз для надёжности
    // На пологих склонах важно найти контакт с поверхностью
    for (float offset = 0.5f; offset <= 4.0f; offset += 0.5f) {
        glm::vec3 testPos = position;
        testPos.y -= offset;
        if (checkCollisionMesh(testPos)) {
            return true;
        }
    }
    return false;
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
    if (axis == 1) { // Y-ось - без изменений, работает хорошо
        glm::vec3 newPos = position;
        newPos.y += velocity.y * deltaTime;

        if (!checkCollisionMesh(newPos)) {
            position.y = newPos.y;
            return;
        }

        if (velocity.y > 0) {
            velocity.y = 0;
            float testY = newPos.y;
            for (int i = 0; i < 20; i++) {
                glm::vec3 testPos = position;
                testPos.y = testY - (i * 0.5f);
                if (!checkCollisionMesh(testPos)) {
                    position.y = testPos.y;
                    break;
                }
            }
        }
        else if (velocity.y < 0) {
            float minY = newPos.y;
            float maxY = position.y;

            for (int i = 0; i < 10; i++) {
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
        else {
            glm::vec3 escapePos = position;
            for (int i = 1; i <= 20; i++) {
                escapePos.y = position.y + i * 0.5f;
                if (!checkCollisionMesh(escapePos)) {
                    position.y = escapePos.y;
                    break;
                }
            }
        }
        return;
    }

    // === ГОРИЗОНТАЛЬНЫЕ ОСИ (X и Z) - ИСПРАВЛЕНО ===
    float moveAmount = (axis == 0) ? velocity.x * deltaTime : velocity.z * deltaTime;
    if (std::abs(moveAmount) < 0.0001f) return;

    glm::vec3 moveDir(0.0f);
    if (axis == 0) moveDir.x = moveAmount;
    else moveDir.z = moveAmount;

    glm::vec3 targetPos = position + moveDir;

    // Сначала пробуем двигаться без подъёма
    if (!checkCollisionMesh(targetPos)) {
        position = targetPos;
        return;
    }

    // Столкновение - пробуем подняться НА МИНИМАЛЬНУЮ высоту препятствия
    // вместо сразу stepHeight
    if (onGround) {
        // Ищем минимальную высоту подъёма бинарным поиском
        float minStep = 0.0f;
        float maxStep = stepHeight; // Максимум - обычная высота ступеньки

        glm::vec3 bestPos = position;
        bool found = false;

        // Сначала проверим, можно ли вообще подняться
        glm::vec3 highPos = position;
        highPos.y += maxStep;
        highPos += moveDir;

        if (!checkCollisionMesh(highPos)) {
            // Можно подняться - ищем минимальную высоту
            for (int i = 0; i < 8; i++) {
                float midStep = (minStep + maxStep) * 0.5f;
                glm::vec3 testPos = position;
                testPos.y += midStep;
                testPos += moveDir;

                if (checkCollisionMesh(testPos)) {
                    // Всё ещё внутри - нужно выше
                    minStep = midStep;
                }
                else {
                    // Можно пройти - запоминаем и пробуем ниже
                    maxStep = midStep;
                    bestPos = testPos;
                    found = true;
                }
            }

            if (found) {
                // Проверяем, есть ли пол под нами на новой позиции
                // чтобы не повиснуть в воздухе на крутом склоне
                bool hasGround = false;
                for (float down = 0.5f; down <= stepHeight + 2.0f; down += 0.5f) {
                    glm::vec3 groundTest = bestPos;
                    groundTest.y -= down;
                    if (checkCollisionMesh(groundTest)) {
                        hasGround = true;
                        // Прилипаем к полу если разница небольшая
                        float groundY = groundTest.y + 0.01f;
                        if (std::abs(bestPos.y - groundY) < 4.0f) {
                            bestPos.y = groundY;
                        }
                        break;
                    }
                }

                position = bestPos;
                velocity.y = 0; // Гасим вертикальную скорость
                return;
            }
        }
    }

    // Не удалось подняться - останавливаем скорость по этой оси
    if (axis == 0) velocity.x = 0;
    else velocity.z = 0;
}

void Player::CategorizePosition() {
    onGround = checkOnGround();
}

void Player::moveWithCollision(float deltaTime) {
    PreThink(deltaTime);

    // Сначала обрабатываем присяд
    Duck(deltaTime);

    // Определяем на земле ли мы ПЕРЕД движением
    CategorizePosition();

    // Счётчик кадров для bunny hop
    if (onGround) {
        if (m_nBunnyHopFrames < 3) {
            m_nBunnyHopFrames++;
        }
    }
    else {
        m_nBunnyHopFrames = 0;
    }

    // Вычисляем желаемую скорость
    if (onGround) {
        WalkMove(deltaTime);
    }
    else {
        FlyMove(deltaTime);
    }

    // Применяем движение с разрешением коллизий
    // Порядок важен: сначала горизонтальное, потом вертикальное
    resolveCollisionAxis(deltaTime, 0); // X
    resolveCollisionAxis(deltaTime, 2); // Z
    resolveCollisionAxis(deltaTime, 1); // Y

    // Пост-обработка
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
    if (isPaused) {
        return; // Просто выходим, не трогая скорость
    }

    yaw = cameraYaw;
    pitch = cameraPitch;
    meshCollider = collider;

    handleInput(deltaTime);

    if (noclipMode) {
        moveNoclip(deltaTime);
    }
    else {
        moveWithCollision(deltaTime);

        if (IsInWater()) {
            ApplyWaterPhysics(deltaTime);
        }
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
// === ВОДНАЯ ФИЗИКА ===

void Player::SetInWater(bool inWater) {
    if (inWater) {
        m_afPhysicsFlags |= PFLAG_INWATER;
    }
    else {
        m_afPhysicsFlags &= ~PFLAG_INWATER;
    }
}

void Player::CheckWater(const std::vector<CFuncWater*>& waterZones) {
    // Получаем капсулу игрока
    Capsule capsule = getPlayerCapsule(position);

    bool inWater = false;
    float waterSurface = 0.0f;

    // Проверяем все зоны воды
    for (const auto* water : waterZones) {
        if (water->intersectsCapsule(capsule)) {
            inWater = true;
            waterSurface = water->getTopHeight();
            break;
        }
    }

    // Определяем уровень воды (0-3 как в HL)
    // 0 - не в воде, 1 - ноги, 2 - пояс, 3 - голова/полностью
    bool currentlyInWater = inWater;

    if (inWater) {
        float feetY = position.y + m_vecHullMin.y;
        float headY = position.y + m_vecHullMax.y;

        if (feetY >= waterSurface) {
            // Ноги выше воды - не в воде
            SetInWater(false);
            m_flWaterLevel = 0;
            currentlyInWater = false;
        }
        else if (headY >= waterSurface) {
            // Голова над водой - частичное погружение
            SetInWater(true);
            if ((position.y) >= waterSurface) {
                m_flWaterLevel = 3.0f;  // Полностью под водой
            }
            else if ((position.y + m_vecHullMax.y * 0.5f) >= waterSurface) {
                m_flWaterLevel = 2.0f;  // По пояс
            }
            else {
                m_flWaterLevel = 1.0f;  // Только ноги
            }
        }
        else {
            // Полностью под водой
            SetInWater(true);
            m_flWaterLevel = 3.0f;
        }
    }
    else {
        SetInWater(false);
        m_flWaterLevel = 0;
    }

    // Логирование только при входе/выходе из воды
    if (currentlyInWater && !m_bWasInWater) {
        std::cout << "[WATER] Entered water! Level=" << m_flWaterLevel
            << " surface=" << waterSurface << std::endl;
    }
    else if (!currentlyInWater && m_bWasInWater) {
        std::cout << "[WATER] Exited water!" << std::endl;
    }

    m_bWasInWater = currentlyInWater;
}

void Player::CheckDoors(const std::vector<CFuncDoor*>& doors) {
    // Получаем капсулу игрока
    Capsule capsule = getPlayerCapsule(position);

    // Проверяем пересечение с каждой дверью
    for (auto* door : doors) {
        if (door->intersectsCapsule(capsule)) {
            // Игрок касается двери - триггер открытие
            door->triggerOpen();
            std::cout << "[Player] Door collision detected! Triggering open.\n";
        }
    }
}

void Player::ApplyWaterPhysics(float deltaTime) {
    if (!IsInWater()) return;

    // ВАЖНО: В воде игрок НЕ на земле - он плавает
    onGround = false;

    // Гравитация в воде меньше (игрок медленнее тонет/всплывает)
    float waterGravity = gravity * 0.3f;

    // Сопротивление воды (сильнее чем воздух)
    // Используем экспоненциальное затухание для независимости от FPS
    float waterDrag = 0.92f;
    float dragFactor = powf(waterDrag, deltaTime * 60.0f); // Нормализуем к 60 FPS

    // Применяем сопротивление ко всей скорости
    velocity *= dragFactor;

    // В воде игрок может двигаться во всех направлениях (как в полете но медленнее)
    GLFWwindow* window = glfwGetCurrentContext();
    float yawRad = glm::radians(yaw);
    glm::vec3 forward(cos(yawRad), 0.0f, sin(yawRad));
    glm::vec3 right(cos(yawRad + glm::half_pi<float>()), 0.0f, sin(yawRad + glm::half_pi<float>()));
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::vec3 wishvel(0.0f);
    float swimSpeed = speed * 0.6f;  // Медленнее чем ходьба

    // Горизонтальное движение
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishvel += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishvel -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishvel -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishvel += right;

    // Пробел - всплытие, Ctrl - погружение
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        wishvel += up;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        wishvel -= up;
    }

    float wishspeed = glm::length(wishvel);
    if (wishspeed > 0) {
        glm::vec3 wishdir = wishvel / wishspeed;
        float accel = 15.0f;  // Ускорение в воде
        float addspeed = swimSpeed * wishspeed - glm::dot(velocity, wishdir);

        if (addspeed > 0) {
            float accelspeed = accel * deltaTime * swimSpeed * wishspeed;
            if (accelspeed > addspeed) accelspeed = addspeed;
            velocity += accelspeed * wishdir;
        }
    }

    // Ограничиваем максимальную скорость в воде
    float maxSwimSpeed = swimSpeed * 1.5f;
    if (glm::length(velocity) > maxSwimSpeed) {
        velocity = glm::normalize(velocity) * maxSwimSpeed;
    }

    // Если игрок не движется активно - медленно всплывает (плавучесть)
    if (wishspeed < 0.1f && !onGround) {
        // Добавляем небольшую силу всплытия
        velocity.y += waterGravity * 0.5f * deltaTime;
    }
    else {
        // Применяем уменьшенную гравитацию только если не всплываем активно
        if (velocity.y < 0) {
            velocity.y -= waterGravity * deltaTime;
        }
    }

    m_flSwimTime += deltaTime;
}

float Player::getCurrentSpeed() const {
    // Считаем скорость только по горизонтали (X и Z), игнорируя падение/подъем по Y
    return glm::length(glm::vec2(velocity.x, velocity.z));
}