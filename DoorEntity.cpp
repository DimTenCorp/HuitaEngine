#include "pch.h"
#include "DoorEntity.h"
#include "TriangleCollider.h"
#include "BSPLoader.h"
#include "LightmapManager.h"  // <-- Добавлено
#include <iostream>
#include <algorithm>
#include <cmath>

static glm::vec3 convertHLAngles(const glm::vec3& angles) {
    return glm::vec3(-angles.x, angles.z, angles.y);
}

void DoorEntity::initFromEntity(const BSPEntity& entity, const BSPLoader& bsp, const LightmapManager* lmManager) {
    classname = entity.classname;
    entityProperties = entity.properties;

    glm::vec3 hlAngles = entity.angles;

    if (classname == "func_door_rotating") {
        moveType = DoorMoveType::Rotating;
    }
    else {
        moveType = DoorMoveType::Linear;
    }

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

    modelMins = glm::vec3(-model.max[0], model.min[2], model.min[1]);
    modelMaxs = glm::vec3(-model.min[0], model.max[2], model.max[1]);

    if (modelMins.x > modelMaxs.x) std::swap(modelMins.x, modelMaxs.x);
    if (modelMins.y > modelMaxs.y) std::swap(modelMins.y, modelMaxs.y);
    if (modelMins.z > modelMaxs.z) std::swap(modelMins.z, modelMaxs.z);

    modelOrigin = glm::vec3(-model.origin[0], model.origin[2], model.origin[1]);

    if (moveType == DoorMoveType::Rotating) {
        startOrigin = modelOrigin;
    }
    else {
        startOrigin = (modelMins + modelMaxs) * 0.5f;
    }

    origin = startOrigin;

    if (entity.origin != glm::vec3(0.0f)) {
        float dist = glm::distance(entity.origin, modelOrigin);
        if (dist > 1.0f) {
            std::cout << "[DOOR] Entity origin differs from modelOrigin by " << dist
                << ", using entity origin" << std::endl;
            origin = entity.origin;
            startOrigin = entity.origin;
        }
    }

    targetname = entity.properties.count("targetname") ? entity.properties.at("targetname") : "";

    if (entity.properties.count("speed")) {
        try { speed = std::stof(entity.properties.at("speed")); }
        catch (...) {}
    }
    if (speed <= 0) speed = 100.0f;

    if (entity.properties.count("wait")) {
        try {
            waitTime = std::stof(entity.properties.at("wait"));
            if (waitTime == -1) noAutoReturn = true;
        }
        catch (...) {}
    }

    if (entity.properties.count("lip")) {
        try { lip = std::stof(entity.properties.at("lip")); }
        catch (...) {}
    }
    else {
        lip = 0.0f;
    }

    if (entity.properties.count("dmg")) {
        try { damage = std::stof(entity.properties.at("dmg")); }
        catch (...) {}
    }

    // === Углы ===
    if (entity.properties.count("angles")) {
        try {
            float ax, ay, az;
            sscanf_s(entity.properties.at("angles").c_str(), "%f %f %f", &ax, &ay, &az);
            hlAngles = glm::vec3(ax, ay, az);
            angles = glm::vec3(-ax, az, ay);
        }
        catch (...) {}
    }
    startAngles = angles;

    // === Направление движения (только для linear) ===
    if (entity.properties.count("movedir")) {
        try {
            float dx, dy, dz;
            sscanf_s(entity.properties.at("movedir").c_str(), "%f %f %f", &dx, &dy, &dz);
            moveDir = glm::normalize(glm::vec3(-dx, dz, dy));
        }
        catch (...) {}
    }
    else if (moveType == DoorMoveType::Linear) {
        float pitch = hlAngles.x;
        float yaw = hlAngles.y;

        while (yaw < 0) yaw += 360.0f;
        while (yaw >= 360) yaw -= 360.0f;

        while (pitch > 180.0f) pitch -= 360.0f;
        while (pitch < -180.0f) pitch += 360.0f;

        if (std::abs(pitch) > 45.0f && std::abs(pitch) < 135.0f) {
            moveDir = glm::vec3(0.0f, (pitch < 0) ? 1.0f : -1.0f, 0.0f);
        }
        else {
            float rad = glm::radians(yaw - 90.0f);
            moveDir = glm::vec3(sin(rad), 0.0f, cos(rad));
        }
    }

    if (entity.properties.count("movesnd")) {
        try { moveSound = std::stoi(entity.properties.at("movesnd")); }
        catch (...) {}
    }
    if (entity.properties.count("stopsnd")) {
        try { stopSound = std::stoi(entity.properties.at("stopsnd")); }
        catch (...) {}
    }

    // === Проверяем distance - если задан, направление фиксировано ===
    if (entity.properties.count("distance")) {
        try {
            rotationAngle = std::stof(entity.properties.at("distance"));
            rotationDirectionFixed = true;
        }
        catch (...) {
            rotationAngle = 90.0f;
            rotationDirectionFixed = false;
        }
    }
    else {
        rotationAngle = 90.0f;
        rotationDirectionFixed = false;
    }

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

    // Рассчитываем конечную позицию/углы
    calculateEndPosition(bsp);

    // Если startOpen — меняем местами начальную и конечную позицию
    if (startOpen) {
        std::swap(startOrigin, endOrigin);
        std::swap(startAngles, endAngles);
        origin = endOrigin;
        angles = endAngles;
        state = DoorState::Open;
        moveProgress = 1.0f;
    }

    buildGeometry(bsp, lmManager);  // <-- Передаём lmManager
    buildBuffers();

    bounds.min = modelMins;
    bounds.max = modelMaxs;
    updateBounds();

    std::cout << "[DOOR] Created " << classname << " model*" << modelIndex
        << "\\n  Type: " << (moveType == DoorMoveType::Linear ? "linear" : "rotating")
        << "\\n  HL angles: (" << hlAngles.x << ", " << hlAngles.y << ", " << hlAngles.z << ")"
        << "\\n  moveDir: (" << moveDir.x << ", " << moveDir.y << ", " << moveDir.z << ")"
        << "\\n  startOrigin: (" << startOrigin.x << ", " << startOrigin.y << ", " << startOrigin.z << ")"
        << "\\n  endOrigin: (" << endOrigin.x << ", " << endOrigin.y << ", " << endOrigin.z << ")"
        << "\\n  rotationFixed: " << (rotationDirectionFixed ? "yes" : "no (dynamic)")
        << std::endl;
}

