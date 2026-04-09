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

    // HL: Initialize hull sizes (Quake/Half-Life style)
    m_vecStandMin = glm::vec3(-16.0f, -36.0f, -16.0f) / 32.0f;  // Scale to meters roughly
    m_vecStandMax = glm::vec3(16.0f, 36.0f, 16.0f) / 32.0f;
    m_vecDuckMin = glm::vec3(-16.0f, -18.0f, -16.0f) / 32.0f;
    m_vecDuckMax = glm::vec3(16.0f, 18.0f, 16.0f) / 32.0f;
    m_vecViewStand = glm::vec3(0.0f, 28.0f, 0.0f) / 32.0f;
    m_vecViewDuck = glm::vec3(0.0f, 12.0f, 0.0f) / 32.0f;

    size = glm::vec3(0.2f, 0.6f, 0.2f);  // Keep old for compatibility

    onGround = false;
    speed = 6.0f;			// HL: sv_maxspeed equivalent
    jumpForce = 8.0f;		// HL: sqrt(2 * gravity * height)
    gravity = 25.0f;		// HL: sv_gravity
    friction = 0.5f;		// HL: sv_friction

    yaw = -90.0f;
    pitch = 0.0f;

    noclipMode = false;
    noclipSpeed = 12.0f;

    // HL: Initialize physics state
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
}

void Player::GetHullSize(glm::vec3& outMin, glm::vec3& outMax) const {
    if (IsFullyDucked()) {
        outMin = m_vecDuckMin;
        outMax = m_vecDuckMax;
    }
    else {
        outMin = m_vecStandMin;
        outMax = m_vecStandMax;
    }
}

float Player::GetViewOffset() const {
    if (IsFullyDucked()) {
        return m_vecViewDuck.y;
    }
    return m_vecViewStand.y;
}

bool Player::IsFullyDucked() const {
    if (!(m_afPhysicsFlags & PFLAG_DUCKING)) return false;
    float duckTime = glfwGetTime() - m_flDuckTime;
    return duckTime >= TIME_TO_DUCK;
}

glm::vec3 Player::getMin(const glm::vec3& pos) const {
    glm::vec3 hullMin, hullMax;
    GetHullSize(hullMin, hullMax);
    return pos + hullMin;
}

glm::vec3 Player::getMax(const glm::vec3& pos) const {
    glm::vec3 hullMin, hullMax;
    GetHullSize(hullMin, hullMax);
    return pos + hullMax;
}

AABB Player::getPlayerAABB(const glm::vec3& pos) const {
    AABB box;
    box.min = getMin(pos);
    box.max = getMax(pos);
    return box;
}

// ========== HL-STYLE INPUT ==========

void Player::handleInput(float deltaTime) {
    if (noclipMode) return;

    GLFWwindow* window = glfwGetCurrentContext();

    // HL-style button tracking
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

    // BHOP: Space для прыжка (edge detection или autohop)
    if (m_bAutoJump && (currentButtons & 16)) {
        // Autohop mode - прыгаем каждый кадр когда можно
        if (onGround && !m_bDidAutoJump) {
            Jump();
            m_bDidAutoJump = true;
        }
    }
    else if (m_afButtonPressed & 16) {
        // Обычный прыжок по нажатию
        Jump();
    }

    // Сбрасываем флаг autohop когда не на земле
    if (!onGround) {
        m_bDidAutoJump = false;
    }

    // HL: Duck handling
    if (currentButtons & 32) {  // CTRL pressed
        if ((m_afButtonPressed & 32) && !IsDucking()) {
            StartDuck();
        }
    }
    else {
        if (IsDucking()) {
            // Try to unduck - check if there's room
            glm::vec3 testPos = position;
            testPos.y += (m_vecStandMax.y - m_vecDuckMax.y);  // Move up by duck difference
            if (!checkCollisionMesh(testPos)) {
                StopDuck();
            }
        }
    }

    // HL: Jump handling (edge detection like HL)
    if (m_afButtonPressed & 16) {  // Space just pressed
        Jump();
    }
}

// ========== HL-STYLE JUMP ==========

