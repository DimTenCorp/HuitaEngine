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
uniform sampler2D skyTex;
uniform int faceIndex;
uniform vec3 fallbackColor;

void main() {
    vec3 dir = normalize(vWorldDir);
    
    // Вычисляем UV для текущей грани куба
    vec2 uv;
    
    // Порядок граней: rt(+X), lf(-X), bk(+Y), ft(-Y), up(+Z), dn(-Z)
    if (faceIndex == 0) { // rt (+X)
        // x = 1, проецируем на YZ плоскость
        uv = vec2(-dir.z, dir.y) / abs(dir.x);
    }
    else if (faceIndex == 1) { // lf (-X)
        // x = -1, проецируем на YZ плоскость
        uv = vec2(dir.z, dir.y) / abs(dir.x);
    }
    else if (faceIndex == 2) { // bk (+Y)
        // y = 1, проецируем на XZ плоскость
        uv = vec2(dir.x, -dir.z) / abs(dir.y);
    }
    else if (faceIndex == 3) { // ft (-Y)
        // y = -1, проецируем на XZ плоскость
        uv = vec2(-dir.x, -dir.z) / abs(dir.y);
    }
    else if (faceIndex == 4) { // up (+Z)
        // z = 1, проецируем на XY плоскость
        uv = vec2(dir.x, dir.y) / abs(dir.z);
    }
    else { // dn (-Z)
        // z = -1, проецируем на XY плоскость
        uv = vec2(dir.x, -dir.y) / abs(dir.z);
    }
    
    // Преобразуем из [-1,1] в [0,1]
    uv = uv * 0.5 + 0.5;
    
    vec4 texColor = texture(skyTex, uv);
    
    // Если текстура черная - показываем fallback цвет
    if (texColor.r < 0.01 && texColor.g < 0.01 && texColor.b < 0.01) {
        FragColor = vec4(fallbackColor, 1.0);
    } else {
        FragColor = texColor;
    }
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
    // Куб от -1 до 1 по всем осям
    // Порядок граней: rt(+X), lf(-X), bk(+Y), ft(-Y), up(+Z), dn(-Z)

    float vertices[6 * 6 * 3];

    auto addFace = [&](int face,
        float x0, float y0, float z0,
        float x1, float y1, float z1,
        float x2, float y2, float z2,
        float x3, float y3, float z3) {

            int base = face * 18;
            // Triangle 1
            vertices[base + 0] = x0; vertices[base + 1] = y0; vertices[base + 2] = z0;
            vertices[base + 3] = x1; vertices[base + 4] = y1; vertices[base + 5] = z1;
            vertices[base + 6] = x2; vertices[base + 7] = y2; vertices[base + 8] = z2;
            // Triangle 2
            vertices[base + 9] = x0; vertices[base + 10] = y0; vertices[base + 11] = z0;
            vertices[base + 12] = x2; vertices[base + 13] = y2; vertices[base + 14] = z2;
            vertices[base + 15] = x3; vertices[base + 16] = y3; vertices[base + 17] = z3;
        };

    // 0: rt (+X) - правая грань, нормаль +X
    // Вершины: (1,-1,-1), (1,1,-1), (1,1,1), (1,-1,1)
    addFace(0, 1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1);

    // 1: lf (-X) - левая грань, нормаль -X  
    // Вершины: (-1,-1,1), (-1,1,1), (-1,1,-1), (-1,-1,-1)
    addFace(1, -1, -1, 1, -1, 1, 1, -1, 1, -1, -1, -1, -1);

    // 2: bk (+Y) - задняя грань, нормаль +Y
    // Вершины: (-1,1,-1), (1,1,-1), (1,1,1), (-1,1,1)
    addFace(2, -1, 1, -1, 1, 1, -1, 1, 1, 1, -1, 1, 1);

    // 3: ft (-Y) - передняя грань, нормаль -Y
    // Вершины: (1,-1,-1), (-1,-1,-1), (-1,-1,1), (1,-1,1)
    addFace(3, 1, -1, -1, -1, -1, -1, -1, -1, 1, 1, -1, 1);

    // 4: up (+Z) - верхняя грань, нормаль +Z
    // Вершины: (-1,1,1), (1,1,1), (1,-1,1), (-1,-1,1)
    addFace(4, -1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1, 1);

    // 5: dn (-Z) - нижняя грань, нормаль -Z
    // Вершины: (-1,1,-1), (-1,-1,-1), (1,-1,-1), (1,1,-1)
    addFace(5, -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, 1, -1);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

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

bool SkyboxRenderer::loadSkyTextures(const std::string& skyName, WADLoader& wadLoader) {
    if (skyName.empty()) return false;
    currentSkyName = skyName;

    for (int i = 0; i < 6; i++) {
        if (skyTextures[i]) glDeleteTextures(1, &skyTextures[i]);
        skyTextures[i] = 0;
        texturesLoaded[i] = false;
    }

    // HL1 порядок: rt, bk, lf, ft, up, dn
    const char* hl1Suffixes[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

    // Маппинг HL1 индексов на наши индексы куба
    // HL1: 0=rt, 1=bk, 2=lf, 3=ft, 4=up, 5=dn
    // Куб:  0=rt, 1=lf, 2=bk, 3=ft, 4=up, 5=dn
    const int hl1ToCube[6] = { 0, 2, 1, 3, 4, 5 };

    int loadedCount = 0;

    for (int hl1Idx = 0; hl1Idx < 6; hl1Idx++) {
        std::vector<uint8_t> rgba;
        int w = 0, h = 0;
        bool loaded = false;
        const char* suffix = hl1Suffixes[hl1Idx];
        int cubeIdx = hl1ToCube[hl1Idx];

        // Пробуем TGA
        std::string tgaPath = "gfx/env/" + skyName + suffix + ".tga";
        if (loadTGA(tgaPath, rgba, w, h)) {
            loaded = true;
            std::cout << "[SKY] Loaded TGA: " << tgaPath << " -> face " << cubeIdx << std::endl;
        }
        else {
            // Пробуем BMP
            std::string bmpPath = "gfx/env/" + skyName + suffix + ".bmp";
            if (loadBMP(bmpPath, rgba, w, h)) {
                loaded = true;
                std::cout << "[SKY] Loaded BMP: " << bmpPath << " -> face " << cubeIdx << std::endl;
            }
        }

        // Fallback на WAD
        if (!loaded) {
            std::string texName = skyName + suffix;
            int tw, th;
            GLuint texId;
            if (wadLoader.getTextureInfo(texName, tw, th, texId)) {
                if (texId != 0) {
                    skyTextures[cubeIdx] = texId;
                    texturesLoaded[cubeIdx] = true;
                    loadedCount++;
                    std::cout << "[SKY] Loaded from WAD: " << texName << " -> face " << cubeIdx << std::endl;
                    continue;
                }
            }
        }

        if (loaded) {
            glGenTextures(1, &skyTextures[cubeIdx]);
            glBindTexture(GL_TEXTURE_2D, skyTextures[cubeIdx]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            texturesLoaded[cubeIdx] = true;
            loadedCount++;
        }
        else {
            // Создаем fallback текстуру
            uint8_t color[3] = { 255, 0, 255 }; // Magenta
            if (cubeIdx == 0) { color[0] = 255; color[1] = 0; color[2] = 0; }      // rt - red
            else if (cubeIdx == 1) { color[0] = 0; color[1] = 255; color[2] = 0; } // lf - green
            else if (cubeIdx == 2) { color[0] = 0; color[1] = 0; color[2] = 255; } // bk - blue
            else if (cubeIdx == 3) { color[0] = 255; color[1] = 255; color[2] = 0; } // ft - yellow
            else if (cubeIdx == 4) { color[0] = 0; color[1] = 255; color[2] = 255; } // up - cyan
            else { color[0] = 255; color[1] = 0; color[2] = 255; } // dn - magenta

            glGenTextures(1, &skyTextures[cubeIdx]);
            glBindTexture(GL_TEXTURE_2D, skyTextures[cubeIdx]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, color);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            std::cerr << "[SKY] Using fallback for " << suffix << " (face " << cubeIdx << ")" << std::endl;
        }
    }

    std::cout << "[SKY] Loaded " << loadedCount << "/6 textures for: " << skyName << std::endl;
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
    for (int i = 0; i < 6; i++) {
        if (skyTextures[i]) {
            glDeleteTextures(1, &skyTextures[i]);
            skyTextures[i] = 0;
        }
    }
    shader.reset();
    loaded = false;
}

void SkyboxRenderer::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& viewPos) {
    (void)viewPos;

    if (!loaded || !shader) return;

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    shader->bind();
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);

    glBindVertexArray(vao);

    for (int i = 0; i < 6; i++) {
        shader->setInt("faceIndex", i);

        // Fallback цвета для отладки
        if (i == 0) shader->setVec3("fallbackColor", glm::vec3(1, 0, 0)); // rt - red
        else if (i == 1) shader->setVec3("fallbackColor", glm::vec3(0, 1, 0)); // lf - green
        else if (i == 2) shader->setVec3("fallbackColor", glm::vec3(0, 0, 1)); // bk - blue
        else if (i == 3) shader->setVec3("fallbackColor", glm::vec3(1, 1, 0)); // ft - yellow
        else if (i == 4) shader->setVec3("fallbackColor", glm::vec3(0, 1, 1)); // up - cyan
        else shader->setVec3("fallbackColor", glm::vec3(1, 0, 1)); // dn - magenta

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, skyTextures[i]);
        glDrawArrays(GL_TRIANGLES, i * 6, 6);
    }

    glBindVertexArray(0);
    shader->unbind();

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}