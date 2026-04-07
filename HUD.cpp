#include "HUD.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <cstdio>

HUD::HUD() {
}

HUD::~HUD() {
}

void HUD::update(float deltaTime, const glm::vec3& position) {
    playerPos = position;

    frameCount++;
    fpsTimer += deltaTime;

    if (fpsTimer >= 0.2f) {
        currentFPS = frameCount / fpsTimer;
        frameCount = 0;
        fpsTimer = 0.0f;
    }
}

void HUD::render(int screenWidth, int screenHeight) {
    // Проверка что ImGui инициализирован перед использованием
    if (!ImGui::GetCurrentContext()) {
        std::cerr << "[HUD] ImGui not initialized!\n";
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // === FPS и Позиция (левый верхний угол) ===
    ImGuiWindowFlags infoFlags =
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::Begin("Info", nullptr, infoFlags);

    if (settings.showFPS) {
        ImVec4 fpsColor;
        if (currentFPS >= 60.0f) fpsColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        else if (currentFPS >= 30.0f) fpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        else fpsColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

        ImGui::TextColored(fpsColor, "FPS: %.0f", currentFPS);
    }

    if (settings.showPosition) {
        ImGui::Text("Position: (%.2f, %.2f, %.2f)",
            playerPos.x, playerPos.y, playerPos.z);
    }

    ImGui::End();

    // === Прицел (центр экрана) ===
    if (settings.showCrosshair) {
        ImGuiWindowFlags crossFlags =
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        float totalSize = (settings.crosshair.gap + settings.crosshair.length) * 2.0f + 10.0f;
        ImVec2 center(screenWidth / 2.0f - totalSize / 2.0f,
            screenHeight / 2.0f - totalSize / 2.0f);

        ImGui::SetNextWindowPos(center);
        ImGui::SetNextWindowSize(ImVec2(totalSize, totalSize));
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::Begin("Crosshair", nullptr, crossFlags);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        float cx = winPos.x + totalSize / 2.0f;
        float cy = winPos.y + totalSize / 2.0f;

        const CrosshairStyle& ch = settings.crosshair;
        ImU32 col = (ImU32)ch.color;

        // Вертикальная линия (две части)
        draw->AddLine(
            ImVec2(cx, cy - ch.gap - ch.length),
            ImVec2(cx, cy - ch.gap),
            col, ch.width
        );
        draw->AddLine(
            ImVec2(cx, cy + ch.gap),
            ImVec2(cx, cy + ch.gap + ch.length),
            col, ch.width
        );

        // Горизонтальная линия (две части)
        draw->AddLine(
            ImVec2(cx - ch.gap - ch.length, cy),
            ImVec2(cx - ch.gap, cy),
            col, ch.width
        );
        draw->AddLine(
            ImVec2(cx + ch.gap, cy),
            ImVec2(cx + ch.gap + ch.length, cy),
            col, ch.width
        );

        // Точка в центре (если размер > 0)
        if (ch.dotSize > 0.0f) {
            draw->AddCircleFilled(ImVec2(cx, cy), ch.dotSize, col);
        }

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}