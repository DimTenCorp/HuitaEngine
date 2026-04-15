#include "pch.h"
#include "DoorEntity.h"
#include "BSPLoader.h"
#include "Player.h"
#include <iostream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

// Реализация OBB::intersectsAABB (SAT - Separating Axis Theorem)
bool OBB::intersectsAABB(const AABB& aabb) const {
    // Преобразуем AABB в OBB с нулевым вращением для унификации
    glm::vec3 aabbCenter = (aabb.min + aabb.max) * 0.5f;
    glm::vec3 aabbHalfExtents = (aabb.max - aabb.min) * 0.5f;

    // Вектор между центрами
    glm::vec3 d = aabbCenter - center;

    // Оси для проверки: локальные оси OBB + мировые оси AABB
    glm::vec3 axes[6] = {
        rotation[0], rotation[1], rotation[2],  // Локальные оси OBB
        glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)  // Мировые оси
    };

    for (int i = 0; i < 6; i++) {
        glm::vec3 axis = axes[i];
        if (glm::length(axis) < 0.001f) continue;
        axis = glm::normalize(axis);

        // Проекция половинных размеров OBB на ось
        float rOBB = 0.0f;
        for (int j = 0; j < 3; j++) {
            rOBB += halfExtents[j] * glm::abs(glm::dot(rotation[j], axis));
        }

        // Проекция половинных размеров AABB на ось
        float rAABB = aabbHalfExtents.x * glm::abs(glm::dot(glm::vec3(1, 0, 0), axis)) +
            aabbHalfExtents.y * glm::abs(glm::dot(glm::vec3(0, 1, 0), axis)) +
            aabbHalfExtents.z * glm::abs(glm::dot(glm::vec3(0, 0, 1), axis));

        // Проекция расстояния между центрами
        float projD = glm::abs(glm::dot(d, axis));

        // Проверка разделения
        if (projD > rOBB + rAABB) {
            return false;  // Есть разделяющая ось - нет пересечения
        }
    }

    return true;  // Нет разделяющей оси - есть пересечение
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
    , timeSinceOpen(0.0f)
    , touchLogged(false)
    , touched(false)
    , damageAccumulator(0.0f)
    , currentAngle(0.0f)
{
    obb.center = glm::vec3(0.0f);
    obb.halfExtents = glm::vec3(0.0f);
    obb.rotation = glm::mat3(1.0f);
}

DoorEntity::~DoorEntity() {
}

