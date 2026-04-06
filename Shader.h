#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

// Типы шейдеров
enum ShaderType {
    SHADER_VERTEX = GL_VERTEX_SHADER,
    SHADER_FRAGMENT = GL_FRAGMENT_SHADER,
    SHADER_GEOMETRY = GL_GEOMETRY_SHADER
};

class Shader {
private:
    unsigned int ID;
    
    // Компиляция шейдера из строки
    unsigned int compileShader(ShaderType type, const char* source);
    
    // Проверка ошибок компиляции
    bool checkCompileErrors(unsigned int shader, ShaderType type, std::string& errorLog);
    bool checkLinkErrors(unsigned int program, std::string& errorLog);
    
public:
    // Конструкторы
    Shader();
    Shader(const char* vertexPath, const char* fragmentPath);
    Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath);
    
    // Конструктор из исходного кода (строк)
    static Shader createFromSource(const char* vertexSource, const char* fragmentSource);
    static Shader createFromSource(const char* vertexSource, const char* fragmentSource, const char* geometrySource);
    
    ~Shader();
    
    // Активация шейдера
    void use() const;
    
    // Uniform методы
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setVec4(const std::string& name, float x, float y, float z, float w) const;
    void setMat2(const std::string& name, const glm::mat2& mat) const;
    void setMat3(const std::string& name, const glm::mat3& mat) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
    
    // Получение ID программы
    unsigned int getID() const { return ID; }
    
    // Проверка валидности
    bool isValid() const { return ID != 0; }
};
