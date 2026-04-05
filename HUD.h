#pragma once
#include <glm/glm.hpp>

// Стиль прицела (можно менять из кода)
struct CrosshairStyle {
    float gap = 3.0f;           // зазор от центра
    float length = 5.0f;        // длина линий
    float width = 2.0f;         // толщина линий
    float dotSize = 0.0f;       // размер точки в центре (0 чтобы убрать)
    unsigned int color = 0xDCFFFFFF; // ABGR: белый полупрозрачный (220 alpha)
};

struct HUDSettings {
    bool showFPS = true;
    bool showPosition = true;
    bool showCrosshair = true;
    CrosshairStyle crosshair;
};

class HUD {
private:
    HUDSettings settings;
    float currentFPS = 0.0f;
    float fpsTimer = 0.0f;
    int frameCount = 0;
    glm::vec3 playerPos;

public:
    HUD();
    ~HUD();

    void update(float deltaTime, const glm::vec3& position);
    void render(int screenWidth, int screenHeight);

    HUDSettings& getSettings() { return settings; }
    void toggleFPS() { settings.showFPS = !settings.showFPS; }
    void togglePosition() { settings.showPosition = !settings.showPosition; }
    void toggleCrosshair() { settings.showCrosshair = !settings.showCrosshair; }
};