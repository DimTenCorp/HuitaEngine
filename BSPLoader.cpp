#define _CRT_SECURE_NO_WARNINGS
#include "pch.h"
#include "BSPLoader.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cctype>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include "WADLoader.h"
#include "TriangleCollider.h"

static glm::vec3 convertPosition(const glm::vec3& bspPos) {
    return glm::vec3(-bspPos.x, bspPos.z, bspPos.y);
}

static glm::vec3 convertNormal(const glm::vec3& bspNormal) {
    return glm::vec3(-bspNormal.x, bspNormal.z, bspNormal.y);
}

static std::string toUpper(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\0", 0);
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r\0");
    return str.substr(start, end - start + 1);
}

BSPLoader::BSPLoader() {
    worldBounds.min = glm::vec3(0);
    worldBounds.max = glm::vec3(0);
}

BSPLoader::~BSPLoader() {
    cleanupTextures();
}

bool BSPLoader::loadVertices(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_VERTICES];
    if (lump.length == 0) return false;
    size_t count = lump.length / (3 * sizeof(float));

    vertices.resize(count);
    originalVertices.resize(count);

    fseek(file, lump.offset, SEEK_SET);
    for (size_t i = 0; i < count; i++) {
        float x, y, z;
        fread(&x, sizeof(float), 1, file);
        fread(&y, sizeof(float), 1, file);
        fread(&z, sizeof(float), 1, file);

        glm::vec3 original(x, y, z);
        originalVertices[i] = original;
        vertices[i] = convertPosition(original);
    }
    return true;
}

bool BSPLoader::loadEdges(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_EDGES];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(BSPEdge);
    edges.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(edges.data(), sizeof(BSPEdge), count, file);
    return true;
}

bool BSPLoader::loadSurfEdges(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_SURFEDGES];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(int);
    surfEdges.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(surfEdges.data(), sizeof(int), count, file);
    return true;
}

bool BSPLoader::loadFaces(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_FACES];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(BSPFace);
    faces.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(faces.data(), sizeof(BSPFace), count, file);
    return true;
}

bool BSPLoader::loadPlanes(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_PLANES];
    if (lump.length == 0) return false;
    size_t count = lump.length / (4 * sizeof(float) + sizeof(int));
    planes.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    for (size_t i = 0; i < count; i++) {
        float nx, ny, nz, dist;
        int type;
        fread(&nx, sizeof(float), 1, file);
        fread(&ny, sizeof(float), 1, file);
        fread(&nz, sizeof(float), 1, file);
        fread(&dist, sizeof(float), 1, file);
        fread(&type, sizeof(int), 1, file);
        planes[i].normal = convertNormal(glm::vec3(nx, ny, nz));
        planes[i].dist = dist;
        planes[i].type = type;
    }
    return true;
}

bool BSPLoader::loadModels(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_MODELS];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(BSPModel);
    models.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(models.data(), sizeof(BSPModel), count, file);

    if (!models.empty()) {
        worldBounds.min = glm::vec3(models[0].min[0], models[0].min[1], models[0].min[2]);
        worldBounds.max = glm::vec3(models[0].max[0], models[0].max[1], models[0].max[2]);
    }
    return true;
}

bool BSPLoader::loadTexInfo(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_TEXINFO];
    if (lump.length == 0) return false;
    size_t count = lump.length / sizeof(BSPTexInfo);
    texInfos.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(texInfos.data(), sizeof(BSPTexInfo), count, file);
    return true;
}

bool BSPLoader::loadRequiredWADsFromEntities() {
    requiredWADs.clear();

    for (const auto& entity : entities) {
        auto it = entity.properties.find("wad");
        if (it != entity.properties.end()) {
            std::string wadValue = it->second;

            size_t start = 0;
            size_t end;

            while (start < wadValue.length()) {
                while (start < wadValue.length() &&
                    (wadValue[start] == ';' || wadValue[start] == ' ' || wadValue[start] == '|')) {
                    start++;
                }
                if (start >= wadValue.length()) break;

                end = start;
                while (end < wadValue.length() &&
                    wadValue[end] != ';' && wadValue[end] != ' ' && wadValue[end] != '|') {
                    end++;
                }

                std::string wadName = wadValue.substr(start, end - start);

                if (!wadName.empty() && wadName.front() == '"') {
                    wadName = wadName.substr(1);
                }
                if (!wadName.empty() && wadName.back() == '"') {
                    wadName.pop_back();
                }

                size_t slashPos = wadName.find_last_of("/\\");
                if (slashPos != std::string::npos) {
                    wadName = wadName.substr(slashPos + 1);
                }

                if (!wadName.empty() && wadName != " " && wadName.length() > 2) {
                    requiredWADs.push_back(wadName);
                    std::cout << "[BSP] Required WAD from BSP: " << wadName << std::endl;
                }

                start = end;
            }
        }
    }

    std::sort(requiredWADs.begin(), requiredWADs.end());
    requiredWADs.erase(std::unique(requiredWADs.begin(), requiredWADs.end()), requiredWADs.end());

    return !requiredWADs.empty();
}

