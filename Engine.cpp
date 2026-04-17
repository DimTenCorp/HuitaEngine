#include "pch.h"
#include "Engine.h"
#include "Game.h"
#include "BSPLoader.h"
#include "WADLoader.h"
#include "Renderer.h"
#include "LightmappedRenderer.h"
#include "LightmapManager.h"
#include "TriangleCollider.h"
#include "Menu.h"
#include "WaterEntity.h"
#include "AABB.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include "Settings.h"

Engine* Engine::instance = nullptr;

static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Engine* engine = Engine::getInstance();
    if (engine) {
        engine->onFramebufferSize(width, height);
    }
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Engine* engine = Engine::getInstance();
    if (engine && engine->isMenuActive() && engine->getMenu()) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        engine->getMenu()->handleMouseButton(button, action, xpos, ypos);
    }
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Engine* engine = Engine::getInstance();
    if (engine && engine->isMenuActive() && engine->getMenu()) {
        engine->getMenu()->handleScroll(xoffset, yoffset);
    }
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    Engine* engine = Engine::getInstance();
    if (engine && engine->isMenuActive() && engine->getMenu()) {
        engine->getMenu()->handleMouseMove(xpos, ypos);
    }
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Engine* engine = Engine::getInstance();
    if (engine && engine->isMenuActive() && engine->getMenu()) {
        engine->getMenu()->handleKey(key, action);
    }

    if (engine && !engine->isMenuActive() && action == GLFW_PRESS && key == GLFW_KEY_E) {
        engine->useDoors();
    }
}

Engine::Engine() {
    instance = this;
}

Engine::~Engine() {
    shutdown();
    instance = nullptr;
}

bool Engine::initGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    SettingsData settings;
    settings.load();

    GLFWmonitor* monitor = nullptr;
    int windowX = 0, windowY = 0;

    if (settings.fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        windowX = 0;
        windowY = 0;
        width = settings.screenWidth;
        height = settings.screenHeight;
    }
    else {
        monitor = nullptr;
        GLFWmonitor* primary = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        windowX = (mode->width - settings.screenWidth) / 2;
        windowY = (mode->height - settings.screenHeight) / 2;
        width = settings.screenWidth;
        height = settings.screenHeight;
    }

    window = glfwCreateWindow(width, height, "HuitaEngine", monitor, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return false;
    }

    if (!settings.fullscreen) {
        glfwSetWindowPos(window, windowX, windowY);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetKeyCallback(window, keyCallback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    return true;
}

bool Engine::initGLAD() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return false;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    return true;
}

bool Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    return true;
}

bool Engine::initSystems() {
    bspLoader = std::make_unique<BSPLoader>();
    wadLoader = std::make_unique<WADLoader>();
    meshCollider = nullptr;
    renderer = nullptr;
    lmRenderer = nullptr;
    lightmapManager = nullptr;
    game = nullptr;
    skyboxRenderer = nullptr;

    menu = std::make_unique<Menu>();
    menu->setFontPath("res/fonts/Neogurotesuku-Regular.otf");

    wadLoader->loadQuakePalette("palette.lmp");

    menu->init(window, width, height);

    menu->setOnSettingsChanged([this](const SettingsData& settings) {
        applySettings(settings);
        });

    menu->setOnMapSelected([this](const std::string& mapPath) {
        loadMap(mapPath);
        });

    menu->setOnExitGame([this]() {
        });

    menu->setOnQuitGame([this]() {
        glfwSetWindowShouldClose(window, true);
        });

    menu->setOnReturnToGame([this]() {
        hideMenu();
        });

    menu->scanMaps("maps/");

    const auto& maps = menu->getMaps();
    if (maps.empty()) {
        std::cerr << "[ENGINE] No maps found! Please add .bsp files to maps/ folder" << std::endl;
    }
    else {
        std::cout << "[ENGINE] Found " << maps.size() << " maps." << std::endl;
    }

    menuActive = true;
    menu->setActive(true);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    pendingLoad = false;
    mapLoadInProgress = false;

    return true;
}

bool Engine::init() {
    if (!initGLFW()) return false;
    if (!initGLAD()) return false;
    if (!initImGui()) return false;
    if (!initSystems()) return false;

    std::cout << "\n=== ENGINE READY ===\n";
    return true;
}

