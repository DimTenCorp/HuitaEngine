#include "Player.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "TriangleCollider.h"

// Вспомогательная функция для вычисления длины вектора
static inline float vec3Length(const glm::vec3& v) {
    return glm::length(v);
}

// Вспомогательная функция для нормализации вектора с возвратом длины
static inline float vec3Normalize(glm::vec3& v) {
    float len = glm::length(v);
    if (len > 0.0f) {
        v /= len;
    }
    return len;
}

// Вспомогательная функция для скалярного произведения
static inline float vec3Dot(const glm::vec3& a, const glm::vec3& b) {
    return glm::dot(a, b);
}

Player::Player() {
    position = glm::vec3(0.0f, 2.0f, 0.0f);
    velocity = glm::vec3(0.0f);
    wishDir = glm::vec3(0.0f);
    size = glm::vec3(0.3f, 0.9f, 0.3f);

    onGround = false;
    speed = 6.0f;
    maxSpeed = 8.0f;          // Quake: sv_maxspeed 320 (~8 m/s)
    jumpForce = 8.0f;
    gravity = 25.0f;          // Quake: sv_gravity 800 (~25 m/s²)
    friction = 4.0f;          // Quake: sv_friction 4
    stopSpeed = 3.0f;         // Quake: sv_stopspeed 100 (~3 m/s)
    accelRate = 10.0f;        // Quake: sv_accelerate 10
    airAccelRate = 1.0f;      // Quake: воздух имеет меньшее ускорение

    noclipMode = false;
    noclipSpeed = 12.0f;

    yaw = -90.0f;
    pitch = 0.0f;
    
    stepHeight = STEPSIZE;
    baseVelocity = glm::vec3(0.0f);
    groundEntity = -1;
}

// ============================================================================
// CLIP VELOCITY - скольжение вдоль плоскости столкновения
// ============================================================================
void Player::clipVelocity(const glm::vec3& in, const glm::vec3& normal, glm::vec3& out, float overbounce) {
    float backoff = vec3Dot(in, normal) * overbounce;
    
    for (int i = 0; i < 3; i++) {
        out[i] = in[i] - normal[i] * backoff;
        // Обрезаем очень маленькие значения до нуля
        if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON) {
            out[i] = 0.0f;
        }
    }
}

// ============================================================================
// ACCELERATE - ускорение на земле (Quake SV_Accelerate)
// ============================================================================
void Player::applyAcceleration(float wishspeed, const glm::vec3& wishdir) {
    float currentspeed = vec3Dot(velocity, wishdir);
    float addspeed = wishspeed - currentspeed;
    
    if (addspeed <= 0.0f) return;
    
    float accelspeed = accelRate * wishspeed;
    if (accelspeed > addspeed) {
        accelspeed = addspeed;
    }
    
    velocity += wishdir * accelspeed;
}

// ============================================================================
// AIR ACCELERATE - ускорение в воздухе (Quake SV_AirAccelerate)
// ============================================================================
void Player::applyAirAcceleration(float wishspeed, const glm::vec3& wishveloc) {
    glm::vec3 wishvel = wishveloc;
    float wishspd = vec3Normalize(wishvel);
    
    // Ограничиваем желаемую скорость
    if (wishspd > 30.0f) wishspd = 30.0f;
    
    float currentspeed = vec3Dot(velocity, wishvel);
    float addspeed = wishspd - currentspeed;
    if (addspeed <= 0.0f) return;
    
    float accelspeed = airAccelRate * wishspeed;
    if (accelspeed > addspeed) {
        accelspeed = addspeed;
    }
    
    velocity += wishvel * accelspeed;
}

// ============================================================================
// USER FRICTION - трение игрока (Quake SV_UserFriction)
// ============================================================================
void Player::userFriction() {
    float speed = vec3Length(glm::vec3(velocity.x, velocity.y, 0.0f));
    if (speed < 0.01f) return;
    
    // Проверка края платформы - увеличенное трение если стоим у края
    float frictionValue = friction;
    glm::vec3 start = position + glm::normalize(glm::vec3(velocity.x, velocity.y, 0.0f)) * 0.16f;
    glm::vec3 stop = start + glm::vec3(0.0f, 0.0f, -0.34f);
    
    // Упрощённая проверка - если нет коллизии под ногами впереди, увеличиваем трение
    if (!checkCollisionMesh(stop)) {
        frictionValue *= 2.0f;  // edgefriction
    }
    
    float control = (speed < stopSpeed) ? stopSpeed : speed;
    float newspeed = speed - control * frictionValue;
    
    if (newspeed < 0.0f) newspeed = 0.0f;
    newspeed /= speed;
    
    velocity.x *= newspeed;
    velocity.y *= newspeed;
    velocity.z *= newspeed;
}