void BSPLoader::getFaceLightmapDims(int faceIndex, int& width, int& height, float& minS, float& minT) const {
    if (faceIndex < 0 || faceIndex >= (int)faces.size()) {
        width = 1;
        height = 1;
        minS = minT = 0.0f;
        return;
    }

    const BSPFace& face = faces[faceIndex];

    if (face.texInfo < 0 || face.texInfo >= (int)texInfos.size()) {
        width = 1;
        height = 1;
        minS = minT = 0.0f;
        return;
    }

    const BSPTexInfo& tex = texInfos[face.texInfo];

    float sMin = 1000000.0f, sMax = -1000000.0f;
    float tMin = 1000000.0f, tMax = -1000000.0f;

    for (int i = 0; i < face.numEdges; i++) {
        int edgeIndex = face.firstEdge + i;

        if (edgeIndex < 0 || edgeIndex >= (int)surfEdges.size()) {
            continue;
        }

        int surfEdge = surfEdges[edgeIndex];
        int vIndex;

        if (surfEdge >= 0) {
            if (surfEdge >= (int)edges.size()) {
                continue;
            }
            vIndex = edges[surfEdge].v[0];
        }
        else {
            int negEdge = -surfEdge;
            if (negEdge < 0 || negEdge >= (int)edges.size()) {
                continue;
            }
            vIndex = edges[negEdge].v[1];
        }

        if (vIndex < 0 || vIndex >= (int)originalVertices.size()) {
            continue;
        }

        const glm::vec3& v = originalVertices[vIndex];

        glm::vec3 sAxis(tex.s[0], tex.s[1], tex.s[2]);
        glm::vec3 tAxis(tex.t[0], tex.t[1], tex.t[2]);

        float s = glm::dot(v, sAxis) + tex.s[3];
        float t = glm::dot(v, tAxis) + tex.t[3];

        sMin = std::min(sMin, s);
        sMax = std::max(sMax, s);
        tMin = std::min(tMin, t);
        tMax = std::max(tMax, t);
    }

    minS = sMin;
    minT = tMin;

    width = static_cast<int>(std::ceil(sMax / 16.0f) - std::floor(sMin / 16.0f)) + 1;
    height = static_cast<int>(std::ceil(tMax / 16.0f) - std::floor(tMin / 16.0f)) + 1;

    width = std::max(1, std::min(width, 512));
    height = std::max(1, std::min(height, 512));
}

bool BSPLoader::loadTextures(FILE* file, const BSPHeader& header, WADLoader& wadLoader) {
    const BSPLump& lump = header.lumps[LUMP_TEXTURES];
    if (lump.length == 0) return false;

    const size_t MAX_TEXTURE_LUMP_SIZE = 64 * 1024 * 1024;
    if (lump.length > MAX_TEXTURE_LUMP_SIZE) {
        std::cerr << "[BSP] Texture lump too large: " << lump.length << " bytes\n";
        return false;
    }

    std::vector<uint8_t> lumpData(lump.length);
    fseek(file, lump.offset, SEEK_SET);
    if (fread(lumpData.data(), 1, lump.length, file) != lump.length) {
        std::cerr << "[BSP] Failed to read texture lump data.\n";
        return false;
    }

    if (lumpData.size() < sizeof(int)) {
        std::cerr << "[BSP] Texture lump data too small.\n";
        return false;
    }

    int numTextures = 0;
    memcpy(&numTextures, lumpData.data(), sizeof(int));
    if (numTextures <= 0 || numTextures > 100000) {
        std::cerr << "[BSP] Invalid number of textures: " << numTextures << "\n";
        return false;
    }

    size_t offsetsSize = numTextures * sizeof(int);
    if (sizeof(int) + offsetsSize > lumpData.size()) {
        std::cerr << "[BSP] Texture offsets array out of bounds.\n";
        return false;
    }

    const int* offsets = reinterpret_cast<const int*>(lumpData.data() + sizeof(int));

    defaultTextureId = wadLoader.getDefaultTexture();
    glTextureIds.reserve(numTextures);
    textureDimensions.reserve(numTextures);
    textureNames.reserve(numTextures);

    int foundCount = 0;
    int notFoundCount = 0;

    for (int i = 0; i < numTextures; ++i) {
        if (offsets[i] == -1 || offsets[i] < 0 || offsets[i] >= (int)lump.length) {
            glTextureIds.push_back(defaultTextureId);
            textureDimensions.push_back({ 16, 16 });
            textureNames.push_back("");
            notFoundCount++;
            continue;
        }

        const uint8_t* texPtr = lumpData.data() + offsets[i];

        if (texPtr + 16 > lumpData.data() + lumpData.size()) {
            glTextureIds.push_back(defaultTextureId);
            textureDimensions.push_back({ 16, 16 });
            textureNames.push_back("");
            notFoundCount++;
            continue;
        }

        char rawName[17] = { 0 };
        memcpy(rawName, texPtr, 16);

        std::string name;
        for (int j = 0; j < 16; j++) {
            if (rawName[j] == '\0' || rawName[j] == ' ') {
                break;
            }
            name += rawName[j];
        }

        if (name.empty()) {
            glTextureIds.push_back(defaultTextureId);
            textureDimensions.push_back({ 16, 16 });
            textureNames.push_back("");
            notFoundCount++;
            continue;
        }

        uint32_t width = 256;
        uint32_t height = 256;
        size_t texDataSize = lumpData.size() - offsets[i];

        if (texDataSize >= 24) {
            memcpy(&width, texPtr + 16, sizeof(uint32_t));
            memcpy(&height, texPtr + 20, sizeof(uint32_t));
        }

        if (width == 0 || height == 0 || width > 4096 || height > 4096) {
            width = 256;
            height = 256;
        }

        GLuint texId = wadLoader.getTexture(name);

        if (texId != defaultTextureId) {
            foundCount++;
        }
        else {
            notFoundCount++;
            static int missingPrinted = 0;
            if (missingPrinted < 30) {
                std::cout << "[BSP] Texture not found in WADs: \"" << name << "\"" << std::endl;
                missingPrinted++;
            }
        }

        glTextureIds.push_back(texId);
        textureDimensions.push_back({ width, height });
        textureNames.push_back(name);
    }

    std::cout << "[BSP] Textures mapped: " << glTextureIds.size()
        << " (found: " << foundCount << ", missing: " << notFoundCount << ")" << std::endl;

    return true;
}

