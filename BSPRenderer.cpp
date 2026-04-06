#include "BSPRenderer.h"
#include <iostream>

BSPRenderer::BSPRenderer() 
    : VAO(0), VBO(0), EBO(0), defaultTextureID(0), isLoaded(false) {
}

BSPRenderer::~BSPRenderer() {
    cleanup();
}

void BSPRenderer::initBuffers(const std::vector<BSPVertex>& verts,
                               const std::vector<unsigned int>& inds,
                               const std::vector<FaceDrawCall>& calls) {
    // Очищаем старые данные если есть
    if (VAO != 0) {
        cleanup();
    }
    
    vertices = verts;
    indices = inds;
    drawCalls = calls;
    
    if (vertices.empty() || indices.empty()) {
        std::cerr << "[BSPRenderer] Error: Empty vertices or indices!" << std::endl;
        return;
    }
    
    // Генерируем OpenGL буферы
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    // Загружаем вершины
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BSPVertex), 
                 vertices.data(), GL_STATIC_DRAW);
    
    // Загружаем индексы
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), 
                 indices.data(), GL_STATIC_DRAW);
    
    // Настраиваем атрибуты вершин
    // position - location 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), 
                          (void*)offsetof(BSPVertex, position));
    glEnableVertexAttribArray(0);
    
    // normal - location 1
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), 
                          (void*)offsetof(BSPVertex, normal));
    glEnableVertexAttribArray(1);
    
    // texCoord - location 2
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), 
                          (void*)offsetof(BSPVertex, texCoord));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
    
    isLoaded = true;
    
    std::cout << "[BSPRenderer] Buffers initialized: " 
              << vertices.size() << " vertices, " 
              << indices.size() << " indices, "
              << drawCalls.size() << " draw calls" << std::endl;
}

void BSPRenderer::render(unsigned int shaderProgram,
                          const glm::mat4& projection,
                          const glm::mat4& view,
                          const glm::mat4& model) {
    if (!isLoaded || VAO == 0) return;
    
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    
    // Получаем локации uniform переменных
    int mvpLoc = glGetUniformLocation(shaderProgram, "mvp");
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    
    glm::mat4 mvp = projection * view * model;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    
    // Рендерим по текстурам (batch rendering)
    GLuint lastTex = 0;
    for (const auto& dc : drawCalls) {
        if (dc.texID != lastTex) {
            glBindTexture(GL_TEXTURE_2D, dc.texID);
            lastTex = dc.texID;
        }
        
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        
        glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
                       (void*)(dc.indexOffset * sizeof(unsigned int)));
    }
    
    glBindVertexArray(0);
}

void BSPRenderer::cleanup() {
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (EBO != 0) {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
    
    vertices.clear();
    indices.clear();
    drawCalls.clear();
    
    isLoaded = false;
}