// ============================================================================
// FLY MOVE - базовое движение с обработкой коллизий (Quake SV_FlyMove)
// ============================================================================
int Player::flyMove(float deltaTime) {
    int bumpcount = 0;
    int numplanes = 0;
    glm::vec3 planes[MAX_CLIP_PLANES];
    glm::vec3 primalVelocity = velocity;
    float timeLeft = deltaTime;
    int blocked = 0;
    
    for (bumpcount = 0; bumpcount < NUM_MOVE_BUMPS; bumpcount++) {
        // Если скорость нулевая, выходим
        if (glm::length(velocity) < 0.001f) {
            break;
        }
        
        // Вычисляем конечную точку движения
        glm::vec3 end = position + velocity * timeLeft;
        
        // Простая проверка коллизий - двигаем игрока и проверяем
        bool collision = checkCollisionMesh(end);
        
        if (collision) {
            // Столкнулись - определяем нормаль плоскости по оси наибольшего проникновения
            glm::vec3 penetration = end - position;
            glm::vec3 normal(0.0f);
            
            // Определяем ось с наибольшим проникновением
            float absX = std::abs(penetration.x);
            float absY = std::abs(penetration.y);
            float absZ = std::abs(penetration.z);
            
            if (absX > absY && absX > absZ) {
                normal.x = (penetration.x > 0) ? -1.0f : 1.0f;
                blocked |= 2;  // стена
            } else if (absY > absZ) {
                normal.y = (penetration.y > 0) ? -1.0f : 1.0f;
                if (normal.y > 0.7f) blocked |= 1;  // пол
            } else {
                normal.z = (penetration.z > 0) ? -1.0f : 1.0f;
                blocked |= 2;  // стена
            }
            
            // Скользим вдоль плоскости
            glm::vec3 newVelocity;
            clipVelocity(velocity, normal, newVelocity, 1.0f);
            
            // Проверяем все накопленные плоскости
            for (int i = 0; i < numplanes; i++) {
                if (vec3Dot(newVelocity, planes[i]) < 0.0f) {
                    // Новая скорость против другой плоскости - продолжаем
                    goto continue_clipping;
                }
            }
            
            velocity = newVelocity;
            
            continue_clipping:;
            
            // Добавляем плоскость в список
            if (numplanes >= MAX_CLIP_PLANES) {
                velocity = glm::vec3(0.0f);
                return 3;
            }
            planes[numplanes++] = normal;
            
            // Если скорость стала противоположной исходной, останавливаемся
            if (vec3Dot(velocity, primalVelocity) <= 0.0f) {
                velocity = glm::vec3(0.0f);
                return blocked;
            }
        } else {
            // Нет коллизии - двигаемся свободно
            position = end;
            break;
        }
        
        timeLeft *= 0.5f;  // Уменьшаем оставшееся время для следующей итерации
    }
    
    return blocked;
}

// ============================================================================
// WALK MOVE - движение по земле с лестницами (Quake SV_WalkMove)
// ============================================================================
void Player::walkMove(float deltaTime) {
    glm::vec3 oldOrg = position;
    glm::vec3 oldVel = velocity;
    
    // Пробуем обычное движение
    int clip = flyMove(deltaTime);
    
    // Если не столкнулись со стеной, выходим
    if (!(clip & 2)) {
        return;
    }
    
    // Проверяем, были ли на земле перед движением
    glm::vec3 groundTest = oldOrg;
    groundTest.y -= 0.1f;
    bool wasOnGround = checkCollisionMesh(groundTest);
    
    // Если не были на земле, не идём по лестнице
    if (!wasOnGround) {
        return;
    }
    
    // Сохраняем состояние перед попыткой лестницы
    glm::vec3 noStepOrg = position;
    glm::vec3 noStepVel = velocity;
    
    // Возвращаемся назад и пробуем лестницу
    position = oldOrg;
    velocity = oldVel;
    
    // Двигаемся вверх на шаг лестницы
    position.y += STEPSIZE;
    
    // Двигаемся вперёд с горизонтальной скоростью
    glm::vec3 horizontalVel(velocity.x, 0.0f, velocity.z);
    velocity = horizontalVel;
    
    clip = flyMove(deltaTime);
    
    // Проверяем, продвинулись ли мы достаточно
    float movedDist = std::abs(oldOrg.x - position.x) + std::abs(oldOrg.z - position.z);
    if (movedDist < 0.03125f) {
        // Не продвинулись - отменяем
        position = noStepOrg;
        velocity = noStepVel;
        return;
    }
    
    // Двигаемся вниз
    position.y -= STEPSIZE;
    
    // Проверяем, приземлились ли хорошо (не слишком низко и не в воздухе)
    if (checkOnGround()) {
        onGround = true;
        velocity.y = 0.0f;
    } else {
        // Плохое приземление - отменяем лестницу
        position = noStepOrg;
        velocity = noStepVel;
    }
}

