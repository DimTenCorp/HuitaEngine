#include "pch.h"
#include "Game.h"
#include "HUD.h"
#include "Engine.h"
#include "Mesh.h"
#include "LightmappedRenderer.h"
#include <iostream>

Game::Game() = default;
Game::~Game() = default;

void Game::init() {
    player = std::make_unique<Player>();
    hud = std::make_unique<HUD>();

    SettingsData settings;
    settings.load();
    mouseSensitivity = settings.mouseSensitivity;
}

void Game::setPaused(bool paused) {
    if (isPaused == paused) return;

    isPaused = paused;

    if (isPaused) {
        // СТАВИМ НА ПАУЗУ - сохраняем скорость
        savedVelocity = player->getVelocity();
        wasOnGround = player->isOnGround();

        // НЕ сбрасываем скорость игроку - просто не обновляем его позицию в update
        player->setPausedState(true);

        std::cout << "[GAME] PAUSED (vel: " << savedVelocity.x << ", " << savedVelocity.y << ", " << savedVelocity.z << ")" << std::endl;
    }
    else {
        // СНИМАЕМ С ПАУЗЫ - восстанавливаем скорость!
        player->setVelocity(savedVelocity);
        player->setPausedState(false);

        std::cout << "[GAME] RESUMED (vel restored: " << savedVelocity.x << ", " << savedVelocity.y << ", " << savedVelocity.z << ")" << std::endl;
    }
}

void Game::setViewAngles(float newYaw, float newPitch) {
    yaw = newYaw;
    pitch = newPitch;
    player->setYaw(yaw);
    player->setPitch(pitch);
}

void Game::processMouse(GLFWwindow* window) {
    if (isPaused) return;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;
    }

    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos);
    lastX = (float)xpos;
    lastY = (float)ypos;

    float sensMultiplier = mouseSensitivity * 0.1f;

    yaw += xoffset * sensMultiplier;
    pitch += yoffset * sensMultiplier;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    player->setYaw(yaw);
    player->setPitch(pitch);
}

void Game::update(float deltaTime) {
    if (isPaused) {
        // Только HUD обновляем для FPS счетчика
        hud->update(deltaTime, player->getPosition());
        return;
    }

    processMouse(Engine::getInstance()->getWindow());

    auto* collider = Engine::getInstance()->getCollider();
    auto& waterZones = Engine::getInstance()->getWaterZones();

    player->CheckWater(waterZones);

    if (collider) {
        player->update(deltaTime, yaw, pitch, collider);
    }

    hud->update(deltaTime, player->getPosition());
}

void Game::processInput(GLFWwindow* window) {
    if (isPaused) return;

    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS && !f1Pressed) {
        hud->toggleCrosshair(); f1Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE) f1Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS && !f2Pressed) {
        hud->toggleFPS(); f2Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_RELEASE) f2Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS && !f3Pressed) {
        hud->togglePosition(); f3Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_RELEASE) f3Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS && !f5Pressed) {
        Engine::getInstance()->toggleLightmappedRenderer();
        std::cout << "Lightmapped: " << (Engine::getInstance()->useLightmappedRenderer() ? "ON" : "OFF") << "\n";
        f5Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_RELEASE) f5Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS && !f6Pressed) {
        static bool showOnly = false;
        showOnly = !showOnly;
        Engine::getInstance()->setShowLightmapsOnly(showOnly);
        std::cout << "Show lightmaps only: " << (showOnly ? "ON" : "OFF") << "\n";
        f6Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_RELEASE) f6Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && !noclipPressed) {
        player->toggleNoclip();
        std::cout << "Noclip: " << (player->isNoclip() ? "ON" : "OFF") << "\n";
        noclipPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_RELEASE) noclipPressed = false;

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !bhopPressed) {
        player->EnableAutoJump(!player->IsAutoJumpEnabled());
        std::cout << "Autohop: " << (player->IsAutoJumpEnabled() ? "ON" : "OFF") << "\n";
        bhopPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE) bhopPressed = false;
}

void Game::render(int screenWidth, int screenHeight) {
    hud->setScreenSize(screenWidth, screenHeight);
    hud->render();
}