bool BSPLoader::parseEntities(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_ENTITIES];
    if (lump.length == 0) return false;
    std::string buffer;
    buffer.resize(lump.length);
    fseek(file, lump.offset, SEEK_SET);
    fread(&buffer[0], 1, lump.length, file);

    size_t pos = 0;
    while (pos < buffer.size()) {
        while (pos < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[pos]))) pos++;
        if (pos >= buffer.size() || buffer[pos] != '{') break;
        pos++;

        BSPEntity entity;
        while (pos < buffer.size()) {
            while (pos < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[pos]))) pos++;
            if (buffer[pos] == '}') { pos++; break; }
            if (buffer[pos] != '"') { pos++; continue; }
            pos++;
            size_t keyStart = pos;
            while (pos < buffer.size() && buffer[pos] != '"') pos++;
            std::string key = buffer.substr(keyStart, pos - keyStart);
            pos++;

            while (pos < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[pos]))) pos++;
            if (buffer[pos] != '"') { pos++; continue; }
            pos++;
            size_t valStart = pos;
            while (pos < buffer.size() && buffer[pos] != '"') pos++;
            std::string value = buffer.substr(valStart, pos - valStart);
            pos++;

            entity.properties[key] = value;
            if (key == "classname") entity.classname = value;
            if (key == "model") entity.model = value;

            if (key == "origin") {
                float ox = 0, oy = 0, oz = 0;
                int parsed = sscanf(value.c_str(), "%f %f %f", &ox, &oy, &oz);
                if (parsed == 3) {
                    entity.origin = glm::vec3(-ox, oz, oy);
                }
            }

            if (key == "angles") {
                float ax = 0, ay = 0, az = 0;
                int parsed = sscanf(value.c_str(), "%f %f %f", &ax, &ay, &az);
                if (parsed != 3) {
                    std::cerr << "[BSP] Failed to parse angles: \"" << value << "\"\n";
                    entity.angles = glm::vec3(0);
                }
                else {
                    entity.angles = glm::vec3(ax, ay, az);
                }
            }
        }
        entities.push_back(entity);
    }
    return true;
}

