#include "pch.h"
#include "Localization.h"
#include <fstream>
#include <sstream>
#include <iostream>

Localization& Localization::getInstance() {
    static Localization instance;
    return instance;
}

void Localization::loadLanguage(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[LOCALIZATION] Failed to open language file: " << filePath << std::endl;
        return;
    }

    translations.clear();
    std::string line;
    int loadedCount = 0;

    while (std::getline(file, line)) {
        // Пропускаем пустые строки и комментарии
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            // Но строки начинающиеся с #dtc_ - это наши ключи
            if (line.rfind("#dtc_", 0) != 0) {
                continue;
            }
        }

        // Формат: #key "value"
        size_t firstQuote = line.find('"');
        size_t lastQuote = line.rfind('"');
        
        if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote != lastQuote) {
            std::string key = line.substr(0, firstQuote);
            // Trim пробелов в конце ключа
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
                key.pop_back();
            }
            
            std::string value = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
            
            if (!key.empty()) {
                translations[key] = value;
                loadedCount++;
            }
        }
    }

    file.close();
    std::cout << "[LOCALIZATION] Loaded " << loadedCount << " translations from " << filePath << std::endl;
}

std::string Localization::get(const std::string& key) const {
    auto it = translations.find(key);
    if (it != translations.end()) {
        return it->second;
    }
    return key; // Возвращаем ключ если перевод не найден
}

std::string Localization::getOrFallback(const std::string& key, const std::string& fallback) const {
    auto it = translations.find(key);
    if (it != translations.end()) {
        return it->second;
    }
    return fallback;
}

void Localization::clear() {
    translations.clear();
}
