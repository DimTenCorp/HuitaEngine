#pragma once
#include "LightSystem.h"
#include <glm/glm.hpp>
#include <vector>

// Quake-style lighting system integration
// Adapted from kabluk/kuzbass/Quake/gl_rlight.c

namespace QuakeLight {

    // Light style patterns (from Quake)
    constexpr const char* LIGHT_STYLE_NORMAL = "m";
    constexpr const char* LIGHT_STYLE_FLICKER1 = "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba";
    constexpr const char* LIGHT_STYLE_FLICKER2 = "mmnnooppqqrrssttuuvvwwxxyyzzoonnmmppqqrrssttuuvvwwxxyyzz";
    constexpr const char* LIGHT_STYLE_PULSE_FAST = "klmnklmnklmnklmnklmnklmnklmnklmnklmnklmnklmnklmnklmn";
    constexpr const char* LIGHT_STYLE_PULSE_SLOW = "abcdefghijklmabcdefghijklmabcdefghijklmabcdefghijklm";
    constexpr const char* LIGHT_STYLE_STROBE = "aazAaazAaazAaazAaazAaazAaazAaazAaazAaazAaazAaazAaazA";
    
    // Dynamic light flags
    enum class DLightFlags {
        None = 0,
        Rocket = 1,           // Rocket trail light
        Explosion = 2,        // Explosion light
        Powerup = 4,          // Powerup glow
        Flashblend = 8        // Use flashblend rendering
    };
    
    inline DLightFlags operator|(DLightFlags a, DLightFlags b) {
        return static_cast<DLightFlags>(static_cast<int>(a) | static_cast<int>(b));
    }
    
    // Extended dynamic light with Quake features
    struct ExtendedDLight : public DLight {
        DLightFlags flags = DLightFlags::None;
        float originalRadius = 0.0f;
        
        void setAsRocket(const glm::vec3& pos, float radius, const glm::vec3& color) {
            origin = pos;
            radius = radius;
            originalRadius = radius;
            this->color = color;
            decay = radius * 5.0f;  // Decay over ~0.2 seconds
            minlight = 10.0f;
            flags = DLightFlags::Rocket;
        }
        
        void setAsExplosion(const glm::vec3& pos, float radius, const glm::vec3& color) {
            origin = pos;
            this->radius = radius;
            originalRadius = radius;
            this->color = color;
            decay = radius * 3.0f;  // Decay over ~0.33 seconds
            minlight = 20.0f;
            flags = DLightFlags::Explosion;
        }
        
        void setAsPowerup(const glm::vec3& pos, float radius, const glm::vec3& color, bool looping = false) {
            origin = pos;
            this->radius = radius;
            originalRadius = radius;
            this->color = color;
            decay = looping ? 0.0f : radius * 2.0f;
            minlight = 5.0f;
            flags = DLightFlags::Powerup;
        }
    };
    
    // Light blend for screen effects (gl_flashblend)
    struct LightBlend {
        float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
        
        void add(float red, float green, float blue, float alpha) {
            float newAlpha = a + alpha * (1.0f - a);
            if (newAlpha <= 0.0f) return;
            
            float scale = alpha / newAlpha;
            r = r * (1.0f - scale) + red * scale;
            g = g * (1.0f - scale) + green * scale;
            b = b * (1.0f - scale) + blue * scale;
            a = newAlpha;
        }
        
        void clear() {
            r = g = b = a = 0.0f;
        }
    };
    
    // Lighting configuration (mirrors Quake cvars)
    struct LightingConfig {
        int flatLightStyles = 0;          // 0=animated, 1=average, 2=peak
        bool coloredPowerupGlow = false;  // Enable colored powerup effects
        float rocketLightIntensity = 1.0f;
        float explosionLightIntensity = 1.0f;
        float cshiftPercent = 100.0f;     // Color shift percentage
        bool flashblend = false;          // Use fast flashblend rendering
        
        // Apply config to LightSystem
        void applyTo(LightSystem& system) const {
            system.setFlatLightStyles(flatLightStyles);
            system.setColoredPowerupGlow(coloredPowerupGlow);
            system.setRocketLightIntensity(rocketLightIntensity);
            system.setExplosionLightIntensity(explosionLightIntensity);
            system.setCShiftPercent(cshiftPercent);
        }
    };
    
    // Helper functions
    inline glm::vec3 parseLightColor(const std::string& lightStr, int& intensity) {
        // Parse Quake light string format: "r g b intensity" or "intensity"
        int r = 255, g = 255, b = 255, i = 200;
        
        int count = sscanf(lightStr.c_str(), "%d %d %d %d", &r, &g, &b, &i);
        if (count == 1) {
            // Single value - use as intensity with white color
            i = r;
            r = g = b = 255;
        } else if (count < 4) {
            // Default intensity if not specified
            i = 200;
        }
        
        intensity = i;
        return glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f);
    }
    
    inline float calculateLightDistance(int intensity) {
        // Quake formula: distance = intensity * 0.025 (approximately)
        return intensity * 0.025f;
    }
    
    inline int getLightStyleValue(char c) {
        // Convert light style character to brightness value
        // 'a' = 0, 'm' = 286, 'z' = 550
        return (c - 'a') * 22;
    }
    
} // namespace QuakeLight