void BSPLoader::buildSubmodelMesh(const BSPModel& subModel, int rendermode, int renderamt, bool isWaterModel, bool isLadderModel) {
    unsigned int baseVertex = (unsigned int)meshVertices.size();

    int modelIdx = -1;
    for (size_t i = 0; i < models.size(); i++) {
        if (&models[i] == &subModel) {
            modelIdx = (int)i;
            break;
        }
    }

    // Пропускаем двери - они будут динамическими
    if (modelIdx > 0 && isModelDoor(modelIdx)) {
        std::cout << "[BSP] Skipping door model *" << modelIdx << " from static mesh" << std::endl;
        return;
    }

    // === ИСПРАВЛЕНИЕ: Пропускаем лестницы - они триггеры, без коллизии ===
    if (isLadderModel) {
        std::cout << "[BSP] Skipping ladder model *" << modelIdx << " from static collision mesh (trigger only)" << std::endl;
        // Не возвращаемся - если нужна визуализация, можно построить меш но пометить как isLadder
        // Но для чистоты - пропускаем полностью, коллизии не будет
        return;
    }

    for (int i = 0; i < subModel.numFaces; i++) {
        int faceIdx = subModel.firstFace + i;
        if (faceIdx < 0 || faceIdx >= (int)faces.size()) continue;

        const BSPFace& face = faces[faceIdx];
        if (face.numEdges < 3) continue;
        if (face.planeNum >= planes.size()) continue;
        if (face.texInfo >= texInfos.size()) continue;

        const BSPTexInfo& texInfo = texInfos[face.texInfo];

        // === НОВОЕ: Пропускаем триггерные текстуры ===
        if (isTriggerTexture(texInfo.textureIndex)) {
            continue; // Не рендерим AAATRIGGER, ORIGIN и т.д.
        }

        glm::vec3 bspNormal = planes[face.planeNum].normal;
        if (face.side != 0) bspNormal = -bspNormal;
        glm::vec3 worldNormal = convertNormal(bspNormal);

        std::vector<glm::vec3> faceBspVerts;
        faceBspVerts.reserve(face.numEdges);

        for (int j = 0; j < face.numEdges; j++) {
            int edgeIdx = face.firstEdge + j;
            if (edgeIdx < 0 || edgeIdx >= (int)surfEdges.size()) break;

            int surfEdge = surfEdges[edgeIdx];
            unsigned short vindex;

            if (surfEdge >= 0) {
                if (surfEdge >= (int)edges.size()) break;
                vindex = edges[surfEdge].v[0];
            }
            else {
                int negEdge = -surfEdge;
                if (negEdge < 0 || negEdge >= (int)edges.size()) break;
                vindex = edges[negEdge].v[1];
            }

            if (vindex >= originalVertices.size()) continue;
            faceBspVerts.push_back(originalVertices[vindex]);
        }

        if (faceBspVerts.size() < 3) continue;

        int texIdx = texInfo.textureIndex;
        if (texIdx < 0 || texIdx >= (int)glTextureIds.size()) texIdx = 0;

        GLuint textureID = glTextureIds[texIdx];
        int texWidth = textureDimensions[texIdx].x;
        int texHeight = textureDimensions[texIdx].y;
        if (texWidth <= 0 || texHeight <= 0) {
            texWidth = texHeight = 256;
        }

        const std::string& texName = getTextureName(texIdx);
        bool isSky = (texName == "sky" || texName == "SKY");

        std::vector<BSPVertex> faceMeshVerts;
        faceMeshVerts.reserve(faceBspVerts.size());

        for (const auto& bspPos : faceBspVerts) {
            BSPVertex v;

            v.position = convertPosition(bspPos);
            v.normal = worldNormal;

            float s = bspPos.x * texInfo.s[0] + bspPos.y * texInfo.s[1]
                + bspPos.z * texInfo.s[2] + texInfo.s[3];
            float t = bspPos.x * texInfo.t[0] + bspPos.y * texInfo.t[1]
                + bspPos.z * texInfo.t[2] + texInfo.t[3];

            v.texCoord = glm::vec2(s / texWidth, t / texHeight);

            faceMeshVerts.push_back(v);
        }

        AABB faceBounds;
        faceBounds.min = faceBounds.max = faceMeshVerts[0].position;
        for (const auto& v : faceMeshVerts) {
            faceBounds.min = glm::min(faceBounds.min, v.position);
            faceBounds.max = glm::max(faceBounds.max, v.position);
        }
        float epsilon = 0.1f;
        faceBounds.min -= glm::vec3(epsilon);
        faceBounds.max += glm::vec3(epsilon);
        faceBoundingBoxes.push_back(faceBounds);

        unsigned int faceStartVertex = baseVertex;
        for (auto& v : faceMeshVerts) {
            meshVertices.push_back(v);
        }

        unsigned int startIdxOffset = (unsigned int)meshIndices.size();
        for (size_t j = 1; j + 1 < faceMeshVerts.size(); j++) {
            meshIndices.push_back(faceStartVertex);
            meshIndices.push_back(faceStartVertex + (unsigned int)j + 1);
            meshIndices.push_back(faceStartVertex + (unsigned int)j);
        }

        FaceDrawCall dc;
        dc.texID = textureID;
        dc.indexOffset = startIdxOffset;
        dc.indexCount = (unsigned int)((faceMeshVerts.size() - 2) * 3);
        dc.faceIndex = faceIdx;

        dc.rendermode = static_cast<unsigned char>(rendermode);
        dc.renderamt = static_cast<unsigned char>(renderamt);

        dc.isWater = isWaterModel;
        dc.isSky = isSky;
        dc.isLadder = isLadderModel;  // Установим флаг (хотя для лестниц мы уже вышли выше)

        // === ИСПРАВЛЕННАЯ ЛОГИКА ПРОЗРАЧНОСТИ ===
        // rendermode: 0=Normal, 1=Color, 2=Texture, 3=Glow, 4=Solid, 5=Additive
        if (rendermode == 0) {
            // Normal - непрозрачный, независимо от renderamt
            dc.isTransparent = false;
        }
        else if (rendermode == 1) {
            // Color - прозрачность от renderamt
            dc.isTransparent = (renderamt < 255);
        }
        else if (rendermode == 2) {
            // Texture - всегда прозрачный (классическая вода HL)
            dc.isTransparent = true;
        }
        else if (rendermode == 3) {
            // Glow - аддитивное смешивание
            dc.isTransparent = true;
        }
        else if (rendermode == 4) {
            // Solid - непрозрачный
            dc.isTransparent = false;
        }
        else if (rendermode == 5) {
            // Additive - всегда прозрачный
            dc.isTransparent = true;
        }
        else {
            // Неизвестный режим - используем renderamt
            dc.isTransparent = (renderamt < 255);
        }

        drawCalls.push_back(dc);
        baseVertex += (unsigned int)faceMeshVerts.size();
    }
}

