#include "pch.h"
#include "SkyboxRenderer.h"
#include "Shader.h"
#include "WADLoader.h"
#include <iostream>
#include <fstream>

SkyboxRenderer::SkyboxRenderer() = default;

SkyboxRenderer::~SkyboxRenderer() {
    unload();
}

const char* SkyboxRenderer::getVertexShader() {
    return R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 vWorldDir;
uniform mat4 projection;
uniform mat4 view;
void main() {
    // Убираем трансляцию из view матрицы (только вращение)
    mat4 viewRot = mat4(mat3(view));
    vec4 pos = projection * viewRot * vec4(aPos, 1.0);
    // Делаем z = w чтобы скайбокс был на far plane
    gl_Position = pos.xyww;
    vWorldDir = aPos;
}
)glsl";
}

const char* SkyboxRenderer::getFragmentShader() {
    return R"glsl(
#version 330 core
in vec3 vWorldDir;
out vec4 FragColor;
uniform samplerCube skyTex;

void main() {
    vec3 dir = normalize(vWorldDir);
    // Для Half-Life скайбокса нужно инвертировать X чтобы грани правильно совпадали
    dir.x = -dir.x;
    vec4 texColor = texture(skyTex, dir);
    FragColor = texColor;
}
)glsl";
}

bool SkyboxRenderer::init() {
    shader = std::make_unique<Shader>();
    if (!shader->compile(getVertexShader(), getFragmentShader())) {
        std::cerr << "[SKY] Shader failed: " << shader->getLastError() << std::endl;
        return false;
    }
    return true;
}

void SkyboxRenderer::createCubeMesh() {
    // Куб от -1 до 1 по всем осям для кубической текстуры
    // Вершины те же, но теперь мы используем cube map texture
    
    float vertices[] = {
        // Right face (+X)
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        // Left face (-X)
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        // Back face (+Y) 
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        // Front face (-Y)
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // Top face (+Z)
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // Bottom face (-Z)
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f
    };

    GLuint indices[] = {
        // Right
        0, 1, 2,  2, 3, 0,
        // Left
        4, 5, 6,  6, 7, 4,
        // Back
        8, 9, 10, 10, 11, 8,
        // Front
        12, 13, 14, 14, 15, 12,
        // Top
        16, 17, 18, 18, 19, 16,
        // Bottom
        20, 21, 22, 22, 23, 20
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
    vertexCount = 36;
}

bool SkyboxRenderer::loadTGA(const std::string& path, std::vector<uint8_t>& rgba, int& width, int& height) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint8_t id_length = file.get();
    uint8_t colormap_type = file.get();
    uint8_t image_type = file.get();

    file.seekg(5, std::ios::cur);
    file.seekg(4, std::ios::cur);

    uint16_t w, h;
    file.read(reinterpret_cast<char*>(&w), 2);
    file.read(reinterpret_cast<char*>(&h), 2);
    width = w;
    height = h;

    uint8_t pixel_size = file.get();
    file.get(); // attributes

    if (id_length > 0) file.seekg(id_length, std::ios::cur);

    if (image_type != 2 && image_type != 10) return false;
    if (pixel_size != 24 && pixel_size != 32) return false;

    rgba.resize(width * height * 4);

    if (image_type == 2) {
        for (int y = height - 1; y >= 0; y--) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                uint8_t b = file.get();
                uint8_t g = file.get();
                uint8_t r = file.get();
                uint8_t a = (pixel_size == 32) ? file.get() : 255;
                rgba[idx + 0] = r;
                rgba[idx + 1] = g;
                rgba[idx + 2] = b;
                rgba[idx + 3] = a;
            }
        }
    }
    else {
        // RLE encoded
        for (int y = height - 1; y >= 0; y--) {
            int x = 0;
            while (x < width) {
                uint8_t packetHeader = file.get();
                uint8_t packetSize = 1 + (packetHeader & 0x7f);

                if (packetHeader & 0x80) {
                    uint8_t b = file.get();
                    uint8_t g = file.get();
                    uint8_t r = file.get();
                    uint8_t a = (pixel_size == 32) ? file.get() : 255;

                    for (int i = 0; i < packetSize && x < width; i++, x++) {
                        int idx = (y * width + x) * 4;
                        rgba[idx + 0] = r;
                        rgba[idx + 1] = g;
                        rgba[idx + 2] = b;
                        rgba[idx + 3] = a;
                    }
                }
                else {
                    for (int i = 0; i < packetSize && x < width; i++, x++) {
                        uint8_t b = file.get();
                        uint8_t g = file.get();
                        uint8_t r = file.get();
                        uint8_t a = (pixel_size == 32) ? file.get() : 255;

                        int idx = (y * width + x) * 4;
                        rgba[idx + 0] = r;
                        rgba[idx + 1] = g;
                        rgba[idx + 2] = b;
                        rgba[idx + 3] = a;
                    }
                }
            }
        }
    }
    return true;
}

