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
#include "LightmapManager.h"
#include "LightmappedRenderer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

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
    bool g_f1Pressed = false, g_f2Pressed = false, g_f3Pressed = false,
        g_f4Pressed = true, g_noclipPressed = false;
    bool g_f5Pressed = false, g_f6Pressed = false;
    static bool g_bhopPressed = false;

    BSPLoader* g_bspLoader = nullptr;
    MeshCollider* g_meshCollider = nullptr;
    LightmapManager* g_lightmapManager = nullptr;
    LightmappedRenderer* g_lmRenderer = nullptr;

    float g_lightmapIntensity = 1.0f;
    bool g_showLightmapsOnly = false;
    bool g_useLightmappedRenderer = false;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
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
    if (!g_player) return;

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

    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && !g_noclipPressed) {
        g_player->toggleNoclip();
        std::cout << "Noclip: " << (g_player->isNoclip() ? "ON" : "OFF") << std::endl;
        g_noclipPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_RELEASE) g_noclipPressed = false;

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !g_bhopPressed) {
        g_player->EnableAutoJump(!g_player->IsAutoJumpEnabled());
        std::cout << "Autohop: " << (g_player->IsAutoJumpEnabled() ? "ON" : "OFF") << std::endl;
        g_bhopPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE) g_bhopPressed = false;

    // ============ НОВОЕ: Управление светом ============

    // F5 - переключение между обычным и lightmapped рендерером
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS && !g_f5Pressed) {
        g_useLightmappedRenderer = !g_useLightmappedRenderer;
        std::cout << "Lightmapped renderer: " << (g_useLightmappedRenderer ? "ON" : "OFF") << std::endl;
        g_f5Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_RELEASE) g_f5Pressed = false;

    // F6 - показать только lightmaps (debug)
    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS && !g_f6Pressed) {
        g_showLightmapsOnly = !g_showLightmapsOnly;
        if (g_lmRenderer) {
            g_lmRenderer->setShowLightmapsOnly(g_showLightmapsOnly);
        }
        std::cout << "Show lightmaps only: " << (g_showLightmapsOnly ? "ON" : "OFF") << std::endl;
        g_f6Pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_RELEASE) g_f6Pressed = false;

    // Клавиши + и - для интенсивности света
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) { // + (без шифта)
        g_lightmapIntensity += 0.1f;
        if (g_lmRenderer) {
            g_lmRenderer->setLightmapIntensity(g_lightmapIntensity);
        }
        std::cout << "Lightmap intensity: " << g_lightmapIntensity << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
        g_lightmapIntensity -= 0.1f;
        if (g_lightmapIntensity < 0.0f) g_lightmapIntensity = 0.0f;
        if (g_lmRenderer) {
            g_lmRenderer->setLightmapIntensity(g_lightmapIntensity);
        }
        std::cout << "Lightmap intensity: " << g_lightmapIntensity << std::endl;
    }
}