void DoorEntity::calculateEndPosition(const BSPLoader& bsp) {
    if (moveType == DoorMoveType::Linear) {
        glm::vec3 size = modelMaxs - modelMins;
        float projection = 0.0f;

        if (std::abs(moveDir.y) > 0.5f) {
            projection = size.y;
        }
        else {
            projection = std::abs(moveDir.x) * size.x + std::abs(moveDir.z) * size.z;
        }

        moveDistance = projection - lip;
        if (moveDistance < 0) moveDistance = projection * 0.8f;
        if (moveDistance < 1.0f) moveDistance = projection;

        endOrigin = startOrigin + moveDir * moveDistance;
    }
    else {
        // Для rotating дверей: если direction фиксирован, устанавливаем endAngles
        if (rotationDirectionFixed) {
            endAngles = startAngles + glm::vec3(0.0f, rotationAngle, 0.0f);
        }
        // Иначе endAngles пока не трогаем - определим при активации
    }
}

void DoorEntity::determineRotationDirection(const glm::vec3& playerPos) {
    if (moveType != DoorMoveType::Rotating) return;
    if (rotationDirectionFixed) return;

    glm::vec3 toPlayer = playerPos - origin;
    glm::vec3 toPlayerProj = toPlayer - rotationAxis * glm::dot(toPlayer, rotationAxis);

    if (glm::length(toPlayerProj) < 0.001f) {
        rotationAngle = 90.0f;
        return;
    }

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(rotationAxis, up);

    float side = glm::dot(glm::normalize(toPlayerProj), glm::normalize(right));
    rotationAngle = (side > 0) ? -90.0f : 90.0f;
}

