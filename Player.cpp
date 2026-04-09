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
    glm::vec3 originalVelocity = velocity;
    float timeLeft = deltaTime;
    int blocked = 0;
    
    for (bumpcount = 0; bumpcount < NUM_MOVE_BUMPS; bumpcount++) {
        // Если скорость нулевая, выходим
        if (velocity.x == 0.0f && velocity.y == 0.0f && velocity.z == 0.0f) {
            break;
        }
        
        // Вычисляем конечную точку движения
        glm::vec3 end = position + velocity * timeLeft;
        
        // Простая проверка коллизий - двигаем игрока и проверяем
        bool collision = checkCollisionMesh(end);
        
        if (collision) {
            // Столкнулись - нужно определить нормаль плоскости
            // Для простоты используем эвристику: определяем ось наибольшего проникновения
            glm::vec3 penetration = end - position;
            glm::vec3 normal(0.0f);
            
            AABB playerBox = getPlayerAABB(end);
            
            // Проверяем каждую ось для определения нормали
            if (std::abs(penetration.x) > std::abs(penetration.y) && 
                std::abs(penetration.x) > std::abs(penetration.z)) {
                normal.x = (penetration.x > 0) ? -1.0f : 1.0f;
            } else if (std::abs(penetration.y) > std::abs(penetration.z)) {
                normal.y = (penetration.y > 0) ? -1.0f : 1.0f;
                if (normal.y > 0.7f) blocked |= 1;  // пол
            } else {
                normal.z = (penetration.z > 0) ? -1.0f : 1.0f;
                if (normal.z == 0.0f) blocked |= 2;  // стена
            }
            
            // Скользим вдоль плоскости
            glm::vec3 newVelocity;
            clipVelocity(originalVelocity, normal, newVelocity, 1.0f);
            
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
        
        timeLeft -= deltaTime * (1.0f - static_cast<float>(bumpcount) / NUM_MOVE_BUMPS);
    }
    
    return blocked;
}

