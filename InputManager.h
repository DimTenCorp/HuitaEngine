// InputManager.h
#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>

class InputManager {
public:
    InputManager(GLFWwindow* window);

    void update(); // вызывать каждый кадр

    // Клавиатура
    bool isPressed(int key) const;
    bool isJustPressed(int key); // один раз при нажатии
    bool isJustReleased(int key);

    // Мышь
    void getMouseDelta(float& dx, float& dy);
    void setMouseEnabled(bool enabled);
    bool isMouseEnabled() const { return mouseEnabled; }

    // Тогглы для HUD
    bool toggleNoclip() { return consume(toggleNoclipFlag); }
    bool toggleCrosshair() { return consume(toggleCrosshairFlag); }
    bool toggleFPS() { return consume(toggleFPSFlag); }
    bool togglePosition() { return consume(togglePositionFlag); }
    bool toggleHitbox() { return consume(toggleHitboxFlag); }

private:
    GLFWwindow* window;
    std::unordered_map<int, bool> prevKeyState;
    std::unordered_map<int, bool> currKeyState;

    float lastMouseX = 0, lastMouseY = 0;
    float mouseDx = 0, mouseDy = 0;
    bool firstMouse = true;
    bool mouseEnabled = true;

    // Флаги для тогглов
    bool toggleNoclipFlag = false;
    bool toggleCrosshairFlag = false;
    bool toggleFPSFlag = false;
    bool togglePositionFlag = false;
    bool toggleHitboxFlag = false;

    bool consume(bool& flag) { bool v = flag; flag = false; return v; }
};