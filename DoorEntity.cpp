#include "pch.h"
#include "DoorEntity.h"
#include "BSPLoader.h"
#include <iostream>
#include <algorithm>
#include <cmath>

// Half-Life 1 door spawnflags
namespace DoorFlags {
    constexpr unsigned int STARTS_OPEN = 1;
    constexpr unsigned int DOOR_DONT_LINK = 2;
    constexpr unsigned int PASSABLE = 4;
    constexpr unsigned int TOGGLE = 32;
    constexpr unsigned int USE_ONLY = 256;
    constexpr unsigned int NO_MONSTERS = 512;
    constexpr unsigned int TOUCH_OPENS = 1024;
    constexpr unsigned int START_LOCKED = 2048;
    constexpr unsigned int SILENT = 4096;
}

DoorEntity::DoorEntity()
    : type(DoorType::SLIDING)
    , state(DoorState::CLOSED)
    , origin(0.0f)
    , angles(0.0f)
    , mins(0.0f)
    , maxs(0.0f)
    , currentMins(0.0f)
    , currentMaxs(0.0f)
    , pos1(0.0f)
    , pos2(0.0f)
    , currentPos(0.0f)
    , moveDir(0.0f, 0.0f, 1.0f)
    , moveDistance(0.0f)
    , speed(100.0f)
    , wait(3.0f)
    , lip(8.0f)
    , dmg(2.0f)
    , health(0)
    , spawnFlags(0)
    , locked(false)
    , moveProgress(0.0f)
    , nextCloseTime(0.0f)
    , touchLogged(false)
    , touched(false)
    , currentAngle(0.0f)
{
}

DoorEntity::~DoorEntity() {
}