bool initSystems(WADLoader& wadLoader) {
    if (!g_bspLoader || !g_player || !g_meshCollider) return false;

    // Загружаем BSP
    if (!g_bspLoader->load("maps/huita1.bsp", wadLoader)) {
        std::cerr << "Failed to load BSP" << std::endl;
        return false;
    }

    // ============ НОВОЕ: Инициализация lightmaps ============

    // Создаем менеджер lightmaps
    g_lightmapManager = new (std::nothrow) LightmapManager();
    if (!g_lightmapManager) {
        std::cerr << "Failed to allocate LightmapManager" << std::endl;
        // Продолжаем без lightmaps
    }
    else {
        // Инициализируем из BSP данных
        if (!g_lightmapManager->initializeFromBSP(*g_bspLoader)) {
            std::cerr << "Warning: Failed to initialize lightmaps, using fallback" << std::endl;
            delete g_lightmapManager;
            g_lightmapManager = nullptr;
        }
        else {
            std::cout << "[LIGHT] Lightmaps initialized successfully" << std::endl;
            std::cout << "[LIGHT] Total lightmap data: " << g_bspLoader->lightingData.size() << " bytes" << std::endl;
        }
    }

    if (g_lightmapManager && g_lightmapManager->hasLightmaps()) {
        g_lightmapManager->debugPrintLightmapInfo();
    }

    // Строим коллизии с информацией о жидкостях
    const auto& vertices = g_bspLoader->getMeshVertices();
    const auto& indices = g_bspLoader->getMeshIndices();
    const auto& faces = g_bspLoader->getFaces();
    const auto& texInfos = g_bspLoader->getTexInfos();
    const auto& drawCalls = g_bspLoader->getDrawCalls();
    
    // Подсчитываем количество жидких текстур для отладки
    int liquidTexCount = 0;
    for (size_t i = 0; i < texInfos.size(); i++) {
        if (g_bspLoader->isTextureLiquid(static_cast<int>(i))) {
            liquidTexCount++;
        }
    }
    std::cout << "[LIQUID] Found " << liquidTexCount << " liquid textures in BSP" << std::endl;
    
    // Создаем маппинг: номер треугольника -> индекс текстуры
    // Количество треугольников = количество индексов / 3
    size_t triangleCount = indices.size() / 3;
    std::vector<int> faceTextureIndices(triangleCount, -1);
    size_t liquidTriangles = 0;
    
    // Сопоставляем draw calls с треугольниками
    // Draw calls идут последовательно, каждый покрывает определенное количество треугольников
    size_t currentIndex = 0;
    for (const auto& drawCall : drawCalls) {
        int texIndex = -1;
        // Находим индекс текстуры в массиве BSP по texture ID
        for (size_t i = 0; i < texInfos.size(); i++) {
            if (g_bspLoader->getTextureID(i) == drawCall.texID) {
                texIndex = static_cast<int>(i);
                break;
            }
        }
        
        // Проставляем этот индекс текстуры всем треугольникам этого draw call
        size_t numTriangles = drawCall.indexCount / 3;
        for (size_t t = 0; t < numTriangles && currentIndex < triangleCount; t++) {
            faceTextureIndices[currentIndex] = texIndex;
            if (texIndex >= 0 && g_bspLoader->isTextureLiquid(texIndex)) {
                liquidTriangles++;
            }
            currentIndex++;
        }
    }
    
    std::cout << "[COLLIDER] Built faceTextureIndices with " << faceTextureIndices.size() 
              << " entries (" << currentIndex << " filled)" << std::endl;
    std::cout << "[LIQUID] Found " << liquidTriangles << " triangles with liquid texture (prefix !)" << std::endl;
    
    g_meshCollider->buildFromBSP(vertices, indices, faceTextureIndices, g_bspLoader);
    std::cout << "Mesh collider built with " << g_meshCollider->getTriangleCount()
        << " triangles" << std::endl;

    // ============ НОВОЕ: Создание рендереров ============

    // Сначала создаем обычный рендерер как fallback
    g_renderer = new (std::nothrow) Renderer();
    if (!g_renderer) {
        std::cerr << "Failed to allocate Renderer" << std::endl;
        return false;
    }

    if (!g_renderer->init(SCR_WIDTH, SCR_HEIGHT)) {
        std::cerr << "Failed to init renderer" << std::endl;
        return false;
    }

    // Если есть lightmaps, создаем lightmapped рендерер
    if (g_lightmapManager && g_lightmapManager->hasLightmaps()) {
        g_lmRenderer = new (std::nothrow) LightmappedRenderer();
        if (g_lmRenderer) {
            if (g_lmRenderer->init(SCR_WIDTH, SCR_HEIGHT)) {
                if (g_lmRenderer->loadWorld(*g_bspLoader, *g_lightmapManager)) {
                    std::cout << "[LIGHT] Lightmapped renderer ready (Press F5 to toggle)" << std::endl;
                    g_lmRenderer->setLightmapIntensity(g_lightmapIntensity);
                    g_useLightmappedRenderer = true; // По умолчанию используем свет
                }
                else {
                    std::cerr << "[LIGHT] Failed to load world into lightmapped renderer" << std::endl;
                    delete g_lmRenderer;
                    g_lmRenderer = nullptr;
                }
            }
            else {
                std::cerr << "[LIGHT] Failed to init lightmapped renderer" << std::endl;
                delete g_lmRenderer;
                g_lmRenderer = nullptr;
            }
        }
    }

    // Загружаем мир в обычный рендерер (fallback)
    if (!g_renderer->loadWorld(*g_bspLoader)) {
        std::cerr << "Failed to load world into renderer" << std::endl;
        return false;
    }

    // ============ Спавн игрока ============

    glm::vec3 spawnPos;
    glm::vec3 spawnAngles;

    if (g_bspLoader->findPlayerStart(spawnPos, spawnAngles)) {
        g_player->setPosition(spawnPos);
        g_yaw = spawnAngles.y;
        g_pitch = spawnAngles.x;
        g_player->setYaw(g_yaw);
        g_player->setPitch(g_pitch);
        std::cout << "[SPAWN] Player spawned at lightmapped position" << std::endl;
    }
    else {
        auto bounds = g_bspLoader->getWorldBounds();
        g_player->setPosition(glm::vec3(0.0f, bounds.min.y + 10.0f, 0.0f));
        std::cout << "[SPAWN] Using fallback spawn position" << std::endl;
    }

    return true;
}

