#include "BSPLoader.h"
#include "Light.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cctype>
#include <fstream>
#include <cfloat>
#include <string>
#include <glm/gtc/type_ptr.hpp>
#include "WADLoader.h"
#include "TriangleCollider.h"
#include <unordered_set>

static const float BSP_SCALE = 0.025f;

// Быстрый парсинг строки в float массив
static bool parseVector3(const std::string& str, float& x, float& y, float& z) {
    return sscanf(str.c_str(), "%f %f %f", &x, &y, &z) == 3;
}

// Проверка является ли класс сущности игнорируемым (ненужные источники света)
static inline bool isIgnoredEntityClass(const std::string& classname) {
    static const std::unordered_set<std::string> IGNORED_CLASSES = {
        "light_spot",
        "light_ambient", 
        "light_glow",
        "env_light"
    };
    return IGNORED_CLASSES.find(classname) != IGNORED_CLASSES.end();
}

static glm::vec3 convertPosition(const glm::vec3& bspPos) {
    return glm::vec3(-bspPos.x * BSP_SCALE, bspPos.z * BSP_SCALE, bspPos.y * BSP_SCALE);
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
    fseek(file, lump.offset, SEEK_SET);
    for (size_t i = 0; i < count; i++) {
        float x, y, z;
        fread(&x, sizeof(float), 1, file);
        fread(&y, sizeof(float), 1, file);
        fread(&z, sizeof(float), 1, file);
        vertices[i] = convertPosition(glm::vec3(x, y, z));
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
        planes[i].dist = dist * BSP_SCALE;
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
    for (auto& model : models) {
        glm::vec3 minBSP(model.min[0], model.min[1], model.min[2]);
        glm::vec3 maxBSP(model.max[0], model.max[1], model.max[2]);
        glm::vec3 originBSP(model.origin[0], model.origin[1], model.origin[2]);
        glm::vec3 minConv = convertPosition(minBSP);
        glm::vec3 maxConv = convertPosition(maxBSP);
        glm::vec3 originConv = convertPosition(originBSP);
        model.min[0] = minConv.x; model.min[1] = minConv.y; model.min[2] = minConv.z;
        model.max[0] = maxConv.x; model.max[1] = maxConv.y; model.max[2] = maxConv.z;
        model.origin[0] = originConv.x; model.origin[1] = originConv.y; model.origin[2] = originConv.z;
    }
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

bool BSPLoader::loadTextures(FILE* file, const BSPHeader& header, WADLoader& wadLoader) {
    const BSPLump& lump = header.lumps[LUMP_TEXTURES];
    if (lump.length == 0) return false;

    // Проверка на разумный размер lump для предотвращения переполнения памяти
    const size_t MAX_TEXTURE_LUMP_SIZE = 64 * 1024 * 1024; // 64 MB
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

    // Безопасное чтение количества текстур с проверкой выравнивания
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

    // Проверка что offsets массив помещается в lumpData
    size_t offsetsSize = numTextures * sizeof(int);
    if (sizeof(int) + offsetsSize > lumpData.size()) {
        std::cerr << "[BSP] Texture offsets array out of bounds.\n";
        return false;
    }

    const int* offsets = reinterpret_cast<const int*>(lumpData.data() + sizeof(int));

    defaultTextureId = wadLoader.getDefaultTexture();
    glTextureIds.reserve(numTextures);
    textureDimensions.reserve(numTextures);

    int foundCount = 0;
    int notFoundCount = 0;

    for (int i = 0; i < numTextures; ++i) {
        if (offsets[i] == -1 || offsets[i] < 0 || offsets[i] >= (int)lump.length) {
            glTextureIds.push_back(defaultTextureId);
            textureDimensions.push_back({ 16, 16 });
            notFoundCount++;
            continue;
        }

        const uint8_t* texPtr = lumpData.data() + offsets[i];

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
            notFoundCount++;
            continue;
        }

        // Безопасное чтение ширины и высоты с проверкой границ
        uint32_t width = 256;
        uint32_t height = 256;
        size_t texDataSize = lumpData.size() - offsets[i];
        if (texDataSize >= 24) { // 16 bytes name + 4 bytes width + 4 bytes height
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
    }

    std::cout << "[BSP] Textures mapped: " << glTextureIds.size()
        << " (found: " << foundCount << ", missing: " << notFoundCount << ")" << std::endl;

    return true;
}

bool BSPLoader::parseEntities(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_ENTITIES];
    if (lump.length == 0) return false;
    
    // Читаем весь lump в буфер
    std::string buffer;
    buffer.resize(lump.length);
    fseek(file, lump.offset, SEEK_SET);
    fread(&buffer[0], 1, lump.length, file);

    // Оптимизированный парсер: пропускаем ненужные сущности
    size_t pos = 0;
    const size_t len = buffer.size();
    
    while (pos < len) {
        // Пропускаем пробелы
        while (pos < len && (buffer[pos] <= 32)) pos++;
        if (pos >= len || buffer[pos] != '{') break;
        pos++;

        // Сначала собираем classname чтобы решить сохранять ли сущность
        std::string classname;
        std::string model;
        glm::vec3 origin(0);
        glm::vec3 angles(0);
        bool hasOrigin = false;
        bool hasAngles = false;
        
        // Быстрый проход для извлечения ключевых полей
        size_t entityStart = pos;
        bool skipEntity = false;
        
        while (pos < len) {
            // Пропускаем пробелы
            while (pos < len && (buffer[pos] <= 32)) pos++;
            if (buffer[pos] == '}') { pos++; break; }
            if (buffer[pos] != '"') { pos++; continue; }
            pos++;
            
            // Ключ
            size_t keyStart = pos;
            while (pos < len && buffer[pos] != '"') pos++;
            if (pos >= len) break;
            std::string key(buffer.data() + keyStart, pos - keyStart);
            pos++;
            
            // Пропускаем пробелы
            while (pos < len && (buffer[pos] <= 32)) pos++;
            if (pos >= len || buffer[pos] != '"') { pos++; continue; }
            pos++;
            
            // Значение
            size_t valStart = pos;
            while (pos < len && buffer[pos] != '"') pos++;
            if (pos >= len) break;
            std::string val(buffer.data() + valStart, pos - valStart);
            pos++;
            
            // Обрабатываем только ключевые поля
            if (key == "classname") {
                classname = val;
                // Если это игнорируемая сущность (ненужный свет), помечаем на пропуск
                if (isIgnoredEntityClass(classname)) {
                    skipEntity = true;
                }
            } else if (key == "origin" && !hasOrigin) {
                float ox, oy, oz;
                if (parseVector3(val, ox, oy, oz)) {
                    origin = convertPosition(glm::vec3(ox, oy, oz));
                    hasOrigin = true;
                }
            } else if (key == "angles" && !hasAngles) {
                float ax, ay, az;
                if (parseVector3(val, ax, ay, az)) {
                    angles = glm::vec3(ax, ay, az);
                    hasAngles = true;
                }
            } else if (key == "model") {
                model = val;
            }
        }
        
        // Сохраняем все сущности кроме игнорируемых (ненужных источников света)
        if (!skipEntity && !classname.empty()) {
            BSPEntity entity;
            entity.classname = classname;
            entity.model = model;
            entity.origin = origin;
            entity.angles = angles;
            
            // Для light_environment сохраняем все свойства
            if (classname == "light_environment") {
                // Перепарсиваем для сохранения всех свойств
                pos = entityStart;
                while (pos < len) {
                    while (pos < len && (buffer[pos] <= 32)) pos++;
                    if (buffer[pos] == '}') break;
                    if (buffer[pos] != '"') { pos++; continue; }
                    pos++;
                    
                    size_t keyStart = pos;
                    while (pos < len && buffer[pos] != '"') pos++;
                    if (pos >= len) break;
                    std::string key = buffer.substr(keyStart, pos - keyStart);
                    pos++;
                    
                    while (pos < len && (buffer[pos] <= 32)) pos++;
                    if (pos >= len || buffer[pos] != '"') { pos++; continue; }
                    pos++;
                    
                    size_t valStart = pos;
                    while (pos < len && buffer[pos] != '"') pos++;
                    if (pos >= len) break;
                    std::string value = buffer.substr(valStart, pos - valStart);
                    pos++;
                    
                    entity.properties[key] = value;
                }
                // Пропускаем закрывающую скобку
                while (pos < len && (buffer[pos] <= 32)) pos++;
                if (pos < len && buffer[pos] == '}') pos++;
            }
            
            entities.push_back(std::move(entity));
        }
    }
    
    std::cout << "[BSP] Parsed " << entities.size() << " entities (optimized, ignored only unnecessary lights)" << std::endl;
    return true;
}

void BSPLoader::buildSubmodelMesh(const BSPModel& subModel) {
    unsigned int baseVertex = (unsigned int)meshVertices.size();

    for (int i = 0; i < subModel.numFaces; i++) {
        const BSPFace& face = faces[subModel.firstFace + i];
        if (face.numEdges < 3) continue;

        if (face.planeNum >= planes.size()) continue;
        if (face.texInfo >= texInfos.size()) continue;

        // Для GoldSrc BSP сторона плоскости определяет направление нормали
        // side == 0: нормаль направлена в положительную сторону
        // side == 1: нормаль направлена в отрицательную сторону
        glm::vec3 normal = planes[face.planeNum].normal;
        if (face.side != 0) {
            normal = -normal;
        }

        std::vector<glm::vec3> faceVerts;
        faceVerts.reserve(face.numEdges);

        for (int j = 0; j < face.numEdges; j++) {
            int edgeIdx = face.firstEdge + j;
            if (edgeIdx >= (int)surfEdges.size()) break;

            int surfEdge = surfEdges[edgeIdx];
            unsigned short vindex;

            if (surfEdge >= 0) {
                if (surfEdge >= (int)edges.size()) break;
                vindex = edges[surfEdge].v[0];
            }
            else {
                int negEdge = -surfEdge;
                if (negEdge >= (int)edges.size()) break;
                vindex = edges[negEdge].v[1];
            }

            if (vindex < vertices.size()) {
                faceVerts.push_back(vertices[vindex]);
            }
        }

        if (faceVerts.size() < 3) continue;

        AABB faceBounds;
        faceBounds.min = faceBounds.max = faceVerts[0];
        for (const auto& v : faceVerts) {
            faceBounds.min = glm::min(faceBounds.min, v);
            faceBounds.max = glm::max(faceBounds.max, v);
        }
        float epsilon = 0.1f * BSP_SCALE;
        faceBounds.min -= glm::vec3(epsilon);
        faceBounds.max += glm::vec3(epsilon);
        faceBoundingBoxes.push_back(faceBounds);

        const BSPTexInfo& texInfo = texInfos[face.texInfo];
        int texIdx = texInfo.textureIndex;

        if (texIdx < 0 || texIdx >= (int)glTextureIds.size()) {
            texIdx = 0;
        }

        GLuint textureID = glTextureIds[texIdx];

        int texWidth = textureDimensions[texIdx].x;
        int texHeight = textureDimensions[texIdx].y;

        if (texWidth <= 0 || texHeight <= 0) {
            texWidth = 256;
            texHeight = 256;
        }

        glm::vec3 s_vec(texInfo.s[0], texInfo.s[1], texInfo.s[2]);
        glm::vec3 t_vec(texInfo.t[0], texInfo.t[1], texInfo.t[2]);

        std::vector<glm::vec2> texCoords;
        texCoords.reserve(faceVerts.size());

        for (const auto& pos : faceVerts) {
            glm::vec3 originalBSPPos(
                -pos.x / BSP_SCALE,
                pos.z / BSP_SCALE,
                pos.y / BSP_SCALE
            );

            float u = glm::dot(originalBSPPos, s_vec) + texInfo.s[3];
            float v = glm::dot(originalBSPPos, t_vec) + texInfo.t[3];

            u = u / (float)texWidth;
            v = v / (float)texHeight;

            texCoords.push_back(glm::vec2(u, v));
        }

        unsigned int faceStartVertex = baseVertex;

        for (size_t j = 0; j < faceVerts.size(); j++) {
            BSPVertex v;
            v.position = faceVerts[j];
            v.normal = normal;
            v.texCoord = texCoords[j];
            meshVertices.push_back(v);
        }

        unsigned int startIdxOffset = (unsigned int)meshIndices.size();
        for (size_t j = 1; j + 1 < faceVerts.size(); j++) {
            meshIndices.push_back(faceStartVertex);
            meshIndices.push_back(faceStartVertex + (unsigned int)j + 1);
            meshIndices.push_back(faceStartVertex + (unsigned int)j);
        }

        drawCalls.push_back({ textureID, startIdxOffset, (unsigned int)((faceVerts.size() - 2) * 3) });
        baseVertex += (unsigned int)faceVerts.size();
    }
}

void BSPLoader::buildMesh() {
    meshVertices.clear();
    meshIndices.clear();
    faceBoundingBoxes.clear();
    drawCalls.clear();
    if (models.empty()) return;

    buildSubmodelMesh(models[0]);

    for (const auto& entity : entities) {
        if (entity.model.empty() || entity.model == "*0") continue;
        if (entity.classname.find("func_") == std::string::npos &&
            entity.classname.find("brush") == std::string::npos) continue;

        // Безопасный парсинг индекса модели
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
        buildSubmodelMesh(models[modelIndex]);
    }

    std::cout << "Built " << meshVertices.size() << " mesh vertices, "
        << meshIndices.size() << " indices, "
        << drawCalls.size() << " draw calls\n";
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
    loadLighting(file, header);  // Загружаем данные освещения из BSP

    loadRequiredWADsFromEntities();

    wadLoader.init();
    wadLoader.loadAllWADs();

    loadTextures(file, header, wadLoader);

    fclose(file);
    buildMesh();
    buildLightmapUVs();  // Строим UV координаты для световой карты
    printStats();
    return !meshVertices.empty();
}

void BSPLoader::cleanupTextures() {
    if (defaultTextureId) glDeleteTextures(1, &defaultTextureId);
    if (!glTextureIds.empty()) {
        for (GLuint tex : glTextureIds) {
            if (tex != defaultTextureId && tex != 0) {
                glDeleteTextures(1, &tex);
            }
        }
    }
    glTextureIds.clear();
    textureDimensions.clear();
    drawCalls.clear();
    defaultTextureId = 0;
    
    // Очищаем текстуру световой карты
    if (lightmapTexture) {
        glDeleteTextures(1, &lightmapTexture);
        lightmapTexture = 0;
    }
    lightmapData.clear();
}

// Загрузка данных освещения из BSP (LUMP_LIGHTING)
bool BSPLoader::loadLighting(FILE* file, const BSPHeader& header) {
    const BSPLump& lump = header.lumps[LUMP_LIGHTING];
    if (lump.length == 0) {
        std::cout << "[BSP] No lighting data found" << std::endl;
        return false;
    }
    
    // Проверка на разумный размер
    const size_t MAX_LIGHTING_SIZE = 64 * 1024 * 1024; // 64 MB
    if (lump.length > MAX_LIGHTING_SIZE) {
        std::cerr << "[BSP] Lighting lump too large: " << lump.length << " bytes" << std::endl;
        return false;
    }
    
    lightmapData.resize(lump.length);
    fseek(file, lump.offset, SEEK_SET);
    if (fread(lightmapData.data(), 1, lump.length, file) != static_cast<size_t>(lump.length)) {
        std::cerr << "[BSP] Failed to read lighting data" << std::endl;
        lightmapData.clear();
        return false;
    }
    
    std::cout << "[BSP] Loaded " << lump.length << " bytes of lighting data" << std::endl;
    
    // Создаем OpenGL текстуру для световой карты
    glGenTextures(1, &lightmapTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, lightmapTexture);
    
    // GoldSrc использует 128x128 страницы световых карт
    // Формат данных: RGB по 1 байту на канал (3 байта на пиксель)
    // Каждая грань ссылается на определенную страницу и UV координаты в ней
    
    // Для простоты создаем одну большую текстуру-атлас
    // В идеале нужно правильно упаковывать все страницы
    int numLightmaps = lump.length / (lightmapSize * lightmapSize * 3);
    if (numLightmaps == 0) numLightmaps = 1;
    
    std::cout << "[BSP] Creating lightmap atlas with " << numLightmaps << " pages" << std::endl;
    
    // Используем GL_TEXTURE_2D_ARRAY для хранения всех страниц
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, lightmapSize, lightmapSize, numLightmaps, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, lightmapData.data());
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    
    return true;
}