void DoorEntity::initFromEntity(const BSPEntity& entity, const BSPLoader& bsp) {
    className = entity.classname;
    origin = entity.origin;
    // HL angles: X=pitch, Y=yaw, Z=roll
    angles = entity.angles;

    // Определяем тип двери
    if (className == "func_door_rotating") {
        type = DoorType::ROTATING;
    }
    else {
        type = DoorType::SLIDING;
    }

    // Читаем свойства из BSP
    auto it = entity.properties.find("targetname");
    if (it != entity.properties.end()) targetName = it->second;

    it = entity.properties.find("spawnflags");
    if (it != entity.properties.end()) {
        try { spawnFlags = static_cast<unsigned int>(std::stoul(it->second)); }
        catch (...) {}
    }

    it = entity.properties.find("speed");
    if (it != entity.properties.end()) {
        try { speed = std::stof(it->second); }
        catch (...) {}
        if (speed <= 0) speed = 100.0f;
    }

    it = entity.properties.find("wait");
    if (it != entity.properties.end()) {
        try { wait = std::stof(it->second); }
        catch (...) {}
    }

    it = entity.properties.find("lip");
    if (it != entity.properties.end()) {
        try { lip = std::stof(it->second); }
        catch (...) {}
    }

    it = entity.properties.find("dmg");
    if (it != entity.properties.end()) {
        try { dmg = std::stof(it->second); }
        catch (...) {}
    }

    it = entity.properties.find("health");
    if (it != entity.properties.end()) {
        try { health = std::stoi(it->second); }
        catch (...) {}
    }

    locked = hasFlag(DoorFlags::START_LOCKED);

    // Вычисляем bounds из модели
    it = entity.properties.find("model");
    if (it != entity.properties.end() && !it->second.empty() && it->second[0] == '*') {
        try {
            int modelIndex = std::stoi(it->second.substr(1));
            calculateBoundsFromModel(modelIndex, bsp);
        }
        catch (...) {
            mins = glm::vec3(-16.0f);
            maxs = glm::vec3(16.0f);
        }
    }
    else {
        mins = glm::vec3(-16.0f);
        maxs = glm::vec3(16.0f);
    }

    // === РАСЧЕТ НАПРАВЛЕНИЯ ДВИЖЕНИЯ (как в HL1) ===
    
    pos1 = origin;
    currentPos = pos1;

    if (type == DoorType::SLIDING) {
        // Размеры двери
        glm::vec3 size = maxs - mins;
        
        // В HL1 углы: X=pitch, Y=yaw, Z=roll
        // Для func_door используется Y (yaw) для определения направления
        float yaw = angles.y;
        
        // Нормализуем угол к диапазону [0, 360)
        while (yaw < 0) yaw += 360.0f;
        while (yaw >= 360.0f) yaw -= 360.0f;

        // HL1 логика определения направления по углу:
        // -1 = вниз (отрицательный Z)
        // 90 = вправо (положительный X)  
        // 180 = вперед (отрицательный Y в HL, положительный Z у нас)
        // 270 = влево (отрицательный X)
        // По умолчанию (0 или нет угла) = вверх (положительный Y у нас)

        // Конвертация координат HL -> наши:
        // HL: X=вправо, Y=вперед, Z=вверх
        // Наши: X=влево, Y=вверх, Z=вперед
        
        if (yaw == 270.0f || (yaw > 265.0f && yaw < 275.0f)) {
            // -90 градусов = движение влево (-X в наших координатах)
            moveDir = glm::vec3(-1.0f, 0.0f, 0.0f);
            moveDistance = size.x - lip;
        }
        else if (yaw == 90.0f || (yaw > 85.0f && yaw < 95.0f)) {
            // 90 градусов = движение вправо (+X в наших координатах)
            moveDir = glm::vec3(1.0f, 0.0f, 0.0f);
            moveDistance = size.x - lip;
        }
        else if (yaw == 180.0f || (yaw > 175.0f && yaw < 185.0f)) {
            // 180 градусов = движение назад/вперед по Z
            moveDir = glm::vec3(0.0f, 0.0f, -1.0f);
            moveDistance = size.z - lip;
        }
        else if (yaw == 0.0f || yaw == 360.0f || (yaw < 5.0f || yaw > 355.0f)) {
            // 0 градусов = движение вверх по Y (наша система)
            moveDir = glm::vec3(0.0f, 1.0f, 0.0f);
            moveDistance = size.y - lip;
        }
        else {
            // По умолчанию - вверх (Y+)
            moveDir = glm::vec3(0.0f, 1.0f, 0.0f);
            moveDistance = size.y - lip;
        }

        // Защита от отрицательного расстояния
        if (moveDistance < 0.0f) moveDistance = 0.0f;

        pos2 = pos1 + moveDir * moveDistance;

        std::cout << "[DOOR] Sliding '" << targetName << "': origin=" << origin.x << "," << origin.y << "," << origin.z
            << " angles=" << angles.x << "," << angles.y << "," << angles.z
            << " dir=" << moveDir.x << "," << moveDir.y << "," << moveDir.z
            << " dist=" << moveDistance << " speed=" << speed << " lip=" << lip << std::endl;
    }
    else {
        // ROTATING DOOR
        // Для вращающихся дверей angle определяет угол вращения
        it = entity.properties.find("distance");
        if (it != entity.properties.end()) {
            try { moveDistance = std::stof(it->second); }
            catch (...) { moveDistance = 90.0f; }
        }
        else {
            // Используем Z угол (roll) как угол вращения по умолчанию
            moveDistance = angles.z;
            if (moveDistance == 0) moveDistance = 90.0f;
        }

        std::cout << "[DOOR] Rotating '" << targetName << "': origin=" << origin.x << "," << origin.y << "," << origin.z
            << " angle=" << moveDistance << " speed=" << speed << std::endl;
    }

    // STARTS_OPEN - дверь начинается открытой
    if (hasFlag(DoorFlags::STARTS_OPEN)) {
        if (type == DoorType::SLIDING) {
            std::swap(pos1, pos2);
            moveDir = -moveDir;
        }
        state = DoorState::OPEN;
        currentPos = pos1;
        moveProgress = 1.0f;

        if (type == DoorType::ROTATING) {
            currentAngle = moveDistance;
        }

        updatePosition(1.0f);
    }
    else {
        state = DoorState::CLOSED;
        moveProgress = 0.0f;
        currentAngle = 0.0f;
        updatePosition(0.0f);
    }
}

void DoorEntity::calculateBoundsFromModel(int modelIndex, const BSPLoader& bsp) {
    const auto& models = bsp.getModels();
    if (modelIndex < 0 || modelIndex >= (int)models.size()) {
        mins = glm::vec3(-32.0f);
        maxs = glm::vec3(32.0f);
        return;
    }

    const BSPModel& model = models[modelIndex];

    // Конвертация HL координат в наши
    // HL: X=вправо, Y=вперёд, Z=вверх
    // Мы: X=влево, Y=вверх, Z=вперёд

    float hlMinX = model.min[0], hlMinY = model.min[1], hlMinZ = model.min[2];
    float hlMaxX = model.max[0], hlMaxY = model.max[1], hlMaxZ = model.max[2];

    glm::vec3 worldMin(-hlMaxX, hlMinZ, hlMinY);
    glm::vec3 worldMax(-hlMinX, hlMaxZ, hlMaxY);

    for (int i = 0; i < 3; i++) {
        if (worldMin[i] > worldMax[i]) std::swap(worldMin[i], worldMax[i]);
    }

    mins = worldMin;
    maxs = worldMax;

    const float epsilon = 0.5f;
    mins -= glm::vec3(epsilon);
    maxs += glm::vec3(epsilon);

    currentMins = mins;
    currentMaxs = maxs;
}

