#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "BSPLoader.h"

// Структура lightmap для одного face
struct FaceLightmap {
    GLuint textureID = 0;           // OpenGL текстура lightmap (индивидуальная)
    int width = 0;                  // Ширина lightmap в пикселях
    int height = 0;                 // Высота lightmap в пикселях
    int offset = 0;                 // Смещение в lighting lump
    float minS = 0.0f, minT = 0.0f; // Минимальные UV для расчёта координат
    glm::vec2 uvMin{ 0.0f };          // UV в атласе (min)
    glm::vec2 uvMax{ 0.0f };          // UV в атласе (max)
    bool valid = false;             // Валидна ли lightmap
};

// Атлас lightmaps
class LightmapAtlas {
public:
    GLuint atlasTexture = 0;
    int atlasSize = 2048;
    int currentX = 0, currentY = 0;
    int rowHeight = 0;
    bool initialized = false;

    bool init();
    // Возвращает UV координаты в атласе: (minU, minV, maxU, maxV)
    glm::vec4 packLightmap(int width, int height, const uint8_t* data);
    void bind(GLuint unit) const;
    void cleanup();
};

// Менеджер lightmaps - ИСПРАВЛЕННЫЙ
class LightmapManager {
public:
    LightmapManager();
    ~LightmapManager();

    LightmapManager(const LightmapManager&) = delete;
    LightmapManager& operator=(const LightmapManager&) = delete;
    LightmapManager(LightmapManager&& other) noexcept;
    LightmapManager& operator=(LightmapManager&& other) noexcept;

    // Инициализация из BSP - КЛЮЧЕВОЙ МЕТОД
    bool initializeFromBSP(const BSPLoader& bsp);

    // Получение lightmap для face
    const FaceLightmap& getFaceLightmap(int faceIndex) const;

    // Проверка наличия lightmaps
    bool hasLightmaps() const { return !faceLightmaps.empty() && hasValidLightmaps; }

    // Получение количества
    size_t getLightmapCount() const { return faceLightmaps.size(); }

    // Атлас
    const LightmapAtlas& getAtlas() const { return atlas; }

    void debugPrintLightmapInfo() const;

    // Очистка
    void cleanup();

private:
    std::vector<FaceLightmap> faceLightmaps;
    LightmapAtlas atlas;
    bool hasValidLightmaps = false;

    // Создание текстуры из raw данных
    GLuint createLightmapTexture(int width, int height, const uint8_t* data);
};