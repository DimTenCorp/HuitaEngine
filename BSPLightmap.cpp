#define _CRT_SECURE_NO_WARNINGS
#include "BSPLightmap.h"
#include "BSPLoader.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

// Включаем stb_rect_pack для упаковки лайтмапов
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

static const float BSP_SCALE = 0.025f;

BSPLightmap::BSPLightmap() {
    m_textureID = 0;
    m_pageSize = 128;  // Стандартный размер страницы в GoldSrc
    m_pageCount = 0;
}

BSPLightmap::~BSPLightmap() {
    destroy();
}

BSPLightmap::BSPLightmap(BSPLightmap&& other) noexcept 
    : m_textureID(other.m_textureID),
      m_pageSize(other.m_pageSize),
      m_pageCount(other.m_pageCount),
      m_surfaceData(std::move(other.m_surfaceData)) {
    
    memcpy(m_lightstyleScales, other.m_lightstyleScales, sizeof(m_lightstyleScales));
    
    other.m_textureID = 0;
    other.m_pageSize = 128;
    other.m_pageCount = 0;
}

BSPLightmap& BSPLightmap::operator=(BSPLightmap&& other) noexcept {
    if (this != &other) {
        destroy();
        
        m_textureID = other.m_textureID;
        m_pageSize = other.m_pageSize;
        m_pageCount = other.m_pageCount;
        m_surfaceData = std::move(other.m_surfaceData);
        memcpy(m_lightstyleScales, other.m_lightstyleScales, sizeof(m_lightstyleScales));
        
        other.m_textureID = 0;
        other.m_pageSize = 128;
        other.m_pageCount = 0;
    }
    return *this;
}

void BSPLightmap::destroy() {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
    m_surfaceData.clear();
    m_pageCount = 0;
}