int main() {
    // ============ Инициализация GLFW ============

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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "HuitaEngine BSP - Lightmapped", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // ============ Инициализация GLAD ============

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer_str = glGetString(GL_RENDERER);
    std::cout << "OpenGL Version: " << version << std::endl;
    std::cout << "Renderer: " << renderer_str << std::endl;

    // ============ Инициализация ImGui ============

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ============ Создание объектов ============

    g_player = new (std::nothrow) Player();
    g_bspLoader = new (std::nothrow) BSPLoader();
    g_meshCollider = new (std::nothrow) MeshCollider();

    if (!g_player || !g_bspLoader || !g_meshCollider) {
        std::cerr << "Failed to allocate core objects" << std::endl;
        delete g_player;
        delete g_bspLoader;
        delete g_meshCollider;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // ============ Загрузка ресурсов ============

    HUD hud;
    WADLoader wadLoader;

    if (wadLoader.loadQuakePalette("palette.lmp")) {
        std::cout << "Quake palette loaded" << std::endl;
    }

    // Инициализируем все системы (включая свет)
    if (!initSystems(wadLoader)) {
        std::cerr << "Failed to init systems" << std::endl;
        // Продолжаем с fallback
    }

    // ============ Настройка OpenGL ============

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error during init: " << err << std::endl;
    }

    // ============ Главный игровой цикл ============

    std::cout << "\n=== CONTROLS ===" << std::endl;
    std::cout << "F1 - Toggle crosshair" << std::endl;
    std::cout << "F2 - Toggle FPS" << std::endl;
    std::cout << "F3 - Toggle position" << std::endl;
    std::cout << "F4 - Toggle hitbox" << std::endl;
    std::cout << "F5 - Toggle lightmapped renderer" << std::endl;
    std::cout << "F6 - Show lightmaps only (debug)" << std::endl;
    std::cout << "+/- - Adjust lightmap intensity" << std::endl;
    std::cout << "V - Toggle noclip" << std::endl;
    std::cout << "================\n" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        // --- Время ---
        float currentFrame = (float)glfwGetTime();
        g_deltaTime = currentFrame - g_lastFrame;
        g_lastFrame = currentFrame;
        if (g_deltaTime > 0.05f) g_deltaTime = 0.05f;

        // --- Ввод ---
        processInput(window, hud);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // --- Обновление игрока ---
        g_player->update(g_deltaTime, g_yaw, g_pitch, g_meshCollider);
        hud.update(g_deltaTime, g_player->getPosition());

        // --- Рендеринг ---

        glm::mat4 view;
        g_player->getViewMatrix(glm::value_ptr(view));

        glm::vec3 eyePos = g_player->getEyePosition();

        // Выбираем рендерер
        if (g_useLightmappedRenderer && g_lmRenderer) {
            // ============ РЕНДЕРИНГ С ЗАПЕЧЁННЫМ СВЕТОМ ============

            // Цвет очистки - темнее т.к. свет от lightmaps
            g_lmRenderer->beginFrame(glm::vec3(0.02f, 0.02f, 0.02f));

            // Рендерим мир с lightmaps
            g_lmRenderer->renderWorld(view, eyePos, *g_bspLoader, glm::vec3(0.05f)); // Низкий ambient

        }
        else if (g_renderer) {
            // ============ ОБЫЧНЫЙ РЕНДЕРИНГ (fallback) ============

            g_renderer->beginFrame(glm::vec3(0.1f, 0.15f, 0.2f));

            g_renderer->renderWorld(view, eyePos);
        }

        // --- HUD ---
        hud.render(SCR_WIDTH, SCR_HEIGHT);

        // --- Swap buffers ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ============ Очистка ============

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Очищаем рендереры
    if (g_renderer) {
        g_renderer->unloadWorld();
    }
    if (g_lmRenderer) {
        g_lmRenderer->unloadWorld();
    }

    g_bspLoader->cleanupTextures();

    if (g_lightmapManager) {
        g_lightmapManager->cleanup();
    }

    // Удаляем объекты
    delete g_player;
    delete g_renderer;
    delete g_lmRenderer;
    delete g_bspLoader;
    delete g_meshCollider;
    delete g_lightmapManager;

    glfwTerminate();
    return 0;
}