bool SkyboxRenderer::loadBMP(const std::string& path, std::vector<uint8_t>& rgba, int& width, int& height) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint16_t magic;
    file.read(reinterpret_cast<char*>(&magic), 2);
    if (magic != 0x4D42) return false;

    file.seekg(8, std::ios::cur);
    uint32_t dataOffset;
    file.read(reinterpret_cast<char*>(&dataOffset), 4);

    uint32_t headerSize;
    file.read(reinterpret_cast<char*>(&headerSize), 4);

    int32_t w, h;
    file.read(reinterpret_cast<char*>(&w), 4);
    file.read(reinterpret_cast<char*>(&h), 4);
    width = abs(w);
    height = abs(h);

    file.seekg(2, std::ios::cur);
    uint16_t bpp;
    file.read(reinterpret_cast<char*>(&bpp), 2);

    if (bpp != 24) return false;

    rgba.resize(width * height * 4);
    file.seekg(dataOffset, std::ios::beg);

    int rowSize = ((width * 3 + 3) / 4) * 4;

    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            uint8_t b = file.get();
            uint8_t g = file.get();
            uint8_t r = file.get();

            int idx = (y * width + x) * 4;
            rgba[idx + 0] = r;
            rgba[idx + 1] = g;
            rgba[idx + 2] = b;
            rgba[idx + 3] = 255;
        }
        for (int p = 0; p < rowSize - width * 3; p++) file.get();
    }
    return true;
}

// Helper для получения данных текстуры из WADLoader через glGetTexImage
bool SkyboxRenderer::loadTextureFromWAD(const std::string& texName, WADLoader& wadLoader, 
                                         std::vector<uint8_t>& rgba, int& width, int& height) {
    int tw, th;
    GLuint texId;
    if (!wadLoader.getTextureInfo(texName, tw, th, texId)) {
        return false;
    }
    
    if (texId == 0) {
        return false;
    }
    
    width = tw;
    height = th;
    rgba.resize(width * height * 4);
    
    // Bind текстуру и читаем данные
    glBindTexture(GL_TEXTURE_2D, texId);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "[SKY] Loaded from WAD: " << texName << " (" << width << "x" << height << ")" << std::endl;
    return true;
}