void Player::Jump() {
    // HL: Can't jump if on ladder
    if (m_afPhysicsFlags & PFLAG_ONLADDER) return;

    // HL: Water jump check (skip)

    // BHOP KEY: Прыгаем только если на земле
    if (!onGround) return;

    // HL: Calculate jump velocity
    float jumpHeight = 45.0f / 32.0f;

    // Сохраняем горизонтальную скорость для BHOP
    float horizSpeed = glm::length(glm::vec2(velocity.x, velocity.z));

    // Longjump check
    if (IsDucking() && m_fLongJump && (m_afButtonLast & 64)) {
        float yawRad = glm::radians(yaw);
        velocity.x = cos(yawRad) * PLAYER_LONGJUMP_SPEED / 32.0f;
        velocity.z = sin(yawRad) * PLAYER_LONGJUMP_SPEED / 32.0f;
        velocity.y = sqrt(2.0f * gravity * 56.0f / 32.0f);
    }
    else {
        // Обычный прыжок - сохраняем текущую горизонтальную скорость!
        velocity.y = sqrt(2.0f * gravity * jumpHeight);
    }

    // BHOP: Сбрасываем флаг земли ДО применения физики
    onGround = false;

    // Небольшой бонус к скорости при perfect bhop (как в HL1)
    // В HL1 это баг с friction, но мы сделаем честный бонус
    if (horizSpeed > 0.1f) {
        // Увеличиваем скорость на 5-10% при каждом прыжке (капимит ~1.7x от base speed)
        float maxBhopSpeed = speed * 1.7f;
        if (horizSpeed < maxBhopSpeed) {
            float boost = 1.0f + (0.05f * (1.0f - horizSpeed / maxBhopSpeed));
            velocity.x *= boost;
            velocity.z *= boost;
        }
    }
}

// ========== HL-STYLE DUCK ==========

void Player::Duck(float deltaTime) {
    if (!IsDucking()) return;

    float duckTime = glfwGetTime() - m_flDuckTime;

    if (duckTime < TIME_TO_DUCK && onGround) {
        // HL: In transition - adjust view offset smoothly
        float duckFraction = duckTime / TIME_TO_DUCK;
        // View offset interpolates between stand and duck
    }
    else {
        // HL: Fully ducked - adjust position and hull
        if (onGround) {
            // When ducking on ground, origin stays same but view goes down
            // When landing while ducking, need to adjust
        }
    }
}

// ========== HL-STYLE MOVEMENT ==========

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

    // HL friction - ТОЛЬКО НА ЗЕМЛЕ! (ключевой момент для BHOP)
    if (onGround) {
        float currentspeed = glm::length(glm::vec2(velocity.x, velocity.z));
        if (currentspeed > 0.001f) {
            float drop = currentspeed * 4.0f * deltaTime;
            float newspeed = currentspeed - drop;
            if (newspeed < 0) newspeed = 0;
            if (newspeed > 0) {
                newspeed /= currentspeed;
                velocity.x *= newspeed;
                velocity.z *= newspeed;
            }
        }
    }
    // В воздухе НЕТ friction - скорость сохраняется!

    // HL acceleration - сильнее на земле, слабее в воздухе
    float accel = onGround ? 10.0f : 0.3f; // В воздухе почти нет ускорения
    float maxSpeed = (m_afButtonLast & 64) ? speed * 1.5f : speed;

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

    // Получаем wish direction
    glm::vec3 wishvel(0.0f);
    GLFWwindow* window = glfwGetCurrentContext();

    bool forwardPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool backPressed = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool leftPressed = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool rightPressed = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    if (forwardPressed) wishvel += forward;
    if (backPressed) wishvel -= forward;
    if (leftPressed) wishvel -= right;
    if (rightPressed) wishvel += right;

    float wishspeed = glm::length(wishvel);
    glm::vec3 wishdir;

    if (wishspeed > 0.001f) {
        wishdir = wishvel / wishspeed;
    }
    else {
        // КЛЮЧЕВОЙ МОМЕНТ: если не нажаты клавиши, 
        // но нажаты только A или D - движемся вперёд!
        if ((leftPressed || rightPressed) && !forwardPressed && !backPressed) {
            wishdir = forward; // Вперёд по взгляду!
            wishspeed = 30.0f / 32.0f;
        }
        else {
            wishdir = glm::vec3(0.0f);
            wishspeed = 0.0f;
        }
    }

    // HL air acceleration
    float airAccel = 10.0f; // sv_airaccelerate

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

// ========== FALL DAMAGE (HL-STYLE) ==========

