#include "pch.h"
#include "Engine.h"
#include <iostream>

int main() {
    Engine engine;

    if (!engine.init()) {
        std::cerr << "Engine init failed\n";
        return -1;
    }

    engine.run();
    return 0;
}