bool BSPLightmap::buildFromBSP(BSPLoader& bspLoader,
                                const std::vector<BSPVertex>& vertices,
                                const std::vector<BSPFace>& faces) {
    destroy();
    
    const auto& lightmapData = bspLoader.getLightmapData();
    
    if (faces.empty()) {
        std::cerr << "[BSPLightmap] No faces to process" << std::endl;
        return false;
    }
    
    m_surfaceData.resize(faces.size());
    
    // Шаг 1: Вычисляем размеры и позиции для каждой поверхности
    int totalLightmapArea = 0;
    std::vector<stbrp_rect> rects;
    rects.reserve(faces.size());
    
    for (size_t i = 0; i < faces.size(); i++) {
        const BSPFace& face = faces[i];
        SurfaceData& data = m_surfaceData[i];
        
        // Проверяем есть ли у грани лайтмап
        if (face.lightOffset < 0 || face.lightOffset >= (int)lightmapData.size()) {
            continue;
        }
        
        // Читаем размеры лайтмапа из данных освещения
        // Формат GoldSrc: [width][height][RGB pixels для style 0][RGB для style 1]...
        uint8_t lmWidth = lightmapData[face.lightOffset];
        uint8_t lmHeight = lightmapData[face.lightOffset + 1];
        
        if (lmWidth < 2 || lmHeight < 2) {
            continue;
        }
        
        data.size = glm::ivec2(lmWidth, lmHeight);
        data.hasLightmap = true;
        
        // Добавляем padding для предотвращения артефактов фильтрации
        stbrp_rect rect;
        rect.id = (int)i;
        rect.w = data.size.x + 2 * LIGHTMAP_PADDING;
        rect.h = data.size.y + 2 * LIGHTMAP_PADDING;
        rects.push_back(rect);
        totalLightmapArea += rect.w * rect.h;
    }
    
    if (rects.empty()) {
        std::cout << "[BSPLightmap] No lightmapped surfaces found" << std::endl;
        return true;  // Не ошибка, просто нет лайтмапов
    }
    
    // Шаг 2: Оцениваем размер текстуры
    int squareSize = (int)std::ceil(totalLightmapArea / (1.0f - LIGHTMAP_BLOCK_WASTED));
    int textureSize = std::min((int)std::sqrt(squareSize), MAX_LIGHTMAP_BLOCK_SIZE);
    
    // Округляем до кратного 4
    if (textureSize % 4 != 0) {
        textureSize += (4 - textureSize % 4);
    }
    
    std::cout << "[BSPLightmap] Estimated texture size: " << textureSize << "x" << textureSize << std::endl;
    
    // Шаг 3: Упаковываем прямоугольники
    stbrp_context packContext;
    std::vector<stbrp_node> packNodes(2 * textureSize);
    stbrp_init_target(&packContext, textureSize, textureSize, packNodes.data(),
                      (int)packNodes.size());
    
    stbrp_pack_rects(&packContext, rects.data(), (int)rects.size());
    
    // Подсчитываем неудачные упаковки
    int failedCount = 0;
    for (const auto& rect : rects) {
        if (!rect.was_packed) {
            failedCount++;
        }
    }
    
    if (failedCount > 0) {
        std::cerr << "[BSPLightmap] " << failedCount << " faces failed to pack" << std::endl;
    }
    
    // Шаг 4: Создаём битмапы для каждого lightstyle
    // Формат: RGB, 1 байт на канал
    std::vector<std::vector<glm::u8vec3>> bitmaps(NUM_LIGHTSTYLES);
    for (auto& bitmap : bitmaps) {
        bitmap.resize(textureSize * textureSize, glm::u8vec3(0));
    }
    
    // Шаг 5: Копируем пиксели лайтмапов в битмапы
    for (const stbrp_rect& rect : rects) {
        if (rect.was_packed) {
            SurfaceData& data = m_surfaceData[rect.id];
            const BSPFace& face = faces[rect.id];
            
            // Пропускаем width и height (первые 2 байта)
            const unsigned char* pixels = lightmapData.data() + face.lightOffset + 2;
            
            for (int style = 0; style < NUM_LIGHTSTYLES; style++) {
                if (face.styles[style] == 255) break;
                
                // Копируем с учётом padding
                for (int y = 0; y < data.size.y; y++) {
                    for (int x = 0; x < data.size.x; x++) {
                        int dstX = rect.x + LIGHTMAP_PADDING + x;
                        int dstY = rect.y + LIGHTMAP_PADDING + y;
                        int dstIdx = dstY * textureSize + dstX;
                        
                        int srcIdx = (y * data.size.x + x) * 3;
                        bitmaps[style][dstIdx] = glm::u8vec3(
                            pixels[srcIdx],
                            pixels[srcIdx + 1],
                            pixels[srcIdx + 2]
                        );
                    }
                }
                
                // Переходим к следующему стилю
                pixels += data.size.x * data.size.y * 3;
            }
            
            data.offset.x = rect.x + LIGHTMAP_PADDING;
            data.offset.y = rect.y + LIGHTMAP_PADDING;
        }
    }
    
    // Шаг 6: Объединяем битмапы в одну текстуру-массив
    std::vector<glm::u8vec3> finalImage(textureSize * textureSize * NUM_LIGHTSTYLES);
    for (int i = 0; i < NUM_LIGHTSTYLES; i++) {
        std::memcpy(finalImage.data() + (textureSize * textureSize * i),
                    bitmaps[i].data(),
                    textureSize * textureSize * sizeof(glm::u8vec3));
    }
    
    // Шаг 7: Загружаем в GPU
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_textureID);
    
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, textureSize, textureSize, NUM_LIGHTSTYLES,
                 0, GL_RGB, GL_UNSIGNED_BYTE, finalImage.data());
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    
    m_pageSize = textureSize;
    m_pageCount = NUM_LIGHTSTYLES;
    
    std::cout << "[BSPLightmap] Created " << textureSize << "x" << textureSize 
              << " lightmap atlas with " << NUM_LIGHTSTYLES << " styles" << std::endl;
    
    return true;
}

void BSPLightmap::bind(GLuint textureUnit) const {
    if (m_textureID != 0) {
        glActiveTexture(textureUnit);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_textureID);
    }
}

void BSPLightmap::setFiltering(bool filterEnabled) {
    if (m_textureID == 0) return;
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_textureID);
    GLenum filter = filterEnabled ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, filter);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void BSPLightmap::setLightstyleScale(int styleIndex, float scale) {
    if (styleIndex >= 0 && styleIndex < NUM_LIGHTSTYLES) {
        m_lightstyleScales[styleIndex] = scale;
    }
}
