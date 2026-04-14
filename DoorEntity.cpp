#define _CRT_SECURE_NO_WARNINGS
#define GLM_ENABLE_EXPERIMENTAL

#include "pch.h"

#include "DoorEntity.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
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
    touchTriggered(false),
    hasGeometry(false) {
    // По умолчанию дверь имеет размер 64x64x64 единиц
    // и открывается вверх (по оси Y в нашей системе координат)
}

CFuncDoor::~CFuncDoor() {
    if (vao != 0) glDeleteVertexArrays(1, &vao);
    if (vbo != 0) glDeleteBuffers(1, &vbo);
    if (ebo != 0) glDeleteBuffers(1, &ebo);
}

void CFuncDoor::initFromProperties(const std::unordered_map<std::string, std::string>& props, const AABB& modelBounds) {
    // Инициализируем границы двери из модели
    // originalBounds - это локальные bounds двери относительно её позиции
    originalBounds = modelBounds;

    // Позиция двери - это центр модели в мировых координатах
    glm::vec3 center = (originalBounds.min + originalBounds.max) * 0.5f;
    setPosition(center);

    // Преобразуем originalBounds в локальные координаты (относительно центра)
    originalBounds.min -= center;
    originalBounds.max -= center;
    
    // bounds используется для начальной проверки, инициализируем его мировыми координатами
    bounds = modelBounds;

    // Читаем параметры из BSP сущности
    // Speed - скорость открытия двери (units per second)
    auto it = props.find("speed");
    if (it != props.end()) {
        try {
            speed = std::stof(it->second);
        }
        catch (...) {
            speed = 100.0f; // Значение по умолчанию
        }
    }
    else {
        speed = 100.0f;
    }

    // Lip - расстояние которое дверь остается закрытой (в Half-Life это выступ)
    it = props.find("lip");
    if (it != props.end()) {
        try {
            lip = std::stof(it->second);
        }
        catch (...) {
            lip = 8.0f; // Значение по умолчанию
        }
    }
    else {
        lip = 8.0f;
    }

    // Pitch Yaw Roll angles - направление открытия двери
    // В Half-Life angles задаются как "pitch yaw roll" где:
    //   pitch - угол вверх/вниз (положительный = вверх, отрицательный = вниз)
    //   yaw   - угол влево/вправо  
    //   roll  - угол наклона (редко используется для дверей)
    // 
    // Для func_door в Half-Life:
    // - Если angles не заданы или "0 0 0" - дверь открывается перпендикулярно своей плоскости
    // - Если angles заданы, они определяют направление движения:
    //   * angles.x (pitch) > 0 - дверь открывается вверх
    //   * angles.x (pitch) < 0 - дверь открывается вниз
    //   * angles.y (yaw) - поворот направления по горизонтали
    
    glm::vec3 hlAngles(0.0f, 0.0f, 0.0f);
    bool hasAngles = false;
    
    it = props.find("angles");
    if (it != props.end()) {
        int parsed = sscanf(it->second.c_str(), "%f %f %f", &hlAngles.x, &hlAngles.y, &hlAngles.z);
        if (parsed == 3) {
            hasAngles = !(hlAngles.x == 0.0f && hlAngles.y == 0.0f && hlAngles.z == 0.0f);
        }
        else {
            std::cerr << "[DOOR] Failed to parse angles: \"" << it->second << "\"\n";
        }
    }

    // Определяем направление движения двери
    // Используем originalBounds (локальные) для вычислений
    if (!hasAngles) {
        // Нет углов - определяем направление автоматически на основе геометрии двери
        // Дверь открывается перпендикулярно своей плоскости (вдоль наименьшей оси)
        
        float dx = originalBounds.max.x - originalBounds.min.x;
        float dy = originalBounds.max.y - originalBounds.min.y;
        float dz = originalBounds.max.z - originalBounds.min.z;

        // Находим толщину двери (наименьший размер)
        // Дверь должна открываться перпендикулярно своей плоскости
        if (dx <= dy && dx <= dz) {
            // Дверь тонкая по X - плоскость YZ, открывается вдоль X
            moveDirection = glm::vec3(1.0f, 0.0f, 0.0f);
            moveDistance = dx - lip;
        }
        else if (dz <= dx && dz <= dy) {
            // Дверь тонкая по Z - плоскость XY, открывается вдоль Z
            moveDirection = glm::vec3(0.0f, 0.0f, 1.0f);
            moveDistance = dz - lip;
        }
        else {
            // Дверь тонкая по Y - плоскость XZ (горизонтальная), открывается вверх/вниз
            // По умолчанию открываем вверх
            moveDirection = glm::vec3(0.0f, 1.0f, 0.0f);
            moveDistance = dy - lip;
        }
    }
    else {
        // Есть angles - используем их для определения направления
        // В Half-Life angles определяют направление движения двери:
        // - pitch (angles.x) > 0 : вверх, < 0 : вниз
        // - yaw (angles.y) : поворот по горизонтали
        
        // Базовое направление - вперед по локальной оси двери
        // В нашей системе координат: Y = up, X/Z = horizontal
        glm::vec3 baseDir(0.0f, 1.0f, 0.0f);  // По умолчанию вверх
        
        // Если pitch положительный - дверь открывается вверх
        // Если pitch отрицательный - дверь открывается вниз
        if (hlAngles.x != 0.0f) {
            // Вертикальное открытие (вверх/вниз)
            baseDir = glm::vec3(0.0f, 1.0f, 0.0f);  // Вверх
            if (hlAngles.x < 0.0f) {
                baseDir = glm::vec3(0.0f, -1.0f, 0.0f);  // Вниз
            }
        }
        
        // Применяем yaw для горизонтального поворота
        if (hlAngles.y != 0.0f) {
            float yawRad = glm::radians(hlAngles.y);
            // Поворачиваем базовое направление вокруг Y оси
            float cosYaw = cos(yawRad);
            float sinYaw = sin(yawRad);
            
            // Если дверь открывается вертикально, yaw не влияет
            if (baseDir.y != 0.0f) {
                // Вертикальная дверь с yaw - это редкий случай
                // Обычно yaw применяется к горизонтальному направлению
            }
            else {
                // Горизонтальное направление - применяем yaw
                glm::vec3 horizDir(cosYaw, 0.0f, sinYaw);
                baseDir = horizDir;
            }
        }
        
        moveDirection = glm::normalize(baseDir);
        
        // Расстояние открытия - максимальный размер двери минус lip
        float doorHeight = originalBounds.max.y - originalBounds.min.y;
        float doorWidth = std::max(originalBounds.max.x - originalBounds.min.x, originalBounds.max.z - originalBounds.min.z);
        moveDistance = std::max(doorHeight, doorWidth) - lip;
    }

    // Убеждаемся что расстояние положительное
    if (moveDistance < 0.0f) {
        moveDistance = 0.0f;
    }

    std::cout << "[func_door] Created door: local_mins("
        << originalBounds.min.x << ", " << originalBounds.min.y << ", " << originalBounds.min.z
        << ") local_maxs(" << originalBounds.max.x << ", " << originalBounds.max.y << ", " << originalBounds.max.z
        << ") world_pos(" << getPosition().x << ", " << getPosition().y << ", " << getPosition().z
        << ") speed=" << speed << " lip=" << lip
        << " direction(" << moveDirection.x << ", " << moveDirection.y << ", " << moveDirection.z
        << ") distance=" << moveDistance;
    if (hasAngles) {
        std::cout << " angles(" << hlAngles.x << ", " << hlAngles.y << ", " << hlAngles.z << ")";
    }
    std::cout << std::endl;
}

