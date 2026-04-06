// InputManager.cpp
#include "InputManager.h"

InputManager::InputManager(GLFWwindow* w) : window(w) {}

void InputManager::update() {
    prevKeyState = currKeyState;

    // Обновляем состояние часто используемых клавиш
    const int keys[] = {
        GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
        GLFW_KEY_SPACE, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_ESCAPE,
        GLFW_KEY_V, GLFW_KEY_F1, GLFW_KEY_F2, GLFW_KEY_F3, GLFW_KEY_F4
    };

    for (int key : keys) {
        currKeyState[key] = glfwGetKey(window, key) == GLFW_PRESS;
    }

    // Обработка тогглов
    if (isJustPressed(GLFW_KEY_V)) toggleNoclipFlag = true;
    if (isJustPressed(GLFW_KEY_F1)) toggleCrosshairFlag = true;
    if (isJustPressed(GLFW_KEY_F2)) toggleFPSFlag = true;
    if (isJustPressed(GLFW_KEY_F3)) togglePositionFlag = true;
    if (isJustPressed(GLFW_KEY_F4)) toggleHitboxFlag = true;

    // Мышь
    if (mouseEnabled) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        if (firstMouse) {
            lastMouseX = (float)x;
            lastMouseY = (float)y;
            firstMouse = false;
        }

        mouseDx = (float)x - lastMouseX;
        mouseDy = lastMouseY - (float)y; // инвертировано как в оригинале

        lastMouseX = (float)x;
        lastMouseY = (float)y;
    }
    else {
        mouseDx = mouseDy = 0;
    }
}

bool InputManager::isPressed(int key) const {
    auto it = currKeyState.find(key);
    return it != currKeyState.end() && it->second;
}

bool InputManager::isJustPressed(int key) {
    return isPressed(key) && !prevKeyState[key];
}

bool InputManager::isJustReleased(int key) {
    return !isPressed(key) && prevKeyState[key];
}

void InputManager::getMouseDelta(float& dx, float& dy) {
    dx = mouseDx;
    dy = mouseDy;
}

void InputManager::setMouseEnabled(bool enabled) {
    if (mouseEnabled == enabled) return;
    mouseEnabled = enabled;
    glfwSetInputMode(window, GLFW_CURSOR,
        enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (enabled) firstMouse = true;
}