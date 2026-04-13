#include "pch.h"
#include "LightmapManager.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <numeric>

// Максимальный размер одной грани (можно увеличить, если карта очень детальная)
constexpr int MAX_LIGHTMAP_SIZE = 1024;

// Максимальный размер атласа (ограничение видеокарты, обычно 16384)
constexpr int MAX_ATLAS_SIZE = 16384;

// ============================================================================
// LightmapAtlas Implementation
// ============================================================================

bool LightmapAtlas::init(int width, int height) {
    if (atlasTexture) {
        glDeleteTextures(1, &atlasTexture);
        atlasTexture = 0;
    }

    // Принимаем любой размер, даже не кратный степени двойки (например 1535x1457)
    // Ограничиваем только максимальным размером GPU
    atlasWidth = std::max(1, std::min(width, MAX_ATLAS_SIZE));
    atlasHeight = std::max(1, std::min(height, MAX_ATLAS_SIZE));

    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);

    // Создаем пустую текстуру черного цвета (0 = отсутствие света)
    // Размер может быть любым прямоугольным
    std::vector<uint8_t> emptyData(atlasWidth * atlasHeight * 3, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, atlasWidth, atlasHeight,
        0, GL_RGB, GL_UNSIGNED_BYTE, emptyData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    currentX = 0;
    currentY = 0;
    rowHeight = 0;
    initialized = true;

    return atlasTexture != 0;
}

glm::vec4 LightmapAtlas::packLightmap(int width, int height, const uint8_t* data) {
    if (!initialized) return glm::vec4(0.0f);

    // Если вдруг лайтмап больше атласа (не должно случиться при правильном расчете)
    if (width > atlasWidth || height > atlasHeight) {
        std::cerr << "LightmapAtlas: Critical error! Lightmap (" << width << "x" << height
            << ") is larger than atlas (" << atlasWidth << "x" << atlasHeight << ")" << std::endl;
        return glm::vec4(0.0f);
    }

    // Алгоритм упаковки строками (Row Packing)
    // Если текущая строка переполняется по ширине, переходим на следующую
    if (currentX + width > atlasWidth) {
        currentX = 0;
        currentY += rowHeight + 1; // +1 пиксель отступ между строками
        rowHeight = 0;
    }

    // Если переполняется высота атласа
    if (currentY + height > atlasHeight) {
        std::cerr << "LightmapAtlas: Atlas overflow! Required height exceeds " << atlasHeight << std::endl;
        return glm::vec4(0.0f);
    }

    // Копируем данные лайтмапа в атлас
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, currentX, currentY, width, height,
        GL_RGB, GL_UNSIGNED_BYTE, data);

    // Расчет UV координат
    // Добавляем небольшое смещение (half-pixel offset) для корректной выборки текстур
    float halfPixelX = 0.5f / (float)atlasWidth;
    float halfPixelY = 0.5f / (float)atlasHeight;

    float u1 = (float)currentX / (float)atlasWidth + halfPixelX;
    float v1 = (float)currentY / (float)atlasHeight + halfPixelY;
    float u2 = (float)(currentX + width) / (float)atlasWidth - halfPixelX;
    float v2 = (float)(currentY + height) / (float)atlasHeight - halfPixelY;

    // Обновляем позицию курсора
    currentX += width + 1; // +1 пиксель отступ между лайтмапами в строке
    rowHeight = std::max(rowHeight, height);

    return glm::vec4(u1, v1, u2, v2);
}

void LightmapAtlas::bind(GLuint unit) const {
    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
}

void LightmapAtlas::cleanup() {
    if (atlasTexture) {
        glDeleteTextures(1, &atlasTexture);
        atlasTexture = 0;
    }
    initialized = false;
    atlasWidth = 0;
    atlasHeight = 0;
}

// ============================================================================
// LightmapManager Implementation
// ============================================================================

LightmapManager::LightmapManager() = default;

LightmapManager::~LightmapManager() {
    cleanup();
}

LightmapManager::LightmapManager(LightmapManager&& other) noexcept
    : faceLightmaps(std::move(other.faceLightmaps)),
    atlas(std::move(other.atlas)),
    hasValidLightmaps(other.hasValidLightmaps) {
    other.hasValidLightmaps = false;
}

LightmapManager& LightmapManager::operator=(LightmapManager&& other) noexcept {
    if (this != &other) {
        cleanup();
        faceLightmaps = std::move(other.faceLightmaps);
        atlas = std::move(other.atlas);
        hasValidLightmaps = other.hasValidLightmaps;
        other.hasValidLightmaps = false;
    }
    return *this;
}

