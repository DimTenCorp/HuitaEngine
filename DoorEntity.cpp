#include "pch.h"
#include "DoorEntity.h"
#include "TriangleCollider.h"
#include "BSPLoader.h"
#include <iostream>
#include <algorithm>
#include <cmath>

static glm::vec3 convertHLAngles(const glm::vec3& angles) {
    return glm::vec3(-angles.x, angles.z, angles.y);
}

void DoorEntity::initFromEntity(const BSPEntity& entity, const BSPLoader& bsp) {
    classname = entity.classname;
    entityProperties = entity.properties;

    if (classname == "func_door_rotating") {
        moveType = DoorMoveType::Rotating;
    }
    else {
        moveType = DoorMoveType::Linear;
    }

    // Парсим индекс модели
    if (!entity.model.empty() && entity.model[0] == '*') {
        try {
            modelIndex = std::stoi(entity.model.substr(1));
        }
        catch (...) {
            std::cerr << "[DOOR] Failed to parse model index: " << entity.model << std::endl;
            return;
        }
    }

    if (modelIndex <= 0 || modelIndex >= (int)bsp.getModels().size()) {
        std::cerr << "[DOOR] Invalid model index: " << modelIndex << std::endl;
        return;
    }

    const BSPModel& model = bsp.getModels()[modelIndex];

    // === ИСПРАВЛЕНО: правильная конвертация координат BSP → OpenGL ===
    // BSP: X=вперед, Y=влево, Z=вверх
    // OpenGL: X=вправо, Y=вверх, Z=назад

    // Mins/maxs конвертация
    modelMins = glm::vec3(-model.max[0], model.min[2], model.min[1]);
    modelMaxs = glm::vec3(-model.min[0], model.max[2], model.max[1]);

    // Корректируем если перепутаны
    if (modelMins.x > modelMaxs.x) std::swap(modelMins.x, modelMaxs.x);
    if (modelMins.y > modelMaxs.y) std::swap(modelMins.y, modelMaxs.y);
    if (modelMins.z > modelMaxs.z) std::swap(modelMins.z, modelMaxs.z);

    // Origin модели (точка вращения/отсчета)
    modelOrigin = glm::vec3(-model.origin[0], model.origin[2], model.origin[1]);

    // === ИСПРАВЛЕНО: startOrigin = реальная позиция в мире ===
    // Центр AABB модели в мировых координатах
    startOrigin = (modelMins + modelMaxs) * 0.5f;
    origin = startOrigin;

    // targetname для активации из других энтити
    targetname = entity.properties.count("targetname") ? entity.properties.at("targetname") : "";

    // Скорость
    if (entity.properties.count("speed")) {
        try { speed = std::stof(entity.properties.at("speed")); }
        catch (...) {}
    }
    if (speed <= 0) speed = 100.0f;

    // Время ожидания перед закрытием (-1 = не закрывать)
    if (entity.properties.count("wait")) {
        try {
            waitTime = std::stof(entity.properties.at("wait"));
            if (waitTime == -1) noAutoReturn = true;
        }
        catch (...) {}
    }

    // Lip - сколько оставлять торчать при открытии
    if (entity.properties.count("lip")) {
        try { lip = std::stof(entity.properties.at("lip")); }
        catch (...) {}
    }
    else {
        lip = 0.0f;  // HL1 дефолт — без lip
    }

    // Урон при защемлении
    if (entity.properties.count("dmg")) {
        try { damage = std::stof(entity.properties.at("dmg")); }
        catch (...) {}
    }

    // Углы (для вращающихся дверей)
    if (entity.properties.count("angles")) {
        try {
            float ax, ay, az;
            sscanf_s(entity.properties.at("angles").c_str(), "%f %f %f", &ax, &ay, &az);
            angles = glm::vec3(ax, ay, az);
        }
        catch (...) {}
    }
    startAngles = angles;

    // Направление движения (для линейных дверей)
    if (entity.properties.count("movedir")) {
        try {
            float dx, dy, dz;
            sscanf_s(entity.properties.at("movedir").c_str(), "%f %f %f", &dx, &dy, &dz);
            moveDir = glm::normalize(glm::vec3(dx, dy, dz));
        }
        catch (...) {}
    }
    else {
        // По умолчанию - по углу yaw
        float yaw = angles.y;
        moveDir = glm::vec3(cos(glm::radians(yaw)), 0.0f, sin(glm::radians(yaw)));
    }

    // Звуки
    if (entity.properties.count("movesnd")) {
        try { moveSound = std::stoi(entity.properties.at("movesnd")); }
        catch (...) {}
    }
    if (entity.properties.count("stopsnd")) {
        try { stopSound = std::stoi(entity.properties.at("stopsnd")); }
        catch (...) {}
    }

    // Spawnflags
    if (entity.properties.count("spawnflags")) {
        try {
            int flags = std::stoi(entity.properties.at("spawnflags"));
            startOpen = (flags & 1) != 0;
            passable = (flags & 8) != 0;
            oneWay = (flags & 16) != 0;
            noAutoReturn = (flags & 32) != 0;
            useOnly = (flags & 256) != 0;
            silent = (flags & 0x80000000) != 0;
        }
        catch (...) {}
    }

    // === ИСПРАВЛЕНО: расчет конечной позиции ДО построения геометрии ===
    calculateEndPosition(bsp);

    // Если startOpen - меняем начальное и конечное положение местами
    if (startOpen) {
        std::swap(startOrigin, endOrigin);
        std::swap(startAngles, endAngles);
        origin = endOrigin;
        angles = endAngles;
        state = DoorState::Open;
        moveProgress = 1.0f;
    }

    // Строим геометрию
    buildGeometry(bsp);

    // Начальные bounds
    bounds.min = modelMins;
    bounds.max = modelMaxs;

    std::cout << "[DOOR] Created " << classname << " model*" << modelIndex
        << " speed=" << speed << " wait=" << waitTime
        << " type=" << (moveType == DoorMoveType::Linear ? "linear" : "rotating")
        << " start=" << startOrigin.x << "," << startOrigin.y << "," << startOrigin.z
        << " end=" << endOrigin.x << "," << endOrigin.y << "," << endOrigin.z
        << std::endl;
}