// ============================================================================
// WALK MOVE - движение по земле с лестницами (Quake SV_WalkMove)
// ============================================================================
void Player::walkMove(float deltaTime) {
    glm::vec3 oldOrg = position;
    glm::vec3 oldVel = velocity;
    
    onGround = false;
    
    // Пробуем обычное движение
    int clip = flyMove(deltaTime);
    
    // Если не столкнулись со стеной/ступенькой, выходим
    if (!(clip & 2)) {
        return;
    }
    
    // Если не были на земле и не в воде, не идём по лестнице
    if (!onGround) {
        return;
    }
    
    // Сохраняем состояние перед попыткой лестницы
    glm::vec3 noStepOrg = position;
    glm::vec3 noStepVel = velocity;
    
    // Возвращаемся назад и пробуем лестницу
    position = oldOrg;
    
    // Двигаемся вверх
    position.y += STEPSIZE;
    
    // Двигаемся вперёд с горизонтальной скоростью
    glm::vec3 horizontalVel(velocity.x, 0.0f, velocity.z);
    velocity = horizontalVel;
    
    clip = flyMove(deltaTime);
    
    // Проверяем, продвинулись ли мы
    if (clip != 0) {
        if (std::abs(oldOrg.x - position.x) < 0.03125f && 
            std::abs(oldOrg.z - position.z) < 0.03125f) {
            // Не продвинулись - отменяем
            position = noStepOrg;
            velocity = noStepVel;
            return;
        }
    }
    
    // Двигаемся вниз
    glm::vec3 downMove(0.0f, -STEPSIZE + oldVel.y * deltaTime, 0.0f);
    position += downMove;
    
    // Проверяем, приземлились ли хорошо
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
    const float gravity = 800.0f;
    const float maxSpeed = 8.0f;
    const float accelerate = 10.0f;
    const float airAccel = 1.0f;
    const float friction = 4.0f;
    const float stepHeight = 0.5f;
    
    // 1. Гравитация
    if (!onGround) {
        velocity.y -= gravity * deltaTime;
    }
    
    // 2. Получаем направление ввода
    glm::vec3 forward = glm::vec3(camera->Front.x, 0.0f, camera->Front.z);
    glm::vec3 right = glm::vec3(camera->Right.x, 0.0f, camera->Right.z);
    
    if (glm::length(forward) > 0.001f) forward = glm::normalize(forward);
    if (glm::length(right) > 0.001f) right = glm::normalize(right);
    
    glm::vec3 wishDir = glm::vec3(0.0f);
    if (keys[GLFW_KEY_W]) wishDir += forward;
    if (keys[GLFW_KEY_S]) wishDir -= forward;
    if (keys[GLFW_KEY_D]) wishDir += right;
    if (keys[GLFW_KEY_A]) wishDir -= right;
    
    float wishSpeed = 0.0f;
    if (glm::length(wishDir) > 0.001f) {
        wishDir = glm::normalize(wishDir);
        wishSpeed = maxSpeed;
    }
    
    // 3. Трение и ускорение
    float currentSpeed = glm::length(glm::vec3(velocity.x, 0, velocity.z));
    float accel = onGround ? accelerate : airAccel;
    
    if (onGround && wishSpeed == 0) {
        // Трение когда стоим
        float drop = currentSpeed * friction * deltaTime;
        float newSpeed = currentSpeed - drop;
        if (newSpeed < 0) newSpeed = 0;
        
        if (currentSpeed > 0.001f) {
            glm::vec3 horizontalVel = glm::vec3(velocity.x, 0, velocity.z);
            horizontalVel = glm::normalize(horizontalVel) * newSpeed;
            velocity.x = horizontalVel.x;
            velocity.z = horizontalVel.z;
        }
    } else if (wishSpeed > 0) {
        // Ускорение при движении
        float addSpeed = wishSpeed - currentSpeed;
        if (addSpeed > 0) {
            float currentSpeedInWishDir = glm::dot(glm::vec3(velocity.x, 0, velocity.z), wishDir);
            float accelSpeed = accel * wishSpeed * deltaTime;
            if (accelSpeed > addSpeed) accelSpeed = addSpeed;
            
            velocity.x += wishDir.x * accelSpeed;
            velocity.z += wishDir.z * accelSpeed;
        }
    }
    
    // 4. Движение с коллизиями (разбиваем на шаги)
    float timeLeft = deltaTime;
    const int maxSteps = 5;
    
    for (int step = 0; step < maxSteps; step++) {
        if (timeLeft <= 0.001f) break;
        
        glm::vec3 moveVec = velocity * timeLeft;
        glm::vec3 desiredPos = position + moveVec;
        
        // Пробуем двигаться
        if (!checkCollisionMesh(desiredPos)) {
            position = desiredPos;
            timeLeft = 0;
        } else {
            // Столкновение - пробуем Step Up
            glm::vec3 stepPos = position;
            stepPos.y += stepHeight;
            
            bool canStep = !checkCollisionMesh(stepPos);
            if (canStep) {
                glm::vec3 stepForwardPos = stepPos + moveVec;
                if (!checkCollisionMesh(stepForwardPos)) {
                    position = stepForwardPos;
                    timeLeft = 0;
                    onGround = true;
                    continue;
                }
            }
            
            // Не смогли шагнуть - это стена, скользим по осям
            float oldVelX = velocity.x;
            float oldVelZ = velocity.z;
            
            // Движение по X
            glm::vec3 testX = position + glm::vec3(moveVec.x, 0, 0);
            if (!checkCollisionMesh(testX)) {
                position.x = testX.x;
            } else {
                velocity.x = 0;
            }
            
            // Движение по Z
            glm::vec3 testZ = position + glm::vec3(0, 0, moveVec.z);
            if (!checkCollisionMesh(testZ)) {
                position.z = testZ.z;
            } else {
                velocity.z = 0;
            }
            
            // Движение по Y
            glm::vec3 testY = position + glm::vec3(0, moveVec.y, 0);
            if (!checkCollisionMesh(testY)) {
                position.y = testY.y;
            } else {
                if (velocity.y < 0) {
                    onGround = true;
                    velocity.y = 0;
                } else {
                    velocity.y = 0;
                }
            }
            
            timeLeft = 0;
        }
    }
    
    // 5. Проверка земли
    glm::vec3 groundTest = position;
    groundTest.y -= 0.1f;
    if (checkCollisionMesh(groundTest)) {
        onGround = true;
        if (velocity.y < 0) velocity.y = 0;
    } else {
        onGround = false;
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
    testPos.y -= 0.01f;
    return checkCollisionMesh(testPos);
}

void Player::resolveCollisionAxis(float deltaTime, int axis) {
    glm::vec3 newPos = position;

    if (axis == 0) newPos.x += velocity.x * deltaTime;
    else if (axis == 1) newPos.y += velocity.y * deltaTime;
    else newPos.z += velocity.z * deltaTime;

    if (checkCollisionMesh(newPos)) {
        if (axis == 1) {
            if (velocity.y > 0) {
                velocity.y = 0;
            }
            else if (velocity.y < 0) {
                onGround = true;
                velocity.y = 0;
            }
        }
        else {
            // Для X и Z осей - просто останавливаем движение
            if (axis == 0) velocity.x = 0;
            else velocity.z = 0;
        }
        return;
    }

    if (axis == 0) position.x = newPos.x;
    else if (axis == 1) position.y = newPos.y;
    else position.z = newPos.z;
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