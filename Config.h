#pragma once
#include "gl_includes.h"

namespace Config {
    // Экран
    static constexpr unsigned int SCR_WIDTH = 1280;
    static constexpr unsigned int SCR_HEIGHT = 720;

    // Камера
    static constexpr float FOV_DEGREES = 75.0f;
    static constexpr float NEAR_PLANE = 0.1f;
    static constexpr float FAR_PLANE = 1000.0f;
    static constexpr float MOUSE_SENSITIVITY = 0.1f;
    static constexpr float MAX_PITCH = 89.0f;
    static constexpr float MIN_DELTA_TIME = 0.05f;

    // Клавиши
    static constexpr int KEY_CROSSHAIR = GLFW_KEY_F1;
    static constexpr int KEY_FPS = GLFW_KEY_F2;
    static constexpr int KEY_POSITION = GLFW_KEY_F3;
    static constexpr int KEY_HITBOX = GLFW_KEY_F4;
    static constexpr int KEY_NOCLIP = GLFW_KEY_V;
    static constexpr int KEY_QUIT = GLFW_KEY_ESCAPE;

    // Рендеринг - яркие цвета для отладки
    static constexpr float CLEAR_R = 0.0f;
    static constexpr float CLEAR_G = 0.0f;
    static constexpr float CLEAR_B = 0.0f;

    // Игрок
    namespace Player {
        static constexpr float HITBOX_WIDTH = 0.6f;
        static constexpr float HITBOX_HEIGHT = 1.8f;
        static constexpr float SPAWN_OFFSET_Y = 10.0f;
    }
}