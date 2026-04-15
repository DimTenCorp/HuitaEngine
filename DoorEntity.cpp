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

    // Конвертация bounds BSP → OpenGL
    modelMins = glm::vec3(-model.max[0], model.min[2], model.min[1]);
    modelMaxs = glm::vec3(-model.min[0], model.max[2], model.max[1]);

    if (modelMins.x > modelMaxs.x) std::swap(modelMins.x, modelMaxs.x);
    if (modelMins.y > modelMaxs.y) std::swap(modelMins.y, modelMaxs.y);
    if (modelMins.z > modelMaxs.z) std::swap(modelMins.z, modelMaxs.z);

    modelOrigin = glm::vec3(-model.origin[0], model.origin[2], model.origin[1]);

    // Центр AABB как начальная позиция
    startOrigin = (modelMins + modelMaxs) * 0.5f;
    origin = startOrigin;

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

    if (entity.properties.count("angles")) {
        try {
            float ax, ay, az;
            sscanf_s(entity.properties.at("angles").c_str(), "%f %f %f", &ax, &ay, &az);
            angles = glm::vec3(ax, ay, az);
        }
        catch (...) {}
    }
    startAngles = angles;

    if (entity.properties.count("movedir")) {
        try {
            float dx, dy, dz;
            sscanf_s(entity.properties.at("movedir").c_str(), "%f %f %f", &dx, &dy, &dz);
            moveDir = glm::normalize(glm::vec3(dx, dy, dz));
        }
        catch (...) {}
    }
    else {
        float yaw = angles.y;
        moveDir = glm::vec3(cos(glm::radians(yaw)), 0.0f, sin(glm::radians(yaw)));
    }

    if (entity.properties.count("movesnd")) {
        try { moveSound = std::stoi(entity.properties.at("movesnd")); }
        catch (...) {}
    }
    if (entity.properties.count("stopsnd")) {
        try { stopSound = std::stoi(entity.properties.at("stopsnd")); }
        catch (...) {}
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

    calculateEndPosition(bsp);

    if (startOpen) {
        std::swap(startOrigin, endOrigin);
        std::swap(startAngles, endAngles);
        origin = endOrigin;
        angles = endAngles;
        state = DoorState::Open;
        moveProgress = 1.0f;
    }

    buildGeometry(bsp);
    buildBuffers();  // Создаём OpenGL буферы

    bounds.min = modelMins;
    bounds.max = modelMaxs;

    std::cout << "[DOOR] Created " << classname << " model*" << modelIndex
        << " speed=" << speed << " wait=" << waitTime
        << " type=" << (moveType == DoorMoveType::Linear ? "linear" : "rotating")
        << " verts=" << vertices.size() << std::endl;
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
        glm::vec3 worldNormal(-bspNormal.x, bspNormal.z, bspNormal.y);
        worldNormal = glm::normalize(worldNormal);

        unsigned int baseIdx = vertices.size();

        for (const auto& v : faceVerts) {
            BSPVertex bv;
            // === ИСПРАВЛЕНО: вершины в ЛОКАЛЬНЫХ координатах относительно modelOrigin
            // НЕ вычитаем modelOrigin здесь - делаем это в шейдере через матрицу
            bv.position = glm::vec3(-v.x, v.z, v.y);

            float s = v.x * texInfo.s[0] + v.y * texInfo.s[1] + v.z * texInfo.s[2] + texInfo.s[3];
            float t = v.x * texInfo.t[0] + v.y * texInfo.t[1] + v.z * texInfo.t[2] + texInfo.t[3];
            bv.texCoord = glm::vec2(s / texW, t / texH);
            bv.normal = worldNormal;

            vertices.push_back(bv);
        }

        for (size_t j = 1; j + 1 < faceVerts.size(); j++) {
            indices.push_back(baseIdx);
            indices.push_back(baseIdx + j + 1);
            indices.push_back(baseIdx + j);
        }
    }

    if (!textureFound) {
        textureID = bsp.getDefaultTextureID();
    }

    buffersDirty = true;
}

void DoorEntity::buildBuffers() {
    cleanupBuffers();  // На всякий случай

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
        vertices.size() * sizeof(BSPVertex),
        vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW);

    // layout(location = 0) position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location = 1) normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, normal));
    glEnableVertexAttribArray(1);

    // layout(location = 2) texCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, texCoord));
    glEnableVertexAttribArray(2);

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
        // Ease-out для плавности
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
        t = t * t;  // Ease-in

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

    // Обновляем bounds только если дверь движется или изменилась
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

void DoorEntity::activate() {
    if (state == DoorState::Closed || state == DoorState::Closing) {
        open();
    }
    else if (state == DoorState::Open || state == DoorState::Opening) {
        if (!noAutoReturn && waitTime >= 0) {
            stateTimer = 0.0f;  // Сброс таймера
        }
    }
}

void DoorEntity::open() {
    if (state == DoorState::Open || state == DoorState::Opening) return;
    state = DoorState::Opening;
    std::cout << "[DOOR] Opening" << std::endl;
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

    // === ИСПРАВЛЕНО: правильный порядок трансформаций ===
    // 1. Перенос в точку вращения (modelOrigin)
    transform = glm::translate(transform, modelOrigin);

    // 2. Линейное движение (только для linear дверей)
    if (moveType == DoorMoveType::Linear) {
        glm::vec3 currentOffset = glm::mix(glm::vec3(0.0f), endOrigin - startOrigin, moveProgress);
        transform = glm::translate(transform, currentOffset);
    }

    // 3. Вращение (только для rotating дверей)
    if (moveType == DoorMoveType::Rotating) {
        glm::vec3 currentAngles = glm::mix(startAngles, endAngles, moveProgress);
        transform = glm::rotate(transform, glm::radians(currentAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    return transform;
}