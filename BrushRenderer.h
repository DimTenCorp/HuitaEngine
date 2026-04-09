#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "BSPLoader.h"
#include "Shader.h"
#include "Camera.h"

// Класс для рендеринга brush моделей (дверей, лифтов, вращающихся объектов и т.д.)
class BrushRenderer {
public:
    BrushRenderer();
    ~BrushRenderer();

    // Инициализация после загрузки BSP
    bool initialize(const BSPLoader& bspLoader);
    void cleanup();

    // Начало кадра для рендеринга brush моделей
    void beginRendering();

    // Рендеринг brush модели без сортировки (для непрозрачных объектов)
    void drawBrushEntity(const Camera& camera, 
                         const glm::vec3& origin,
                         const glm::vec3& angles,
                         unsigned int firstFace,
                         unsigned int numFaces,
                         const Shader& shader);

    // Рендеринг brush модели с сортировкой поверхностей (для прозрачных объектов)
    void drawSortedBrushEntity(const Camera& camera,
                               const glm::vec3& origin,
                               const glm::vec3& angles,
                               unsigned int firstFace,
                               unsigned int numFaces,
                               const Shader& shader,
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

    // Методы
    void createBuffers(const BSPLoader& bspLoader);
    
    // Подготовка entity к рендерингу
    // @returns true если entity отсечена frustum'ом
    bool prepareEntity(const Camera& camera,
                       const glm::vec3& origin,
                       const glm::vec3& angles,
                       const glm::vec3& mins,
                       const glm::vec3& maxs,
                       glm::mat4& outTransform) const;

    // Получение матрицы трансформации brush модели
    glm::mat4 getBrushTransform(const glm::vec3& origin, const glm::vec3& angles) const;

    // Добавление поверхности в текущий батч
    void batchAddSurface(unsigned int vertexOffset, unsigned int vertexCount);

    // Отправка текущего батча на рендеринг
    void batchFlush(const Shader& shader);

    // Сброс текущего батча
    void batchReset();
};
