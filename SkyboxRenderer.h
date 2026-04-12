#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <vector>

class Shader;
class WADLoader;

class SkyboxRenderer {
public:
    SkyboxRenderer();
    ~SkyboxRenderer();

    bool init();
    bool loadSky(const std::string& skyName, WADLoader& wadLoader);
    void unload();

    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& viewPos);

    bool isLoaded() const { return loaded; }

private:
    std::unique_ptr<Shader> shader;

    GLuint skyTextures[6] = { 0,0,0,0,0,0 };
    bool texturesLoaded[6] = { false,false,false,false,false,false };

    GLuint vao = 0, vbo = 0;
    int vertexCount = 0;

    bool loaded = false;
    std::string currentSkyName;

    bool loadSkyTextures(const std::string& skyName, WADLoader& wadLoader);
    void createCubeMesh();

    // File loading helpers
    bool loadTGA(const std::string& path, std::vector<uint8_t>& rgba, int& width, int& height);
    bool loadBMP(const std::string& path, std::vector<uint8_t>& rgba, int& width, int& height);

    static const char* getVertexShader();
    static const char* getFragmentShader();
};