#include "BrushRenderer.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

// Константы
#define PRIMITIVE_RESTART_IDX 0xFFFF

BrushRenderer::BrushRenderer() = default;

BrushRenderer::~BrushRenderer() {
    cleanup();
}

bool BrushRenderer::initialize(const BSPLoader& bspLoader) {
    if (initialized) {
        return true;
    }

    // Создаем буферы для батчинга
    createBuffers(bspLoader);

    initialized = true;
    return true;
}

void BrushRenderer::cleanup() {
    if (worldEBO != 0) {
        glDeleteBuffers(1, &worldEBO);
        worldEBO = 0;
    }
    worldEBOBuf.clear();
    sortBuffer.clear();
    initialized = false;
}

void BrushRenderer::beginRendering() {
    batchReset();
}

void BrushRenderer::drawBrushEntity(const Camera& camera,
                                    const glm::vec3& origin,
                                    const glm::vec3& angles,
                                    unsigned int firstFace,
                                    unsigned int numFaces,
                                    const Shader& shader) {
    if (!initialized || numFaces == 0) {
        return;
    }

    // Заглушка для AABB - в реальной реализации нужно получить из BSP
    glm::vec3 mins(-64, -64, -64);
    glm::vec3 maxs(64, 64, 64);

    glm::mat4 transformMat;
    if (prepareEntity(camera, origin, angles, mins, maxs, transformMat)) {
        return; // Отсечено frustum'ом
    }

    // Настраиваем GL
    glBindVertexArray(/* нужен VAO */ 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);

    // Включаем шейдер и настраиваем матрицу модели
    shader.bind();
    // shader.setUniform("modelMatrix", transformMat);
    // shader.setUniform("renderMode", 0); // kRenderNormal
    // shader.setUniform("renderFxAmount", 0.0f);
    // shader.setUniform("renderFxColor", glm::vec3(0));

    // Рисуем поверхности без сортировки
    for (unsigned int i = 0; i < numFaces; i++) {
        unsigned int surfIdx = firstFace + i;
        
        // Заглушка: в реальности нужно получить данные о поверхности из BSPLoader
        unsigned int vertexOffset = surfIdx * 4; // Пример
        unsigned int vertexCount = 4;            // Пример
        
        // Проверка смены материала
        unsigned int matIdx = 0; // Заглушка
        
        if (matIdx != currentMaterialIdx) {
            batchFlush(shader);
            currentMaterialIdx = matIdx;
            
            // В реальности: mat->activateTextures()
            // shader.setUniform("modelMatrix", transformMat);
        }

        batchAddSurface(vertexOffset, vertexCount);
    }

    batchFlush(shader);
    stats.brushEntPolys += numFaces;
}

void BrushRenderer::drawSortedBrushEntity(const Camera& camera,
                                          const glm::vec3& origin,
                                          const glm::vec3& angles,
                                          unsigned int firstFace,
                                          unsigned int numFaces,
                                          const Shader& shader,
                                          float renderFxAmount,
                                          const glm::vec3& renderFxColor) {
    if (!initialized || numFaces == 0) {
        return;
    }

    // Заглушка для AABB
    glm::vec3 mins(-64, -64, -64);
    glm::vec3 maxs(64, 64, 64);

    glm::mat4 transformMat;
    if (prepareEntity(camera, origin, angles, mins, maxs, transformMat)) {
        return; // Отсечено frustum'ом
    }

    // Настраиваем GL
    glBindVertexArray(/* нужен VAO */ 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);

    // Очищаем буфер сортировки
    sortBuffer.clear();

    // Собираем поверхности для сортировки
    for (unsigned int i = 0; i < numFaces; i++) {
        unsigned int surfIdx = firstFace + i;
        sortBuffer.push_back({surfIdx, 0.0f});
    }

    // Сортируем поверхности по дальности от камеры
    // Примечание: в оригинальном коде используется dot product с forward вектором
    // Получаем forward вектор через существующий API Camera
    float frontX, frontY, frontZ;
    // В вашем Camera нет getFront(), используем внутреннюю логику или заглушку
    // Для правильной работы нужно добавить getter во Front вектор в Camera.h
    glm::vec3 viewForward(0, 0, -1); // Заглушка - направление взгляда по умолчанию
    
    std::sort(sortBuffer.begin(), sortBuffer.end(), 
              [&, this](const SurfaceSortData& lhs, const SurfaceSortData& rhs) {
                  // Заглушка: в реальности нужно получить origin поверхности
                  glm::vec3 lhsOrg = glm::vec3(0) + origin; // surface[lhs.surfaceIndex].origin + origin
                  glm::vec3 rhsOrg = glm::vec3(0) + origin; // surface[rhs.surfaceIndex].origin + origin
                  
                  float lhsDist = glm::dot(lhsOrg, viewForward);
                  float rhsDist = glm::dot(rhsOrg, viewForward);
                  
                  // Сортировка от дальнего к ближнему (для прозрачности)
                  return lhsDist > rhsDist;
              });

    // Включаем шейдер и настраиваем параметры
    shader.bind();
    // shader.setUniform("modelMatrix", transformMat);
    // shader.setUniform("renderMode", 0); // kRenderNormal или другой
    // shader.setUniform("renderFxAmount", renderFxAmount / 255.0f);
    // shader.setUniform("renderFxColor", renderFxColor / 255.0f);

    // Рисуем отсортированные поверхности
    for (const auto& sortData : sortBuffer) {
        unsigned int surfIdx = sortData.surfaceIndex;
        
        // Заглушка: в реальности нужно получить данные о поверхности
        unsigned int vertexOffset = surfIdx * 4;
        unsigned int vertexCount = 4;
        
        unsigned int matIdx = 0; // Заглушка
        
        if (matIdx != currentMaterialIdx) {
            batchFlush(shader);
            currentMaterialIdx = matIdx;
            
            // В реальности: mat->activateTextures()
            // shader.setUniform("modelMatrix", transformMat);
        }

        batchAddSurface(vertexOffset, vertexCount);
    }

    batchFlush(shader);
    stats.brushEntPolys += numFaces;
}

