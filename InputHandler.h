#pragma once
#include "gl_includes.h"

class HUD;
class Player;
class GameManager;

class InputHandler {
public:
    static void registerCallbacks(GLFWwindow* window);
    static void processActions(GLFWwindow* window, HUD& hud, Player& player, GameManager& game);

private:
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static float lastX, lastY;
    static bool firstMouse;
    static bool f1Pressed, f2Pressed, f3Pressed, f4Pressed, noclipPressed;
};