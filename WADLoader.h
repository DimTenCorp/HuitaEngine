#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct WADTexture {
    std::string name;
    int width, height;
    std::vector<uint8_t> rgba;
    GLuint textureID = 0;
};

struct TextureInfo {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

class WADLoader {
private:
    std::unordered_map<std::string, TextureInfo> textureCache;
    std::unordered_map<std::string, bool> liquidTextures;  // Текстуры с префиксом ! - жидкости
    GLuint defaultTexture = 0;
    TextureInfo defaultTextureInfo;
    bool initialized = false;

    // Палитра Quake
    uint8_t quakePalette[768] = { 0 };
    bool hasQuakePalette = false;

    GLuint createTexture(const WADTexture& tex);
    void createDefaultTexture();

public:
    WADLoader() = default;
    ~WADLoader();

    void init();
    bool loadAllWADs();
    bool loadWADFile(const std::string& path);

    // Загрузка палитры Quake
    bool loadQuakePalette(const std::string& path);

    GLuint getTexture(const std::string& name);
    bool getTextureInfo(const std::string& name, int& width, int& height, GLuint& textureId);
    int getTextureWidth(const std::string& name);
    int getTextureHeight(const std::string& name);

    GLuint getDefaultTexture() const { return defaultTexture; }
    const TextureInfo& getDefaultTextureInfo() const { return defaultTextureInfo; }
    bool isInitialized() const { return initialized; }
    bool hasPalette() const { return hasQuakePalette; }

    // Debug методы
    void debugPrintCache() const;
    bool hasTexture(const std::string& name) const;

    // Проверка на жидкость (текстуры с префиксом !)
    bool isLiquidTexture(const std::string& name) const;
    const std::unordered_map<std::string, bool>& getLiquidTextures() const { return liquidTextures; }

    // Очистка
    void cleanup();
};