void DoorEntity::initFromEntity(const BSPEntity& entity, const BSPLoader& bsp) {
    className = entity.classname;
    origin = entity.origin;
    angles = entity.angles;

    // Определяем тип
    if (className == "func_door_rotating") {
        type = DoorType::ROTATING;
    }
    else {
        type = DoorType::SLIDING;
    }

    // Читаем свойства
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

    // === НАСТРОЙКА ДВИЖЕНИЯ ===
    // Сначала устанавливаем pos1 и currentPos, чтобы calculateBoundsFromModel мог использовать origin
    pos1 = origin;
    currentPos = pos1;

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

    if (type == DoorType::SLIDING) {
        // === SLIDING DOOR ===
        // В Half-Life направление движения вычисляется от angles.y через SetMovedir()
        // angles.y = 0 → movedir = (0, 0, 1) [+Z]
        // angles.y = 90 → movedir = (-1, 0, 0) [-X]
        // angles.y = 180 → movedir = (0, 0, -1) [-Z]
        // angles.y = 270/-90 → movedir = (1, 0, 0) [+X]
        
        float yaw = angles.y;
        glm::vec3 movedir;
        
        // Нормализуем угол к [0, 360)
        while (yaw < 0) yaw += 360.0f;
        while (yaw >= 360.0f) yaw -= 360.0f;
        
        // Преобразуем угол в направление (как в SetMovedir из HL SDK)
        if (yaw >= 315.0f || yaw < 45.0f) {
            movedir = glm::vec3(0.0f, 0.0f, 1.0f);   // +Z
        } else if (yaw >= 45.0f && yaw < 135.0f) {
            movedir = glm::vec3(-1.0f, 0.0f, 0.0f);  // -X
        } else if (yaw >= 135.0f && yaw < 225.0f) {
            movedir = glm::vec3(0.0f, 0.0f, -1.0f);  // -Z
        } else {
            movedir = glm::vec3(1.0f, 0.0f, 0.0f);   // +X
        }
        
        moveDir = movedir;
        
        // Размеры двери (в мировых координатах, т.к. mins/maxs уже локальные относительно origin)
        glm::vec3 size = maxs - mins;
        
        // Вычисляем расстояние движения по формуле Half-Life:
        // distance = |movedir.x| * (size.x - lip) + |movedir.y| * (size.y - lip) + |movedir.z| * (size.z - lip)
        // Поскольку movedir - единичный вектор вдоль одной оси, это просто размер по этой оси минус lip
        moveDistance = fabs(movedir.x) * (size.x - lip) + 
                       fabs(movedir.y) * (size.y - lip) + 
                       fabs(movedir.z) * (size.z - lip);
        
        // Защита от отрицательного расстояния
        if (moveDistance < 0.0f) moveDistance = 0.0f;
        
        // pos1 - закрытая позиция (origin двери)
        // pos2 - открытая позиция (origin + movedir * distance)
        pos1 = origin;
        pos2 = origin + movedir * moveDistance;
        
        std::cout << "[DOOR] Sliding '" << targetName << "': origin=" << origin.x << "," << origin.y << "," << origin.z
            << " movedir=" << moveDir.x << "," << moveDir.y << "," << moveDir.z
            << " size=" << size.x << "," << size.y << "," << size.z
            << " dist=" << moveDistance << " speed=" << speed << std::endl;

    }
    else {
        // === ROTATING DOOR ===
        it = entity.properties.find("distance");
        if (it != entity.properties.end()) {
            try { moveDistance = std::stof(it->second); }
            catch (...) { moveDistance = 90.0f; }
        }
        else {
            moveDistance = 90.0f;
        }

        std::cout << "[DOOR] Rotating '" << targetName << "': origin=" << origin.x << "," << origin.y << "," << origin.z
            << " angle=" << moveDistance << " speed=" << speed << std::endl;
    }

    // STARTS_OPEN - инвертируем состояние без изменения pos1/pos2
    if (hasFlag(DoorFlags::STARTS_OPEN)) {
        state = DoorState::OPEN;
        currentPos = pos2;
        moveProgress = 1.0f;

        if (type == DoorType::ROTATING) {
            currentAngle = moveDistance;
        }

        updatePosition(1.0f);

        // Устанавливаем время закрытия если wait > 0
        // Дверь начнёт закрываться через wait секунд после загрузки карты
        if (wait > 0) {
            timeSinceOpen = 0.0f;  // Начинаем отсчёт с 0, закроется когда timeSinceOpen >= wait
        }
        else if (wait == 0) {
            // Закрываем сразу в следующем кадре update()
            timeSinceOpen = 0.0f;
        }
        // wait < 0 - никогда не закрывается автоматически
    }
    else {
        state = DoorState::CLOSED;
        moveProgress = 0.0f;
        currentAngle = 0.0f;
        currentPos = pos1;
        updatePosition(0.0f);
    }

    // Финальная инициализация bounds для коллизий
    updatePosition(moveProgress);
    
    std::cout << "[DOOR] Init complete: currentMins=(" 
        << currentMins.x << "," << currentMins.y << "," << currentMins.z 
        << ") currentMaxs=(" << currentMaxs.x << "," << currentMaxs.y << "," << currentMaxs.z << ")" << std::endl;
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

    const float epsilon = 0.5f;
    worldMin -= glm::vec3(epsilon);
    worldMax += glm::vec3(epsilon);

    // mins/maxs хранятся в ЛОКАЛЬНЫХ координатах относительно origin двери
    // Вершины в BSP модели уже в мировых координатах, поэтому вычитаем origin
    // чтобы получить локальное смещение относительно точки вращения/движения
    mins = worldMin - origin;
    maxs = worldMax - origin;

    std::cout << "[DOOR] Model bounds: worldMin=(" << worldMin.x << "," << worldMin.y << "," << worldMin.z 
              << ") worldMax=(" << worldMax.x << "," << worldMax.y << "," << worldMax.z << ")" << std::endl;
    std::cout << "[DOOR] Door origin: (" << origin.x << "," << origin.y << "," << origin.z << ")" << std::endl;
    std::cout << "[DOOR] Local mins=(" << mins.x << "," << mins.y << "," << mins.z 
              << ") maxs=(" << maxs.x << "," << maxs.y << "," << maxs.z << ")" << std::endl;

    // Инициализируем currentMins/maxs как мировые координаты (для закрытой позиции)
    currentMins = worldMin;
    currentMaxs = worldMax;
}

