#pragma once
#include <glm/glm.hpp>
#include <vector>

// Константы для водных эффектов
constexpr float TURB_SCALE = 256.0f / (2.0f * 3.14159265358979323846f);
constexpr float TURB_AMPLITUDE = 8.0f;
constexpr float TURB_SPEED = 160.0f;
constexpr float SUBDIVIDE_SIZE = 64.0f;

// Предварительно вычисленная таблица синусов для ускорения расчетов
class TurbSinTable {
public:
    static const TurbSinTable& instance() {
        static TurbSinTable table;
        return table;
    }

    float get(int index) const {
        return data[index & 255];
    }

private:
    TurbSinTable() {
        for (int i = 0; i < 256; i++) {
            data[i] = glm::sin((i / 256.0f) * 2.0f * 3.14159265358979323846f) * TURB_AMPLITUDE;
        }
    }

    float data[256];
};

// Структура для хранения параметров воды
struct WaterParams {
    glm::vec3 color{0.0f, 0.3f, 0.5f};  // Цвет воды (RGB)
    float fogDensity{0.5f};              // Плотность тумана под водой
    float waveSpeed{1.0f};               // Скорость анимации волн
    float waveAmplitude{TURB_AMPLITUDE}; // Амплитуда волн
    
    // Параметры из палитры текстуры (как в оригинальном HL1)
    unsigned char paletteColor[4];       // RGB + fog из палитры
};

// Типы поверхностей
enum class SurfaceType {
    Normal,
    Water,          // Обычная вода с warp-эффектом
    Slime,          // Лава/слизь с более агрессивным warp
    Sky,            // Небо
    Underwater      // Поверхность под водой
};

// Структура для subdivided полигона воды
struct WaterPoly {
    std::vector<glm::vec3> vertices;    // Позиции вершин
    std::vector<glm::vec2> texCoords;   // Текстурные координаты
    std::vector<glm::vec2> lightCoords; // Координаты для lightmap
    SurfaceType type;
    
    void clear() {
        vertices.clear();
        texCoords.clear();
        lightCoords.clear();
    }
};

// Класс для управления водными эффектами
class WaterManager {
public:
    WaterManager();
    ~WaterManager();

    // Инициализация таблицы синусов
    void init();

    // Обновление состояния воды (время, параметры)
    void update(float deltaTime, float currentTime);

    // Получение смещения вершины для warp-эффекта
    float getWarpOffset(const glm::vec3& vertex, float time) const;

    // Расчет текстурных координат с учетом warp
    glm::vec2 getWarpedTexCoord(const glm::vec2& originalTC, 
                                 const glm::vec3& vertex, 
                                 float time) const;

    // Разбиение полигона на части для корректного warp (как GL_SubdivideSurface)
    void subdividePolygon(const std::vector<glm::vec3>& verts,
                          const std::vector<glm::vec2>& texCoords,
                          SurfaceType type,
                          std::vector<WaterPoly>& outPolys);

    // Проверка, находится ли камера под водой
    bool isCameraUnderwater(const glm::vec3& cameraPos, 
                            const std::vector<WaterPoly>& waterPolys) const;

    // Получение параметров воды для текущей позиции
    const WaterParams& getWaterParams(SurfaceType type) const;

    // Установка цветового фильтра для подводного вида
    void setUnderwaterColor(const glm::vec3& color, float density);

    // Получение текущего цветового фильтра
    glm::vec3 getUnderwaterColor() const { return underwaterColor; }
    float getUnderwaterFogDensity() const { return underwaterFogDensity; }

private:
    float currentTime;
    glm::vec3 underwaterColor;
    float underwaterFogDensity;
    
    std::vector<WaterParams> waterParams;
    
    // Рекурсивная функция для разбиения полигона
    void subdividePolygonRecursive(const std::vector<glm::vec3>& verts,
                                   const std::vector<glm::vec2>& texCoords,
                                   SurfaceType type,
                                   int stage,
                                   std::vector<WaterPoly>& outPolys);
    
    // Ограничение полигона по границам
    void boundPoly(const std::vector<glm::vec3>& verts,
                   glm::vec3& mins, glm::vec3& maxs) const;
    
    // Разрезание полигона плоскостью
    bool clipPolygon(const std::vector<glm::vec3>& inVerts,
                     const std::vector<glm::vec2>& inTexCoords,
                     const glm::vec3& normal,
                     float dist,
                     std::vector<glm::vec3>& frontVerts,
                     std::vector<glm::vec2>& frontTexCoords,
                     std::vector<glm::vec3>& backVerts,
                     std::vector<glm::vec2>& backTexCoords) const;
};
