#define NOMINMAX
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <memory>

#include "InputManager.h"
#include "Renderer.h"
#include "Player.h"
#include "HUD.h"
#include "BSPLoader.h"
#include "TriangleCollider.h"
#include "WADLoader.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

constexpr int SCR_WIDTH = 1280;
constexpr int SCR_HEIGHT = 720;

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "HuitaEngine", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    InputManager input(window);
    Renderer renderer;
    HUD hud;

    if (!renderer.init(SCR_WIDTH, SCR_HEIGHT)) {
        std::cerr << "Renderer init failed\n";
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    WADLoader wadLoader;
    wadLoader.loadQuakePalette("palette.lmp");
    wadLoader.loadAllWADs();

    BSPLoader bspLoader;
    Player player;
    std::unique_ptr<MeshCollider> collider;
    
    if (bspLoader.load("maps/de_pasan.bsp", wadLoader)) { 
        renderer.loadWorld(bspLoader); 

        collider = std::make_unique<MeshCollider>();
        collider->buildFromBSP(bspLoader.getMeshVertices(), bspLoader.getMeshIndices());

        glm::vec3 spawnPos, spawnAngles;
        if (bspLoader.findPlayerStart(spawnPos, spawnAngles)) {
            player.setPosition(spawnPos);
        }
    }

    float lastFrame = 0.0f;
    bool flashlightOn = false;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        float deltaTime = std::min(currentFrame - lastFrame, 0.05f);
        lastFrame = currentFrame;

        input.update();
        if (input.isPressed(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(window, true);

        if (input.isPressed(GLFW_KEY_F)) {
            flashlightOn = !flashlightOn;
        }

        float dx, dy;
        input.getMouseDelta(dx, dy);
        static float yaw = -90.0f, pitch = 0.0f;
        yaw += dx * 0.1f;
        pitch = glm::clamp(pitch + dy * 0.1f, -89.0f, 89.0f);

        if (input.toggleNoclip()) player.toggleNoclip();
        if (input.toggleHitbox()) renderer.setShowHitbox(!renderer.getShowHitbox());

        player.update(deltaTime, yaw, pitch, collider.get());
        hud.update(deltaTime, player.getPosition());

        glm::vec3 viewDir(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        );
        renderer.setFlashlight(player.getEyePosition(), viewDir, flashlightOn);

        renderer.beginFrame(glm::vec3(0.02f, 0.02f, 0.05f));

        glm::mat4 view;
        player.getViewMatrix(glm::value_ptr(view));

        renderer.renderWorld(view, player.getEyePosition());

        glm::mat4 projection = glm::perspective(glm::radians(75.0f),
            (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 1000.0f);
        renderer.renderHitbox(view, projection, player.getPosition(), true);

        hud.render(SCR_WIDTH, SCR_HEIGHT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();

    return 0;
}