void Player::CheckFalling(float deltaTime) {
    // HL: Track fall velocity when in air
    if (!onGround) {
        if (velocity.y < 0) {
            m_flFallVelocity = -velocity.y;
        }
    }
    else {
        // HL: Just landed
        if (m_flFallVelocity > PLAYER_FALL_PUNCH_THRESHOLD / 32.0f) {
            // HL: Fall impact
            float fallDamage = 0.0f;

            if (m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED / 32.0f) {
                // Calculate damage
                fallDamage = (m_flFallVelocity * 32.0f - PLAYER_MAX_SAFE_FALL_SPEED) * DAMAGE_FOR_FALL_SPEED;

                // HL: Play fall pain sound (removed as requested)
                // HL: Screen punch
                // pitch = m_flFallVelocity * 0.013f;  // HL formula
                if (fallDamage > 0) {
                    std::cout << "Fall damage: " << fallDamage << std::endl;
                    // Apply damage to player here
                }
            }
        }
        m_flFallVelocity = 0.0f;
    }
}

// ========== PRE/POST THINK (HL-STYLE) ==========

void Player::PreThink(float deltaTime) {
    // HL: Check ladder, water, etc. here
    // Simplified: just track buttons
}

void Player::PostThink(float deltaTime) {
    // HL: Fall damage, footsteps, etc.
    CheckFalling(deltaTime);
}

// ========== COLLISION (your existing + HL integration) ==========

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
    // HL-style ground check: trace down slightly
    glm::vec3 testPos = position;
    testPos.y -= 0.05f;
    return checkCollisionMesh(testPos);
}

glm::vec3 Player::ClipVelocity(const glm::vec3& in, const glm::vec3& normal, float overbounce) {
    float backoff = glm::dot(in, normal) * overbounce;
    glm::vec3 out = in - normal * backoff;

    // HL: если скорость слишком маленькая - обнуляем
    if (out.length() < 0.001f) {
        return glm::vec3(0.0f);
    }

    return out;
}

bool Player::TryMove(const glm::vec3& start, const glm::vec3& end, glm::vec3& outPos) {
    // Пробуем прямой путь
    if (!checkCollisionMesh(end)) {
        outPos = end;
        return true;
    }

    // Находим нормаль столкновения (упрощенно - проверяем оси)
    glm::vec3 move = end - start;

    // Пробуем только X
    if (std::abs(move.x) > 0.001f) {
        glm::vec3 testX = start;
        testX.x += move.x;
        if (!checkCollisionMesh(testX)) {
            outPos = testX;
            return true;
        }
    }

    // Пробуем только Z  
    if (std::abs(move.z) > 0.001f) {
        glm::vec3 testZ = start;
        testZ.z += move.z;
        if (!checkCollisionMesh(testZ)) {
            outPos = testZ;
            return true;
        }
    }

    return false;

}

bool Player::stepSlideMove(float deltaTime, int axis) {
    float moveAmount = 0.0f;
    if (axis == 0) moveAmount = velocity.x * deltaTime;
    else if (axis == 2) moveAmount = velocity.z * deltaTime;
    else return false;

    if (std::abs(moveAmount) < 0.0001f) return true;

    glm::vec3 startPos = position;
    glm::vec3 moveDir(0.0f);
    if (axis == 0) moveDir.x = moveAmount;
    else moveDir.z = moveAmount;

    // 1. Пробуем обычное движение (для склонов - sliding)
    glm::vec3 newPos = startPos + moveDir;

    // Проверяем коллизию
    if (!checkCollisionMesh(newPos)) {
        position = newPos;
        return true;
    }

    // 2. Есть коллизия - пробуем step up ТОЛЬКО если на земле
    if (!onGround) {
        return false; // В воздухе не делаем step
    }

    // HL-style step: вверх на stepHeight
    glm::vec3 stepUp = startPos;
    stepUp.y += stepHeight;

    // Проверяем место над головой
    if (checkCollisionMesh(stepUp)) {
        return false; // Не влезаем
    }

    // Двигаем в сторону на высоте ступеньки
    glm::vec3 stepOver = stepUp;
    stepOver.x += moveDir.x;
    stepOver.z += moveDir.z;

    if (checkCollisionMesh(stepOver)) {
        return false; // Стена на уровне ступеньки
    }

    // Спускаемся вниз на stepHeight (не сканируем!)
    glm::vec3 stepDown = stepOver;
    stepDown.y -= stepHeight;

    // Проверяем, есть ли там пол (одна проверка!)
    if (checkCollisionMesh(stepDown)) {
        // Есть коллизия внизу - значит есть пол, встаём на stepHeight над ним
        // Находим точную высоту одним движением вверх
        glm::vec3 finalPos = stepDown;
        finalPos.y += 0.02f; // Чуть выше пола

        // Проверяем что не застряли
        if (!checkCollisionMesh(finalPos)) {
            position = finalPos;
            onGround = true;
            velocity.y = 0.0f;
            return true;
        }
    }

    // Нет пола под step - пробуем скользить по склону
    // Упрощенно: пробуем позицию между start и end по Y
    glm::vec3 slidePos = startPos + moveDir * 0.5f;
    if (!checkCollisionMesh(slidePos)) {
        position = slidePos;
        return true;
    }

    return false;
}