GLuint LightmapManager::createLightmapTexture(int width, int height, const uint8_t* data) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
        0, GL_RGB, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texID;
}

// Функция расчета оптимального размера атласа на основе всех лайтмапов
void LightmapManager::calculateOptimalAtlasSize(const std::vector<FaceLightmap>& tempMaps, int& outWidth, int& outHeight) {
    int totalArea = 0;
    int maxWidth = 0;
    int maxHeight = 0;

    // Считаем общую площадь и максимальные размеры отдельных лайтмапов
    for (const auto& lm : tempMaps) {
        if (lm.valid) {
            totalArea += lm.width * lm.height;
            maxWidth = std::max(maxWidth, lm.width);
            maxHeight = std::max(maxHeight, lm.height);
        }
    }

    // Добавляем запас на отступы (padding) ~20%
    totalArea = static_cast<int>(totalArea * 1.2f);

    // Начальный расчет размеров
    // Пытаемся сделать атлас близким к квадрату, но не меньше максимальных размеров граней
    int estimatedSide = static_cast<int>(std::sqrt(totalArea));

    int w = std::max(estimatedSide, maxWidth);
    int h = std::max(estimatedSide, maxHeight);

    // Округляем до ближайшей степени двойки для лучшей совместимости и производительности GPU,
    // НО сохраняем возможность неквадратных форм, если одна сторона явно больше другой.
    // Однако, если пользователь хочет ТОЧНО 1535x1457, мы можем просто вернуть ближайшие большие значения.
    // Здесь мы используем стратегию: округление вверх до степени двойки, но независимо для W и H,
    // если соотношение сторон сильно отличается.

    auto nextPow2 = [](int v) -> int {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return std::max(v, 1);
    };

    // Простая эвристика: если стороны сильно различаются, оставляем прямоугольник, иначе квадрат
    float aspectRatio = (float)w / (float)h;

    if (aspectRatio > 1.5f || aspectRatio < 0.66f) {
        // Прямоугольный атлас
        outWidth = nextPow2(w);
        outHeight = nextPow2(h);
    }
    else {
        // Квадратный атлас
        int side = nextPow2(std::max(w, h));
        outWidth = side;
        outHeight = side;
    }

    // Финальная проверка ограничений
    outWidth = std::min(outWidth, MAX_ATLAS_SIZE);
    outHeight = std::min(outHeight, MAX_ATLAS_SIZE);

    // Гарантируем, что атлас не меньше максимальной грани
    outWidth = std::max(outWidth, maxWidth);
    outHeight = std::max(outHeight, maxHeight);
}

void LightmapManager::debugPrintLightmapInfo() const {
    std::cout << "=== LightmapManager Debug Info ===" << std::endl;
    std::cout << "Atlas Size: " << atlas.getWidth() << "x" << atlas.getHeight() << std::endl;
    std::cout << "Face lightmaps count: " << faceLightmaps.size() << std::endl;

    int validCount = 0;
    for (size_t i = 0; i < faceLightmaps.size(); i++) {
        if (faceLightmaps[i].valid) validCount++;
    }
    std::cout << "Valid lightmaps: " << validCount << std::endl;
    std::cout << "===================================" << std::endl;
}

