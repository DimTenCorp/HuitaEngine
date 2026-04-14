#include "pch.h"
#include "DoorEntity.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

CFuncDoor::CFuncDoor() 
    : bounds(AABB(glm::vec3(-32.0f, 0.0f, -32.0f), glm::vec3(32.0f, 64.0f, 32.0f))),
      originalBounds(bounds),
      moveDirection(0.0f, 1.0f, 0.0f),
      moveDistance(64.0f),
      speed(100.0f),
      lip(8.0f),
      openWaitTime(2.0f),
      currentPos(0.0f),
      state(DOOR_CLOSED),
      stateTimer(0.0f),
      touchTriggered(false) {
    // По умолчанию дверь имеет размер 64x64x64 единиц
    // и открывается вверх (по оси Y в нашей системе координат)
}

void CFuncDoor::initFromProperties(const std::unordered_map<std::string, std::string>& props, const AABB& modelBounds) {
    // Инициализируем границы двери из модели
    bounds = modelBounds;
    originalBounds = modelBounds;
    
    // Вычисляем центр для позиции сущности
    glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
    setPosition(center);
    
    // Читаем параметры из BSP сущности
    // Speed - скорость открытия двери (units per second)
    auto it = props.find("speed");
    if (it != props.end()) {
        try {
            speed = std::stof(it->second);
        } catch (...) {
            speed = 100.0f; // Значение по умолчанию
        }
    } else {
        speed = 100.0f;
    }
    
    // Lip - расстояние которое дверь остается закрытой (в Half-Life это выступ)
    it = props.find("lip");
    if (it != props.end()) {
        try {
            lip = std::stof(it->second);
        } catch (...) {
            lip = 8.0f; // Значение по умолчанию
        }
    } else {
        lip = 8.0f;
    }
    
    // Pitch Yaw Roll (Y Z X) angles - направление открытия двери
    // В Half-Life angles задаются как "pitch yaw roll" (Y Z X)
    glm::vec3 angles(0.0f, 0.0f, 0.0f);
    it = props.find("angles");
    if (it != props.end()) {
        int parsed = sscanf(it->second.c_str(), "%f %f %f", &angles.y, &angles.z, &angles.x);
        if (parsed != 3) {
            std::cerr << "[DOOR] Failed to parse angles: \"" << it->second << "\"\n";
            angles = glm::vec3(0.0f, 0.0f, 0.0f);
        }
    }
    
    // Конвертируем углы Half-Life в нашу систему координат
    // Half-Life: X=forward, Y=left, Z=up
    // Наша система: X=-HL_X, Y=HL_Z, Z=HL_Y
    // Углы: pitch (X), yaw (Y), roll (Z)
    float pitch = glm::radians(angles.x); // Roll в HL -> X rotation у нас
    float yaw = glm::radians(angles.y);   // Pitch в HL -> Y rotation у нас  
    float roll = glm::radians(angles.z);  // Yaw в HL -> Z rotation у нас
    
    // Создаем матрицу вращения из углов Эйлера
    // В Half-Life дверь обычно открывается вдоль своей локальной оси
    // По умолчанию дверь открывается вверх (по мировой Y)
    // Но если заданы angles, то открываться должна вдоль локальной оси после поворота
    
    // Для func_door в Half-Life angles определяют направление движения двери
    // Если angles не заданы, дверь открывается перпендикулярно своей плоскости
    
    // Вычисляем направление движения на основе углов
    // В Half-Life door движется вдоль своей локальной оси после применения углов
    glm::vec3 baseDirection(0.0f, 1.0f, 0.0f); // Базовое направление - вверх
    
    // Применяем вращение к базовому направлению
    glm::mat3 rotation = glm::mat3_cast(glm::quat(roll, glm::vec3(0, 0, 1)) * 
                                        glm::quat(yaw, glm::vec3(0, 1, 0)) * 
                                        glm::quat(pitch, glm::vec3(1, 0, 0)));
    
    moveDirection = glm::normalize(rotation * baseDirection);
    
    // Вычисляем расстояние открытия на основе размеров двери
    // В Half-Life дверь открывается на свою высоту минус lip
    float doorHeight = bounds.max.y - bounds.min.y;
    float doorWidth = std::max(bounds.max.x - bounds.min.x, bounds.max.z - bounds.min.z);
    
    // Определяем направление открытия по умолчанию на основе ориентации двери
    // Если дверь широкая по X/Z и низкая по Y - она вероятно открывается вверх
    // Если дверь высокая по Y - она вероятно открывается в сторону
    
    // Проверяем angles для определения направления
    bool hasAngles = (angles.x != 0.0f || angles.y != 0.0f || angles.z != 0.0f);
    
    if (!hasAngles) {
        // Нет углов - определяем направление автоматически
        // Дверь открывается перпендикулярно своей плоскости
        
        // Находим наименьшую грань - это толщина двери
        float dx = bounds.max.x - bounds.min.x;
        float dy = bounds.max.y - bounds.min.y;
        float dz = bounds.max.z - bounds.min.z;
        
        if (dx <= dy && dx <= dz) {
            // Дверь тонкая по X - открывается вдоль X
            moveDirection = glm::vec3(1.0f, 0.0f, 0.0f);
            moveDistance = dx - lip;
        } else if (dy <= dx && dy <= dz) {
            // Дверь тонкая по Y - открывается вдоль Y (вверх)
            moveDirection = glm::vec3(0.0f, 1.0f, 0.0f);
            moveDistance = dy - lip;
        } else {
            // Дверь тонкая по Z - открывается вдоль Z
            moveDirection = glm::vec3(0.0f, 0.0f, 1.0f);
            moveDistance = dz - lip;
        }
    } else {
        // Есть angles - используем вычисленное направление
        // Расстояние открытия - максимальный размер двери минус lip
        moveDistance = std::max({doorHeight, doorWidth, 
                                bounds.max.x - bounds.min.x}) - lip;
    }
    
    // Убеждаемся что расстояние положительное
    if (moveDistance < 0.0f) {
        moveDistance = 0.0f;
    }
    
    std::cout << "[func_door] Created door: mins(" 
              << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z 
              << ") maxs(" << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z 
              << ") speed=" << speed << " lip=" << lip
              << " direction(" << moveDirection.x << ", " << moveDirection.y << ", " << moveDirection.z 
              << ") distance=" << moveDistance << std::endl;
}

