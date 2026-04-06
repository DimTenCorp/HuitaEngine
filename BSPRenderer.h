#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

// Вершина BSP для рендеринга
struct BSPVertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec3 normal;
};

// Draw call для отрисовки группы граней с одной текстурой
struct FaceDrawCall {
    GLuint texID;
    unsigned int indexOffset;
    unsigned int indexCount;
};

// Класс для управления загрузкой и рендерингом BSP карт
class BSPLoader;  // Forward declaration

class BSPRenderer {
private:
    // OpenGL буферы
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;
    
    // Данные меша
    std::vector<BSPVertex> vertices;
    std::vector<unsigned int> indices;
    
    // Draw calls (группировка по текстурам)
    std::vector<FaceDrawCall> drawCalls;
    
    // Текстуры
    GLuint defaultTextureID;
    
    // Статус загрузки
    bool isLoaded;
    
public:
    BSPRenderer();
    ~BSPRenderer();
    
    // Инициализация OpenGL буферов
    void initBuffers(const std::vector<BSPVertex>& verts, 
                     const std::vector<unsigned int>& inds,
                     const std::vector<FaceDrawCall>& calls);
    
    // Рендеринг BSP карты
    void render(unsigned int shaderProgram,
                const glm::mat4& projection,
                const glm::mat4& view,
                const glm::mat4& model = glm::mat4(1.0f));
    
    // Очистка ресурсов
    void cleanup();
    
    // Геттеры
    bool getIsLoaded() const { return isLoaded; }
    size_t getVertexCount() const { return vertices.size(); }
    size_t getIndexCount() const { return indices.size(); }
    size_t getDrawCallCount() const { return drawCalls.size(); }
};
