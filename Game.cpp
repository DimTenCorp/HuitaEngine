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
}

void Game::setViewAngles(float newYaw, float newPitch) {
    yaw = newYaw;
    pitch = newPitch;
    player->setYaw(yaw);
    player->setPitch(pitch);
}

void Game::processMouse(GLFWwindow* window) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;  // Пропускаем первый кадр - не обрабатываем движение
    }

    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos);
    lastX = (float)xpos;
    lastY = (float)ypos;

    yaw += xoffset * 0.1f;
    pitch += yoffset * 0.1f;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    player->setYaw(yaw);
    player->setPitch(pitch);
}

void Game::update(float deltaTime) {
    processMouse(Engine::getInstance()->getWindow());

    auto* collider = Engine::getInstance()->getCollider();
    if (collider) {  // Добавьте проверку
        player->update(deltaTime, yaw, pitch, collider);
    }

    hud->update(deltaTime, player->getPosition());
}

void Game::processInput(GLFWwindow* window) {
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

    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
        auto* lm = Engine::getInstance()->getLMRenderer();
        if (lm) {
            Engine::getInstance()->setLightmapIntensity(lm->getLightmapIntensity() + 0.1f);
        }
    }
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
        auto* lm = Engine::getInstance()->getLMRenderer();
        if (lm) {
            lm->setLightmapIntensity(std::max(0.0f, lm->getLightmapIntensity() - 0.1f));
        }
    }

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
    hud->render(screenWidth, screenHeight);
}