#include "InputHandler.h"
#include "Config.h"
#include "GameManager.h"
#include "HUD.h"
#include "Player.h"
#include <iostream>

float InputHandler::lastX = static_cast<float>(Config::SCR_WIDTH) / 2.0f;
float InputHandler::lastY = static_cast<float>(Config::SCR_HEIGHT) / 2.0f;
bool InputHandler::firstMouse = true;
bool InputHandler::f1Pressed = false;
bool InputHandler::f2Pressed = false;
bool InputHandler::f3Pressed = false;
bool InputHandler::f4Pressed = true;
bool InputHandler::noclipPressed = false;

void InputHandler::registerCallbacks(GLFWwindow* window) {
    glfwSetCursorPosCallback(window, mouseCallback);
}

void InputHandler::mouseCallback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }
    float xoffset = static_cast<float>(xpos - lastX);
    float yoffset = static_cast<float>(lastY - ypos);
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    auto& game = GameManager::instance();
    game.setYaw(game.getYaw() + xoffset * Config::MOUSE_SENSITIVITY);

    float pitch = game.getPitch() + yoffset * Config::MOUSE_SENSITIVITY;
    if (pitch > Config::MAX_PITCH) pitch = Config::MAX_PITCH;
    if (pitch < -Config::MAX_PITCH) pitch = -Config::MAX_PITCH;
    game.setPitch(pitch);
}

void InputHandler::processActions(GLFWwindow* window, HUD& hud, Player& player, GameManager& game) {
    if (glfwGetKey(window, Config::KEY_CROSSHAIR) == GLFW_PRESS && !f1Pressed) { hud.toggleCrosshair(); f1Pressed = true; }
    if (glfwGetKey(window, Config::KEY_CROSSHAIR) == GLFW_RELEASE) f1Pressed = false;

    if (glfwGetKey(window, Config::KEY_FPS) == GLFW_PRESS && !f2Pressed) { hud.toggleFPS(); f2Pressed = true; }
    if (glfwGetKey(window, Config::KEY_FPS) == GLFW_RELEASE) f2Pressed = false;

    if (glfwGetKey(window, Config::KEY_POSITION) == GLFW_PRESS && !f3Pressed) { hud.togglePosition(); f3Pressed = true; }
    if (glfwGetKey(window, Config::KEY_POSITION) == GLFW_RELEASE) f3Pressed = false;

    if (glfwGetKey(window, Config::KEY_HITBOX) == GLFW_PRESS && !f4Pressed) { game.toggleHitbox(); f4Pressed = true; }
    if (glfwGetKey(window, Config::KEY_HITBOX) == GLFW_RELEASE) f4Pressed = false;

    if (glfwGetKey(window, Config::KEY_NOCLIP) == GLFW_PRESS && !noclipPressed) {
        player.toggleNoclip();
        std::cout << "Noclip: " << (player.isNoclip() ? "ON" : "OFF") << "\n";
        noclipPressed = true;
    }
    if (glfwGetKey(window, Config::KEY_NOCLIP) == GLFW_RELEASE) noclipPressed = false;
}