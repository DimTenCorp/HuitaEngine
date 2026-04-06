#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Структура для хранения состояния ввода
struct InputState {
    // Позиция мыши
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    
    // Смещения мыши для камеры
    float mouseOffsetX = 0.0f;
    float mouseOffsetY = 0.0f;
    
    // Состояние мыши
    bool isFirstMouse = true;
    bool isCursorDisabled = false;
    
    // Состояние клавиш (кэшированное)
    bool keys[512] = { false };
    bool keysJustPressed[512] = { false };
    bool keysJustReleased[512] = { false };
    
    // Специальные флаги для предотвращения повторного срабатывания
    bool f1Pressed = false;
    bool f2Pressed = false;
    bool f3Pressed = false;
    bool f4Pressed = false;
    bool vPressed = false;  // Для noclip
};

class InputHandler {
private:
    InputState state;
    GLFWwindow* window = nullptr;
    
    // Callback функции
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    
public:
    InputHandler();
    ~InputHandler();
    
    // Инициализация обработчика ввода
    void init(GLFWwindow* window);
    
    // Обновление состояния ввода (вызывать каждый кадр)
    void update();
    
    // Проверка состояния клавиш
    bool isKeyPressed(int key) const;
    bool isKeyJustPressed(int key);
    bool isKeyJustReleased(int key);
    
    // Получение смещения мыши (сбрасывается после чтения)
    glm::vec2 getMouseOffset();
    
    // Получение текущей позиции мыши
    glm::vec2 getMousePosition() const;
    
    // Управление курсором
    void disableCursor();
    void enableCursor();
    bool isCursorDisabled() const;
    
    // Доступ к состоянию
    const InputState& getState() const { return state; }
    
    // Геттеры для флагов HUD
    bool consumeF1Press();
    bool consumeF2Press();
    bool consumeF3Press();
    bool consumeF4Press();
    bool consumeVPress();
};
