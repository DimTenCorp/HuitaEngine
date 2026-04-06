#include "InputHandler.h"
#include <iostream>

// Статический указатель для callback
static InputHandler* g_inputHandler = nullptr;

void InputHandler::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (g_inputHandler == nullptr) return;
    
    InputState& state = g_inputHandler->state;
    
    if (state.isFirstMouse) {
        state.lastMouseX = (float)xpos;
        state.lastMouseY = (float)ypos;
        state.mouseX = (float)xpos;
        state.mouseY = (float)ypos;
        state.isFirstMouse = false;
    }
    
    state.mouseOffsetX = (float)(xpos - state.lastMouseX);
    state.mouseOffsetY = (float)(state.lastMouseY - ypos);
    state.lastMouseX = (float)xpos;
    state.lastMouseY = (float)ypos;
    state.mouseX = (float)xpos;
    state.mouseY = (float)ypos;
}

InputHandler::InputHandler() {
    g_inputHandler = this;
}

InputHandler::~InputHandler() {
    if (g_inputHandler == this) {
        g_inputHandler = nullptr;
    }
}

void InputHandler::init(GLFWwindow* window) {
    this->window = window;
    glfwSetCursorPosCallback(window, mouseCallback);
    disableCursor();
}

void InputHandler::update() {
    // Обновляем состояние клавиш
    for (int i = 0; i < 512; i++) {
        bool wasPressed = state.keys[i];
        state.keys[i] = (glfwGetKey(window, i) == GLFW_PRESS);
        
        state.keysJustPressed[i] = state.keys[i] && !wasPressed;
        state.keysJustReleased[i] = !state.keys[i] && wasPressed;
    }
    
    // Обновляем специальные флаги
    if (!state.f1Pressed && isKeyPressed(GLFW_KEY_F1)) {
        state.f1Pressed = true;
    }
    if (isKeyPressed(GLFW_KEY_F1) == false) {
        state.f1Pressed = false;
    }
    
    if (!state.f2Pressed && isKeyPressed(GLFW_KEY_F2)) {
        state.f2Pressed = true;
    }
    if (isKeyPressed(GLFW_KEY_F2) == false) {
        state.f2Pressed = false;
    }
    
    if (!state.f3Pressed && isKeyPressed(GLFW_KEY_F3)) {
        state.f3Pressed = true;
    }
    if (isKeyPressed(GLFW_KEY_F3) == false) {
        state.f3Pressed = false;
    }
    
    if (!state.f4Pressed && isKeyPressed(GLFW_KEY_F4)) {
        state.f4Pressed = true;
    }
    if (isKeyPressed(GLFW_KEY_F4) == false) {
        state.f4Pressed = false;
    }
    
    if (!state.vPressed && isKeyPressed(GLFW_KEY_V)) {
        state.vPressed = true;
    }
    if (isKeyPressed(GLFW_KEY_V) == false) {
        state.vPressed = false;
    }
}

bool InputHandler::isKeyPressed(int key) const {
    if (key < 0 || key >= 512) return false;
    return state.keys[key];
}

bool InputHandler::isKeyJustPressed(int key) {
    if (key < 0 || key >= 512) return false;
    bool result = state.keysJustPressed[key];
    state.keysJustPressed[key] = false;  // Сбрасываем после чтения
    return result;
}

bool InputHandler::isKeyJustReleased(int key) {
    if (key < 0 || key >= 512) return false;
    bool result = state.keysJustReleased[key];
    state.keysJustReleased[key] = false;  // Сбрасываем после чтения
    return result;
}

glm::vec2 InputHandler::getMouseOffset() {
    glm::vec2 offset(state.mouseOffsetX, state.mouseOffsetY);
    state.mouseOffsetX = 0.0f;
    state.mouseOffsetY = 0.0f;
    return offset;
}

glm::vec2 InputHandler::getMousePosition() const {
    return glm::vec2(state.mouseX, state.mouseY);
}

void InputHandler::disableCursor() {
    if (window) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        state.isCursorDisabled = true;
    }
}

void InputHandler::enableCursor() {
    if (window) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        state.isCursorDisabled = false;
    }
}

bool InputHandler::isCursorDisabled() const {
    return state.isCursorDisabled;
}

bool InputHandler::consumeF1Press() {
    if (state.f1Pressed && !isKeyPressed(GLFW_KEY_F1)) {
        state.f1Pressed = false;
        return true;
    }
    return false;
}

bool InputHandler::consumeF2Press() {
    if (state.f2Pressed && !isKeyPressed(GLFW_KEY_F2)) {
        state.f2Pressed = false;
        return true;
    }
    return false;
}

bool InputHandler::consumeF3Press() {
    if (state.f3Pressed && !isKeyPressed(GLFW_KEY_F3)) {
        state.f3Pressed = false;
        return true;
    }
    return false;
}

bool InputHandler::consumeF4Press() {
    if (state.f4Pressed && !isKeyPressed(GLFW_KEY_F4)) {
        state.f4Pressed = false;
        return true;
    }
    return false;
}

bool InputHandler::consumeVPress() {
    if (state.vPressed && !isKeyPressed(GLFW_KEY_V)) {
        state.vPressed = false;
        return true;
    }
    return false;
}