void BSPLoader::buildMesh() {
    meshVertices.clear();
    meshIndices.clear();
    faceBoundingBoxes.clear();
    drawCalls.clear();
    if (models.empty()) return;

    // Модель 0 - это worldspawn, всегда непрозрачная
    buildSubmodelMesh(models[0], 0, 255, false, false);

    // Обрабатываем суб-модели из энтитей
    for (const auto& entity : entities) {
        if (entity.model.empty() || entity.model == "*0") continue;
        if (entity.model.length() < 2 || entity.model[0] != '*') continue;

        int modelIndex = 0;
        try {
            modelIndex = std::stoi(entity.model.substr(1));
        }
        catch (const std::exception& e) {
            std::cerr << "[BSP] Failed to parse model index from \"" << entity.model << "\": " << e.what() << std::endl;
            continue;
        }

        if (modelIndex <= 0 || modelIndex >= (int)models.size()) continue;

        // === ИСПРАВЛЕНИЕ: Пропускаем trigger_ и func_ladder (нет коллизии) ===
        if (entity.classname.find("trigger_") == 0 || entity.classname == "func_ladder") {
            std::cout << "[BSP] Skipping trigger/ladder entity: " << entity.classname << " model *" << modelIndex << std::endl;
            continue;
        }

        int rendermode = 0;
        int renderamt = 255;
        bool isWater = false;
        bool isLadder = false;  // Флаг для лестницы

        if (entity.classname == "func_water") {
            isWater = true;

            // Читаем rendermode из свойств карты (по умолчанию 0 = Normal)
            auto it = entity.properties.find("rendermode");
            if (it != entity.properties.end()) {
                try {
                    rendermode = std::stoi(it->second);
                    std::cout << "[BSP] Water *" << modelIndex << " rendermode from map: " << rendermode << std::endl;
                }
                catch (...) {
                    rendermode = 0;
                }
            }
            else {
                std::cout << "[BSP] Water *" << modelIndex << " no rendermode specified, using default 0 (Normal)" << std::endl;
            }

            // Читаем renderamt из свойств карты (по умолчанию 255)
            it = entity.properties.find("renderamt");
            if (it != entity.properties.end()) {
                try {
                    renderamt = std::stoi(it->second);
                    std::cout << "[BSP] Water *" << modelIndex << " renderamt from map: " << renderamt << std::endl;
                }
                catch (...) {
                    renderamt = 255;
                }
            }

            // Альтернативное имя ключа (FX Amount)
            it = entity.properties.find("FX Amount");
            if (it != entity.properties.end()) {
                try {
                    int fxAmt = std::stoi(it->second);
                    // Используем FX Amount только если renderamt не был задан явно
                    if (renderamt == 255 && entity.properties.find("renderamt") == entity.properties.end()) {
                        renderamt = fxAmt;
                        std::cout << "[BSP] Water *" << modelIndex << " FX Amount: " << renderamt << std::endl;
                    }
                }
                catch (...) {}
            }

            // Ограничиваем диапазон
            renderamt = std::max(0, std::min(255, renderamt));

            std::cout << "[BSP] Final water *" << modelIndex << " settings: rendermode="
                << rendermode << ", renderamt=" << renderamt << std::endl;
        }
        // === ИСПРАВЛЕНИЕ: func_ladder обрабатывается как триггер, без коллизии ===
        else if (entity.classname == "func_ladder") {
            // Лестницы пропускаются выше по continue, сюда не попадаем
            // Но если убрать continue выше - можно добавить обработку здесь
            isLadder = true;
            // Не строим меш для лестницы - она чистый триггер
            continue;
        }
        else {
            // Для не-воды читаем стандартные свойства
            auto it = entity.properties.find("rendermode");
            if (it != entity.properties.end()) {
                try { rendermode = std::stoi(it->second); }
                catch (...) {}
            }
            it = entity.properties.find("renderamt");
            if (it != entity.properties.end()) {
                try { renderamt = std::stoi(it->second); }
                catch (...) {}
            }
            it = entity.properties.find("FX Amount");
            if (it != entity.properties.end()) {
                try { renderamt = std::stoi(it->second); }
                catch (...) {}
            }
            renderamt = std::max(0, std::min(255, renderamt));
        }

        buildSubmodelMesh(models[modelIndex], rendermode, renderamt, isWater, isLadder);
    }

    int transparentCount = 0;
    int waterCount = 0;
    int skyCount = 0;
    int normalWaterCount = 0;  // Вода с rendermode=0
    int textureWaterCount = 0; // Вода с rendermode=2
    int ladderCount = 0;       // <-- ДОБАВЛЕНО: счетчик лестниц

    for (const auto& dc : drawCalls) {
        if (dc.isTransparent) transparentCount++;
        if (dc.isWater) {
            waterCount++;
            if (dc.rendermode == 0) normalWaterCount++;
            if (dc.rendermode == 2) textureWaterCount++;
        }
        if (dc.isLadder) ladderCount++;  // <-- ДОБАВЛЕНО
        if (dc.isSky) skyCount++;
    }

    std::cout << "Built " << meshVertices.size() << " mesh vertices, "
        << meshIndices.size() << " indices, "
        << drawCalls.size() << " draw calls\n";
    std::cout << "  - Opaque: " << (drawCalls.size() - transparentCount - skyCount)
        << ", Transparent: " << transparentCount
        << ", Water: " << waterCount << " (Normal: " << normalWaterCount << ", Texture: " << textureWaterCount << ")"
        << ", Ladders: " << ladderCount  // <-- ДОБАВЛЕНО
        << ", Sky: " << skyCount << "\n";
}

