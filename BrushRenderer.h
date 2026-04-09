#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "BSPLoader.h"
#include "Shader.h"
#include "BSPLightmap.h"

// Класс для рендеринга brush моделей (дверей, лифтов, вращающихся объектов и т.д.)
class BrushRenderer {
public:
    BrushRenderer();
    ~BrushRenderer();

    // Инициализация после загрузки BSP
    bool init(const BSPLoader& bspLoader);
    void cleanup();

    // Основной метод рендеринга всех brush моделей
    void render(const glm::vec3& viewPos, const glm::mat4& view, const glm::mat4& projection,
                Shader& shader, const BSPLightmap& lightmap);

    // Рендеринг brush модели без сортировки (для непрозрачных объектов)
    void drawBrushEntity(const glm::vec3& viewPos,
                         const glm::mat4& view, const glm::mat4& projection,
                         const glm::vec3& origin,
                         const glm::vec3& angles,
                         unsigned int firstFace,
                         unsigned int numFaces,
                         const Shader& shader,
                         const BSPLightmap& lightmap);

    // Рендеринг brush модели с сортировкой поверхностей (для прозрачных объектов)
    void drawSortedBrushEntity(const glm::vec3& viewPos,
                               const glm::mat4& view, const glm::mat4& projection,
                               const glm::vec3& origin,
                               const glm::vec3& angles,
                               unsigned int firstFace,
                               unsigned int numFaces,
                               const Shader& shader,
                               const BSPLightmap& lightmap,
                               float renderFxAmount = 0.0f,
                               const glm::vec3& renderFxColor = glm::vec3(0));

    // Статистика
    struct Stats {
        unsigned int drawCalls = 0;
        unsigned int brushEntPolys = 0;
        unsigned int eboOverflows = 0;
        void reset() { drawCalls = 0; brushEntPolys = 0; eboOverflows = 0; }
    };
    const Stats& getStats() const { return stats; }
    
    // Получение статистики для Renderer
    unsigned int getLastDrawCalls() const { return lastDrawCalls; }
    unsigned int getLastTriangles() const { return lastTriangles; }

private:
    // Данные о поверхностях для сортировки
    struct SurfaceSortData {
        unsigned int surfaceIndex = 0;
        float distance = 0.0f;
    };

    // Буферы для батчинга
    GLuint worldEBO = 0;
    std::vector<uint16_t> worldEBOBuf;
    unsigned int worldEBOOffset = 0;
    unsigned int worldEBOIdx = 0;

    // Текущий материал батча
    unsigned int currentMaterialIdx = 0;

    // Буфер для сортировки
    std::vector<SurfaceSortData> sortBuffer;

    // Статистика
    Stats stats;
    bool initialized = false;
    
    // Статистика для последнего кадра
    unsigned int lastDrawCalls = 0;
    unsigned int lastTriangles = 0;

    // Методы
    void createBuffers(const BSPLoader& bspLoader);
    
    // Подготовка entity к рендерингу
    // @returns true если entity отсечена frustum'ом
    bool prepareEntity(const glm::vec3& viewPos,
                       const glm::mat4& view, const glm::mat4& projection,
                       const glm::vec3& origin,
                       const glm::vec3& angles,
                       const glm::vec3& mins,
                       const glm::vec3& maxs,
                       glm::mat4& outTransform) const;

    // Получение матрицы трансформации brush модели
    glm::mat4 getBrushTransformation(const glm::vec3& origin, const glm::vec3& angles) const;
    
    // Псевдоним для совместимости
    glm::mat4 getBrushTransform(const glm::vec3& origin, const glm::vec3& angles) const {
        return getBrushTransformation(origin, angles);
    }

    // Добавление поверхности в текущий батч
    void batchAddSurface(unsigned int vertexOffset, unsigned int vertexCount);

    // Отправка текущего батча на рендеринг
    void batchFlush(const Shader& shader);

    // Сброс текущего батча
    void batchReset();
};