void DoorEntity::calculateEndPosition(const BSPLoader& bsp) {
    if (moveType == DoorMoveType::Linear) {
        glm::vec3 size = modelMaxs - modelMins;
        float projection = std::abs(glm::dot(size, glm::abs(moveDir)));
        moveDistance = projection - lip;
        if (moveDistance < 0) moveDistance = projection * 0.8f;

        endOrigin = startOrigin - moveDir * moveDistance;
    }
    else {
        float rotationAngle = 90.0f;

        // Используем сохраненные свойства
        auto it = entityProperties.find("distance");
        if (it != entityProperties.end()) {
            try { rotationAngle = std::stof(it->second); }
            catch (...) {}
        }

        endAngles = startAngles + glm::vec3(0.0f, rotationAngle, 0.0f);
    }
}

void DoorEntity::buildGeometry(const BSPLoader& bsp) {
    const auto& faces = bsp.getFaces();
    const auto& texInfos = bsp.getTexInfos();
    const auto& surfEdges = bsp.getSurfEdges();
    const auto& edges = bsp.getEdges();
    const auto& originalVertices = bsp.getOriginalVertices();
    const auto& planes = bsp.getPlanes();

    const BSPModel& model = bsp.getModels()[modelIndex];

    vertices.clear();
    indices.clear();

    // === ИСПРАВЛЕНО: используем первую валидную текстуру для всей двери ===
    textureID = 0;
    bool textureFound = false;

    for (int i = 0; i < model.numFaces; i++) {
        int faceIdx = model.firstFace + i;
        if (faceIdx < 0 || faceIdx >= (int)faces.size()) continue;

        const BSPFace& face = faces[faceIdx];
        if (face.numEdges < 3) continue;
        if (face.texInfo < 0 || face.texInfo >= (int)texInfos.size()) continue;

        const BSPTexInfo& texInfo = texInfos[face.texInfo];

        // Собираем вершины грани
        std::vector<glm::vec3> faceVerts;
        faceVerts.reserve(face.numEdges);

        for (int j = 0; j < face.numEdges; j++) {
            int seIdx = face.firstEdge + j;
            if (seIdx < 0 || seIdx >= (int)surfEdges.size()) break;

            int edgeIdx = surfEdges[seIdx];
            int vIdx;
            if (edgeIdx >= 0) {
                if (edgeIdx >= (int)edges.size()) break;
                vIdx = edges[edgeIdx].v[0];
            }
            else {
                int negIdx = -edgeIdx;
                if (negIdx < 0 || negIdx >= (int)edges.size()) break;
                vIdx = edges[negIdx].v[1];
            }

            if (vIdx < 0 || vIdx >= (int)originalVertices.size()) continue;
            faceVerts.push_back(originalVertices[vIdx]);
        }

        if (faceVerts.size() < 3) continue;

        // === ИСПРАВЛЕНО: берем текстуру только один раз ===
        if (!textureFound) {
            int texIdx = texInfo.textureIndex;
            if (texIdx < 0 || texIdx >= (int)bsp.getTextureCount()) texIdx = 0;
            textureID = bsp.getTextureID(texIdx);
            textureFound = true;
        }

        glm::uvec2 texDim = bsp.getTextureDimensions(texInfo.textureIndex);
        int texW = texDim.x > 0 ? texDim.x : 256;
        int texH = texDim.y > 0 ? texDim.y : 256;

        // Нормаль
        glm::vec3 bspNormal = planes[face.planeNum].normal;
        if (face.side != 0) bspNormal = -bspNormal;
        glm::vec3 worldNormal(-bspNormal.x, bspNormal.z, bspNormal.y);
        worldNormal = glm::normalize(worldNormal);

        unsigned int baseIdx = vertices.size();

        // Вершины относительно modelOrigin (для правильного преобразования)
        for (const auto& v : faceVerts) {
            BSPVertex bv;
            // Конвертация координат BSP → OpenGL
            bv.position = glm::vec3(-v.x, v.z, v.y);

            // Вычитаем modelOrigin чтобы вершины были в локальных координатах
            bv.position -= modelOrigin;

            // UV
            float s = v.x * texInfo.s[0] + v.y * texInfo.s[1] + v.z * texInfo.s[2] + texInfo.s[3];
            float t = v.x * texInfo.t[0] + v.y * texInfo.t[1] + v.z * texInfo.t[2] + texInfo.t[3];
            bv.texCoord = glm::vec2(s / texW, t / texH);
            bv.normal = worldNormal;

            vertices.push_back(bv);
        }

        // Триангуляция веером
        for (size_t j = 1; j + 1 < faceVerts.size(); j++) {
            indices.push_back(baseIdx);
            indices.push_back(baseIdx + j + 1);
            indices.push_back(baseIdx + j);
        }
    }

    // Fallback текстура если не нашли
    if (!textureFound) {
        textureID = bsp.getDefaultTextureID();
    }

    std::cout << "[DOOR] Built geometry: " << vertices.size() << " verts, "
        << indices.size() << " indices, texID=" << textureID << std::endl;
}