// <-- ИСПРАВЛЕНО: Добавлена поддержка lightmap координат
void DoorEntity::buildGeometry(const BSPLoader& bsp, const LightmapManager* lmManager) {
    const auto& faces = bsp.getFaces();
    const auto& texInfos = bsp.getTexInfos();
    const auto& surfEdges = bsp.getSurfEdges();
    const auto& edges = bsp.getEdges();
    const auto& originalVertices = bsp.getOriginalVertices();
    const auto& planes = bsp.getPlanes();

    const BSPModel& model = bsp.getModels()[modelIndex];

    vertices.clear();
    indices.clear();

    GLuint firstTexture = 0;
    bool textureFound = false;

    for (int i = 0; i < model.numFaces; i++) {
        int faceIdx = model.firstFace + i;
        if (faceIdx < 0 || faceIdx >= (int)faces.size()) continue;

        const BSPFace& face = faces[faceIdx];
        if (face.numEdges < 3) continue;
        if (face.texInfo < 0 || face.texInfo >= (int)texInfos.size()) continue;

        const BSPTexInfo& texInfo = texInfos[face.texInfo];

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

        if (!textureFound) {
            int texIdx = texInfo.textureIndex;
            if (texIdx < 0 || texIdx >= (int)bsp.getTextureCount()) texIdx = 0;
            textureID = bsp.getTextureID(texIdx);
            textureFound = true;
        }

        glm::uvec2 texDim = bsp.getTextureDimensions(texInfo.textureIndex);
        int texW = texDim.x > 0 ? texDim.x : 256;
        int texH = texDim.y > 0 ? texDim.y : 256;

        glm::vec3 bspNormal = planes[face.planeNum].normal;
        if (face.side != 0) bspNormal = -bspNormal;
        glm::vec3 worldNormal = glm::normalize(glm::vec3(-bspNormal.x, bspNormal.z, bspNormal.y));

        // <-- НОВОЕ: Получаем lightmap информацию для этого face
        const FaceLightmap* lm = nullptr;
        if (lmManager) {
            lm = &lmManager->getFaceLightmap(faceIdx);
        }

        unsigned int baseIdx = (unsigned int)vertices.size();

        for (const auto& bspPos : faceVerts) {
            DoorVertex dv;  // <-- Используем DoorVertex вместо BSPVertex

            glm::vec3 worldPos = glm::vec3(-bspPos.x, bspPos.z, bspPos.y);
            dv.position = worldPos - modelOrigin;

            float s = bspPos.x * texInfo.s[0] + bspPos.y * texInfo.s[1]
                + bspPos.z * texInfo.s[2] + texInfo.s[3];
            float t = bspPos.x * texInfo.t[0] + bspPos.y * texInfo.t[1]
                + bspPos.z * texInfo.t[2] + texInfo.t[3];
            dv.texCoord = glm::vec2(s / texW, t / texH);

            dv.normal = worldNormal;

            // <-- НОВОЕ: Вычисляем lightmap UV координаты
            if (lm && lm->valid && lm->width > 0 && lm->height > 0) {
                // Преобразуем текстурные координаты в lightmap координаты
                float lmU = (s / 16.0f) - std::floor(lm->minS / 16.0f);
                float lmV = (t / 16.0f) - std::floor(lm->minT / 16.0f);

                // Нормализуем к размеру lightmap
                lmU = (lmU + 0.5f) / (float)lm->width;
                lmV = (lmV + 0.5f) / (float)lm->height;

                // Преобразуем в координаты атласа
                dv.lightmapCoord.x = lm->uvMin.x + lmU * (lm->uvMax.x - lm->uvMin.x);
                dv.lightmapCoord.y = lm->uvMin.y + lmV * (lm->uvMax.y - lm->uvMin.y);
            }
            else {
                // Fallback: используем маленький чёрный уголок атласа
                dv.lightmapCoord = glm::vec2(0.001f, 0.001f);
            }

            vertices.push_back(dv);
        }

        for (size_t j = 1; j + 1 < faceVerts.size(); j++) {
            indices.push_back(baseIdx);
            indices.push_back(baseIdx + (unsigned int)j + 1);
            indices.push_back(baseIdx + (unsigned int)j);
        }
    }

    if (!textureFound) {
        textureID = bsp.getDefaultTextureID();
    }

    buffersDirty = true;
}

void DoorEntity::buildBuffers() {
    cleanupBuffers();

    if (vertices.empty() || indices.empty()) return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    updateBuffers();
}

void DoorEntity::updateBuffers() {
    if (!buffersDirty) return;
    if (vao == 0) return;

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(DoorVertex),  // <-- Изменён размер
        vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW);

    // <-- ИСПРАВЛЕНО: Обновлены stride и offsets для DoorVertex
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DoorVertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DoorVertex),
        (void*)offsetof(DoorVertex, normal));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DoorVertex),
        (void*)offsetof(DoorVertex, texCoord));
    glEnableVertexAttribArray(2);

    // <-- НОВОЕ: Атрибут lightmapCoord (location = 3)
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(DoorVertex),
        (void*)offsetof(DoorVertex, lightmapCoord));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
    buffersDirty = false;
}