bool DoorEntity::intersectsCapsule(const Capsule& capsule) const {
    AABB doorAABB;
    doorAABB.min = currentMins;
    doorAABB.max = currentMaxs;

    AABB capsuleAABB = capsule.getBounds();

    // Быстрая проверка AABB - если нет пересечения, сразу возвращаем false
    if (capsuleAABB.max.x < doorAABB.min.x || capsuleAABB.min.x > doorAABB.max.x ||
        capsuleAABB.max.y < doorAABB.min.y || capsuleAABB.min.y > doorAABB.max.y ||
        capsuleAABB.max.z < doorAABB.min.z || capsuleAABB.min.z > doorAABB.max.z) {
        return false;
    }

    // Точная проверка - расширяем AABB двери на радиус капсулы
    glm::vec3 capsuleCenter = capsule.getCenter();

    bool intersects = (capsuleCenter.x >= doorAABB.min.x - capsule.radius &&
        capsuleCenter.x <= doorAABB.max.x + capsule.radius &&
        capsuleCenter.y >= doorAABB.min.y - capsule.radius &&
        capsuleCenter.y <= doorAABB.max.y + capsule.radius &&
        capsuleCenter.z >= doorAABB.min.z - capsule.radius &&
        capsuleCenter.z <= doorAABB.max.z + capsule.radius);

    return intersects;
}

glm::mat4 DoorEntity::getRenderTransform() const {
    if (type == DoorType::SLIDING) {
        // Для скользящих дверей: вершины в BSP хранятся в МИРОВЫХ координатах
        // mins/maxs - это локальное смещение относительно origin (вычислено как worldBounds - origin)
        // При рендеринге нужно применить только СМЕЩЕНИЕ от начальной позиции
        // Текущее смещение = currentPos - pos1 (где pos1 = origin)
        glm::vec3 offset = currentPos - pos1;
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), offset);
        
        return transform;
    }
    else {
        // Для вращающихся дверей: вращение вокруг origin
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
        // Интерполируем мировую позицию origin
        currentPos = glm::mix(pos1, pos2, progress);

        // Обновляем bounds - смещаем локальные bounds на текущую позицию
        // mins/maxs хранятся в локальном пространстве относительно origin
        // currentPos - это мировая позиция двери
        currentMins = mins + currentPos;
        currentMaxs = maxs + currentPos;

    }
    else {
        // Rotating
        currentAngle = progress * moveDistance;
        updateRotatedBounds();
    }
    
    std::cout << "[DOOR UPDATE] progress=" << progress 
              << " currentPos=(" << currentPos.x << "," << currentPos.y << "," << currentPos.z << ")"
              << " currentMins=(" << currentMins.x << "," << currentMins.y << "," << currentMins.z << ")"
              << " currentMaxs=(" << currentMaxs.x << "," << currentMaxs.y << "," << currentMaxs.z << ")"
              << std::endl;
}

void DoorEntity::updateRotatedBounds() {
    float angleRad = glm::radians(currentAngle);
    float cosA = cos(angleRad);
    float sinA = sin(angleRad);

    // Для вращающихся дверей используем tight AABB вместо избыточного
    // Вычисляем границы во всех осях с учётом вращения
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

    // Обновляем OBB для точных коллизий
    updateOBB();
}

// Обновление OBB для вращающихся дверей
void DoorEntity::updateOBB() {
    if (type != DoorType::ROTATING) return;

    // Центр OBB - это центр двери в мировых координатах
    obb.center = (currentMins + currentMaxs) * 0.5f;

    // Половинные размеры в локальном пространстве
    obb.halfExtents = (maxs - mins) * 0.5f;

    // Матрица вращения вокруг Y
    float angleRad = glm::radians(currentAngle);
    float cosA = cos(angleRad);
    float sinA = sin(angleRad);

    // Колонки матрицы вращения - локальные оси OBB
    obb.rotation[0] = glm::vec3(cosA, 0.0f, sinA);   // Локальная X ось
    obb.rotation[1] = glm::vec3(0.0f, 1.0f, 0.0f);   // Локальная Y ось (без изменений)
    obb.rotation[2] = glm::vec3(-sinA, 0.0f, cosA);  // Локальная Z ось
}

