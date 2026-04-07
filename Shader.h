#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

class Shader {
private:
    unsigned int ID;
    std::string lastError;
    std::string readFile(const char* filePath);
    void checkCompileErrors(unsigned int shader, std::string type);

public:
    Shader();
    Shader(const char* vertexPath, const char* fragmentPath);
    Shader(const std::string& vertexCode, const std::string& fragmentCode);
    ~Shader();

    bool loadFromStrings(const std::string& vertexCode, const std::string& fragmentCode);
    void bind();
    void unbind();
    int getLocation(const std::string& name);
    std::string getError() const { return lastError; }

    void use();
    unsigned int getID() const { return ID; }
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setMat4(const std::string& name, const float* value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
};