void Engine::onFramebufferSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        std::cout << "[ENGINE] Ignoring invalid framebuffer size: " << width << "x" << height << std::endl;
        return;
    }

    this->width = width;
    this->height = height;

    glViewport(0, 0, width, height);

    if (renderer) {
        renderer->setViewport(width, height);
    }

    if (lmRenderer) {
        lmRenderer->setViewport(width, height);
    }

    if (menu) {
        menu->init(window, width, height);
    }

    if (game && !menuActive) {
        glfwSetCursorPos(window, width / 2.0, height / 2.0);
        game->centerMouseAt((float)width / 2.0f, (float)height / 2.0f);
    }

    std::cout << "[ENGINE] Window resized to " << width << "x" << height << std::endl;
}

void Engine::unloadCurrentMap() {
    std::cout << "[ENGINE] Unloading current map resources...\n";

    if (game) {
        game.reset();
    }

    if (lmRenderer) {
        lmRenderer->unloadWorld();
        lmRenderer.reset();
    }

    if (renderer) {
        renderer->unloadWorld();
        renderer.reset();
    }

    if (lightmapManager) {
        lightmapManager->cleanup();
        lightmapManager.reset();
    }

    if (skyboxRenderer) {
        skyboxRenderer->unload();
        skyboxRenderer.reset();
    }

    if (meshCollider) {
        meshCollider.reset();
    }

    if (bspLoader) {
        bspLoader->cleanupTextures();
        bspLoader.reset();
    }

    if (wadLoader) {
        wadLoader->cleanup();
        wadLoader->init();
    }

    for (auto* water : waterZones) {
        delete water;
    }
    waterZones.clear();
    cleanupDoors();
    useLightmapped = false;
    showLightmapsOnly = false;

    std::cout << "[ENGINE] Resources unloaded.\n";
}

void Engine::loadMap(const std::string& mapPath) {
    if (mapLoadInProgress) {
        std::cout << "[ENGINE] Load already in progress, ignoring request\n";
        return;
    }

    pendingMapPath = mapPath;
    pendingLoad = true;
    mapLoadInProgress = true;
}

void Engine::processPendingLoad() {
    if (!pendingLoad) return;

    pendingLoad = false;

    if (menu) {
        std::string mapName = pendingMapPath;
        size_t lastSlash = mapName.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            mapName = mapName.substr(lastSlash + 1);
        }
        size_t dotPos = mapName.find_last_of('.');
        if (dotPos != std::string::npos) {
            mapName = mapName.substr(0, dotPos);
        }
        menu->showLoading(mapName);
    }

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (menu && menu->isActive()) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        menu->render();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glfwSwapBuffers(window);
    glfwPollEvents();

    doLoadMap(pendingMapPath);

    mapLoadInProgress = false;
    pendingMapPath.clear();
}

