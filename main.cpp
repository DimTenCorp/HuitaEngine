// main.cpp — ТОЛЬКО точка входа
#include "gl_includes.h"  // ← Первым!
#include "GameManager.h"
#include <iostream>

int main() {
    auto& game = GameManager::instance();

    if (!game.initialize()) {
        std::cerr << "Initialization failed!\n";
        return -1;
    }

    game.run();
    game.shutdown();

    return 0;
}