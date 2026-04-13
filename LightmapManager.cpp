#include "pch.h"
#include "LightmapManager.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>

// Максимальный размер лайтмапа для одной грани (можно увеличить при необходимости)
constexpr int MAX_LIGHTMAP_SIZE = 1024;

// Базовый размер атласа (будет увеличен при необходимости)
constexpr int BASE_ATLAS_WIDTH = 2048;
constexpr int BASE_ATLAS_HEIGHT = 2048;

// Максимальный размер атласа (ограничение видеокарты, обычно 16384)
constexpr int MAX_ATLAS_SIZE = 16384;

// Шаг увеличения атласа
constexpr int ATLAS_SIZE_STEP = 2048;

// ============================================================================
// LightmapAtlas Implementation
// ============================================================================

bool LightmapAtlas::init() {
    return init(BASE_ATLAS_WIDTH, BASE_ATLAS_HEIGHT);
}

bool LightmapAtlas::init(int width, int height) {
    if (atlasTexture) {
        glDeleteTextures(1, &atlasTexture);
        atlasTexture = 0;
    }
    
    atlasWidth = std::min(width, MAX_ATLAS_SIZE);
    atlasHeight = std::min(height, MAX_ATLAS_SIZE);
    
    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);

    // ИСПРАВЛЕНО: чёрный фон (0 = нет света) вместо белого
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

    // Не нужен белый пиксель - теперь где нет lightmap будет чёрный цвет из (0,0)

    return atlasTexture != 0;
}

glm::vec4 LightmapAtlas::packLightmap(int width, int height, const uint8_t* data) {
    if (!initialized) return glm::vec4(0.0f);

    // Проверка: лайтмап слишком большой для текущего атласа
    if (width > atlasWidth || height > atlasHeight) {
        std::cerr << "LightmapAtlas: Lightmap too big (" << width << "x" << height 
                  << ") for atlas size " << atlasWidth << "x" << atlasHeight << ", attempting to resize..." << std::endl;
        
        // Попытка увеличить размер атласа
        int newWidth = atlasWidth;
        int newHeight = atlasHeight;
        
        // Увеличиваем ширину или высоту по необходимости
        while (width > newWidth && newWidth < MAX_ATLAS_SIZE) {
            newWidth += ATLAS_SIZE_STEP;
        }
        while (height > newHeight && newHeight < MAX_ATLAS_SIZE) {
            newHeight += ATLAS_SIZE_STEP;
        }
        
        if ((newWidth > atlasWidth || newHeight > atlasHeight) && 
            newWidth <= MAX_ATLAS_SIZE && newHeight <= MAX_ATLAS_SIZE) {
            // Пересоздаем атлас с большим размером
            // Сохраняем старую текстуру для копирования данных
            GLuint oldTexture = atlasTexture;
            int oldWidth = atlasWidth;
            int oldHeight = atlasHeight;
            
            // Создаем новый атлас
            atlasTexture = 0;
            initialized = false;
            
            if (!init(newWidth, newHeight)) {
                std::cerr << "LightmapAtlas: Failed to create larger atlas!" << std::endl;
                return glm::vec4(0.0f);
            }
            
            // Копируем данные со старой текстуры на новую
            glBindTexture(GL_TEXTURE_2D, atlasTexture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, oldWidth, oldHeight);
            
            glDeleteTextures(1, &oldTexture);
            
            // Сбрасываем позицию упаковки (упрощенно - переупакуем все заново)
            // Для полной реализации нужно хранить все упакованные лайтмапы и переупаковывать их
            currentX = 0;
            currentY = 0;
            rowHeight = 0;
        } else {
            std::cerr << "LightmapAtlas: Cannot fit lightmap (" << width << "x" << height 
                      << "), max atlas size reached: " << MAX_ATLAS_SIZE << std::endl;
            return glm::vec4(0.0f);
        }
    }

    // Проверка переполнения строки
    if (currentX + width > atlasWidth) {
        currentX = 0;
        currentY += rowHeight + 2; // +2 для padding
        rowHeight = 0;
    }

    // Проверка переполнения атласа
    if (currentY + height > atlasHeight) {
        std::cerr << "LightmapAtlas: Atlas full! Size: " << atlasWidth << "x" << atlasHeight << ", trying to expand..." << std::endl;
        
        // Попытка увеличить атлас
        int newHeight = std::min(atlasHeight + ATLAS_SIZE_STEP, MAX_ATLAS_SIZE);
        if (newHeight > atlasHeight) {
            GLuint oldTexture = atlasTexture;
            int oldWidth = atlasWidth;
            int oldHeight = atlasHeight;
            
            atlasTexture = 0;
            initialized = false;
            
            if (!init(oldWidth, newHeight)) {
                std::cerr << "LightmapAtlas: Failed to expand atlas!" << std::endl;
                return glm::vec4(0.0f);
            }
            
            // Копируем данные со старой текстуры
            glBindTexture(GL_TEXTURE_2D, atlasTexture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, oldWidth, oldHeight);
            
            glDeleteTextures(1, &oldTexture);
            
            currentX = 0;
            currentY = 0;
            rowHeight = 0;
        } else {
            return glm::vec4(0.0f);
        }
    }

    // Копируем данные
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, currentX, currentY, width, height,
        GL_RGB, GL_UNSIGNED_BYTE, data);

    // UV координаты (с небольшим смещением 0.5 пикселя для центрирования)
    float halfPixelX = 0.5f / atlasWidth;
    float halfPixelY = 0.5f / atlasHeight;
    float u1 = (float)currentX / atlasWidth + halfPixelX;
    float v1 = (float)currentY / atlasHeight + halfPixelY;
    float u2 = (float)(currentX + width) / atlasWidth - halfPixelX;
    float v2 = (float)(currentY + height) / atlasHeight - halfPixelY;

    currentX += width + 2; // +2 padding
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

    // ВАЖНО: выравнивание 1 байт для lightmaps!
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
        0, GL_RGB, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texID;
}