void DoorEntity::cleanupBuffers() {
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
}

void DoorEntity::update(float deltaTime, MeshCollider* worldCollider) {
    DoorState oldState = state;

    switch (state) {
    case DoorState::Closed:
        break;

    case DoorState::Opening: {
        float moveSpeed = speed;
        if (moveType == DoorMoveType::Linear) {
            moveSpeed = speed / moveDistance;
        }
        else {
            moveSpeed = speed / 90.0f;
        }

        moveProgress += moveSpeed * deltaTime;
        moveProgress = glm::clamp(moveProgress, 0.0f, 1.0f);

        float t = moveProgress;
        t = 1.0f - (1.0f - t) * (1.0f - t);

        if (moveType == DoorMoveType::Linear) {
            origin = glm::mix(startOrigin, endOrigin, t);
        }
        else {
            angles = glm::mix(startAngles, endAngles, t);
        }

        if (moveProgress >= 1.0f) {
            state = DoorState::Open;
            stateTimer = 0.0f;
            std::cout << "[DOOR] Fully opened" << std::endl;
        }
        break;
    }

    case DoorState::Open:
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

        float t = moveProgress;
        t = t * t;

        if (moveType == DoorMoveType::Linear) {
            origin = glm::mix(startOrigin, endOrigin, t);
        }
        else {
            angles = glm::mix(startAngles, endAngles, t);
        }

        if (moveProgress <= 0.0f) {
            state = DoorState::Closed;
            std::cout << "[DOOR] Fully closed" << std::endl;
        }
        break;
    }
    }

    if (oldState != state || state == DoorState::Opening || state == DoorState::Closing) {
        updateBounds();
    }
}

void DoorEntity::updateBounds() {
    glm::mat4 transform = getTransform();

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

void DoorEntity::activate(const glm::vec3& playerPos) {
    if (state == DoorState::Closed || state == DoorState::Closing) {
        open(playerPos);
    }
    else if (state == DoorState::Open || state == DoorState::Opening) {
        if (!noAutoReturn && waitTime >= 0) {
            stateTimer = 0.0f;
        }
    }
}

void DoorEntity::open(const glm::vec3& playerPos) {
    if (state == DoorState::Open || state == DoorState::Opening) return;

    if (moveType == DoorMoveType::Rotating) {
        determineRotationDirection(playerPos);
    }

    state = DoorState::Opening;
    std::cout << "[DOOR] Opening with angle: " << rotationAngle << std::endl;
}

void DoorEntity::close() {
    if (state == DoorState::Closed || state == DoorState::Closing) return;
    state = DoorState::Closing;
    std::cout << "[DOOR] Closing" << std::endl;
}

bool DoorEntity::intersectsPlayer(const glm::vec3& playerPos, const Capsule& playerCapsule) const {
    AABB expanded = bounds;
    expanded.min -= glm::vec3(playerCapsule.radius);
    expanded.max += glm::vec3(playerCapsule.radius);

    glm::vec3 closest = glm::clamp(playerPos, expanded.min, expanded.max);
    float dist = glm::length(playerPos - closest);

    return dist < playerCapsule.radius;
}

AABB DoorEntity::getCurrentBounds() const {
    return bounds;
}

glm::mat4 DoorEntity::getTransform() const {
    glm::mat4 transform = glm::mat4(1.0f);

    if (moveType == DoorMoveType::Rotating) {
        // Для rotating дверей
        transform = glm::translate(transform, origin);
        glm::vec3 currentAngles = glm::mix(startAngles, endAngles, moveProgress);
        transform = glm::rotate(transform, glm::radians(currentAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));

        if (currentAngles.x != 0.0f) {
            transform = glm::rotate(transform, glm::radians(currentAngles.x), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        if (currentAngles.z != 0.0f) {
            transform = glm::rotate(transform, glm::radians(currentAngles.z), glm::vec3(0.0f, 0.0f, 1.0f));
        }
    }
    else {
        // Для linear дверей
        glm::vec3 worldPos = modelOrigin + (origin - startOrigin);
        transform = glm::translate(transform, worldPos);
    }

    return transform;
}