bool BSPLoader::isTriggerTexture(int textureIndex) const {
    if (textureIndex < 0 || textureIndex >= (int)textureNames.size()) return false;

    const std::string& name = textureNames[textureIndex];
    std::string upperName = toUpper(name);

    // Текстуры, которые не должны рендериться
    return (upperName == "AAATRIGGER" ||
        upperName == "ORIGIN" ||
        upperName == "CLIP" ||
        upperName == "NULL" ||
        upperName == "HINT" ||
        upperName == "SKIP");
}

bool BSPLoader::load(const std::string& filename, WADLoader& wadLoader) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) { std::cerr << "Cannot open BSP file: " << filename << "\n"; return false; }

    BSPHeader header;
    if (fread(&header, sizeof(BSPHeader), 1, file) != 1) {
        std::cerr << "Failed to read BSP header\n"; fclose(file); return false;
    }
    if (header.version != 29 && header.version != 30) {
        std::cerr << "Unsupported BSP version: " << header.version << "\n"; fclose(file); return false;
    }

    loadVertices(file, header);
    loadEdges(file, header);
    loadSurfEdges(file, header);
    loadFaces(file, header);
    loadPlanes(file, header);
    loadModels(file, header);
    loadTexInfo(file, header);
    parseEntities(file, header);
    loadLighting(file, header);
    
    // Загружаем данные для VIS
    loadNodes(file, header);
    loadLeaves(file, header);
    loadVisibility(file, header);

    loadRequiredWADsFromEntities();

    wadLoader.init();
    wadLoader.loadAllWADs();

    loadTextures(file, header, wadLoader);

    // Ищем skyname в worldspawn
    for (const auto& entity : entities) {
        if (entity.classname == "worldspawn") {
            auto it = entity.properties.find("skyname");
            if (it != entity.properties.end()) {
                skyName = it->second;
                std::cout << "[BSP] Sky name: " << skyName << std::endl;
            }
            break;
        }
    }

    fclose(file);
    buildMesh();
    printStats();
    return !meshVertices.empty();
}

void BSPLoader::cleanupTextures() {
    glTextureIds.clear();
    textureDimensions.clear();
    textureNames.clear();
    drawCalls.clear();
    defaultTextureId = 0;
}

void BSPLoader::printStats() const {
    std::cout << "=== BSP Loaded ===\n";
    std::cout << "Vertices: " << vertices.size() << "\n";
    std::cout << "Original Vertices: " << originalVertices.size() << "\n";
    std::cout << "Faces: " << faces.size() << "\n";
    std::cout << "Textures: " << glTextureIds.size() << "\n";
    std::cout << "Draw Calls: " << drawCalls.size() << "\n";
    std::cout << "Required WADs: " << requiredWADs.size() << "\n";
    std::cout << "Entities: " << entities.size() << "\n";
    std::cout << "Sky: " << (skyName.empty() ? "none" : skyName) << "\n";
    std::cout << "==================\n";
}

bool BSPLoader::loadLighting(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_LIGHTING];
    if (lump.length == 0) return false;

    lightingData.resize(lump.length);
    fseek(file, lump.offset, SEEK_SET);
    fread(lightingData.data(), 1, lump.length, file);

    std::cout << "[BSP] Loaded lighting: " << lump.length << " bytes" << std::endl;
    return true;
}

bool BSPLoader::findPlayerStart(glm::vec3& outPosition, glm::vec3& outAngles) const {
    std::vector<std::string> spawnClasses = {
        "info_player_start",
        "info_player_deathmatch",
        "info_player_coop",
        "info_player_team1",
        "info_player_team2",
        "info_player_axis",
        "info_player_allies",
        "player_start"
    };

    for (const auto& className : spawnClasses) {
        for (const auto& entity : entities) {
            if (entity.classname == className) {
                outPosition = entity.origin;
                outAngles = entity.angles;

                std::cout << "[BSP] Found player spawn: " << className
                    << " at (" << outPosition.x << ", " << outPosition.y << ", " << outPosition.z << ")"
                    << " angles (" << outAngles.x << ", " << outAngles.y << ", " << outAngles.z << ")"
                    << std::endl;
                return true;
            }
        }
    }

    outPosition = glm::vec3(0.0f, 100.0f, 0.0f);
    outAngles = glm::vec3(0.0f, 0.0f, 0.0f);
    std::cout << "[BSP] No player start found, using fallback position" << std::endl;
    return false;
}

