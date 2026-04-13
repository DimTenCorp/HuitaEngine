#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "BSPLoader.h"

// Данные lightmap для одной грани (face)
struct FaceLightmap {
    GLuint textureID = 0;           // Индивидуальная текстура (для отладки)
    int width = 0;                  // Ширина в пикселях
    int height = 0;                 // Высота в пикселях
    int offset = 0;                 // Смещение в lighting lump
    float minS = 0.0f, minT = 0.0f; // Минимальные UV для расчета координат
    float maxS = 0.0f, maxT = 0.0f; // Максимальные UV для расчета координат
    glm::vec2 uvMin{ 0.0f };        // UV в атласе (min)
    glm::vec2 uvMax{ 0.0f };        // UV в атласе (max)
    bool valid = false;             // Валиден ли лайтмап
};

// Атлас лайтмапов
class LightmapAtlas {
public:
    GLuint atlasTexture = 0;
    int atlasWidth = 0;   // Реальная ширина атласа (может быть любой, например 1535)
    int atlasHeight = 0;  // Реальная высота атласа (может быть любой, например 1457)

    // Для упаковки (row packing)
    int currentX = 0;
    int currentY = 0;
    int rowHeight = 0;

    bool initialized = false;

    // Инициализация с точным размером (поддерживает неквадратные и нестандартные размеры)
    bool init(int width, int height);

    // Упаковка лайтмапа в атлас
    // Возвращает UV координаты: (minU, minV, maxU, maxV)
    glm::vec4 packLightmap(int width, int height, const uint8_t* data);

    void bind(GLuint unit) const;
    void cleanup();

    int getWidth() const { return atlasWidth; }
    int getHeight() const { return atlasHeight; }
};

// Менеджер лайтмапов
class LightmapManager {
public:
    LightmapManager();
    ~LightmapManager();

    LightmapManager(const LightmapManager&) = delete;
    LightmapManager& operator=(const LightmapManager&) = delete;
    LightmapManager(LightmapManager&& other) noexcept;
    LightmapManager& operator=(LightmapManager&& other) noexcept;

    // Основной метод: читает BSP, считает размеры и создает атлас нужного размера
    bool initializeFromBSP(const BSPLoader& bsp);

    const FaceLightmap& getFaceLightmap(int faceIndex) const;
    bool hasLightmaps() const { return !faceLightmaps.empty() && hasValidLightmaps; }
    size_t getLightmapCount() const { return faceLightmaps.size(); }

    const LightmapAtlas& getAtlas() const { return atlas; }
    void debugPrintLightmapInfo() const;
    void cleanup();

private:
    std::vector<FaceLightmap> faceLightmaps;
    LightmapAtlas atlas;
    bool hasValidLightmaps = false;

    GLuint createLightmapTexture(int width, int height, const uint8_t* data);

    // Вспомогательная функция для расчета оптимального размера атласа
    void calculateOptimalAtlasSize(const std::vector<FaceLightmap>& tempMaps, int& outWidth, int& outHeight);
};