#include "QuakeLighting.h"
#include <cstdio>

// Implementation file for Quake-style lighting helpers
// Most functionality is in header as inline functions

namespace QuakeLight {

    // Global lighting configuration instance
    static LightingConfig g_config;
    
    const LightingConfig& getDefaultConfig() {
        return g_config;
    }
    
    void setGlobalConfig(const LightingConfig& config) {
        g_config = config;
    }
    
    // Light style pattern lookup
    const char* getLightStylePattern(int styleIndex) {
        // Standard Quake light styles
        switch (styleIndex % 10) {
            case 0: return LIGHT_STYLE_NORMAL;
            case 1: return LIGHT_STYLE_FLICKER1;
            case 2: return LIGHT_STYLE_FLICKER2;
            case 3: return LIGHT_STYLE_PULSE_FAST;
            case 4: return LIGHT_STYLE_PULSE_SLOW;
            case 5: return LIGHT_STYLE_STROBE;
            default: return LIGHT_STYLE_NORMAL;
        }
    }
    
} // namespace QuakeLight