// ============================================================================
// HANDLE INPUT - обработка ввода для Quake-style движения
// ============================================================================
void Player::handleInput(float deltaTime) {
    if (noclipMode) return;

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) return;

    // Вычисляем направление движения на основе угла поворота камеры
    float yawRad = glm::radians(yaw);
    glm::vec3 forward(cos(yawRad), 0.0f, sin(yawRad));
    glm::vec3 right(sin(yawRad), 0.0f, -cos(yawRad));

    // Считываем ввод
    wishDir = glm::vec3(0.0f);
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir += right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir -= right;

    // Нормализуем направление желания
    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir);
    }

    // Обработка прыжка
    bool spacePressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

    if (spacePressed) {
        if (!jumpKeyWasHeld && onGround) {
            velocity.y = jumpForce;
            onGround = false;
        }
        jumpKeyWasHeld = true;
    } else {
        jumpKeyWasHeld = false;
    }
}

// ============================================================================
// MOVE NOCLIP - полёт сквозь стены
// ============================================================================
void Player::moveNoclip(float deltaTime) {
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) return;

    glm::vec3 wishDir(0.0f);

    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);

    glm::vec3 forward(cos(yawRad) * cos(pitchRad), sin(pitchRad), sin(yawRad) * cos(pitchRad));
    glm::vec3 right(sin(yawRad), 0.0f, -cos(yawRad));
    glm::vec3 up(0.0f, 1.0f, 0.0f);

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
    wishDir = glm::vec3(0.0f);
}

// ============================================================================
// MOVE WITH COLLISION - основное движение с коллизиями (Quake-style)
// ============================================================================
void Player::moveWithCollision(float deltaTime) {
    // 1. Гравитация (используем гравитацию из членов класса)
    if (!onGround) {
        velocity.y -= gravity * deltaTime;
    } else {
        // На земле - применяем трение
        float currentSpeed = glm::length(glm::vec3(velocity.x, 0, velocity.z));
        if (currentSpeed > 0.001f) {
            float drop = currentSpeed * friction * deltaTime;
            float newSpeed = currentSpeed - drop;
            if (newSpeed < 0) newSpeed = 0;
            
            glm::vec3 horizontalVel = glm::normalize(glm::vec3(velocity.x, 0, velocity.z)) * newSpeed;
            velocity.x = horizontalVel.x;
            velocity.z = horizontalVel.z;
        }
    }
    
    // 2. Используем wishDir из handleInput
    glm::vec3 wishDir = this->wishDir;
    float wishSpeed = 0.0f;
    if (glm::length(wishDir) > 0.001f) {
        wishSpeed = maxSpeed;
    }
    
    // 3. Ускорение при движении
    if (wishSpeed > 0) {
        float accel = onGround ? accelRate : airAccelRate;
        float currentSpeed = glm::length(glm::vec3(velocity.x, 0, velocity.z));
        float addSpeed = wishSpeed - currentSpeed;
        
        if (addSpeed > 0) {
            float currentSpeedInWishDir = glm::dot(glm::vec3(velocity.x, 0, velocity.z), wishDir);
            float accelSpeed = accel * wishSpeed * deltaTime;
            if (accelSpeed > addSpeed) accelSpeed = addSpeed;
            
            velocity.x += wishDir.x * accelSpeed;
            velocity.z += wishDir.z * accelSpeed;
        }
    }
    
    // 4. Движение с коллизиями через flyMove/walkMove (Quake-style)
    walkMove(deltaTime);
    
    // 5. Проверка земли после движения
    if (!checkOnGround()) {
        onGround = false;
    } else {
        onGround = true;
        if (velocity.y < 0) velocity.y = 0;
    }
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
    testPos.y -= 0.1f;  // Увеличенный отступ для лучшей проверки земли
    return checkCollisionMesh(testPos);
}

void Player::update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider) {
    yaw = cameraYaw;
    pitch = cameraPitch;
    meshCollider = collider;

    handleInput(deltaTime);

    if (noclipMode) {
        moveNoclip(deltaTime);
    } else {
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