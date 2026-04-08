#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <new>
#include "Shader.h"
#include "Player.h"
#include "HUD.h"
#include "BSPLoader.h"
#include "TriangleCollider.h"
#include "Renderer.h"
#include "WADLoader.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Глобальные переменные инкапсулированы в namespace для лучшей организации
namespace {
    Player* g_player = nullptr;
    Renderer* g_renderer = nullptr;
    float g_lastX = SCR_WIDTH / 2.0f;
    float g_lastY = SCR_HEIGHT / 2.0f;
    bool g_firstMouse = true;
    float g_deltaTime = 0.0f;
    float g_lastFrame = 0.0f;
    float g_yaw = -90.0f;
    float g_pitch = 0.0f;
    bool g_f1Pressed = false, g_f2Pressed = false, g_f3Pressed = false, g_f4Pressed = true, g_noclipPressed = false;
    
    BSPLoader* g_bspLoader = nullptr;
    MeshCollider* g_meshCollider = nullptr;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_firstMouse) {
        g_lastX = (float)xpos;
        g_lastY = (float)ypos;
        g_firstMouse = false;
    }
    float xoffset = (float)(xpos - g_lastX);
    float yoffset = (float)(g_lastY - ypos);
    g_lastX = (float)xpos;
    g_lastY = (float)ypos;
    g_yaw += xoffset * 0.1f;
    g_pitch += yoffset * 0.1f;
    if (g_pitch > 89.0f) g_pitch = 89.0f;
    if (g_pitch < -89.0f) g_pitch = -89.0f;
}

void processInput(GLFWwindow* window, HUD& hud) {
    if (!g_player || !g_renderer) return;

    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS && !g_f1Pressed) {
        hud.toggleCrosshair(); g_f1Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE) g_f1Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS && !g_f2Pressed) {
        hud.toggleFPS(); g_f2Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_RELEASE) g_f2Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS && !g_f3Pressed) {
        hud.togglePosition(); g_f3Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_RELEASE) g_f3Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS && !g_f4Pressed) {
        g_renderer->setShowHitbox(!g_renderer->getShowHitbox()); g_f4Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_RELEASE) g_f4Pressed = false;

    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && !g_noclipPressed) {
        g_player->toggleNoclip();
        std::cout << "Noclip: " << (g_player->isNoclip() ? "ON" : "OFF") << std::endl;
        g_noclipPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_RELEASE) g_noclipPressed = false;
}

bool initSystems(WADLoader& wadLoader) {
    if (!g_bspLoader || !g_player || !g_renderer || !g_meshCollider) return false;

    if (!g_bspLoader->load("maps/crossfire.bsp", wadLoader)) {
        std::cerr << "Failed to load BSP" << std::endl;
        return false;
    }

    const auto& vertices = g_bspLoader->getMeshVertices();
    g_meshCollider->buildFromBSP(vertices, g_bspLoader->getMeshIndices());
    std::cout << "Mesh collider built with " << g_meshCollider->getTriangleCount() << " triangles" << std::endl;

    if (!g_renderer->loadWorld(*g_bspLoader)) {
        std::cerr << "Failed to load world into renderer" << std::endl;
        return false;
    }

    glm::vec3 spawnPos;
    glm::vec3 spawnAngles;

    if (g_bspLoader->findPlayerStart(spawnPos, spawnAngles)) {
        g_player->setPosition(spawnPos);
        g_yaw = spawnAngles.y;
        g_pitch = spawnAngles.x;
        g_player->setYaw(g_yaw);
        g_player->setPitch(g_pitch);
    }
    else {
        auto bounds = g_bspLoader->getWorldBounds();
        g_player->setPosition(glm::vec3(0.0f, bounds.min.y + 10.0f, 0.0f));
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

    // Инициализация ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::cout << "ImGui initialized successfully" << std::endl;

    // Выделяем память для глобальных объектов
    g_player = new (std::nothrow) Player();
    g_renderer = new (std::nothrow) Renderer();
    g_bspLoader = new (std::nothrow) BSPLoader();
    g_meshCollider = new (std::nothrow) MeshCollider();

    if (!g_player || !g_renderer || !g_bspLoader || !g_meshCollider) {
        std::cerr << "Failed to allocate global objects" << std::endl;
        delete g_player;
        delete g_renderer;
        delete g_bspLoader;
        delete g_meshCollider;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Инициализация рендерера
    if (!g_renderer->init(SCR_WIDTH, SCR_HEIGHT)) {
        std::cerr << "Failed to init renderer" << std::endl;
        delete g_player;
        delete g_renderer;
        delete g_bspLoader;
        delete g_meshCollider;
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
        g_player->setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    }

    // Главный цикл
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        g_deltaTime = currentFrame - g_lastFrame;
        g_lastFrame = currentFrame;
        if (g_deltaTime > 0.05f) g_deltaTime = 0.05f;

        // Обработка ввода
        processInput(window, hud);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Обновление игровой логики
        g_player->update(g_deltaTime, g_yaw, g_pitch, g_meshCollider);
        hud.update(g_deltaTime, g_player->getPosition());

        // Рендеринг
        g_renderer->beginFrame(glm::vec3(0.1f, 0.15f, 0.2f));

        glm::mat4 view;
        g_player->getViewMatrix(glm::value_ptr(view));

        g_renderer->renderWorld(view, g_player->getEyePosition());

        if (g_renderer->getShowHitbox()) {
            glm::mat4 projection = glm::perspective(glm::radians(75.0f),
                (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 1000.0f);
            g_renderer->renderHitbox(view, projection, g_player->getPosition(), true);
        }

        hud.render(SCR_WIDTH, SCR_HEIGHT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Очистка
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    g_renderer->unloadWorld();
    g_bspLoader->cleanupTextures();
    
    delete g_player;
    delete g_renderer;
    delete g_bspLoader;
    delete g_meshCollider;
    g_player = nullptr;
    g_renderer = nullptr;
    g_bspLoader = nullptr;
    g_meshCollider = nullptr;
    
    glfwTerminate();
    return 0;
}