void CFuncDoor::Update(float deltaTime) {
    switch (state) {
        case DOOR_CLOSED:
            // Дверь закрыта, ждем触发ления
            touchTriggered = false;
            break;
            
        case DOOR_OPENING:
            // Открываем дверь
            {
                float deltaMove = speed * deltaTime / moveDistance;
                if (moveDistance <= 0.0f) {
                    deltaMove = 1.0f;
                }
                
                currentPos += deltaMove;
                if (currentPos >= 1.0f) {
                    currentPos = 1.0f;
                    state = DOOR_OPEN;
                    stateTimer = openWaitTime;
                }
                
                // Обновляем позицию сущности
                glm::vec3 offset = moveDirection * (currentPos * moveDistance);
                setPosition(originalBounds.min + offset + (originalBounds.max - originalBounds.min) * 0.5f);
            }
            break;
            
        case DOOR_OPEN:
            // Дверь открыта, ждем времени до закрытия
            stateTimer -= deltaTime;
            if (stateTimer <= 0.0f) {
                state = DOOR_CLOSING;
            }
            break;
            
        case DOOR_CLOSING:
            // Закрываем дверь
            {
                float deltaMove = speed * deltaTime / moveDistance;
                if (moveDistance <= 0.0f) {
                    deltaMove = 1.0f;
                }
                
                currentPos -= deltaMove;
                if (currentPos <= 0.0f) {
                    currentPos = 0.0f;
                    state = DOOR_CLOSED;
                    // Сбрасываем позицию
                    glm::vec3 center = (originalBounds.min + originalBounds.max) * 0.5f;
                    setPosition(center);
                }
                
                // Обновляем позицию сущности
                glm::vec3 offset = moveDirection * (currentPos * moveDistance);
                setPosition(originalBounds.min + offset + (originalBounds.max - originalBounds.min) * 0.5f);
            }
            break;
    }
}

void CFuncDoor::Render() {
    // Дверь рендерится как часть BSP карты
    // Этот метод может быть использован для отладки или специальных эффектов
}

bool CFuncDoor::intersectsCapsule(const Capsule& capsule) const {
    // Получаем текущие границы двери с учетом смещения
    AABB currentBounds = getCurrentBounds();
    
    // Проверка пересечения капсулы с AABB двери
    AABB capsuleBounds = capsule.getBounds();
    
    if (capsuleBounds.max.x < currentBounds.min.x || capsuleBounds.min.x > currentBounds.max.x) return false;
    if (capsuleBounds.max.y < currentBounds.min.y || capsuleBounds.min.y > currentBounds.max.y) return false;
    if (capsuleBounds.max.z < currentBounds.min.z || capsuleBounds.min.z > currentBounds.max.z) return false;
    
    // Более точная проверка - проверяем обе сферы капсулы
    auto pointInAABB = [&currentBounds](const glm::vec3& p) -> bool {
        return p.x >= currentBounds.min.x && p.x <= currentBounds.max.x &&
               p.y >= currentBounds.min.y && p.y <= currentBounds.max.y &&
               p.z >= currentBounds.min.z && p.z <= currentBounds.max.z;
    };
    
    if (pointInAABB(capsule.a)) return true;
    if (pointInAABB(capsule.b)) return true;
    
    return true;
}

AABB CFuncDoor::getCurrentBounds() const {
    // Вычисляем смещение двери на основе текущего состояния
    glm::vec3 offset = moveDirection * (currentPos * moveDistance);
    
    AABB currentBounds;
    currentBounds.min = originalBounds.min + offset;
    currentBounds.max = originalBounds.max + offset;
    
    return currentBounds;
}

void CFuncDoor::triggerOpen() {
    if (state == DOOR_CLOSED || state == DOOR_CLOSING) {
        state = DOOR_OPENING;
        currentPos = 0.0f;
        std::cout << "[func_door] Door triggered to open\n";
    }
}
