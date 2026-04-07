#define NOMINMAX
#include "WADLoader.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <vector>
#include <glad/glad.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#pragma pack(push, 1)

struct WADHeader {
    char magic[4];
    int32_t numTextures;
    int32_t infoOffset;
};

struct WAD3TextureInfo {
    int32_t filepos;
    int32_t disksize;
    int32_t size;
    uint8_t type;
    uint8_t compression;
    int16_t pad;
    char name[16];
};

struct WAD2TextureInfo {
    int32_t filepos;
    int32_t disksize;
    int32_t size;
    uint8_t type;
    uint8_t compression;
    int16_t dummy;
    char name[16];
};

struct MIPTexHeader {
    char name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[4];
};

#pragma pack(pop)

static std::string cleanWadName(const char* rawName, size_t maxLen = 16) {
    std::string s(rawName, maxLen);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && s[start] == '\0') start++;
    if (start > 0) s = s.substr(start);
    return s;
}

static std::string normalizeTextureName(const std::string& name) {
    std::string result = name;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

void WADLoader::init() {
    if (initialized) return;
    createDefaultTexture();
    initialized = true;
    std::cout << "[WAD] System initialized." << std::endl;
}

void WADLoader::createDefaultTexture() {
    uint8_t pixels[32 * 32 * 4];
    for (int i = 0; i < 32 * 32; i++) {
        int x = i % 32, y = i / 32;
        bool check = ((x / 4) + (y / 4)) % 2;
        // Шахматная доска: чередование белого и серого
        if (check) {
            pixels[i * 4 + 0] = 255;  // R - белый
            pixels[i * 4 + 1] = 255;  // G
            pixels[i * 4 + 2] = 255;  // B
        } else {
            pixels[i * 4 + 0] = 128;  // R - серый
            pixels[i * 4 + 1] = 128;  // G
            pixels[i * 4 + 2] = 128;  // B
        }
        pixels[i * 4 + 3] = 255;  // A - полностью непрозрачный
    }
    glGenTextures(1, &defaultTexture);
    glBindTexture(GL_TEXTURE_2D, defaultTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    defaultTextureInfo.id = defaultTexture;
    defaultTextureInfo.width = 32;
    defaultTextureInfo.height = 32;
}

WADLoader::~WADLoader() {
    cleanup();
}

void WADLoader::cleanup() {
    for (auto& pair : textureCache) {
        if (pair.second.id != defaultTexture) {
            glDeleteTextures(1, &pair.second.id);
        }
    }
    textureCache.clear();
    if (defaultTexture) {
        glDeleteTextures(1, &defaultTexture);
        defaultTexture = 0;
    }
}

bool WADLoader::loadQuakePalette(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "[WAD] Could not open palette file: " << path << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(quakePalette), 768);
    if (file.gcount() == 768) {
        hasQuakePalette = true;
        std::cout << "[WAD] Loaded Quake palette from: " << path << " (768 bytes)" << std::endl;

        std::cout << "[WAD] First few colors in palette: ";
        for (int i = 0; i < 3; i++) {
            std::cout << "[" << (int)quakePalette[i * 3] << ","
                << (int)quakePalette[i * 3 + 1] << ","
                << (int)quakePalette[i * 3 + 2] << "] ";
        }
        std::cout << std::endl;

        return true;
    }

    std::cout << "[WAD] Failed to load palette (got " << file.gcount() << " bytes, expected 768)" << std::endl;
    return false;
}

GLuint WADLoader::createTexture(const WADTexture& tex) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Для прозрачных текстур немного меняем фильтрацию
    if (!tex.name.empty() && tex.name[0] == '{') {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

bool WADLoader::loadWADFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "[WAD] Could not open: " << path << std::endl;
        return false;
    }

    WADHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    bool isWAD3 = (memcmp(header.magic, "WAD3", 4) == 0);
    bool isWAD2 = (memcmp(header.magic, "WAD2", 4) == 0);

    if (!isWAD2 && !isWAD3) {
        std::cout << "[WAD] Not a WAD2/WAD3 file: " << path << " (magic: "
            << header.magic[0] << header.magic[1] << header.magic[2] << header.magic[3] << ")" << std::endl;
        return false;
    }

    std::cout << "[WAD] Loading " << (isWAD3 ? "WAD3" : "WAD2") << ": " << path
        << " (" << header.numTextures << " entries)..." << std::endl;

    if (header.numTextures <= 0 || header.infoOffset <= 0) return false;

    const size_t entrySize = isWAD3 ? sizeof(WAD3TextureInfo) : 32;

    std::vector<uint8_t> dirData(entrySize * header.numTextures);
    file.seekg(header.infoOffset, std::ios::beg);
    file.read(reinterpret_cast<char*>(dirData.data()), dirData.size());

    if (!file.good()) {
        std::cout << "[WAD] Failed to read directory" << std::endl;
        return false;
    }

    int loadedCount = 0;

    for (int i = 0; i < header.numTextures; ++i) {
        const uint8_t* entry = dirData.data() + i * entrySize;

        int32_t filepos, disksize, size;
        uint8_t type, compression;
        char name[17] = { 0 };

        if (isWAD3) {
            const WAD3TextureInfo* info = reinterpret_cast<const WAD3TextureInfo*>(entry);
            filepos = info->filepos;
            disksize = info->disksize;
            size = info->size;
            type = info->type;
            compression = info->compression;
            memcpy(name, info->name, 16);
        }
        else {
            filepos = *reinterpret_cast<const int32_t*>(entry + 0);
            disksize = *reinterpret_cast<const int32_t*>(entry + 4);
            size = *reinterpret_cast<const int32_t*>(entry + 8);
            type = *(entry + 12);
            compression = *(entry + 13);
            memcpy(name, entry + 16, 16);
        }

        name[16] = '\0';

        // Проверка типа текстуры
        if (isWAD3) {
            if (type != 0x43) continue;
        }
        else {
            if (type != 0x44 && type != 0x43) continue;
        }

        if (filepos == 0) continue;

        std::string rawName = cleanWadName(name, 16);
        if (rawName.empty()) continue;

        std::string lowerName = normalizeTextureName(rawName);

        if (textureCache.find(lowerName) != textureCache.end()) continue;

        file.clear();
        file.seekg(filepos, std::ios::beg);
        if (!file.good()) continue;

        // Читаем MIP texture header
        MIPTexHeader texHeader;
        file.read(reinterpret_cast<char*>(&texHeader), sizeof(MIPTexHeader));
        if (!file.good()) continue;

        if (texHeader.width == 0 || texHeader.height == 0 ||
            texHeader.width > 2048 || texHeader.height > 2048) {
            continue;
        }

        // Читаем пиксельные данные (первый MIP уровень)
        file.seekg(filepos + texHeader.offsets[0], std::ios::beg);

        uint32_t pixelCount = texHeader.width * texHeader.height;
        std::vector<uint8_t> pixels(pixelCount);
        file.read(reinterpret_cast<char*>(pixels.data()), pixelCount);
        if (file.gcount() != pixelCount) continue;

        uint8_t palette[768] = { 0 };
        bool palOk = false;

        if (isWAD3) {
            // WAD3 (Half-Life): палитра встроена в текстуру
            uint32_t totalMipSize = 0;
            uint32_t w = texHeader.width, h = texHeader.height;
            for (int mip = 0; mip < 4; ++mip) {
                totalMipSize += w * h;
                w = (std::max)(1u, w / 2);
                h = (std::max)(1u, h / 2);
            }

            // В WAD3 перед палитрой идёт 2-байтовый счётчик цветов (обычно 256)
            file.clear();
            file.seekg(filepos + texHeader.offsets[0] + totalMipSize + 2, std::ios::beg);
            file.read(reinterpret_cast<char*>(palette), 768);
            palOk = (file.gcount() == 768);

            if (!palOk) {
                file.clear();
                file.seekg(filepos + texHeader.offsets[0] + totalMipSize, std::ios::beg);
                file.read(reinterpret_cast<char*>(palette), 768);
                palOk = (file.gcount() == 768);
            }
        }
        else {
            // WAD2 (Quake): сначала пробуем найти палитру в файле
            file.clear();
            file.seekg(filepos + texHeader.offsets[0] + pixelCount, std::ios::beg);
            file.read(reinterpret_cast<char*>(palette), 768);
            palOk = (file.gcount() == 768);

            if (!palOk) {
                uint32_t totalMipSize = 0;
                uint32_t w = texHeader.width, h = texHeader.height;
                for (int mip = 0; mip < 4; ++mip) {
                    totalMipSize += w * h;
                    w = (std::max)(1u, w / 2);
                    h = (std::max)(1u, h / 2);
                }

                file.clear();
                file.seekg(filepos + texHeader.offsets[0] + totalMipSize, std::ios::beg);
                file.read(reinterpret_cast<char*>(palette), 768);
                palOk = (file.gcount() == 768);
            }
        }

        // Используем внешнюю палитру Quake для WAD2
        if (!palOk && !isWAD3 && hasQuakePalette) {
            memcpy(palette, quakePalette, 768);
            palOk = true;
            static int warnCount = 0;
            if (warnCount++ < 10) {
                std::cout << "[WAD] Using external Quake palette for: " << rawName << std::endl;
            }
        }

        // Fallback градиентная палитра
        if (!palOk) {
            if (!isWAD3) {
                std::cout << "[WAD] No palette for: " << rawName << ", using gradient" << std::endl;
            }
            for (int j = 0; j < 256; j++) {
                uint8_t val = static_cast<uint8_t>(j);
                palette[j * 3 + 0] = val;
                palette[j * 3 + 1] = val;
                palette[j * 3 + 2] = val;
            }
            palOk = true;
        }

        WADTexture tex;
        tex.name = rawName;
        tex.width = texHeader.width;
        tex.height = texHeader.height;
        tex.rgba.resize(pixelCount * 4);

        // Прозрачность для { текстур (спрайты/декали/сетки)
        bool isTransparent = (!rawName.empty() && rawName[0] == '{');

        for (size_t pi = 0; pi < pixelCount; ++pi) {
            uint8_t idx = pixels[pi];

            // Для прозрачных текстур
            if (isTransparent && idx == 255) {
                // Ставим ЧЁРНЫЙ цвет вместо синего - это убирает синее свечение
                tex.rgba[pi * 4 + 0] = 0;   // R
                tex.rgba[pi * 4 + 1] = 0;   // G
                tex.rgba[pi * 4 + 2] = 0;   // B
                tex.rgba[pi * 4 + 3] = 0;   // A = полностью прозрачный
            }
            else {
                // Непрозрачный пиксель
                tex.rgba[pi * 4 + 3] = 255;

                if (idx * 3 + 2 < 768) {
                    tex.rgba[pi * 4 + 0] = palette[idx * 3 + 0];
                    tex.rgba[pi * 4 + 1] = palette[idx * 3 + 1];
                    tex.rgba[pi * 4 + 2] = palette[idx * 3 + 2];
                }
                else {
                    // Fallback magenta
                    tex.rgba[pi * 4 + 0] = 255;
                    tex.rgba[pi * 4 + 1] = 0;
                    tex.rgba[pi * 4 + 2] = 255;
                }
            }
        }

        tex.textureID = createTexture(tex);

        TextureInfo info;
        info.id = tex.textureID;
        info.width = tex.width;
        info.height = tex.height;

        textureCache[lowerName] = info;
        textureCache[rawName] = info;

        loadedCount++;
    }

    std::cout << "[WAD] Loaded " << loadedCount << " textures from " << path << std::endl;
    return loadedCount > 0;
}