bool DoorEntity::tryActivate(bool isPlayerUse) {
    if (locked) {
        // Дверь заблокирована - играем звук замка (если есть)
        if (!touchLogged) {
            std::cout << "[DOOR] '" << targetName << "' is LOCKED!" << std::endl;
            touchLogged = true;
        }
        return false;
    }

    // Проверяем флаги активации
    bool useOnly = isUseOnly();
    bool touchOpens = isTouchOpens();
    bool toggle = hasFlag(DoorFlags::TOGGLE);  // SF_DOOR_NO_AUTO_RETURN

    // Логика активации по аналогии с Half-Life:
    // 1. Если есть targetname - дверь открывается ТОЛЬКО триггером/кнопкой (USE_ONLY)
    // 2. USE_ONLY (SF_DOOR_USE_ONLY) - открывается только по использованию игроком (клавиша E)
    // 3. TOUCH_OPENS (наш флаг) - открывается при касании игроком
    // 4. Обычная дверь (без targetname, без USE_ONLY) - открывается и от касания, и от использования
    
    // В HL: если есть targetname, SetTouch(NULL) и дверь не реагирует на касания
    // Если нет targetname, SetTouch(&CBaseDoor::DoorTouch) и открывается при касании
    
    if (useOnly) {
        // Дверь требует явного использования (клавиша E или триггер)
        if (!isPlayerUse) {
            return false;  // Игнорируем касания
        }
    }
    else if (touchOpens) {
        // Дверь открывается по касанию - разрешаем всегда при пересечении
        // isPlayerUse не важен для TOUCH_OPENS
    }
    else {
        // Обычная дверь без targetname - работает как HL: открывается при касании
        // Разрешаем активацию в любом случае
    }

    // Обработка состояний (аналогично DoorActivate() в HL)
    if (state == DoorState::CLOSED || state == DoorState::CLOSING) {
        // Открываем дверь
        open();
        return true;
    }
    else if (state == DoorState::OPEN) {
        // Если есть флаг TOGGLE (NO_AUTO_RETURN) - закрываем ТОЛЬКО при явном использовании
        if (toggle && isPlayerUse) {
            close();
            return true;
        }
        // Иначе ждём автозакрытия (обрабатывается в update())
    }
    else if (state == DoorState::OPENING) {
        // Если дверь уже открывается и есть TOGGLE - останавливаем при явном использовании
        if (toggle && isPlayerUse) {
            stop();
            return true;
        }
    }

    return false;
}

// Проверка блокировки двери игроком и нанесение урона
bool DoorEntity::checkBlocked(const Capsule& playerCapsule, float deltaTime) {
    if (dmg <= 0.0f || health <= 0) return false;

    // Проверяем только когда дверь движется
    if (state != DoorState::OPENING && state != DoorState::CLOSING) {
        return false;
    }

    if (intersectsCapsule(playerCapsule)) {
        // Дверь заблокирована игроком - наносит урон
        // В Half-Life: если игрок блокирует дверь, она наносит урон каждый кадр
        // и меняет направление движения (если wait >= 0)
        return true;
    }

    return false;
}

// Нанесение урона игроку при блокировке двери (аналог Blocked() в HL)
void DoorEntity::applyDamageToPlayer(Player* player, float deltaTime) {
    if (dmg <= 0.0f || !player) return;

    // Проверяем только когда дверь движется
    if (state != DoorState::OPENING && state != DoorState::CLOSING) {
        damageAccumulator = 0.0f;  // Сбрасываем накопленный урон
        return;
    }

    Capsule playerCapsule = player->getPlayerCapsule(player->getPosition());

    if (intersectsCapsule(playerCapsule)) {
        // Игрок блокирует дверь - наносим урон
        // В HL урон наносится каждый кадр: pOther->TakeDamage(pev, pev, pev->dmg, DMG_CRUSH)
        
        // Накопление урона пропорционально времени блокировки
        damageAccumulator += dmg * deltaTime;

        // Применяем урон порогово (каждые 10 единиц накопленного урона)
        if (damageAccumulator >= 10.0f) {
            player->TakeDamage(damageAccumulator);
            std::cout << "[DOOR] Dealing " << damageAccumulator << " damage to player (blocked)!" << std::endl;
            damageAccumulator = 0.0f;
            
            // Если wait >= 0, меняем направление двери (как в HL Blocked())
            // Это реализовано в update() через проверку blocked
        }
    }
    else {
        damageAccumulator = 0.0f;  // Сброс если игрок не блокирует
    }
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
        currentPos = pos2;
        if (type == DoorType::ROTATING) {
            currentAngle = moveDistance;
        }

        if (wait > 0) {
            timeSinceOpen = 0.0f;  // Начинаем отсчёт с момента открытия
        }
        // wait <= 0 обрабатывается в update() - закрываем сразу или не закрываем

    }
    else if (state == DoorState::CLOSING) {
        state = DoorState::CLOSED;
        moveProgress = 0.0f;
        currentPos = pos1;
        if (type == DoorType::ROTATING) {
            currentAngle = 0.0f;
        }
    }

    updatePosition(moveProgress);
}

void DoorEntity::update(float deltaTime) {
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
        if (wait > 0) {
            timeSinceOpen += deltaTime;
            if (timeSinceOpen >= wait) {
                close();
            }
        }
        else if (wait == 0) {
            // Закрываем сразу (wait = 0) - как в HL
            close();
        }
        // wait < 0 или TOGGLE flag - никогда не закрываться автоматически
    }
    
    // Сбрасываем touchLogged для каждой двери индивидуально
    if (touchLogged) {
        touchLogged = false;
    }
}

AABB DoorEntity::getBounds() const {
    AABB bounds;
    bounds.min = currentMins;
    bounds.max = currentMaxs;
    return bounds;
}