void LightmapManager::debugPrintLightmapInfo() const {
    std::cout << "=== LightmapManager Debug Info ===" << std::endl;
    std::cout << "Face lightmaps count: " << faceLightmaps.size() << std::endl;
    std::cout << "Has valid lightmaps: " << (hasValidLightmaps ? "YES" : "NO") << std::endl;
    std::cout << "Atlas initialized: " << (atlas.initialized ? "YES" : "NO") << std::endl;

    int validCount = 0;
    for (size_t i = 0; i < faceLightmaps.size(); i++) {
        if (faceLightmaps[i].valid) {
            validCount++;
            if (validCount < 10) { // Печатаем первые 10
                std::cout << "  Face " << i << ": valid, offset=" << faceLightmaps[i].offset
                    << ", size=" << faceLightmaps[i].width << "x" << faceLightmaps[i].height
                    << ", uvMin=(" << faceLightmaps[i].uvMin.x << "," << faceLightmaps[i].uvMin.y << ")"
                    << ", uvMax=(" << faceLightmaps[i].uvMax.x << "," << faceLightmaps[i].uvMax.y << ")"
                    << std::endl;
            }
        }
    }
    std::cout << "Valid lightmaps: " << validCount << "/" << faceLightmaps.size() << std::endl;
    std::cout << "===================================" << std::endl;
}

