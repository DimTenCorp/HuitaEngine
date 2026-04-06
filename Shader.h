#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class Shader {
public:
    Shader();
    ~Shader();

    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);
    bool loadFromStrings(const std::string& vertSource, const std::string& fragSource);
    void unload();

    void bind() const;
    void unbind() const;
    bool isValid() const { return programID != 0; }
    GLuint getID() const { return programID; }

    void setBool(const std::string& name, bool value);
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec2(const std::string& name, float x, float y);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec3(const std::string& name, float x, float y, float z);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setMat3(const std::string& name, const glm::mat3& value);
    void setMat4(const std::string& name, const glm::mat4& value);
    void setMat4(const std::string& name, const float* value);

    void setIntArray(const std::string& name, int* values, int count);
    void setFloatArray(const std::string& name, float* values, int count);
    void setVec3Array(const std::string& name, const std::vector<glm::vec3>& values);

    std::string getError() const { return lastError; }
    void printActiveUniforms() const;

private:
    GLuint programID = 0;
    mutable std::unordered_map<std::string, GLint> uniformCache;
    std::string lastError;

    bool readFile(const std::string& path, std::string& out);
    GLuint compile(GLenum type, const std::string& source);
    bool link(GLuint vert, GLuint frag);
    GLint getUniformLocation(const std::string& name) const;

    std::string vertPath, fragPath;
    std::string vertSourceCached, fragSourceCached;
};