std::vector<BSPEntity> BSPLoader::getEntitiesByClass(const std::string& classname) const {
    std::vector<BSPEntity> result;
    for (const auto& entity : entities) {
        if (entity.classname == classname) {
            result.push_back(entity);
        }
    }
    return result;
}

void BSPLoader::debugPrintEntities() const {
    std::cout << "=== BSP Entities (" << entities.size() << ") ===" << std::endl;
    for (const auto& entity : entities) {
        std::cout << "  Class: " << entity.classname;
        if (entity.classname == "info_player_start" ||
            entity.classname == "info_player_deathmatch") {
            std::cout << " at (" << entity.origin.x << ", "
                << entity.origin.y << ", " << entity.origin.z << ")";
        }
        std::cout << std::endl;
    }
}

bool BSPLoader::isModelDoor(int modelIndex) const {
    std::string modelStr = "*" + std::to_string(modelIndex);
    for (const auto& ent : entities) {
        if ((ent.classname == "func_door" || ent.classname == "func_door_rotating")
            && ent.model == modelStr) {
            return true;
        }
    }
    return false;
}

// ==================== VIS LOADING ====================

bool BSPLoader::loadNodes(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_NODES];
    if (lump.length == 0) return false;
    
    size_t count = lump.length / sizeof(BSPNode);
    nodes.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    fread(nodes.data(), sizeof(BSPNode), count, file);
    
    std::cout << "[BSP] Loaded " << count << " nodes" << std::endl;
    return true;
}

bool BSPLoader::loadLeaves(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_LEAVES];
    if (lump.length == 0) return false;
    
    // Размер листа BSP: 28 байт
    size_t count = lump.length / 28;
    leaves.resize(count);
    fseek(file, lump.offset, SEEK_SET);
    
    for (size_t i = 0; i < count; i++) {
        BSPLeaf leaf;
        fread(&leaf.contents, sizeof(int), 1, file);
        fread(&leaf.cluster, sizeof(int), 1, file);
        fread(&leaf.min[0], sizeof(short), 3, file);
        fread(&leaf.max[0], sizeof(short), 3, file);
        fread(&leaf.firstMarkSurface, sizeof(unsigned short), 1, file);
        fread(&leaf.numMarkSurfaces, sizeof(unsigned short), 1, file);
        fread(&leaf.area, sizeof(unsigned char), 1, file);
        fread(&leaf.numPortals, sizeof(unsigned char), 1, file);
        leaves[i] = leaf;
    }
    
    std::cout << "[BSP] Loaded " << count << " leaves" << std::endl;
    return true;
}

bool BSPLoader::loadVisibility(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_VISIBILITY];
    if (lump.length == 0) {
        std::cout << "[BSP] No VIS data found" << std::endl;
        return false;
    }
    
    rawVisData.resize(lump.length);
    fseek(file, lump.offset, SEEK_SET);
    fread(rawVisData.data(), 1, lump.length, file);
    
    parseVISData();
    
    std::cout << "[BSP] Loaded VIS data: " << lump.length << " bytes, " 
              << numVisClusters << " clusters" << std::endl;
    return true;
}

void BSPLoader::parseVISData() {
    if (rawVisData.empty()) return;
    
    size_t offset = 0;
    if (offset + 4 > rawVisData.size()) return;
    
    memcpy(&numVisClusters, rawVisData.data() + offset, sizeof(int));
    offset += 4;
    
    if (numVisClusters <= 0 || numVisClusters > 10000) {
        std::cerr << "[BSP] Invalid number of VIS clusters: " << numVisClusters << std::endl;
        numVisClusters = 0;
        return;
    }
    
    visClusters.resize(numVisClusters);
    
    for (int i = 0; i < numVisClusters; i++) {
        if (offset >= rawVisData.size()) break;
        
        int rowSize = rawVisData[offset++];
        
        visClusters[i].visibilityBits.resize(rowSize);
        for (int j = 0; j < rowSize; j++) {
            if (offset + j < rawVisData.size()) {
                visClusters[i].visibilityBits[j] = rawVisData[offset + j];
            }
        }
        offset += rowSize;
        
        // Парсим битовую маску в список видимых кластеров
        visClusters[i].visibleClusters.clear();
        visClusters[i].numVisibleClusters = 0;
        
        for (int cluster = 0; cluster < numVisClusters; cluster++) {
            int byteIndex = cluster / 8;
            int bitIndex = cluster % 8;
            
            if (byteIndex < (int)visClusters[i].visibilityBits.size()) {
                if (visClusters[i].visibilityBits[byteIndex] & (1 << bitIndex)) {
                    visClusters[i].visibleClusters.push_back(cluster);
                    visClusters[i].numVisibleClusters++;
                }
            }
        }
        
        // Кластер всегда видит сам себя
        bool seesSelf = false;
        for (int c : visClusters[i].visibleClusters) {
            if (c == i) { seesSelf = true; break; }
        }
        if (!seesSelf) {
            visClusters[i].visibleClusters.push_back(i);
            visClusters[i].numVisibleClusters++;
        }
    }
    
    // Строим маппинг face -> cluster
    buildFaceToClusterMapping();
}