bool LightmapManager::initializeFromBSP(const BSPLoader& bsp) {
    cleanup();

    if (bsp.lightingData.empty()) {
        std::cerr << "LightmapManager: No lighting data in BSP" << std::endl;
        return false;
    }

    // ВАЖНО: выравнивание 1 байт!
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const auto& faces = bsp.getFaces();
    const auto& texInfos = bsp.getTexInfos();
    const auto& surfEdges = bsp.getSurfEdges();
    const auto& edges = bsp.getEdges();
    const auto& originalVertices = bsp.getOriginalVertices();
    const auto& lightingData = bsp.lightingData;

    faceLightmaps.resize(faces.size());

    if (!atlas.init()) {
        std::cerr << "LightmapManager: Failed to create atlas" << std::endl;
        return false;
    }

    int validCount = 0;
    int skipCount = 0;
    int noLightCount = 0;

    for (size_t i = 0; i < faces.size(); i++) {
        const BSPFace& face = faces[i];
        FaceLightmap& lm = faceLightmaps[i];

        lm.offset = face.lightofs;

        // Проверка: нет lightmap (триггеры, небо и т.д.)
        if (face.lightofs < 0) {
            lm.valid = false;
            lm.uvMin = glm::vec2(0.0f, 0.0f);
            lm.uvMax = glm::vec2(0.001f, 0.001f);
            noLightCount++;
            continue;
        }

        if (face.lightofs >= (int)lightingData.size()) {
            std::cerr << "Face " << i << ": lightofs out of bounds: " << face.lightofs << std::endl;
            lm.valid = false;
            skipCount++;
            continue;
        }

        if (face.texInfo < 0 || face.texInfo >= (int)texInfos.size()) {
            std::cerr << "Face " << i << ": texInfo out of bounds: " << face.texInfo << std::endl;
            lm.valid = false;
            skipCount++;
            continue;
        }

        const BSPTexInfo& tex = texInfos[face.texInfo];

        // Расчёт размеров LIGHTMAP
        float sMin = 999999.0f, sMax = -999999.0f;
        float tMin = 999999.0f, tMax = -999999.0f;

        // Используем оригинальные вершины!
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

            // Проекция на текстурные оси
            float s = v.x * tex.s[0] + v.y * tex.s[1] + v.z * tex.s[2] + tex.s[3];
            float t = v.x * tex.t[0] + v.y * tex.t[1] + v.z * tex.t[2] + tex.t[3];

            sMin = std::min(sMin, s);
            sMax = std::max(sMax, s);
            tMin = std::min(tMin, t);
            tMax = std::max(tMax, t);
        }

        // Сохраняем min для UV расчёта
        lm.minS = sMin;
        lm.minT = tMin;

        // Правильная формула HL1 с ceil и floor
        int sSize = static_cast<int>(std::ceil(sMax / 16.0f) - std::floor(sMin / 16.0f)) + 1;
        int tSize = static_cast<int>(std::ceil(tMax / 16.0f) - std::floor(tMin / 16.0f)) + 1;

        lm.width = std::max(1, std::min(sSize, MAX_LIGHTMAP_SIZE));
        lm.height = std::max(1, std::min(tSize, MAX_LIGHTMAP_SIZE));

        // Проверка размера данных
        int dataSize = lm.width * lm.height * 3; // RGB
        if (face.lightofs + dataSize > (int)lightingData.size()) {
            std::cerr << "Face " << i << ": Not enough lighting data! "
                << "Offset: " << face.lightofs
                << ", Need: " << dataSize
                << ", Have: " << lightingData.size() - face.lightofs << std::endl;
            lm.valid = false;
            skipCount++;
            continue;
        }

        // Создаём индивидуальную текстуру (для отладки)
        lm.textureID = createLightmapTexture(lm.width, lm.height,
            lightingData.data() + face.lightofs);

        // Пакуем в атлас
        glm::vec4 atlasUV = atlas.packLightmap(lm.width, lm.height,
            lightingData.data() + face.lightofs);
        lm.uvMin = glm::vec2(atlasUV.x, atlasUV.y);
        lm.uvMax = glm::vec2(atlasUV.z, atlasUV.w);

        lm.valid = true;
        validCount++;
    }

    hasValidLightmaps = (validCount > 0);

    std::cout << "[LIGHTMAP] Total faces: " << faces.size() << std::endl;
    std::cout << "[LIGHTMAP] Valid: " << validCount << ", No light: " << noLightCount << ", Errors: " << skipCount << std::endl;
    std::cout << "[LIGHTMAP] Atlas initialized: " << (atlas.initialized ? "YES" : "NO") << std::endl;
    std::cout << "[LIGHT] Lightmaps initialized successfully" << std::endl;
    std::cout << "[LIGHT] Total lightmap data: " << lightingData.size() << " bytes" << std::endl;

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