void DoorEntity::update(float deltaTime, MeshCollider* worldCollider) {
    switch (state) {
    case DoorState::Closed:
        break;

    case DoorState::Opening: {
        // Скорость в единицах/сек или градусах/сек
        float moveSpeed = speed;
        if (moveType == DoorMoveType::Linear) {
            moveSpeed = speed / moveDistance; // Нормализуем к [0,1]
        }
        else {
            moveSpeed = speed / 90.0f; // Нормализуем для 90 градусов
        }

        moveProgress += moveSpeed * deltaTime;
        moveProgress = glm::clamp(moveProgress, 0.0f, 1.0f);

        // Интерполяция с ease-out
        float t = moveProgress;

        if (moveType == DoorMoveType::Linear) {
            origin = glm::mix(startOrigin, endOrigin, t);
        }
        else {
            angles = glm::mix(startAngles, endAngles, t);
        }

        // Достигли конца?
        if (moveProgress >= 1.0f) {
            state = DoorState::Open;
            stateTimer = 0.0f;
            if (!silent) {
                // Play stop sound
            }
            std::cout << "[DOOR] Fully opened" << std::endl;
        }
        break;
    }

    case DoorState::Open:
        // === ИСПРАВЛЕНО: автозакрытие только если не noAutoReturn и waitTime >= 0 ===
        if (!noAutoReturn && waitTime >= 0) {
            stateTimer += deltaTime;
            if (stateTimer >= waitTime) {
                close();
            }
        }
        break;

    case DoorState::Closing: {
        float moveSpeed = speed;
        if (moveType == DoorMoveType::Linear) {
            moveSpeed = speed / moveDistance;
        }
        else {
            moveSpeed = speed / 90.0f;
        }

        moveProgress -= moveSpeed * deltaTime;
        moveProgress = glm::clamp(moveProgress, 0.0f, 1.0f);

        // Интерполяция с ease-in
        float t = moveProgress;

        if (moveType == DoorMoveType::Linear) {
            origin = glm::mix(startOrigin, endOrigin, t);
        }
        else {
            angles = glm::mix(startAngles, endAngles, t);
        }

        // Проверка столкновения с игроком при закрытии
        // TODO: если защемлен игрок - остановить или нанести урон

        if (moveProgress <= 0.0f) {
            state = DoorState::Closed;
            if (!silent) {
                // Play stop sound
            }
            std::cout << "[DOOR] Fully closed" << std::endl;
        }
        break;
    }
    }

    // Обновляем bounds для коллизий
    updateBounds();
}

