#include "pch.h"
#include "Menu.h"
#include "imgui.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

//чё 

namespace fs = std::filesystem;

static const float MENU_FONT_SIZE = 20.0f;

Menu::Menu() = default;
Menu::~Menu() = default;

void Menu::init(GLFWwindow* win, int w, int h) {
    if (initialized) {
        window = win;
        width = w;
        height = h;
        reset();
        return;
    }

    window = win;
    width = w;
    height = h;

    reset();

    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();
    io.FontDefault = nullptr;

    ImFont* customFont = nullptr;
    bool fontLoaded = false;

    if (!fontPath.empty() && fs::exists(fontPath)) {
        customFont = io.Fonts->AddFontFromFileTTF(
            fontPath.c_str(),
            MENU_FONT_SIZE,
            nullptr,
            io.Fonts->GetGlyphRangesCyrillic()
        );

        if (customFont) {
            std::cout << "[MENU] Loaded custom font: " << fontPath << std::endl;
            fontLoaded = true;
        }
        else {
            std::cerr << "[MENU] Failed to load custom font: " << fontPath << std::endl;
        }
    }

    if (!fontLoaded) {
        const char* systemFont = "C:\\Windows\\Fonts\\Arial.ttf";
        if (fs::exists(systemFont)) {
            customFont = io.Fonts->AddFontFromFileTTF(
                systemFont,
                MENU_FONT_SIZE,
                nullptr,
                io.Fonts->GetGlyphRangesCyrillic()
            );
            if (customFont) {
                std::cout << "[MENU] Loaded system Arial" << std::endl;
                fontLoaded = true;
            }
        }
    }

    if (!fontLoaded) {
        io.Fonts->AddFontDefault();
        std::cerr << "[MENU] Using default ImGui font (no Cyrillic support!)" << std::endl;
    }
    else {
        io.FontDefault = customFont;
    }

    io.Fonts->Build();

    std::cout << "[MENU] Fonts count: " << io.Fonts->Fonts.size() << std::endl;
    std::cout << "[MENU] Default font: " << (io.FontDefault ? "set" : "null") << std::endl;

    initialized = true;
}

void Menu::reset() {
    currentState = State::MAIN_MENU;
    selectedMapIndex = -1;
    lastClickTime = 0.0;
    lastClickedMap = -1;
    returnToMenu = false;
    mousePressed = false;
}

void Menu::setActive(bool act) {
    active = act;
    if (active) {
        reset();
    }
}

void Menu::scanMaps(const std::string& directory) {
    maps.clear();
    selectedMapIndex = -1;

    if (!fs::exists(directory)) {
        std::cerr << "[MENU] Maps directory not found: " << directory << std::endl;
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".bsp") {
                    maps.push_back(entry.path().filename().string());
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[MENU] Error scanning maps: " << e.what() << std::endl;
        return;
    }

    std::sort(maps.begin(), maps.end());
    std::cout << "[MENU] Found " << maps.size() << " maps" << std::endl;
}

void Menu::update(float deltaTime) {
    (void)deltaTime;
}

void Menu::handleMouseMove(double x, double y) {
    mouseX = x;
    mouseY = y;
}

void Menu::handleMouseButton(int button, int action, double x, double y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    mouseX = x;
    mouseY = y;
    mousePressed = (action == GLFW_PRESS);
}

void Menu::handleKey(int key, int action) {
    if (action != GLFW_PRESS) return;

    if (currentState == State::MAP_SELECT) {
        if (key == GLFW_KEY_ESCAPE) {
            currentState = State::MAIN_MENU;
            selectedMapIndex = -1;
        }
    }
    else if (currentState == State::CONFIRM_EXIT) {
        if (key == GLFW_KEY_ESCAPE) {
            if (onReturnToGame) onReturnToGame();
        }
    }
}

void Menu::handleScroll(double xoffset, double yoffset) {
    (void)xoffset;
    (void)yoffset;
}

void Menu::render() {
    if (!active) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    bool open = true;
    ImGui::Begin("Menu", &open,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        ImVec2(0, 0),
        ImVec2((float)width, (float)height),
        IM_COL32(0, 0, 0, 180)
    );

    switch (currentState) {
    case State::MAIN_MENU:
        renderMainMenu();
        break;
    case State::MAP_SELECT:
        renderMapSelect();
        break;
    case State::CONFIRM_EXIT:
        renderConfirmExit();
        break;
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void Menu::renderMainMenu() {
    float buttonX = 50.0f;
    float startY = height - 200.0f;
    float buttonHeight = 35.0f;
    float spacing = 8.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, spacing));

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 100, 100, 80));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(150, 150, 150, 100));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 200));

    ImGui::SetCursorPos(ImVec2(buttonX, startY));
    if (ImGui::Button(u8"НАЧАТЬ ИГРУ", ImVec2(0, buttonHeight))) {
        currentState = State::MAP_SELECT;
    }

    ImGui::SetCursorPos(ImVec2(buttonX, startY + buttonHeight + spacing));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 150, 150));
    ImGui::Button(u8"НАСТРОЙКИ", ImVec2(0, buttonHeight));
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(buttonX, startY + (buttonHeight + spacing) * 2));
    if (ImGui::Button(u8"ВЫХОД", ImVec2(0, buttonHeight))) {
        if (onExitGame) onExitGame();
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}

