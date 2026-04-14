#pragma once
#include <glm/glm.hpp>

struct ImFont;

struct CrosshairStyle {
    float gap = 3.0f;
    float length = 5.0f;
    float width = 2.0f;
    float dotSize = 0.0f;
    unsigned int color = 0xDCFFFFFF;
};

struct HUDSettings {
    bool showFPS = true;
    bool showPosition = true;
    bool showCrosshair = true;
    bool showSpeed = false;  // Счетчик скорости игрока
    CrosshairStyle crosshair;
};

class HUD {
private:
    HUDSettings settings;
    float currentFPS = 0.0f;
    float fpsTimer = 0.0f;
    int frameCount = 0;
    glm::vec3 playerPos;
    float playerSpeed = 0.0f;  // Текущая скорость игрока
    ImFont* smallFont = nullptr;
    int screenWidth = 1280;
    int screenHeight = 720;

public:
    HUD();
    ~HUD();

    void update(float deltaTime, const glm::vec3& position, float speed = 0.0f);

    HUDSettings& getSettings() { return settings; }
    void toggleFPS() { settings.showFPS = !settings.showFPS; }
    void togglePosition() { settings.showPosition = !settings.showPosition; }
    void toggleCrosshair() { settings.showCrosshair = !settings.showCrosshair; }
    void toggleSpeed() { settings.showSpeed = !settings.showSpeed; }

    void setScreenSize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
    }

    void render();
};