void Engine::doLoadMap(const std::string& mapPath) {
    std::cout << "\n=== LOADING MAP: " << mapPath << " ===\n";

    if (wadLoader) {
        wadLoader->loadAllWADs();
    }

    bspLoader = std::make_unique<BSPLoader>();
    meshCollider = std::make_unique<MeshCollider>();
    lightmapManager = std::make_unique<LightmapManager>();
    renderer = std::make_unique<Renderer>();
    lmRenderer = std::make_unique<LightmappedRenderer>();
    game = std::make_unique<Game>();
    game->init();

    if (!bspLoader->load(mapPath, *wadLoader)) {
        std::cerr << "[ENGINE] Failed to load map: " << mapPath << std::endl;
        if (menu) {
            menu->hideLoading();
            showMenu();
        }
        return;
    }

    auto doorEntities = bspLoader->getEntitiesByClass("func_door");
    auto rotDoorEntities = bspLoader->getEntitiesByClass("func_door_rotating");

    std::vector<BSPEntity> allDoors = doorEntities;
    allDoors.insert(allDoors.end(), rotDoorEntities.begin(), rotDoorEntities.end());

    for (const auto& entity : allDoors) {
        if (entity.model.empty() || entity.model[0] != '*') continue;

        auto door = std::make_unique<DoorEntity>();
        door->initFromEntity(entity, *bspLoader);

        if (door->modelIndex > 0) {
            doors.push_back(std::move(door));
        }
    }

    std::cout << "[ENGINE] Loaded " << doors.size() << " doors" << std::endl;

    std::string skyName = bspLoader->getSkyName();
    if (!skyName.empty()) {
        skyboxRenderer = std::make_unique<SkyboxRenderer>();
        if (skyboxRenderer->loadSky(skyName, *wadLoader)) {
            std::cout << "[ENGINE] Skybox loaded: " << skyName << std::endl;
        }
        else {
            std::cout << "[ENGINE] Failed to load skybox: " << skyName << std::endl;
            skyboxRenderer.reset();
        }
    }

    if (lightmapManager->initializeFromBSP(*bspLoader)) {
        std::cout << "[LIGHT] Lightmaps initialized\n";
        lightmapManager->debugPrintLightmapInfo();
    }

    {
        const auto& allVertices = bspLoader->getMeshVertices();
        const auto& allIndices = bspLoader->getMeshIndices();
        const auto& drawCalls = bspLoader->getDrawCalls();

        std::vector<BSPVertex> collisionVertices;
        std::vector<unsigned int> collisionIndices;

        std::unordered_map<unsigned int, unsigned int> vertexMap;

        for (const auto& dc : drawCalls) {
            if (dc.isWater) continue;

            for (unsigned int i = 0; i < dc.indexCount; i++) {
                unsigned int oldIdx = allIndices[dc.indexOffset + i];

                auto it = vertexMap.find(oldIdx);
                if (it == vertexMap.end()) {
                    unsigned int newIdx = (unsigned int)collisionVertices.size();
                    vertexMap[oldIdx] = newIdx;
                    collisionVertices.push_back(allVertices[oldIdx]);
                    collisionIndices.push_back(newIdx);
                }
                else {
                    collisionIndices.push_back(it->second);
                }
            }
        }

        meshCollider->buildFromBSP(collisionVertices, collisionIndices);
        std::cout << "[COLLIDER] " << meshCollider->getTriangleCount() << " triangles (water excluded)\n";
    }

    if (renderer) {
        if (!renderer->init(width, height)) {
            std::cerr << "[ENGINE] Failed to init renderer\n";
        }
        renderer->loadWorld(*bspLoader);
    }

    if (lmRenderer && lightmapManager->hasLightmaps()) {
        if (lmRenderer->init(width, height)) {
            lmRenderer->setSkipSkyFaces(true);
            if (lmRenderer->loadWorld(*bspLoader, *lightmapManager)) {
                std::cout << "[LIGHT] Lightmapped renderer ready (F5 to toggle)\n";
                lmRenderer->setLightmapIntensity(DEFAULT_LIGHTMAP_INTENSITY);
                useLightmapped = true;
            }
        }
    }

    glm::vec3 spawnPos, spawnAngles;
    if (bspLoader->findPlayerStart(spawnPos, spawnAngles)) {
        if (game && game->getPlayer()) {
            game->getPlayer()->setPosition(spawnPos);
            game->setViewAngles(spawnAngles.y, spawnAngles.x);
        }
    }
    else {
        auto bounds = bspLoader->getWorldBounds();
        if (game && game->getPlayer()) {
            game->getPlayer()->setPosition(glm::vec3(0.0f, bounds.min.y + 100.0f, 0.0f));
        }
        std::cout << "[SPAWN] Using fallback position\n";
    }

    waterZones.clear();
    auto waterEntities = bspLoader->getEntitiesByClass("func_water");

    for (const auto& entity : waterEntities) {
        if (entity.model.empty() || entity.model[0] != '*') {
            std::cerr << "[WATER] Skipping func_water without valid model: " << entity.classname << std::endl;
            continue;
        }

        int modelIndex = 0;
        try {
            modelIndex = std::stoi(entity.model.substr(1));
        }
        catch (...) {
            std::cerr << "[WATER] Failed to parse model index from: " << entity.model << std::endl;
            continue;
        }

        if (modelIndex <= 0 || modelIndex >= (int)bspLoader->getModels().size()) {
            std::cerr << "[WATER] Invalid model index: " << modelIndex << std::endl;
            continue;
        }

        const auto& models = bspLoader->getModels();
        const BSPModel& model = models[modelIndex];

        AABB modelBounds;
        modelBounds.min = glm::vec3(-model.min[0], model.min[2], model.min[1]);
        modelBounds.max = glm::vec3(-model.max[0], model.max[2], model.max[1]);

        if (modelBounds.min.x > modelBounds.max.x) std::swap(modelBounds.min.x, modelBounds.max.x);
        if (modelBounds.min.y > modelBounds.max.y) std::swap(modelBounds.min.y, modelBounds.max.y);
        if (modelBounds.min.z > modelBounds.max.z) std::swap(modelBounds.min.z, modelBounds.max.z);

        CFuncWater* water = new CFuncWater();
        water->initFromBounds(modelBounds);
        waterZones.push_back(water);
    }
    std::cout << "[WATER] Loaded " << waterZones.size() << " func_water zones\n";

    std::cout << "=== MAP LOADED ===\n\n";

    if (menu) {
        menu->hideLoading();
    }

    menuActive = false;
    if (menu) {
        menu->setActive(false);
    }

    if (game) {
        game->resetMouseState();
    }

    glfwSetCursorPos(window, width / 2.0, height / 2.0);
    glfwPollEvents();

    if (game) {
        game->centerMouseAt((float)width / 2.0f, (float)height / 2.0f);
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    doorColliderDirty = true;
    doorColliderUpdateTimer = 0.0f;
}

void Engine::updateTime() {
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    if (deltaTime > 0.05f) deltaTime = 0.05f;
}

void Engine::setShowLightmapsOnly(bool show) {
    showLightmapsOnly = show;
    if (lmRenderer) lmRenderer->setShowLightmapsOnly(show);
}

void Engine::setLightmapIntensity(float intensity) {
    lightmapIntensity = intensity;
    if (lmRenderer) lmRenderer->setLightmapIntensity(intensity);
}

void Engine::showMenu() {
    menuActive = true;
    if (menu) {
        menu->setActive(true);
        menu->setState(Menu::State::MAIN_MENU);
    }
    if (window) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Engine::applySettings(const SettingsData& settings) {
    if (game) {
        game->setMouseSensitivity(settings.mouseSensitivity);
    }

    width = settings.screenWidth;
    height = settings.screenHeight;

    std::cout << "[ENGINE] Applied settings: sensitivity=" << settings.mouseSensitivity << std::endl;
}

void Engine::hideMenu() {
    menuActive = false;
    if (menu) {
        menu->setActive(false);
    }
    if (window && game) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPos(window, width / 2.0, height / 2.0);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        game->setLastMousePos((float)xpos, (float)ypos);
    }
}

void Engine::run() {
    while (!glfwWindowShouldClose(window)) {
        updateTime();

        processPendingLoad();

        if (menuActive && menu) {
            if (menu->shouldReturnToMenu()) {
                menu->clearReturnToMenuFlag();
                unloadCurrentMap();
                showMenu();
                continue;
            }

            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            menu->handleMouseMove(xpos, ypos);

            menu->update(deltaTime);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            glClearColor(0.05f, 0.05f, 0.1f, 0.5f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            menu->render();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        if (!game) {
            showMenu();
            continue;
        }

        if (game) {
            game->update(deltaTime);
            game->processInput(window);
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !menuActive) {
            menuActive = true;
            if (menu) {
                menu->setActive(true);
                menu->setState(Menu::State::PAUSE);
            }
            if (window) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Engine::setLightingEnabled(bool enabled) {
    if (lmRenderer) {
        lmRenderer->setUseLighting(enabled);
        useLightmapped = enabled;
    }
}

void Engine::toggleLightmappedRenderer() {
    useLightmapped = !useLightmapped;
    if (lmRenderer) {
        lmRenderer->setUseLighting(useLightmapped);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    std::cout << "Lightmapped renderer: " << (useLightmapped ? "ON (with lighting)" : "OFF (no lighting)") << std::endl;
}

void Engine::render() {
    if (!game || !game->getPlayer()) {
        glClearColor(0.1f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    glm::mat4 view;
    game->getPlayer()->getViewMatrix(glm::value_ptr(view));
    glm::vec3 eyePos = game->getPlayer()->getEyePosition();

    glm::mat4 projection = glm::perspective(glm::radians(75.0f),
        (float)width / (float)height, 0.1f, 10000.0f);

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (skyboxRenderer && skyboxRenderer->isLoaded()) {
        skyboxRenderer->render(view, projection, eyePos);
    }

    glClear(GL_DEPTH_BUFFER_BIT);

    if (lmRenderer && lightmapManager && lightmapManager->hasLightmaps()) {
        lmRenderer->renderWorld(view, eyePos, *bspLoader, glm::vec3(0.05f));
        lmRenderer->renderDoors(doors, view, projection);
    }

    if (game) {
        game->render(width, height);
    }
}

void Engine::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (menu) menu.reset();
    if (wadLoader) wadLoader.reset();

    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

// ============================================================================
// === ОПТИМИЗИРОВАННЫЕ МЕТОДЫ ДЛЯ ДВЕРЕЙ ===
// ============================================================================

void Engine::updateDoors(float deltaTime) {
    bool anyMoving = false;
    bool anyStateChanged = false;

    for (auto& door : doors) {
        DoorState oldState = door->state;
        door->update(deltaTime, nullptr);

        if (door->isMoving()) anyMoving = true;
        if (oldState != door->state) anyStateChanged = true;
    }

    checkPlayerTouchDoors();

    // ОПТИМИЗАЦИЯ: Обновляем коллайдер реже
    if (anyMoving) {
        doorColliderUpdateTimer += deltaTime;
        if (doorColliderUpdateTimer >= DOOR_COLLIDER_UPDATE_INTERVAL) {
            doorColliderUpdateTimer = 0.0f;
            updateDoorCollider();
        }
    }
    else if (anyStateChanged) {
        updateDoorCollider();
    }
}

// === НОВЫЙ МЕТОД: Проверка касания дверей игроком ===
void Engine::checkPlayerTouchDoors() {
    if (!game || !game->getPlayer()) return;

    Player* player = game->getPlayer();
    glm::vec3 playerPos = player->getPosition();
    Capsule playerCapsule = player->getPlayerCapsule(playerPos);

    for (auto& door : doors) {
        // Пропускаем проходимые двери
        if (door->passable) continue;

        // Пропускаем двери, которые уже движутся
        if (door->isMoving()) continue;

        // Для дверей с useOnly - только через клавишу E
        if (door->useOnly) continue;

        // Проверяем касание
        if (door->intersectsPlayer(playerPos, playerCapsule)) {
            // Дверь закрыта или закрывается - открываем (с передачей позиции игрока!)
            if (door->state == DoorState::Closed || door->state == DoorState::Closing) {
                std::cout << "[DOOR] Player touched door *" << door->modelIndex << ", opening" << std::endl;
                door->open(playerPos);  // === Передаём позицию игрока ===
            }
            // Дверь открыта и не ждёт автозакрытия - закрываем вручную
            else if (door->state == DoorState::Open && door->noAutoReturn) {
                std::cout << "[DOOR] Player touched door *" << door->modelIndex << ", closing (no auto-return)" << std::endl;
                door->close();
            }
        }
    }
}

// === НОВЫЙ МЕТОД: Использование дверей по клавише E ===
void Engine::useDoors() {
    if (!game || !game->getPlayer()) return;

    Player* player = game->getPlayer();
    glm::vec3 playerPos = player->getPosition();
    Capsule playerCapsule = player->getPlayerCapsule(playerPos);

    // Ищем ближайшую дверь, к которой можно применить use
    DoorEntity* nearestDoor = nullptr;
    float nearestDist = 999999.0f;

    for (auto& door : doors) {
        // Пропускаем проходимые
        if (door->passable) continue;

        // Проверяем расстояние до двери
        AABB doorBounds = door->getCurrentBounds();
        glm::vec3 doorCenter = (doorBounds.min + doorBounds.max) * 0.5f;
        float dist = glm::distance(playerPos, doorCenter);

        // Дверь должна быть достаточно близко (64 units - типичная дистанция use в HL)
        if (dist > 64.0f) continue;

        // Проверяем пересечение
        if (door->intersectsPlayer(playerPos, playerCapsule)) {
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestDoor = door.get();
            }
        }
    }

    if (nearestDoor) {
        std::cout << "[DOOR] Player used door *" << nearestDoor->modelIndex << std::endl;
        // === КЛЮЧЕВОЕ: Передаём позицию игрока ===
        nearestDoor->activate(playerPos);
    }
}

void Engine::updateDoorCollider() {
    if (!meshCollider || doors.empty()) return;

    std::vector<Triangle> doorTriangles;

    size_t estimatedTris = 0;
    for (const auto& door : doors) {
        if (!door->passable && door->vao != 0) {
            estimatedTris += door->indices.size() / 3;
        }
    }
    doorTriangles.reserve(estimatedTris);

    for (const auto& door : doors) {
        if (door->passable) continue;
        if (door->vao == 0 || door->indices.empty()) continue;

        glm::mat4 transform = door->getTransform();
        const auto& verts = door->vertices;
        const auto& idxs = door->indices;

        for (size_t i = 0; i < idxs.size(); i += 3) {
            Triangle tri;

            glm::vec4 v0 = transform * glm::vec4(verts[idxs[i]].position, 1.0f);
            glm::vec4 v1 = transform * glm::vec4(verts[idxs[i + 1]].position, 1.0f);
            glm::vec4 v2 = transform * glm::vec4(verts[idxs[i + 2]].position, 1.0f);

            tri.vertices[0] = glm::vec3(v0);
            tri.vertices[1] = glm::vec3(v1);
            tri.vertices[2] = glm::vec3(v2);

            tri.normal = glm::normalize(glm::cross(
                tri.vertices[1] - tri.vertices[0],
                tri.vertices[2] - tri.vertices[0]
            ));

            doorTriangles.push_back(tri);
        }
    }

    meshCollider->updateDynamicTriangles(std::move(doorTriangles));

    static int updateCount = 0;
    if (++updateCount < 5 || updateCount % 60 == 0) {
        std::cout << "[DOOR] Collider updated: " << doorTriangles.size()
            << " triangles from " << doors.size() << " doors" << std::endl;
    }
}

bool Engine::checkDoorCollision(const Capsule& capsule, glm::vec3& outPush) const {
    outPush = glm::vec3(0.0f);
    bool hit = false;

    for (const auto& door : doors) {
        if (door->passable) continue;

        AABB doorBounds = door->getCurrentBounds();

        AABB expandedBounds = doorBounds;
        expandedBounds.min -= glm::vec3(capsule.radius);
        expandedBounds.max += glm::vec3(capsule.radius);

        glm::vec3 capsuleCenter = capsule.getCenter();

        if (!expandedBounds.contains(capsuleCenter)) {
            glm::vec3 closest = glm::clamp(capsuleCenter, expandedBounds.min, expandedBounds.max);
            float distSq = glm::length(capsuleCenter - closest);
            distSq = distSq * distSq;
            if (distSq > capsule.radius * capsule.radius) {
                continue;
            }
        }

        glm::vec3 closestOnAABB = glm::clamp(capsuleCenter, doorBounds.min, doorBounds.max);
        glm::vec3 toCapsule = capsuleCenter - closestOnAABB;
        float dist = glm::length(toCapsule);

        if (dist < capsule.radius) {
            glm::vec3 pushDir = (dist > 0.001f) ? (toCapsule / dist) : glm::vec3(0.0f, 1.0f, 0.0f);
            float pushAmount = capsule.radius - dist;

            outPush += pushDir * pushAmount;
            hit = true;
        }
    }

    return hit;
}

bool Engine::checkDoorCollisionSimple(const Capsule& capsule) const {
    glm::vec3 push;
    return checkDoorCollision(capsule, push);
}

void Engine::renderDoors(const glm::mat4& view, const glm::mat4& projection) {
    if (doors.empty()) return;

    if (lmRenderer && useLightmapped) {
        lmRenderer->renderDoors(doors, view, projection);
    }
    else if (renderer) {
        // Fallback - можно добавить в Renderer если нужно
    }
}

void Engine::cleanupDoors() {
    doors.clear();
    doorColliderDirty = true;
}