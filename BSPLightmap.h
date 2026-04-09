#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

// Forward declaration
class BSPLoader;

/**
 * Адаптированная система стандартных BSP лайтмапов из BSPRenderer
 * 
 * Особенности:
 * - Упаковка всех поверхностей в один атлас через stb_rect_pack
 * - Поддержка 4 световых стилей (lightstyles) для анимированного освещения
 * - Gamma коррекция (2.5)
 * - Padding для предотвращения артефактов фильтрации
 */
class BSPLightmap {
public:
    BSPLightmap();
    ~BSPLightmap();

    BSPLightmap(const BSPLightmap&) = delete;
    BSPLightmap& operator=(const BSPLightmap&) = delete;
    BSPLightmap(BSPLightmap&& other) noexcept;
    BSPLightmap& operator=(BSPLightmap&& other) noexcept;

    /**
     * Инициализирует лайтмапы из данных BSP
     * @param bspLoader Загруженный BSP файл
     * @param vertices Вершины меша
     * @param faces Данные граней из BSP
     * @return true если успешно
     */
    bool buildFromBSP(BSPLoader& bspLoader, 
                      const std::vector<struct BSPVertex>& vertices,
                      const std::vector<struct BSPFace>& faces);

    /**
     * Очищает ресурсы
     */
    void destroy();

    /**
     * Привязывает текстуру лайтмапа к указанному юниту
     */
    void bind(GLuint textureUnit = GL_TEXTURE0) const;

    /**
     * @return ID текстуры лайтмапа
     */
    GLuint getTextureID() const { return m_textureID; }

    /**
     * @return Размер одной страницы лайтмапа (например, 128)
     */
    int getPageSize() const { return m_pageSize; }

    /**
     * @return Количество страниц (слоёв) в текстуре-массиве
     */
    int getPageCount() const { return m_pageCount; }

    /**
     * @return Gamma значение для корректного отображения
     */
    static constexpr float getGamma() { return 2.5f; }

    /**
     * Обновляет фильтр текстуры
     * @param filterEnabled true для билинейной фильтрации, false для nearest
     */
    void setFiltering(bool filterEnabled);

    /**
     * Устанавливает масштаб для каждого lightstyle (для анимации)
     * @param styleIndex Индекс стиля (0-3)
     * @param scale Множитель яркости
     */
    void setLightstyleScale(int styleIndex, float scale);

    /**
     * @return Массив масштабов lightstyles
     */
    const float* getLightstyleScales() const { return m_lightstyleScales; }

private:
    static constexpr int LIGHTMAP_DIVISOR = 16;
    static constexpr int MAX_LIGHTMAP_BLOCK_SIZE = 2048;
    static constexpr float LIGHTMAP_BLOCK_WASTED = 0.40f;
    static constexpr int LIGHTMAP_PADDING = 2;
    static constexpr int NUM_LIGHTSTYLES = 4;

    struct SurfaceData {
        glm::vec2 textureMins{0.0f, 0.0f};
        glm::ivec2 size{0, 0};
        glm::ivec2 offset{0, 0};
        bool hasLightmap = false;
    };

    GLuint m_textureID = 0;
    int m_pageSize = 128;
    int m_pageCount = 0;
    
    // Данные для обновления UV координат вершин
    std::vector<SurfaceData> m_surfaceData;
    
    // Масштабы для lightstyles (анимация освещения)
    float m_lightstyleScales[NUM_LIGHTSTYLES] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Внутренние методы
    bool packLightmaps(const std::vector<struct BSPFace>& faces,
                       std::vector<SurfaceData>& surfaceData,
                       int& outTextureSize);
    
    void uploadToGPU(const std::vector<unsigned char>& lightmapData,
                     int textureSize,
                     const std::vector<SurfaceData>& surfaceData);
};