// Построение UV координат световой карты для каждой вершины
void BSPLoader::buildLightmapUVs() {
    // Инициализируем все UV нулями по умолчанию
    for (auto& v : meshVertices) {
        v.lightmapUV = glm::vec3(0.0f, 0.0f, 0.0f);
    }
    
    if (faces.empty() || lightmapData.empty()) {
        std::cout << "[BSP] No lighting data or faces, skipping lightmap UVs" << std::endl;
        return;
    }
    
    std::cout << "[BSP] Building lightmap UVs for " << faces.size() << " faces" << std::endl;
    
    // Сначала нужно понять структуру lightmap данных
    // В GoldSrc формат LUMP_LIGHTING:
    // Для каждой грани со светом:
    //   [lightOffset] -> 4 байта заголовка (width, height как uint8_t каждый, но занимают 4 байта)
    //                  -> width * height * 3 байт RGB данных
    
    // Нам нужно для каждой грани:
    // 1. Определить её lightOffset и размеры сетки освещения
    // 2. Вычислить page index (номер страницы 128x128)
    // 3. Для каждой вершины грани вычислить UV в пределах этой страницы
    
    int currentLightOffset = 0;
    int processedFaces = 0;
    
    // Проходим по всем граням и собираем информацию о lightmap страницах
    std::vector<int> faceLightmapPage(faces.size(), -1);
    std::vector<glm::vec2> faceLightmapSize(faces.size(), glm::vec2(0, 0));
    std::vector<glm::vec2> faceLightmapOrigin(faces.size(), glm::vec2(0, 0));
    
    for (size_t faceIdx = 0; faceIdx < faces.size(); ++faceIdx) {
        const BSPFace& face = faces[faceIdx];
        
        if (face.lightOffset < 0) {
            continue;  // Нет освещения для этой грани
        }
        
        if (face.texInfo >= texInfos.size()) continue;
        
        // Читаем размеры сетки освещения из lightmap данных
        // Формат: первые 4 байта по lightOffset содержат ширину и высоту
        if (face.lightOffset + 4 > (int)lightmapData.size()) continue;
        
        uint8_t width = lightmapData[face.lightOffset];
        uint8_t height = lightmapData[face.lightOffset + 1];
        // Байты 2 и 3 зарезервированы
        
        if (width < 2 || height < 2) continue;  // Минимальный размер 2x2
        
        faceLightmapSize[faceIdx] = glm::vec2(width, height);
        
        // Вычисляем на какой странице (128x128) находится эта грань
        // В GoldSrc страницы идут последовательно, каждая 128*128*3 = 49152 байта
        int pageStride = lightmapSize * lightmapSize * 3;  // 49152 байта
        int pageIndex = face.lightOffset / pageStride;
        
        faceLightmapPage[faceIdx] = pageIndex;
        
        processedFaces++;
    }
    
    std::cout << "[BSP] Found " << processedFaces << " faces with valid lightmaps" << std::endl;
    
    // Теперь для каждой грани находим её вершины в meshVertices и вычисляем lightmap UV
    // Проблема: meshVertices содержит все вершины всех граней, но мы не знаем какая вершина к какой грани относится
    
    // Решение: при построении меша (buildMesh) мы проходим по граням, 
    // поэтому нам нужно сохранить информацию о том, какие вершины относятся к какой грани
    
    // Альтернативный подход: вычисляем UV на основе позиции вершины и параметров грани
    // Используем texInfo для вычисления UV координат в пространстве световой карты
    
    for (size_t faceIdx = 0; faceIdx < faces.size(); ++faceIdx) {
        const BSPFace& face = faces[faceIdx];
        
        if (faceLightmapPage[faceIdx] < 0) continue;
        
        if (face.texInfo >= texInfos.size()) continue;
        const BSPTexInfo& texInfo = texInfos[face.texInfo];
        
        glm::vec2 lmSize = faceLightmapSize[faceIdx];
        int pageIdx = faceLightmapPage[faceIdx];
        
        // Получаем все ребра этой грани
        std::vector<glm::vec3> faceVerts;
        for (int j = 0; j < face.numEdges; j++) {
            int edgeIdx = face.firstEdge + j;
            if (edgeIdx < 0 || edgeIdx >= (int)surfEdges.size()) continue;
            
            int surfEdge = surfEdges[edgeIdx];
            unsigned short vindex;
            
            if (surfEdge >= 0) {
                if (surfEdge >= (int)edges.size()) continue;
                vindex = edges[surfEdge].v[0];
            } else {
                int negEdge = -surfEdge;
                if (negEdge >= (int)edges.size()) continue;
                vindex = edges[negEdge].v[1];
            }
            
            if (vindex < vertices.size()) {
                faceVerts.push_back(vertices[vindex]);
            }
        }
        
        if (faceVerts.size() < 3) continue;
        
        // Находим минимальные и максимальные S,T координаты для нормализации UV
        float minS = FLT_MAX, maxS = FLT_MIN;
        float minT = FLT_MAX, maxT = FLT_MIN;
        
        for (const auto& vertPos : faceVerts) {
            float s = glm::dot(vertPos, glm::vec3(texInfo.s[0], texInfo.s[1], texInfo.s[2])) + texInfo.s[3];
            float t = glm::dot(vertPos, glm::vec3(texInfo.t[0], texInfo.t[1], texInfo.t[2])) + texInfo.t[3];
            minS = std::min(minS, s);
            maxS = std::max(maxS, s);
            minT = std::min(minT, t);
            maxT = std::max(maxT, t);
        }
        
        // Добавляем небольшой отступ чтобы избежать проблем с границами
        float sRange = maxS - minS;
        float tRange = maxT - minT;
        if (sRange < 0.001f) sRange = 0.001f;
        if (tRange < 0.001f) tRange = 0.001f;
        
        // Для каждой вершины грани вычисляем lightmap UV
        for (const auto& vertPos : faceVerts) {
            // Вычисляем UV координаты в пространстве lightmap этой грани
            float s = glm::dot(vertPos, glm::vec3(texInfo.s[0], texInfo.s[1], texInfo.s[2])) + texInfo.s[3];
            float t = glm::dot(vertPos, glm::vec3(texInfo.t[0], texInfo.t[1], texInfo.t[2])) + texInfo.t[3];
            
            // Нормализуем UV в пределах [0, 1] для этой грани
            float u = (s - minS) / sRange;
            float v = (t - minT) / tRange;
            
            // Смещаем UV на номер страницы
            // В texture array Z координата - это индекс слоя (страницы)
            glm::vec3 lightmapUV(u, v, (float)pageIdx);
            
            // Находим соответствующую вершину в meshVertices и обновляем её lightmapUV
            // Это приближение - в реальности нужно точно сопоставлять вершины
            for (auto& mv : meshVertices) {
                if (glm::distance(mv.position, vertPos) < 0.01f) {
                    mv.lightmapUV = lightmapUV;
                    break;
                }
            }
        }
    }
    
    // Подсчитываем сколько вершин получили valid lightmap UV
    int validUVCount = 0;
    for (const auto& v : meshVertices) {
        if (glm::length(v.lightmapUV) > 0.001f) {
            validUVCount++;
        }
    }
    
    std::cout << "[BSP] " << validUVCount << " / " << meshVertices.size() 
              << " vertices have valid lightmap UVs" << std::endl;
}

