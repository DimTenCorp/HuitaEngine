#pragma once
#include <glm/glm.hpp>
#include <memory>
#include "Player.h"

struct GLFWwindow;

class HUD;

class Game {
public:
    Game();
    ~Game();

    void init();
    void update(float deltaTime);
    void processInput(GLFWwindow* window);
    void render(int screenWidth, int screenHeight);

    Player* getPlayer() const { return player.get(); }
    void setViewAngles(float yaw, float pitch);

    void resetMouseState() { firstMouse = true; }

    void setLastMousePos(float x, float y) {
        lastX = x;
        lastY = y;
    }

    void resetCameraAndMouse() {
        firstMouse = true;
        yaw = -90.0f;
        pitch = 0.0f;
        player->setYaw(yaw);
        player->setPitch(pitch);
    }

    void centerMouseAt(float x, float y) {
        lastX = x;
        lastY = y;
        firstMouse = false;
    }

    void setMouseSensitivity(float sens) {
        mouseSensitivity = sens;
    }
    float getMouseSensitivity() const { return mouseSensitivity; }

    // Управление паузой
    void setPaused(bool paused);
    bool getPaused() const { return isPaused; }

    float mouseSensitivity = 0.5f;

private:
    std::unique_ptr<Player> player;
    std::unique_ptr<HUD> hud;

    float yaw = -90.0f;
    float pitch = 0.0f;
    float lastX = 640.0f;
    float lastY = 360.0f;
    bool firstMouse = true;

    bool f1Pressed = false, f2Pressed = false, f3Pressed = false;
    bool f5Pressed = false, f6Pressed = false;
    bool noclipPressed = false, bhopPressed = false;
    bool f10Pressed = false;

    bool isPaused = false;

    // Сохраняем состояние для возобновления
    glm::vec3 savedVelocity;
    bool wasOnGround = false;

    void processMouse(GLFWwindow* window);
};