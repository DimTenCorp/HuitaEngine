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
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>

Engine* Engine::instance = nullptr;

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
}

Engine::Engine() {
    instance = this;
}

Engine::~Engine() {
    shutdown();
    instance = nullptr;
}

bool Engine::init() {
    if (!initGLFW()) return false;
    if (!initGLAD()) return false;
    if (!initImGui()) return false;
    if (!initSystems()) return false;

    std::cout << "\n=== ENGINE READY ===\n";
    return true;
}

bool Engine::initGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "HuitaEngine", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

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

    menu = std::make_unique<Menu>();
    menu->setFontPath("res/fonts/Neogurotesuku-Regular.otf");

    wadLoader->loadQuakePalette("palette.lmp");

    menu->init(window, width, height);

    menu->setOnMapSelected([this](const std::string& mapPath) {
        loadMap(mapPath);  // Теперь это планирует загрузку, а не выполняет сразу
        });

    menu->setOnExitGame([this]() {
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

    useLightmapped = false;
    showLightmapsOnly = false;

    std::cout << "[ENGINE] Resources unloaded.\n";
}

void Engine::loadMap(const std::string& mapPath) {
    // Просто планируем загрузку на следующий кадр
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

    // Показываем экран загрузки (рендерится на следующем кадре)
    if (menu) {
        // Извлекаем имя карты из пути
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

    // Даём возможность отрендерить экран загрузки
    // Просто вызываем рендер одного кадра прямо здесь
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (menu && menu->isActive()) {
        // Рендерим только меню в состоянии LOADING
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        menu->render();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glfwSwapBuffers(window);
    glfwPollEvents();

    // Теперь реальная загрузка
    doLoadMap(pendingMapPath);

    mapLoadInProgress = false;
    pendingMapPath.clear();
}

void Engine::doLoadMap(const std::string& mapPath) {
    std::cout << "\n=== LOADING MAP: " << mapPath << " ===\n";

    if (wadLoader) {
        wadLoader->loadAllWADs();
    }

    // Создаём новые объекты
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

    if (lightmapManager->initializeFromBSP(*bspLoader)) {
        std::cout << "[LIGHT] Lightmaps initialized\n";
        lightmapManager->debugPrintLightmapInfo();
    }

    meshCollider->buildFromBSP(bspLoader->getMeshVertices(), bspLoader->getMeshIndices());
    std::cout << "[COLLIDER] " << meshCollider->getTriangleCount() << " triangles\n";

    if (renderer) {
        if (!renderer->init(width, height)) {
            std::cerr << "[ENGINE] Failed to init renderer\n";
        }
        renderer->loadWorld(*bspLoader);
    }

    if (lmRenderer && lightmapManager->hasLightmaps()) {
        if (lmRenderer->init(width, height)) {
            if (lmRenderer->loadWorld(*bspLoader, *lightmapManager)) {
                std::cout << "[LIGHT] Lightmapped renderer ready (F5 to toggle)\n";
                lmRenderer->setLightmapIntensity(lightmapIntensity);
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

    std::cout << "=== MAP LOADED ===\n\n";

    // Скрываем экран загрузки
    if (menu) {
        menu->hideLoading();
    }

    // ====== ВАЖНО: настраиваем состояние мыши для игры ======
    menuActive = false;
    if (menu) {
        menu->setActive(false);
    }

    // Сбрасываем состояние мыши ДО того как включаем захват
    if (game) {
        game->resetMouseState();
    }

    // Устанавливаем курсор в центр экрана
        // Возвращаем мышку в центр для корректного старта
    glfwSetCursorPos(window, width / 2.0, height / 2.0);
    glfwPollEvents();  // Применяем позицию курсора сразу

    // Центрируем мышь в Game - пропускаем логику firstMouse
    if (game) {
        game->centerMouseAt(width / 2.0f, height / 2.0f);
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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

        // Обрабатываем отложенную загрузку в первую очередь
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
                menu->setState(Menu::State::CONFIRM_EXIT);
            }
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
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

    if (useLightmapped && lmRenderer && lightmapManager && lightmapManager->hasLightmaps()) {
        lmRenderer->beginFrame(glm::vec3(0.02f, 0.02f, 0.02f));
        lmRenderer->renderWorld(view, eyePos, *bspLoader, glm::vec3(0.05f));
    }
    else if (renderer && bspLoader) {
        renderer->beginFrame(glm::vec3(0.1f, 0.15f, 0.2f));
        renderer->renderWorld(view, eyePos);
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