void CFuncDoor::Update(float deltaTime) {
    switch (state) {
    case DOOR_CLOSED:
        // Дверь закрыта, ждем триггеринга
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
            std::cout << "[func_door] Door fully opened\n";
        }
        // Позиция сущности не меняется, смещение применяется в getCurrentBounds()
    }
    break;

    case DOOR_OPEN:
        // Дверь открыта, ждем времени до закрытия
        stateTimer -= deltaTime;
        if (stateTimer <= 0.0f) {
            state = DOOR_CLOSING;
            std::cout << "[func_door] Door started closing\n";
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
            std::cout << "[func_door] Door fully closed\n";
        }
        // Позиция сущности не меняется, смещение применяется в getCurrentBounds()
    }
    break;
    }
}

void CFuncDoor::initGeometry(const std::vector<glm::vec3>& bspVertices, 
                              const std::vector<glm::vec3>& bspNormals,
                              const std::vector<glm::vec2>& bspTexCoords,
                              const std::vector<unsigned int>& bspIndices,
                              GLuint texId) {
    if (bspVertices.empty() || bspIndices.empty()) {
        hasGeometry = false;
        return;
    }

    textureID = texId;
    
    // Копируем вершины и индексы
    vertices.resize(bspVertices.size());
    for (size_t i = 0; i < bspVertices.size(); i++) {
        vertices[i].position = bspVertices[i];
        if (i < bspNormals.size()) {
            vertices[i].normal = bspNormals[i];
        } else {
            vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        if (i < bspTexCoords.size()) {
            vertices[i].texCoord = bspTexCoords[i];
        } else {
            vertices[i].texCoord = glm::vec2(0.0f, 0.0f);
        }
    }
    
    indices = bspIndices;
    hasGeometry = true;
    
    // Создаем VAO/VBO/EBO
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(DoorVertex), 
                 vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), 
                 indices.data(), GL_STATIC_DRAW);
    
    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DoorVertex), (void*)0);
    
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DoorVertex), 
                         (void*)offsetof(DoorVertex, normal));
    
    // texCoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DoorVertex), 
                         (void*)offsetof(DoorVertex, texCoord));
    
    glBindVertexArray(0);
    
    std::cout << "[DOOR] Geometry initialized with " << vertices.size() 
              << " vertices and " << indices.size() << " indices\n";
}

void CFuncDoor::Render() {
    if (!hasGeometry || vao == 0) return;
    
    // Вычисляем матрицу модели с учетом смещения двери
    glm::vec3 offset = moveDirection * (currentPos * moveDistance);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), getPosition() + offset);
    
    // Рендерим дверь
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), 
                   GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
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
    // Получаем мировую позицию двери
    glm::vec3 worldPos = getPosition();
    
    // Вычисляем смещение двери на основе текущего состояния
    glm::vec3 offset = moveDirection * (currentPos * moveDistance);

    AABB currentBounds;
    // Преобразуем локальные bounds в мировые с учетом смещения при открытии
    currentBounds.min = worldPos + originalBounds.min + offset;
    currentBounds.max = worldPos + originalBounds.max + offset;

    return currentBounds;
}

void CFuncDoor::triggerOpen() {
    if (state == DOOR_CLOSED) {
        state = DOOR_OPENING;
        currentPos = 0.0f;
        std::cout << "[func_door] Door triggered to open from closed\n";
    }
    else if (state == DOOR_CLOSING) {
        // Дверь закрывается - прерываем закрытие и начинаем открывать с текущей позиции
        state = DOOR_OPENING;
        // Не сбрасываем currentPos - продолжаем с текущей позиции
        std::cout << "[func_door] Door triggered to open while closing (pos=" << currentPos << ")\n";
    }
}