void BrushRenderer::createBuffers(const BSPLoader& bspLoader) {
    // Вычисляем размер буфера для всех brush моделей
    // В GoldSrc brush модели идут после мировых поверхностей
    
    // Заглушка: в реальности нужно получить количество вершин из BSP
    unsigned int vertexCount = 50000;
    unsigned int surfaceCount = 5000;
    
    // Максимальный размер EBO = вершины + primitive restart индексы
    unsigned int maxEboSize = vertexCount + surfaceCount;
    worldEBOBuf.resize(maxEboSize);

    glGenBuffers(1, &worldEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 maxEboSize * sizeof(uint16_t),
                 nullptr,
                 GL_DYNAMIC_DRAW);
}

bool BrushRenderer::prepareEntity(const Camera& camera,
                                  const glm::vec3& origin,
                                  const glm::vec3& angles,
                                  const glm::vec3& mins,
                                  const glm::vec3& maxs,
                                  glm::mat4& outTransform) const {
    // Frustum culling
    glm::vec3 entityMins, entityMaxs;
    
    if (angles == glm::vec3(0)) {
        // Без вращения
        entityMins = origin + mins;
        entityMaxs = origin + maxs;
    } else {
        // С вращением - используем сферу вокруг объекта
        float radius = glm::length(maxs - mins) * 0.5f;
        entityMins = origin - glm::vec3(radius);
        entityMaxs = origin + glm::vec3(radius);
    }

    // Проверка видимости AABB во frustum'е
    // if (camera.cullBox(entityMins, entityMaxs)) {
    //     return true; // Отсечено
    // }

    outTransform = getBrushTransform(origin, angles);
    return false; // Не отсечено
}

glm::mat4 BrushRenderer::getBrushTransform(const glm::vec3& origin, 
                                           const glm::vec3& angles) const {
    glm::mat4 modelMat = glm::identity<glm::mat4>();
    
    // Применяем вращение в порядке Z-X-Y (как в GoldSrc/Half-Life)
    modelMat = glm::rotate(modelMat, glm::radians(angles.z), glm::vec3(1.0f, 0.0f, 0.0f));
    modelMat = glm::rotate(modelMat, glm::radians(angles.x), glm::vec3(0.0f, 1.0f, 0.0f));
    modelMat = glm::rotate(modelMat, glm::radians(angles.y), glm::vec3(0.0f, 0.0f, 1.0f));
    
    // Применяем трансляцию
    modelMat = glm::translate(modelMat, origin);
    
    return modelMat;
}

void BrushRenderer::batchAddSurface(unsigned int vertexOffset, unsigned int vertexCount) {
    if (worldEBOIdx + vertexCount >= worldEBOBuf.size()) {
        // Переполнение EBO - сбрасываем текущий батч
        batchFlush(/* shader */ Shader()); // Заглушка
        batchReset();
        stats.eboOverflows++;
    }

    // Добавляем индексы вершин
    for (uint16_t j = 0; j < vertexCount; j++) {
        if (worldEBOIdx + j < worldEBOBuf.size()) {
            worldEBOBuf[worldEBOIdx + j] = (uint16_t)vertexOffset + j;
        }
    }

    // Добавляем primitive restart индекс
    if (worldEBOIdx + vertexCount < worldEBOBuf.size()) {
        worldEBOBuf[worldEBOIdx + vertexCount] = PRIMITIVE_RESTART_IDX;
    }
    
    worldEBOIdx += vertexCount + 1;
}

void BrushRenderer::batchFlush(const Shader& shader) {
    if (worldEBOOffset == worldEBOIdx) {
        return; // Пустой батч
    }

    // Убираем последний PRIMITIVE_RESTART_IDX
    if (worldEBOIdx > 0) {
        worldEBOIdx--;
    }

    unsigned int vertexCount = worldEBOIdx - worldEBOOffset;
    
    if (vertexCount > 0 && worldEBOOffset + vertexCount <= worldEBOBuf.size()) {
        // Обновляем EBO
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                       worldEBOOffset * sizeof(uint16_t),
                       vertexCount * sizeof(uint16_t),
                       worldEBOBuf.data() + worldEBOOffset);

        // Рисуем элементы
        glDrawElements(GL_TRIANGLE_FAN, vertexCount, GL_UNSIGNED_SHORT,
                      reinterpret_cast<const void*>(worldEBOOffset * sizeof(uint16_t)));

        stats.drawCalls++;
    }

    worldEBOOffset = worldEBOIdx;
}

void BrushRenderer::batchReset() {
    worldEBOIdx = 0;
    worldEBOOffset = 0;
    currentMaterialIdx = 0;
}
