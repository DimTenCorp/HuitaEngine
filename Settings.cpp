#include "pch.h"
#include "Settings.h"
#include <fstream>
#include <iostream>
#include <GLFW/glfw3.h>
#include <vector>
#include <algorithm>

DisplayMode SettingsData::getNativeResolution() {
    DisplayMode native(1920, 1080);

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    if (primary) {
        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        if (mode) {
            native.width = mode->width;
            native.height = mode->height;
        }
    }

    return native;
}

std::vector<DisplayMode> SettingsData::getAvailableDisplayModes() {
    std::vector<DisplayMode> modes;

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    if (!primary) {
        modes.push_back(DisplayMode(1920, 1080));
        modes.push_back(DisplayMode(1600, 900));
        modes.push_back(DisplayMode(1366, 768));
        modes.push_back(DisplayMode(1280, 720));
        return modes;
    }

    int count;
    const GLFWvidmode* vidmodes = glfwGetVideoModes(primary, &count);

    for (int i = 0; i < count; i++) {
        bool exists = false;
        for (const auto& mode : modes) {
            if (mode.width == vidmodes[i].width && mode.height == vidmodes[i].height) {
                exists = true;
                break;
            }
        }

        if (!exists && vidmodes[i].width >= 800 && vidmodes[i].height >= 600) {
            modes.push_back(DisplayMode(vidmodes[i].width, vidmodes[i].height));
        }
    }

    std::sort(modes.begin(), modes.end(), [](const DisplayMode& a, const DisplayMode& b) {
        return (a.width * a.height) > (b.width * b.height);
        });

    return modes;
}

void SettingsData::detectNativeResolution() {
    nativeResolution = getNativeResolution();
}

void SettingsData::save(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[SETTINGS] Failed to save settings to " << filename << std::endl;
        return;
    }

    file << "[Display]\n";
    file << "fullscreen=" << (fullscreen ? "1" : "0") << "\n";
    file << "width=" << screenWidth << "\n";
    file << "height=" << screenHeight << "\n";
    file << "modeIndex=" << displayModeIndex << "\n";

    file << "[Mouse]\n";
    file << "sensitivity=" << mouseSensitivity << "\n";

    file << "[Audio]\n";
    file << "masterVolume=" << masterVolume << "\n";
    file << "mute=" << (mute ? "1" : "0") << "\n";

    file.close();
    std::cout << "[SETTINGS] Saved to " << filename << std::endl;
}

void SettingsData::load(const std::string& filename) {
    // Сначала устанавливаем дефолтные значения
    detectNativeResolution();

    fullscreen = true;
    screenWidth = nativeResolution.width;
    screenHeight = nativeResolution.height;
    displayModeIndex = -1;
    mouseSensitivity = 0.5f;
    masterVolume = 1.0f;
    mute = false;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[SETTINGS] No settings file found, using defaults" << std::endl;
        return;
    }

    std::string line;
    std::string section;
    bool displaySectionFound = false;
    bool mouseSectionFound = false;
    bool audioSectionFound = false;

    while (std::getline(file, line)) {
        // Пропускаем пустые строки и комментарии
        if (line.empty()) continue;

        // Убираем пробелы в начале строки
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        if (start > 0) line = line.substr(start);

        // Комментарии
        if (line[0] == ';' || line[0] == '#') continue;

        // Секция [Section]
        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                section = line.substr(1, end - 1);
                // Убираем пробелы из имени секции
                size_t secStart = section.find_first_not_of(" \t");
                size_t secEnd = section.find_last_not_of(" \t");
                if (secStart != std::string::npos && secEnd != std::string::npos) {
                    section = section.substr(secStart, secEnd - secStart + 1);
                }

                if (section == "Display") displaySectionFound = true;
                else if (section == "Mouse") mouseSectionFound = true;
                else if (section == "Audio") audioSectionFound = true;
            }
            continue;
        }

        // Парсим key=value
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // Убираем пробелы
        size_t keyStart = key.find_first_not_of(" \t");
        size_t keyEnd = key.find_last_not_of(" \t");
        if (keyStart != std::string::npos && keyEnd != std::string::npos) {
            key = key.substr(keyStart, keyEnd - keyStart + 1);
        }

        size_t valStart = value.find_first_not_of(" \t");
        if (valStart != std::string::npos) {
            value = value.substr(valStart);
        }
        // Убираем \r в конце (Windows)
        size_t valEnd = value.find_last_not_of(" \t\r\n");
        if (valEnd != std::string::npos) {
            value = value.substr(0, valEnd + 1);
        }

        try {
            if (section == "Display") {
                if (key == "fullscreen") {
                    fullscreen = (value == "1" || value == "true" || value == "yes");
                    std::cout << "[SETTINGS] Loaded fullscreen=" << fullscreen << " (value='" << value << "')" << std::endl;
                }
                else if (key == "width") {
                    screenWidth = std::stoi(value);
                }
                else if (key == "height") {
                    screenHeight = std::stoi(value);
                }
                else if (key == "modeIndex") {
                    displayModeIndex = std::stoi(value);
                }
            }
            else if (section == "Mouse") {
                if (key == "sensitivity") {
                    mouseSensitivity = std::stof(value);
                }
            }
            else if (section == "Audio") {
                if (key == "masterVolume") {
                    masterVolume = std::stof(value);
                }
                else if (key == "mute") {
                    mute = (value == "1" || value == "true" || value == "yes");
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[SETTINGS] Failed to parse '" << key << "=" << value << "': " << e.what() << std::endl;
        }
    }

    file.close();

    // Валидация: если fullscreen=0 но разрешение не задано или нулевое - используем дефолты
    if (!fullscreen && (screenWidth <= 0 || screenHeight <= 0)) {
        std::cerr << "[SETTINGS] Invalid windowed resolution, using native" << std::endl;
        screenWidth = nativeResolution.width;
        screenHeight = nativeResolution.height;
    }

    std::cout << "[SETTINGS] Loaded from " << filename
        << " (fullscreen=" << fullscreen
        << ", " << screenWidth << "x" << screenHeight << ")" << std::endl;
}