bool DoorEntity::intersectsCapsule(const Capsule& capsule) const {
    AABB doorAABB;
    doorAABB.min = currentMins;
    doorAABB.max = currentMaxs;

    AABB capsuleAABB = capsule.getBounds();

    // Быстрая проверка AABB
    if (capsuleAABB.max.x < doorAABB.min.x || capsuleAABB.min.x > doorAABB.max.x ||
        capsuleAABB.max.y < doorAABB.min.y || capsuleAABB.min.y > doorAABB.max.y ||
        capsuleAABB.max.z < doorAABB.min.z || capsuleAABB.min.z > doorAABB.max.z) {
        return false;
    }

    // Точная проверка - расширяем AABB на радиус капсулы
    glm::vec3 capsuleCenter = capsule.getCenter();

    return (capsuleCenter.x >= doorAABB.min.x - capsule.radius &&
        capsuleCenter.x <= doorAABB.max.x + capsule.radius &&
        capsuleCenter.y >= doorAABB.min.y - capsule.radius &&
        capsuleCenter.y <= doorAABB.max.y + capsule.radius &&
        capsuleCenter.z >= doorAABB.min.z - capsule.radius &&
        capsuleCenter.z <= doorAABB.max.z + capsule.radius);
}

glm::mat4 DoorEntity::getRenderTransform() const {
    if (type == DoorType::SLIDING) {
        // Сдвиг на разницу между текущей и оригинальной позицией
        glm::vec3 offset = currentPos - origin;
        return glm::translate(glm::mat4(1.0f), offset);
    }
    else {
        // Вращение вокруг origin по Y
        float angleRad = glm::radians(currentAngle);

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, origin);
        transform = glm::rotate(transform, angleRad, glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::translate(transform, -origin);

        return transform;
    }
}

void DoorEntity::updatePosition(float progress) {
    if (type == DoorType::SLIDING) {
        // Интерполируем позицию origin
        currentPos = glm::mix(pos1, pos2, progress);

        // Обновляем bounds - смещаем на разницу позиций
        glm::vec3 offset = currentPos - origin;
        currentMins = mins + offset;
        currentMaxs = maxs + offset;

    }
    else {
        // Rotating
        currentAngle = progress * moveDistance;
        updateRotatedBounds();
    }
}

void DoorEntity::updateRotatedBounds() {
    float angleRad = glm::radians(currentAngle);
    float cosA = cos(angleRad);
    float sinA = sin(angleRad);

    glm::vec3 corners[8] = {
        glm::vec3(mins.x, mins.y, mins.z),
        glm::vec3(maxs.x, mins.y, mins.z),
        glm::vec3(mins.x, maxs.y, mins.z),
        glm::vec3(maxs.x, maxs.y, mins.z),
        glm::vec3(mins.x, mins.y, maxs.z),
        glm::vec3(maxs.x, mins.y, maxs.z),
        glm::vec3(mins.x, maxs.y, maxs.z),
        glm::vec3(maxs.x, maxs.y, maxs.z)
    };

    glm::vec3 newMin(FLT_MAX), newMax(-FLT_MAX);

    for (int i = 0; i < 8; i++) {
        // Локальные координаты относительно origin
        glm::vec3 local = corners[i] - origin;

        // Вращаем вокруг Y
        float newX = local.x * cosA - local.z * sinA;
        float newZ = local.x * sinA + local.z * cosA;

        glm::vec3 rotated(newX, local.y, newZ);
        glm::vec3 world = origin + rotated;

        newMin = glm::min(newMin, world);
        newMax = glm::max(newMax, world);
    }

    currentMins = newMin;
    currentMaxs = newMax;
}

bool DoorEntity::tryActivate(float touchDistance, bool isPlayerUse) {
    if (locked) {
        if (!touchLogged) {
            std::cout << "[DOOR] '" << targetName << "' is LOCKED!" << std::endl;
            touchLogged = true;
        }
        return false;
    }

    if (isUseOnly() && !isPlayerUse) return false;
    if (!isTouchOpens() && !isPlayerUse && touchDistance >= 1.0f) return false;

    if (state == DoorState::CLOSED || state == DoorState::CLOSING) {
        open();
        return true;
    }
    else if (state == DoorState::OPEN && hasFlag(DoorFlags::TOGGLE)) {
        close();
        return true;
    }

    return false;
}