bool WADLoader::loadAllWADs() {
    if (!initialized) init();

    std::vector<std::string> foundWADs;

#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("*.wad", &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            foundWADs.push_back(findData.cFileName);
            std::cout << "[WAD] Found WAD: " << findData.cFileName << std::endl;
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    hFind = FindFirstFileA("wads/*.wad", &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            foundWADs.push_back(std::string("wads/") + findData.cFileName);
            std::cout << "[WAD] Found WAD: wads/" << findData.cFileName << std::endl;
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#endif

    if (foundWADs.empty()) {
        std::cout << "[WAD] No WAD files found in current directory or wads/ folder!" << std::endl;
        return false;
    }

    bool ok = false;
    for (const auto& wad : foundWADs) {
        if (loadWADFile(wad)) ok = true;
    }

    std::cout << "[WAD] Total textures in cache: " << textureCache.size() << std::endl;
    return ok;
}

GLuint WADLoader::getTexture(const std::string& name) {
    if (!initialized) return defaultTexture;
    if (name.empty()) return defaultTexture;

    auto it = textureCache.find(name);
    if (it != textureCache.end()) {
        return it->second.id;
    }

    std::string lowerName = name;
    for (char& c : lowerName) {
        c = tolower(c);
    }

    it = textureCache.find(lowerName);
    if (it != textureCache.end()) {
        return it->second.id;
    }

    std::string cleanName = name;
    while (!cleanName.empty()) {
        char c = cleanName[0];
        if (c == '+' || c == '-' || c == '~' || c == '{' || c == '!' || c == '^') {
            cleanName = cleanName.substr(1);
        }
        else if (c >= '0' && c <= '9') {
            cleanName = cleanName.substr(1);
        }
        else {
            break;
        }
    }

    if (!cleanName.empty() && cleanName != name) {
        it = textureCache.find(cleanName);
        if (it != textureCache.end()) return it->second.id;

        std::string cleanLower = cleanName;
        for (char& c : cleanLower) c = tolower(c);
        it = textureCache.find(cleanLower);
        if (it != textureCache.end()) return it->second.id;
    }

    return defaultTexture;
}

bool WADLoader::getTextureInfo(const std::string& name, int& width, int& height, GLuint& textureId) {
    if (!initialized) {
        width = defaultTextureInfo.width;
        height = defaultTextureInfo.height;
        textureId = defaultTexture;
        return false;
    }
    if (name.empty()) {
        width = defaultTextureInfo.width;
        height = defaultTextureInfo.height;
        textureId = defaultTexture;
        return false;
    }

    auto it = textureCache.find(name);
    if (it != textureCache.end()) {
        width = it->second.width;
        height = it->second.height;
        textureId = it->second.id;
        return true;
    }

    std::string lowerName = normalizeTextureName(name);
    it = textureCache.find(lowerName);
    if (it != textureCache.end()) {
        width = it->second.width;
        height = it->second.height;
        textureId = it->second.id;
        return true;
    }

    width = defaultTextureInfo.width;
    height = defaultTextureInfo.height;
    textureId = defaultTexture;
    return false;
}

int WADLoader::getTextureWidth(const std::string& name) {
    int w, h;
    GLuint id;
    getTextureInfo(name, w, h, id);
    return w;
}

int WADLoader::getTextureHeight(const std::string& name) {
    int w, h;
    GLuint id;
    getTextureInfo(name, w, h, id);
    return h;
}

void WADLoader::debugPrintCache() const {
    std::cout << "=== WAD CACHE (" << textureCache.size() << " entries) ===" << std::endl;
    int count = 0;
    for (const auto& pair : textureCache) {
        std::cout << "  \"" << pair.first << "\"" << std::endl;
        if (++count >= 50) break;
    }
}

bool WADLoader::hasTexture(const std::string& name) const {
    auto it = textureCache.find(name);
    if (it != textureCache.end()) return true;

    std::string lowerName = normalizeTextureName(name);
    return textureCache.find(lowerName) != textureCache.end();
}