bool SkyboxRenderer::loadSkyTextures(const std::string& skyName, WADLoader& wadLoader) {
    if (skyName.empty()) return false;
    currentSkyName = skyName;

    // Удаляем старую cube map текстуру если есть
    if (cubeMapTexture) {
        glDeleteTextures(1, &cubeMapTexture);
        cubeMapTexture = 0;
    }

    // HL1 порядок суффиксов: rt, bk, lf, ft, up, dn
    const char* hl1Suffixes[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

    // Маппинг Half-Life граней на OpenGL cube map грани
    // OpenGL: GL_TEXTURE_CUBE_MAP_POSITIVE_X = +X (right)
    //         GL_TEXTURE_CUBE_MAP_NEGATIVE_X = -X (left)
    //         GL_TEXTURE_CUBE_MAP_POSITIVE_Y = +Y (top)
    //         GL_TEXTURE_CUBE_MAP_NEGATIVE_Y = -Y (bottom)
    //         GL_TEXTURE_CUBE_MAP_POSITIVE_Z = +Z (back)
    //         GL_TEXTURE_CUBE_MAP_NEGATIVE_Z = -Z (front)
    // Half-Life: rt(+X), lf(-X), bk(+Z), ft(-Z), up(+Y), dn(-Y)
    const int hl1ToGL[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,  // rt -> +X
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,  // bk -> +Z 
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,  // lf -> -X
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,  // ft -> -Z
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,  // up -> +Y
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y   // dn -> -Y
    };

    // Создаем cube map текстуру
    glGenTextures(1, &cubeMapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);

    int loadedCount = 0;

    for (int hl1Idx = 0; hl1Idx < 6; hl1Idx++) {
        std::vector<uint8_t> rgba;
        int w = 0, h = 0;
        bool loaded = false;
        const char* suffix = hl1Suffixes[hl1Idx];
        GLint glFace = hl1ToGL[hl1Idx];

        // Пробуем загрузить TGA из gfx/env/
        std::string tgaPath = "gfx/env/" + skyName + suffix + ".tga";
        if (loadTGA(tgaPath, rgba, w, h)) {
            loaded = true;
            std::cout << "[SKY] Loaded TGA: " << tgaPath << std::endl;
        }
        else {
            // Пробуем BMP
            std::string bmpPath = "gfx/env/" + skyName + suffix + ".bmp";
            if (loadBMP(bmpPath, rgba, w, h)) {
                loaded = true;
                std::cout << "[SKY] Loaded BMP: " << bmpPath << std::endl;
            }
        }

        // Fallback на WAD текстуры
        if (!loaded) {
            std::string texName = skyName + suffix;
            if (loadTextureFromWAD(texName, wadLoader, rgba, w, h)) {
                loaded = true;
            }
        }

        // Если всё ещё не загружено - пробуем без префикса (иногда текстуры называются просто rt, lf и т.д.)
        if (!loaded) {
            std::string texName = std::string(suffix);
            if (loadTextureFromWAD(texName, wadLoader, rgba, w, h)) {
                loaded = true;
            }
        }

        if (loaded) {
            // Загружаем в cube map
            glTexImage2D(glFace, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            loadedCount++;
        }
        else {
            // Создаем fallback текстуру - цветную заглушку
            uint8_t color[4] = { 128, 128, 128, 255 }; // Серый по умолчанию
            if (hl1Idx == 0) { color[0] = 100; color[1] = 50; color[2] = 50; }      // rt - красноватый
            else if (hl1Idx == 1) { color[0] = 50; color[1] = 50; color[2] = 100; } // bk - синеватый
            else if (hl1Idx == 2) { color[0] = 50; color[1] = 100; color[2] = 50; } // lf - зеленоватый
            else if (hl1Idx == 3) { color[0] = 100; color[1] = 100; color[2] = 50; } // ft - желтоватый
            else if (hl1Idx == 4) { color[0] = 50; color[1] = 100; color[2] = 100; } // up - голубоватый
            else { color[0] = 100; color[1] = 50; color[2] = 100; } // dn - фиолетовый

            // Создаем небольшую текстуру 1x1
            glTexImage2D(glFace, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
            std::cerr << "[SKY] Using fallback color for " << suffix << std::endl;
        }
    }

    // Настраиваем параметры cube map текстуры
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    std::cout << "[SKY] Loaded " << loadedCount << "/6 faces for: " << skyName << std::endl;
    return true;
}

bool SkyboxRenderer::loadSky(const std::string& skyName, WADLoader& wadLoader) {
    unload();
    if (!init()) return false;
    loadSkyTextures(skyName, wadLoader);
    createCubeMesh();
    loaded = true;
    return true;
}

void SkyboxRenderer::unload() {
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    if (cubeMapTexture) {
        glDeleteTextures(1, &cubeMapTexture);
        cubeMapTexture = 0;
    }
    shader.reset();
    loaded = false;
}

void SkyboxRenderer::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& viewPos) {
    (void)viewPos;

    if (!loaded || !shader || !cubeMapTexture) return;

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    shader->bind();
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);

    // Привязываем cube map текстуру
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    shader->unbind();

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}