void DoorEntity::open() {
    if (state == DoorState::OPENING || state == DoorState::OPEN) return;

    std::cout << "[DOOR] Opening '" << targetName << "' (" << className << ")" << std::endl;
    state = DoorState::OPENING;
}

void DoorEntity::close() {
    if (state == DoorState::CLOSING || state == DoorState::CLOSED) return;

    std::cout << "[DOOR] Closing '" << targetName << "' (" << className << ")" << std::endl;
    state = DoorState::CLOSING;
}

void DoorEntity::stop() {
    if (state == DoorState::OPENING) {
        state = DoorState::OPEN;
        moveProgress = 1.0f;

        if (wait > 0) {
            nextCloseTime = glfwGetTime() + wait;
        }
        else if (wait < 0) {
            nextCloseTime = -1.0f;  // Никогда не закрываться
        }
        else {
            nextCloseTime = 0.0f;   // Закрыться сразу
        }

    }
    else if (state == DoorState::CLOSING) {
        state = DoorState::CLOSED;
        moveProgress = 0.0f;
    }

    updatePosition(moveProgress);
}

void DoorEntity::update(float deltaTime) {
    float now = glfwGetTime();

    // === ОТКРЫВАЕМСЯ ===
    if (state == DoorState::OPENING) {
        if (type == DoorType::SLIDING) {
            float remaining = glm::distance(currentPos, pos2);
            float moveStep = speed * deltaTime;

            if (moveStep >= remaining) {
                // Дошли до конца
                currentPos = pos2;
                moveProgress = 1.0f;
                updatePosition(1.0f);
                stop();
                std::cout << "[DOOR] '" << targetName << "' fully opened" << std::endl;
            }
            else {
                // Движемся
                glm::vec3 dir = glm::normalize(pos2 - currentPos);
                currentPos += dir * moveStep;

                float totalDist = glm::distance(pos1, pos2);
                moveProgress = totalDist > 0.001f ?
                    glm::distance(pos1, currentPos) / totalDist : 1.0f;

                updatePosition(moveProgress);
            }

        }
        else {
            // Rotating
            float angleStep = speed * deltaTime;
            float targetAngle = moveDistance;
            float newAngle = currentAngle + angleStep;

            if (newAngle >= targetAngle) {
                newAngle = targetAngle;
                moveProgress = 1.0f;
                currentAngle = newAngle;
                updatePosition(1.0f);
                stop();
                std::cout << "[DOOR] '" << targetName << "' fully opened (angle=" << newAngle << ")" << std::endl;
            }
            else {
                currentAngle = newAngle;
                moveProgress = currentAngle / moveDistance;
                updatePosition(moveProgress);
            }
        }
    }
    // === ЗАКРЫВАЕМСЯ ===
    else if (state == DoorState::CLOSING) {
        if (type == DoorType::SLIDING) {
            float remaining = glm::distance(currentPos, pos1);
            float moveStep = speed * deltaTime;

            if (moveStep >= remaining) {
                currentPos = pos1;
                moveProgress = 0.0f;
                updatePosition(0.0f);
                stop();
                std::cout << "[DOOR] '" << targetName << "' fully closed" << std::endl;
            }
            else {
                glm::vec3 dir = glm::normalize(pos1 - currentPos);
                currentPos += dir * moveStep;

                float totalDist = glm::distance(pos1, pos2);
                moveProgress = totalDist > 0.001f ?
                    glm::distance(pos1, currentPos) / totalDist : 0.0f;

                updatePosition(moveProgress);
            }

        }
        else {
            // Rotating
            float angleStep = speed * deltaTime;
            float newAngle = currentAngle - angleStep;

            if (newAngle <= 0.0f) {
                newAngle = 0.0f;
                moveProgress = 0.0f;
                currentAngle = 0.0f;
                updatePosition(0.0f);
                stop();
                std::cout << "[DOOR] '" << targetName << "' fully closed" << std::endl;
            }
            else {
                currentAngle = newAngle;
                moveProgress = currentAngle / moveDistance;
                updatePosition(moveProgress);
            }
        }
    }
    // === ОТКРЫТА - проверяем автозакрытие ===
    else if (state == DoorState::OPEN) {
        if (wait > 0 && nextCloseTime > 0 && now >= nextCloseTime) {
            close();
        }
    }

    // Сброс лога касания
    static float lastLogReset = 0;
    if (touchLogged && now - lastLogReset > 2.0f) {
        touchLogged = false;
        lastLogReset = now;
    }
}

AABB DoorEntity::getBounds() const {
    AABB bounds;
    bounds.min = currentMins;
    bounds.max = currentMaxs;
    return bounds;
}