void Menu::renderMapSelect() {
    ImVec2 center = ImVec2(width * 0.5f, 100.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y));
    ImGui::Text(u8"ВЫБОР КАРТЫ");
    ImGui::PopStyleColor();

    float listX = width * 0.5f - 200.0f;
    float listY = 180.0f;
    float listWidth = 400.0f;
    float listHeight = height - 300.0f;
    float itemHeight = 30.0f;

    ImGui::SetCursorPos(ImVec2(listX, listY));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));

    ImGui::BeginChild("MapList", ImVec2(listWidth, listHeight), false);

    for (int i = 0; i < (int)maps.size(); i++) {
        bool isSelected = (i == selectedMapIndex);

        std::string displayName = maps[i];
        size_t dotPos = displayName.find_last_of('.');
        if (dotPos != std::string::npos) {
            displayName = displayName.substr(0, dotPos);
        }

        ImU32 headerColor, headerHoveredColor, textColor;
        if (isSelected) {
            headerColor = IM_COL32(200, 200, 200, 100);
            headerHoveredColor = IM_COL32(220, 220, 220, 150);
            textColor = IM_COL32(255, 255, 255, 255);
        }
        else {
            headerColor = IM_COL32(0, 0, 0, 0);
            headerHoveredColor = IM_COL32(100, 100, 100, 100);
            textColor = IM_COL32(180, 180, 180, 255);
        }

        ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        ImGui::SetCursorPosX(10);
        bool clicked = ImGui::Selectable(displayName.c_str(), isSelected,
            ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(listWidth - 20, itemHeight));

        if (clicked) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                if (onMapSelected) {
                    onMapSelected("maps/" + maps[i]);
                }
            }
            else {
                selectedMapIndex = i;
                lastClickTime = glfwGetTime();
                lastClickedMap = i;
            }
        }

        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    float buttonY = height - 80.0f;
    float buttonWidth = 150.0f;
    float buttonHeight = 40.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    ImGui::SetCursorPos(ImVec2(width * 0.5f - 160.0f, buttonY));
    if (selectedMapIndex >= 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 80, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 120, 120, 200));
        if (ImGui::Button(u8"ЗАГРУЗИТЬ", ImVec2(buttonWidth, buttonHeight))) {
            if (onMapSelected) {
                onMapSelected("maps/" + maps[selectedMapIndex]);
            }
        }
        ImGui::PopStyleColor(2);
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 50, 50, 150));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(50, 50, 50, 150));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(50, 50, 50, 150));
        ImGui::Button(u8"ЗАГРУЗИТЬ", ImVec2(buttonWidth, buttonHeight));
        ImGui::PopStyleColor(3);
    }

    ImGui::SetCursorPos(ImVec2(width * 0.5f + 10.0f, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 60, 60, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 100, 100, 200));
    if (ImGui::Button(u8"НАЗАД", ImVec2(buttonWidth, buttonHeight))) {
        currentState = State::MAIN_MENU;
        selectedMapIndex = -1;
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(width * 0.5f - 150, height - 30.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 120, 120, 255));
    ImGui::Text(u8"Двойной клик для быстрой загрузки");
    ImGui::PopStyleColor();
}

void Menu::renderConfirmExit() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(
        ImVec2(0, 0),
        ImVec2((float)width, (float)height),
        IM_COL32(0, 0, 0, 200)
    );

    float dialogWidth = 400.0f;
    float dialogHeight = 150.0f;
    float dialogX = (width - dialogWidth) * 0.5f;
    float dialogY = (height - dialogHeight) * 0.5f;

    drawList->AddRectFilled(
        ImVec2(dialogX, dialogY),
        ImVec2(dialogX + dialogWidth, dialogY + dialogHeight),
        IM_COL32(50, 50, 50, 255)
    );
    drawList->AddRect(
        ImVec2(dialogX, dialogY),
        ImVec2(dialogX + dialogWidth, dialogY + dialogHeight),
        IM_COL32(120, 120, 120, 255),
        0.0f, 0, 2.0f
    );

    ImGui::SetCursorPos(ImVec2(dialogX + 20, dialogY + 30));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::Text(u8"Выйти в главное меню?");
    ImGui::PopStyleColor();

    float buttonY = dialogY + 90;
    float buttonWidth = 120.0f;
    float buttonHeight = 35.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    ImGui::SetCursorPos(ImVec2(dialogX + 60, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 80, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 120, 120, 200));
    if (ImGui::Button(u8"ДА", ImVec2(buttonWidth, buttonHeight))) {
        returnToMenu = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(ImVec2(dialogX + dialogWidth - 60 - buttonWidth, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 80, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 120, 120, 200));
    if (ImGui::Button(u8"НЕТ", ImVec2(buttonWidth, buttonHeight))) {
        if (onReturnToGame) onReturnToGame();
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar();
}

void Menu::setOnMapSelected(std::function<void(const std::string&)> callback) {
    onMapSelected = callback;
}

void Menu::setOnExitGame(std::function<void()> callback) {
    onExitGame = callback;
}

void Menu::setOnReturnToGame(std::function<void()> callback) {
    onReturnToGame = callback;
}

void Menu::selectMap(int index) {
    if (index >= 0 && index < (int)maps.size()) {
        selectedMapIndex = index;
    }
}