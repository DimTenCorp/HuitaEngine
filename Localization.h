#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class Localization {
public:
    static Localization& getInstance();
    
    void loadLanguage(const std::string& filePath);
    std::string get(const std::string& key) const;
    std::string getOrFallback(const std::string& key, const std::string& fallback) const;
    bool isLoaded() const { return !translations.empty(); }
    void clear();

private:
    Localization() = default;
    ~Localization() = default;
    Localization(const Localization&) = delete;
    Localization& operator=(const Localization&) = delete;
    
    std::unordered_map<std::string, std::string> translations;
};