std::vector<Light> BSPLoader::extractLights() const {
    std::vector<Light> result;

    for (const auto& entity : entities) {
        if (entity.classname.find("light") == 0 ||
            entity.classname.find("light_") != std::string::npos) {
            result.push_back(Light::fromBSPEntity(entity));
        }
    }

    std::cout << "[BSP] Extracted " << result.size() << " lights" << std::endl;
    return result;
}

void BSPLoader::printStats() const {
    std::cout << "=== BSP Loaded ===\n";
    std::cout << "Vertices: " << vertices.size() << "\n";
    std::cout << "Faces: " << faces.size() << "\n";
    std::cout << "Textures: " << glTextureIds.size() << "\n";
    std::cout << "Draw Calls: " << drawCalls.size() << "\n";
    std::cout << "Required WADs: " << requiredWADs.size() << "\n";
    std::cout << "Entities: " << entities.size() << "\n";
    std::cout << "==================\n";
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

    // Also try light_environment for sun direction
    for (const auto& entity : entities) {
        if (entity.classname == "light_environment") {
            std::cout << "[BSP] Found light_environment" << std::endl;
            // Can be used to extract _light and angles for sun direction
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

// Адаптированный код освещения из GoldSrc для HuitaEngine
// Оригинал: lights.cpp из Half-Life SDK

// Парсинг сущности light_environment из BSP и настройка освещения
void BSPLoader::setupLightEnvironment(LightManager& lightManager) const {
    for (const auto& entity : entities) {
        if (entity.classname == "light_environment") {
            std::cout << "[BSP] Processing light_environment entity" << std::endl;
            
            // Извлекаем углы для направления солнца
            glm::vec3 angles = entity.angles;
            
            // Конвертируем углы в вектор направления
            // В GoldSrc/Half-Life:
            // - angles.y (yaw): 0 = South, 90 = East, 180 = North, 270 = West
            // - angles.x (pitch): положительный = вниз, отрицательный = вверх
            // Солнце светит В направлении, куда смотрят углы
            
            float yawRad = glm::radians(angles.y);
            float pitchRad = glm::radians(angles.x);
            
            // Вычисляем направление взгляда (куда смотрят углы)
            // В системе координат Half-Life: X вперед, Y вправо, Z вверх
            // Формула для конвертации углов в вектор направления:
            glm::vec3 forward;
            forward.x = cos(pitchRad) * sin(yawRad);
            forward.y = cos(pitchRad) * cos(yawRad);
            forward.z = -sin(pitchRad);  // +pitch в HL это вниз, поэтому инвертируем для Z-up
            forward = glm::normalize(forward);
            
            // Свет идет ОТ солнца к земле, поэтому используем направление forward как есть
            // Если сущность смотрит вниз (+pitch), forward.z будет отрицательным (свет сверху вниз)
            glm::vec3 sunDir = forward;
            
            std::cout << "[BSP] light_environment angles: (" 
                      << angles.x << ", " << angles.y << ", " << angles.z << ")" << std::endl;
            std::cout << "[BSP] Forward vector: (" 
                      << forward.x << ", " << forward.y << ", " << forward.z << ")" << std::endl;
            std::cout << "[BSP] Calculated sun direction: (" 
                      << sunDir.x << ", " << sunDir.y << ", " << sunDir.z << ")" << std::endl;
            
            
            // Извлекаем цвет и интенсивность из свойства "_light"
            auto it = entity.properties.find("_light");
            if (it != entity.properties.end()) {
                const std::string& lightValue = it->second;
                int r, g, b, v;
                int parsed = sscanf(lightValue.c_str(), "%d %d %d %d", &r, &g, &b, &v);
                
                if (parsed >= 1) {
                    if (parsed == 1) {
                        // Только одно значение - используем как яркость для всех каналов
                        g = b = r;
                    }
                    
                    if (parsed >= 4) {
                        // Применяем множитель интенсивности
                        r = static_cast<int>(r * (v / 255.0f));
                        g = static_cast<int>(g * (v / 255.0f));
                        b = static_cast<int>(b * (v / 255.0f));
                    }
                    
                    // Ограничиваем значения максимум 255 (используем glm::clamp для совместимости)
                    r = static_cast<int>(glm::clamp(static_cast<float>(r), 0.0f, 255.0f));
                    g = static_cast<int>(glm::clamp(static_cast<float>(g), 0.0f, 255.0f));
                    b = static_cast<int>(glm::clamp(static_cast<float>(b), 0.0f, 255.0f));
                    
                    // Конвертируем в диапазон [0, 1] для шейдера
                    glm::vec3 sunColor(r / 255.0f, g / 255.0f, b / 255.0f);
                    
                    std::cout << "[BSP] light_environment color: (" 
                              << r << ", " << g << ", " << b << ")" << std::endl;
                   
                }
            }
            
            return; // Обрабатываем только первый light_environment
        }
    }
    
    std::cout << "[BSP] No light_environment found, using default sun settings" << std::endl;
}