bool LightmapManager::initializeFromBSP(const BSPLoader& bsp) {
    cleanup();

    if (bsp.lightingData.empty()) {
        std::cerr << "LightmapManager: No lighting data in BSP" << std::endl;
        return false;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const auto& faces = bsp.getFaces();
    const auto& texInfos = bsp.getTexInfos();
    const auto& surfEdges = bsp.getSurfEdges();
    const auto& edges = bsp.getEdges();
    const auto& originalVertices = bsp.getOriginalVertices();
    const auto& lightingData = bsp.lightingData;

    // Шаг 1: Пре-сканирование (Dry Run)
    // Вычисляем размеры всех лайтмапов, но НЕ создаем текстуры и НЕ упаковываем их.
    // Это нужно, чтобы узнать точный необходимый размер атласа.
    std::vector<FaceLightmap> tempMaps(faces.size());

    for (size_t i = 0; i < faces.size(); i++) {
        const BSPFace& face = faces[i];
        FaceLightmap& lm = tempMaps[i];

        lm.offset = face.lightofs;

        if (face.lightofs < 0) {
            lm.valid = false;
            continue;
        }

        if (face.lightofs >= (int)lightingData.size()) {
            lm.valid = false;
            continue;
        }

        if (face.texInfo < 0 || face.texInfo >= (int)texInfos.size()) {
            lm.valid = false;
            continue;
        }

        const BSPTexInfo& tex = texInfos[face.texInfo];

        float sMin = 999999.0f, sMax = -999999.0f;
        float tMin = 999999.0f, tMax = -999999.0f;

        for (int j = 0; j < face.numEdges; j++) {
            int surfEdgeIdx = face.firstEdge + j;
            if (surfEdgeIdx < 0 || surfEdgeIdx >= (int)surfEdges.size()) continue;

            int edgeIdx = surfEdges[surfEdgeIdx];
            int vIdx;

            if (edgeIdx >= 0) {
                if (edgeIdx >= (int)edges.size()) continue;
                vIdx = edges[edgeIdx].v[0];
            }
            else {
                int negIdx = -edgeIdx;
                if (negIdx < 0 || negIdx >= (int)edges.size()) continue;
                vIdx = edges[negIdx].v[1];
            }

            if (vIdx < 0 || vIdx >= (int)originalVertices.size()) continue;

            const glm::vec3& v = originalVertices[vIdx];
            float s = v.x * tex.s[0] + v.y * tex.s[1] + v.z * tex.s[2] + tex.s[3];
            float t = v.x * tex.t[0] + v.y * tex.t[1] + v.z * tex.t[2] + tex.t[3];

            sMin = std::min(sMin, s);
            sMax = std::max(sMax, s);
            tMin = std::min(tMin, t);
            tMax = std::max(tMax, t);
        }

        lm.minS = sMin;
        lm.minT = tMin;
        lm.maxS = sMax;
        lm.maxT = tMax;

        int sSize = static_cast<int>(std::ceil(sMax / 16.0f) - std::floor(sMin / 16.0f)) + 1;
        int tSize = static_cast<int>(std::ceil(tMax / 16.0f) - std::floor(tMin / 16.0f)) + 1;

        lm.width = std::max(1, std::min(sSize, MAX_LIGHTMAP_SIZE));
        lm.height = std::max(1, std::min(tSize, MAX_LIGHTMAP_SIZE));

        // Проверка границ данных
        int dataSize = lm.width * lm.height * 3;
        if (face.lightofs + dataSize > (int)lightingData.size()) {
            lm.valid = false;
            continue;
        }

        lm.valid = true;
    }

    // Шаг 2: Расчет оптимального размера атласа на основе пре-сканирования
    int atlasW, atlasH;
    calculateOptimalAtlasSize(tempMaps, atlasW, atlasH);

    std::cout << "[LIGHT] Calculated optimal atlas size: " << atlasW << "x" << atlasH << std::endl;

    // Шаг 3: Создание атласа ПОД КОНКРЕТНЫЙ РАЗМЕР
    if (!atlas.init(atlasW, atlasH)) {
        std::cerr << "LightmapManager: Failed to create atlas of size " << atlasW << "x" << atlasH << std::endl;
        return false;
    }

    // Шаг 4: Вторая проход - реальная загрузка и упаковка
    faceLightmaps = std::move(tempMaps);
    int validCount = 0;

    for (size_t i = 0; i < faceLightmaps.size(); i++) {
        FaceLightmap& lm = faceLightmaps[i];
        if (!lm.valid) continue;

        const BSPFace& face = faces[i];

        // Создаем индивидуальную текстуру (опционально, для отладки)
        lm.textureID = createLightmapTexture(lm.width, lm.height,
            lightingData.data() + face.lightofs);

        // Упаковываем в общий атлас
        glm::vec4 atlasUV = atlas.packLightmap(lm.width, lm.height,
            lightingData.data() + face.lightofs);

        lm.uvMin = glm::vec2(atlasUV.x, atlasUV.y);
        lm.uvMax = glm::vec2(atlasUV.z, atlasUV.w);

        validCount++;
    }

    hasValidLightmaps = (validCount > 0);

    std::cout << "[LIGHTMAP] Total faces: " << faces.size() << std::endl;
    std::cout << "[LIGHTMAP] Valid lightmaps packed: " << validCount << std::endl;
    std::cout << "[LIGHTMAP] Atlas dimensions: " << atlas.getWidth() << "x" << atlas.getHeight() << std::endl;

    return hasValidLightmaps;
}

const FaceLightmap& LightmapManager::getFaceLightmap(int faceIndex) const {
    if (faceIndex >= 0 && faceIndex < (int)faceLightmaps.size()) {
        return faceLightmaps[faceIndex];
    }
    static FaceLightmap empty;
    return empty;
}

void LightmapManager::cleanup() {
    for (auto& lm : faceLightmaps) {
        if (lm.textureID) {
            glDeleteTextures(1, &lm.textureID);
            lm.textureID = 0;
        }
    }
    faceLightmaps.clear();
    atlas.cleanup();
    hasValidLightmaps = false;
}