void DoorEntity::updateBounds() {
    // Получаем текущую трансформацию
    glm::mat4 transform = getTransform();

    // Трансформируем все 8 углов AABB
    glm::vec3 corners[8] = {
        glm::vec3(modelMins.x, modelMins.y, modelMins.z),
        glm::vec3(modelMaxs.x, modelMins.y, modelMins.z),
        glm::vec3(modelMins.x, modelMaxs.y, modelMins.z),
        glm::vec3(modelMaxs.x, modelMaxs.y, modelMins.z),
        glm::vec3(modelMins.x, modelMins.y, modelMaxs.z),
        glm::vec3(modelMaxs.x, modelMins.y, modelMaxs.z),
        glm::vec3(modelMins.x, modelMaxs.y, modelMaxs.z),
        glm::vec3(modelMaxs.x, modelMaxs.y, modelMaxs.z)
    };

    bool first = true;
    for (const auto& corner : corners) {
        glm::vec4 transformed = transform * glm::vec4(corner, 1.0f);
        glm::vec3 worldPos(transformed);

        if (first) {
            bounds.min = bounds.max = worldPos;
            first = false;
        }
        else {
            bounds.min = glm::min(bounds.min, worldPos);
            bounds.max = glm::max(bounds.max, worldPos);
        }
    }
}

void DoorEntity::activate() {
    if (state == DoorState::Closed || state == DoorState::Closing) {
        open();
    }
    else if (state == DoorState::Open || state == DoorState::Opening) {
        // === ИСПРАВЛЕНО: если уже открывается/открыта, не переключаем сразу ===
        // Ждем автозакрытия или принудительного закрытия
        if (!noAutoReturn && waitTime >= 0) {
            // Сбрасываем таймер автозакрытия (игрок "подержал" дверь)
            stateTimer = 0.0f;
        }
    }
}

void DoorEntity::open() {
    if (state == DoorState::Open || state == DoorState::Opening) return;

    state = DoorState::Opening;
    if (!silent) {
        // Play move sound
    }
    std::cout << "[DOOR] Opening" << std::endl;
}

void DoorEntity::close() {
    if (state == DoorState::Closed || state == DoorState::Closing) return;

    state = DoorState::Closing;
    if (!silent) {
        // Play move sound
    }
    std::cout << "[DOOR] Closing" << std::endl;
}

bool DoorEntity::intersectsPlayer(const glm::vec3& playerPos, const Capsule& playerCapsule) const {
    // Расширяем bounds на радиус капсулы
    AABB expanded = bounds;
    expanded.min -= glm::vec3(playerCapsule.radius);
    expanded.max += glm::vec3(playerCapsule.radius);

    // Проверяем пересечение точки центра капсулы с расширенным AABB
    glm::vec3 closest = glm::clamp(playerPos, expanded.min, expanded.max);
    float dist = glm::length(playerPos - closest);

    return dist < playerCapsule.radius;
}

AABB DoorEntity::getCurrentBounds() const {
    return bounds;
}

glm::mat4 DoorEntity::getTransform() const {
    glm::mat4 transform = glm::mat4(1.0f);

    // === ИСПРАВЛЕНО: правильный порядок трансформаций ===
    // 1. Сначала переносим в modelOrigin (опорная точка двери)
    transform = glm::translate(transform, modelOrigin);

    // 2. Добавляем смещение для линейного движения
    if (moveType == DoorMoveType::Linear) {
        float t = moveProgress;
        glm::vec3 currentOffset = glm::mix(startOrigin, endOrigin, t) - startOrigin;
        transform = glm::translate(transform, currentOffset);
    }

    // 3. Вращение (для вращающихся дверей)
    if (moveType == DoorMoveType::Rotating) {
        float t = moveProgress;
        glm::vec3 currentAngles = glm::mix(startAngles, endAngles, t);

        // Вращаем вокруг modelOrigin
        transform = glm::rotate(transform, glm::radians(currentAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(currentAngles.x), glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(currentAngles.z), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    return transform;
}