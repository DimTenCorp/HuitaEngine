#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include "Shader.h"
#include "Player.h"
#include "HUD.h"
#include "BSPLoader.h"
#include "TriangleCollider.h"
#include "Renderer.h"
#include "WADLoader.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Player player;
Renderer renderer;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float yaw = -90.0f;
float pitch = 0.0f;
bool f1Pressed = false, f2Pressed = false, f3Pressed = false, f4Pressed = true, noclipPressed = false;

BSPLoader bspLoader;
MeshCollider meshCollider;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }
    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos);
    lastX = (float)xpos;
    lastY = (float)ypos;
    yaw += xoffset * 0.1f;
    pitch += yoffset * 0.1f;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void processInput(GLFWwindow* window, HUD& hud) {
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS && !f1Pressed) {
        hud.toggleCrosshair(); f1Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE) f1Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS && !f2Pressed) {
        hud.toggleFPS(); f2Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_RELEASE) f2Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS && !f3Pressed) {
        hud.togglePosition(); f3Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_RELEASE) f3Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS && !f4Pressed) {
        renderer.setShowHitbox(!renderer.getShowHitbox()); f4Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_RELEASE) f4Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && !noclipPressed) {
        player.toggleNoclip();
        std::cout << "Noclip: " << (player.isNoclip() ? "ON" : "OFF") << std::endl;
        noclipPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_RELEASE) noclipPressed = false;
}

bool initSystems(WADLoader& wadLoader) {
    if (!bspLoader.load("maps/crossfire.bsp", wadLoader)) {
        std::cerr << "Failed to load BSP" << std::endl;
        return false;
    }

    const auto& vertices = bspLoader.getMeshVertices();
    meshCollider.buildFromBSP(vertices, bspLoader.getMeshIndices());
    std::cout << "Mesh collider built with " << meshCollider.getTriangleCount() << " triangles" << std::endl;

    if (!renderer.loadWorld(bspLoader)) {
        std::cerr << "Failed to load world into renderer" << std::endl;
        return false;
    }

    glm::vec3 spawnPos;
    glm::vec3 spawnAngles;

    if (bspLoader.findPlayerStart(spawnPos, spawnAngles)) {
        player.setPosition(spawnPos);
        yaw = spawnAngles.y;
        pitch = spawnAngles.x;
        player.setYaw(yaw);
        player.setPitch(pitch);
    }
    else {
        auto bounds = bspLoader.getWorldBounds();
        player.setPosition(glm::vec3(0.0f, bounds.min.y + 10.0f, 0.0f));
    }

    return true;
}

int main() {
    // Инициализация GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "HuitaEngine BSP", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Инициализация GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Проверяем версию OpenGL
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer_str = glGetString(GL_RENDERER);
    std::cout << "OpenGL Version: " << version << std::endl;
    std::cout << "Renderer: " << renderer_str << std::endl;

    // Инициализация рендерера
    if (!renderer.init(SCR_WIDTH, SCR_HEIGHT)) {
        std::cerr << "Failed to init renderer" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Настройка OpenGL
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    // Проверяем ошибки OpenGL
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error during init: " << err << std::endl;
    }

    HUD hud;
    WADLoader wadLoader;

    if (wadLoader.loadQuakePalette("palette.lmp")) {
        std::cout << "Quake palette loaded" << std::endl;
    }

    if (!initSystems(wadLoader)) {
        std::cerr << "Failed to init systems, using fallback" << std::endl;
        player.setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    }

    // Главный цикл
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        // Обработка ввода
        processInput(window, hud);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Обновление игровой логики
        player.update(deltaTime, yaw, pitch, &meshCollider);
        hud.update(deltaTime, player.getPosition());

        // Рендеринг
        renderer.beginFrame(glm::vec3(0.1f, 0.15f, 0.2f));

        glm::mat4 view;
        player.getViewMatrix(glm::value_ptr(view));

        renderer.renderWorld(view, player.getEyePosition());

        if (renderer.getShowHitbox()) {
            glm::mat4 projection = glm::perspective(glm::radians(75.0f),
                (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 1000.0f);
            renderer.renderHitbox(view, projection, player.getPosition(), true);
        }

        hud.render(SCR_WIDTH, SCR_HEIGHT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Очистка
    renderer.unloadWorld();
    bspLoader.cleanupTextures();
    glfwTerminate();
    return 0;
}