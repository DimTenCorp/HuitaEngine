#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct DisplayMode {
    int width;
    int height;
    std::string name;

    DisplayMode() : width(1920), height(1080), name("1920x1080") {}

    DisplayMode(int w, int h) : width(w), height(h) {
        name = std::to_string(w) + "x" + std::to_string(h);
    }

    bool operator==(const DisplayMode& other) const {
        return width == other.width && height == other.height;
    }
};

struct SettingsData {
    bool fullscreen = true;
    int screenWidth = 1920;
    int screenHeight = 1080;
    int displayModeIndex = -1;

    float mouseSensitivity = 0.5f; // Изменено с 0.1f на 0.5f (более комфортное значение)

    float masterVolume = 1.0f;
    bool mute = false;

    void save(const std::string& filename = "settings.ini");
    void load(const std::string& filename = "settings.ini");

    static std::vector<DisplayMode> getAvailableDisplayModes();
    static DisplayMode getNativeResolution();

private:
    void detectNativeResolution();
    DisplayMode nativeResolution;
};