void BSPLoader::buildFaceToClusterMapping() {
    faceToCluster.assign(faces.size(), -1);
    
    for (size_t leafIdx = 0; leafIdx < leaves.size(); leafIdx++) {
        const BSPLeaf& leaf = leaves[leafIdx];
        int cluster = leaf.cluster;
        
        if (cluster < 0 || cluster >= numVisClusters) continue;
        
        for (int i = 0; i < leaf.numMarkSurfaces; i++) {
            int markSurfaceIndex = leaf.firstMarkSurface + i;
            if (markSurfaceIndex < 0 || markSurfaceIndex >= (int)surfEdges.size()) continue;
            
            // Получаем индекс поверхности из marksurfaces
            // Но нам нужно сопоставить с faces... marksurfaces содержит индексы faces
            // В Quake/Half-Life marksurfaces содержит индексы в массиве faces
        }
    }
    
    // Альтернативный подход: для каждого face ищем leaf который его содержит
    for (size_t faceIdx = 0; faceIdx < faces.size(); faceIdx++) {
        int bestLeaf = -1;
        float bestDist = 1e10f;
        
        // Находим лист, содержащий центр фейса
        glm::vec3 center(0);
        int vertexCount = 0;
        
        const BSPFace& face = faces[faceIdx];
        for (int i = 0; i < face.numEdges; i++) {
            int edgeIdx = face.firstEdge + i;
            if (edgeIdx < 0 || edgeIdx >= (int)surfEdges.size()) continue;
            
            int surfEdge = surfEdges[edgeIdx];
            int vIdx = (surfEdge >= 0) ? edges[surfEdge].v[0] : edges[-surfEdge].v[1];
            
            if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                center += vertices[vIdx];
                vertexCount++;
            }
        }
        
        if (vertexCount > 0) {
            center /= (float)vertexCount;
            
            // Ищем лист содержащий эту точку
            for (size_t leafIdx = 0; leafIdx < leaves.size(); leafIdx++) {
                const BSPLeaf& leaf = leaves[leafIdx];
                
                glm::vec3 leafMin(leaf.min[0], leaf.min[1], leaf.min[2]);
                glm::vec3 leafMax(leaf.max[0], leaf.max[1], leaf.max[2]);
                
                if (center.x >= leafMin.x && center.x <= leafMax.x &&
                    center.y >= leafMin.y && center.y <= leafMax.y &&
                    center.z >= leafMin.z && center.z <= leafMax.z) {
                    
                    if (leaf.cluster >= 0 && leaf.cluster < numVisClusters) {
                        faceToCluster[faceIdx] = leaf.cluster;
                        break;
                    }
                }
            }
        }
    }
}

int BSPLoader::getLeafFromPosition(const glm::vec3& pos) const {
    if (nodes.empty()) return 0;
    
    int nodeIdx = 0;
    while (nodeIdx >= 0) {
        const BSPNode& node = nodes[nodeIdx];
        const BSPPlane& plane = planes[node.planeNum];
        
        float dist = glm::dot(pos, plane.normal) - plane.dist;
        
        if (dist >= 0) {
            nodeIdx = node.children[0];
        } else {
            nodeIdx = node.children[1];
        }
    }
    
    return ~nodeIdx;  // Возвращаем индекс листа
}

int BSPLoader::getClusterFromLeaf(int leafIndex) const {
    if (leafIndex < 0 || leafIndex >= (int)leaves.size()) return -1;
    return leaves[leafIndex].cluster;
}

int BSPLoader::getClusterForCamera(const glm::vec3& cameraPos) const {
    int leaf = getLeafFromPosition(cameraPos);
    return getClusterFromLeaf(leaf);
}

bool BSPLoader::isFaceVisible(int faceIndex, int clusterIndex) const {
    if (faceIndex < 0 || faceIndex >= (int)faces.size()) return false;
    if (clusterIndex < 0 || clusterIndex >= numVisClusters) return false;
    
    int faceCluster = faceToCluster[faceIndex];
    if (faceCluster < 0) return true;  // Если не удалось определить кластер, считаем видимым
    
    // Проверяем, видит ли кластер камеры кластер фейса
    const VISCluster& vis = visClusters[clusterIndex];
    
    for (int visibleCluster : vis.visibleClusters) {
        if (visibleCluster == faceCluster) return true;
    }
    
    return false;
}

const std::vector<int>& BSPLoader::getVisibleClusters(int clusterIndex) const {
    static std::vector<int> empty;
    if (clusterIndex < 0 || clusterIndex >= numVisClusters) return empty;
    return visClusters[clusterIndex].visibleClusters;
}
}