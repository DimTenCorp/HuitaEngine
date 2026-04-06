#include "GameManager.h"
#include "gl_includes.h"
#include "Config.h"
#include "InputHandler.h"
#include "BSPRenderer.h"
#include "ShaderManager.h"
#include "DebugRenderer.h"
#include "WADLoader.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <algorithm>

GameManager& GameManager::instance() {
    static GameManager inst;
    return inst;
}

bool GameManager::initialize() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(Config::SCR_WIDTH, Config::SCR_HEIGHT, "HuitaEngine BSP", nullptr, nullptr);
    if (!window) { std::cerr << "Failed to create window\n"; return false; }

    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n"; return false;
    }

    if (!initOpenGL()) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    InputHandler::registerCallbacks(window);
    return loadGameContent();
}

bool GameManager::initOpenGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    return true;
}

bool GameManager::loadGameContent() {
    WADLoader wadLoader;
    if (!wadLoader.loadQuakePalette("palette.lmp"))
        std::cout << "No palette.lmp found, using fallback colors\n";

    if (bspLoader.load("maps/de_pasan.bsp", wadLoader)) {
        if (!BSPRenderer::initialize(bspLoader, meshCollider)) {
            std::cerr << "Failed to initialize BSP renderer\n";
            return false;
        }
        setupPlayerSpawn();
    }
    else {
        std::cerr << "Failed to load BSP, using fallback\n";
        player.setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    }

    if (!ShaderManager::initialize()) {
        std::cerr << "Failed to compile shaders\n";
        return false;
    }
    DebugRenderer::initialize();
    return true;
}

void GameManager::setupPlayerSpawn() {
    glm::vec3 spawnPos, spawnAngles;
    if (bspLoader.findPlayerStart(spawnPos, spawnAngles)) {
        player.setPosition(spawnPos);
        yaw = spawnAngles.y;
        pitch = spawnAngles.x;
        player.setYaw(yaw);
        player.setPitch(pitch);
        std::cout << "Player spawned at: " << spawnPos.x << ", " << spawnPos.y << ", " << spawnPos.z << "\n";
    }
    else {
        auto bounds = bspLoader.getWorldBounds();
        float centerY = bounds.min.y + (bounds.max.y - bounds.min.y) * 0.5f;
        player.setPosition(glm::vec3(0.0f, centerY + Config::Player::SPAWN_OFFSET_Y, 0.0f));
    }
}

void GameManager::run() {
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (deltaTime > Config::MIN_DELTA_TIME) deltaTime = Config::MIN_DELTA_TIME;

        update(deltaTime);
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

// ✅ ОБЯЗАТЕЛЬНО: GameManager:: перед именем функции
void GameManager::update(float dt) {
    player.update(dt, yaw, pitch, &meshCollider);
    hud.update(dt, player.getPosition());
    InputHandler::processActions(window, hud, player, *this);

    if (glfwGetKey(window, Config::KEY_QUIT) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// ✅ ОБЯЗАТЕЛЬНО: GameManager:: перед именем функции
void GameManager::render() {
    glClearColor(Config::CLEAR_R, Config::CLEAR_G, Config::CLEAR_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 projection = glm::perspective(
        glm::radians(Config::FOV_DEGREES),
        static_cast<float>(Config::SCR_WIDTH) / Config::SCR_HEIGHT,
        Config::NEAR_PLANE,
        Config::FAR_PLANE
    );

    glm::mat4 view;
    player.getViewMatrix(glm::value_ptr(view));

    // Find first light entity from BSP and use its position
    auto lightEntities = bspLoader.getEntitiesByClass("light");
    PointLight pointLight;
    if (!lightEntities.empty()) {
        pointLight.position = lightEntities[0].origin;
        // Try to parse light properties
        auto colorIt = lightEntities[0].properties.find("_color");
        if (colorIt != lightEntities[0].properties.end()) {
            float r, g, b;
            if (sscanf(colorIt->second.c_str(), "%f %f %f", &r, &g, &b) == 3) {
                pointLight.color = glm::vec3(r, g, b);
            } else {
                pointLight.color = glm::vec3(1.0f, 0.9f, 0.7f);
            }
        } else {
            pointLight.color = glm::vec3(1.0f, 0.9f, 0.7f);
        }
        
        auto intensityIt = lightEntities[0].properties.find("light");
        if (intensityIt != lightEntities[0].properties.end()) {
            pointLight.intensity = std::stof(intensityIt->second) / 300.0f;
        } else {
            pointLight.intensity = 1.5f;
        }
        
        pointLight.radius = 25.0f;
        std::cout << "[GameManager] Using light entity at (" 
                  << pointLight.position.x << ", " << pointLight.position.y << ", " << pointLight.position.z << ")\n";
    } else {
        // Fallback to default position if no light entity found
        pointLight.position = glm::vec3(0.0f, 5.0f, 0.0f);
        pointLight.color = glm::vec3(1.0f, 0.9f, 0.7f);
        pointLight.intensity = 1.5f;
        pointLight.radius = 25.0f;
        std::cout << "[GameManager] No light entity found, using fallback position\n";
    }
    ShaderManager::setPointLight(pointLight);

    // Setup sun light for shadows
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.5f, -0.8f, 0.3f));
    ShaderManager::setLightDir(sunDir);
    ShaderManager::enableShadows(true);

    // Calculate light space matrix for shadows
    float nearPlane = -10.0f;
    float farPlane = 50.0f;
    float orthoSize = 30.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
    glm::mat4 lightView = glm::lookAt(-sunDir * 20.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    
    ShaderManager::setSunLightSpaceMatrix(lightSpaceMatrix);

    // Shadow pass
    BSPRenderer::renderShadowPass(bspLoader, lightSpaceMatrix);

    // Restore viewport
    glViewport(0, 0, Config::SCR_WIDTH, Config::SCR_HEIGHT);

    // Main render pass
    ShaderManager::bindShadowMap(BSPRenderer::getShadowMapTexture());
    BSPRenderer::render(bspLoader, projection, view, player.getEyePosition());

    if (showPlayerHitbox) {
        DebugRenderer::renderHitbox(projection, view, player.getPosition());
    }

    hud.render(Config::SCR_WIDTH, Config::SCR_HEIGHT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GameManager::shutdown() {
    BSPRenderer::cleanup();
    DebugRenderer::cleanup();
    ShaderManager::cleanup();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}