void Player::resolveCollisionAxis(float deltaTime, int axis) {
    if (axis == 1) {
        // Вертикальная ось
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

    // Горизонтальная ось - НЕ используем stepSlideMove, делаем по-нормальному
    float moveAmount = (axis == 0) ? velocity.x * deltaTime : velocity.z * deltaTime;

    if (std::abs(moveAmount) < 0.0001f) return;

    glm::vec3 moveDir(0.0f);
    if (axis == 0) moveDir.x = moveAmount;
    else moveDir.z = moveAmount;

    glm::vec3 newPos = position + moveDir;

    // Пробуем прямое движение
    if (!checkCollisionMesh(newPos)) {
        position = newPos;
        return;
    }

    // Есть коллизия - пробуем step up (только на земле!)
    if (onGround) {
        // Вверх на stepHeight
        glm::vec3 upPos = position;
        upPos.y += stepHeight;

        if (!checkCollisionMesh(upPos)) {
            // В сторону наверху
            glm::vec3 sidePos = upPos + moveDir;

            if (!checkCollisionMesh(sidePos)) {
                // Вниз до пола (просто -stepHeight, без цикла)
                glm::vec3 downPos = sidePos;
                downPos.y -= stepHeight;

                // Проверяем что внизу есть что-то
                if (checkCollisionMesh(downPos)) {
                    // Ставим чуть выше
                    position = downPos;
                    position.y += 0.02f;
                    return;
                }

                // Пробуем промежуточные позиции (максимум 5 шагов, не 100!)
                for (int i = 1; i <= 5; i++) {
                    glm::vec3 testPos = sidePos;
                    testPos.y -= stepHeight * (i / 5.0f);
                    if (checkCollisionMesh(testPos)) {
                        position = testPos;
                        position.y += 0.02f;
                        onGround = true;
                        velocity.y = 0.0f;
                        return;
                    }
                }
            }
        }
    }

    // Не получилось - останавливаем скорость по этой оси
    if (axis == 0) velocity.x = 0;
    else velocity.z = 0;
}

void Player::CategorizePosition() {
    // HL: Determine if on ground, in air, on ladder, etc.
    if (checkOnGround()) {
        onGround = true;
    }
    else {
        onGround = false;
    }
}

// ========== MAIN UPDATE (HL-STYLE FLOW) ==========

void Player::moveWithCollision(float deltaTime) {
    // HL: Pre-think
    PreThink(deltaTime);

    // HL: Categorize position
    CategorizePosition();

    // HL: Apply movement based on state
    if (onGround) {
        if (m_nBunnyHopFrames < 3) { // 3 кадра окна для perfect bhop
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

    Duck(deltaTime);

    // Apply movement with collision
    bool groundBelow = checkOnGround();
    if (onGround && !groundBelow && velocity.y > 0) {
        onGround = false;
    }

    // Resolve collisions per axis
    resolveCollisionAxis(deltaTime, 0);
    resolveCollisionAxis(deltaTime, 2);
    resolveCollisionAxis(deltaTime, 1);

    // Check ground after movement
    if (!onGround && velocity.y <= 0) {
        if (checkOnGround()) {
            onGround = true;
            velocity.y = 0;
        }
    }

    // HL: Ground snap (prevent micro-bouncing)
    if (onGround && std::abs(velocity.y) < 0.01f) {
        glm::vec3 testPos = position;
        for (int i = 0; i < 15; i++) {
            testPos.y -= 0.01f;
            if (checkCollisionMesh(testPos)) {
                float groundHeight = testPos.y + 0.01f + 0.001f;
                float diff = position.y - groundHeight;
                if (diff > 0.001f && diff < 0.05f) {
                    position.y = groundHeight;
                }
                break;
            }
        }
    }

    // HL: Post-think (fall damage, etc.)
    PostThink(deltaTime);
}

// ========== NOCLIP (unchanged) ==========

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

// ========== MAIN UPDATE ENTRY ==========

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

// ========== CAMERA ==========

glm::vec3 Player::getEyePosition() const {
    // HL: Position + view offset based on duck state
    float viewOffset = GetViewOffset();
    return glm::vec3(position.x, position.y + viewOffset, position.z);
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