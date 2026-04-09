# Quake Lighting System Implementation

## Overview

This implementation adapts the lighting system from [kabluk/kuzbass/Quake](https://github.com/dimashaa10/kabluk/tree/kuzbass/Quake), specifically from `gl_rlight.c`. The system provides:

- **Light Styles**: Animated light patterns (flickering, pulsing, strobe effects)
- **Dynamic Lights**: Temporary lights from rockets, explosions, powerups
- **Light Grid**: Fast 3D grid-based lighting sampling (BSPX format)
- **Legacy Lightmap Sampling**: Fallback for maps without lightgrid
- **Flashblend Rendering**: Fast screen-space dynamic light blending

## Files Created

### Core System
- `LightSystem.h` - Main lighting system header with data structures
- `LightSystem.cpp` - Implementation of lighting functions
- `QuakeLighting.h` - Quake-specific helpers and configuration
- `QuakeLighting.cpp` - Helper implementations

## Key Features

### 1. Light Styles

Light styles allow animated lighting patterns. Each style has a pattern string where characters represent brightness levels:
- `'a'` = no light (0)
- `'m'` = normal light (286)
- `'z'` = double bright (550)

```cpp
// Set a flickering light style
g_lightSystem.setLightStyle(0, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

// Animate all light styles (call each frame)
g_lightSystem.animateLightStyles(currentTime);

// Get current brightness value for a style
const auto& values = g_lightSystem.getLightStyleValues();
int brightness = values[styleIndex]; // 0-550 range
```

**Predefined Patterns:**
- `LIGHT_STYLE_NORMAL` - Steady light ("m")
- `LIGHT_STYLE_FLICKER1` - Smooth flicker
- `LIGHT_STYLE_FLICKER2` - Random flicker
- `LIGHT_STYLE_PULSE_FAST` - Fast pulse
- `LIGHT_STYLE_PULSE_SLOW` - Slow pulse
- `LIGHT_STYLE_STROBE` - Strobe effect

### 2. Dynamic Lights

Temporary lights that decay over time:

```cpp
// Allocate a dynamic light
DLight* dl = g_lightSystem.allocDLight(entityKey);
if (dl) {
    dl->origin = explosionPosition;
    dl->radius = 250.0f;
    dl->color = glm::vec3(1.0f, 0.5f, 0.0f);  // Orange
    dl->decay = 100.0f;  // Units per second
    dl->die = currentTime + 0.5;  // Expire after 0.5 seconds
}

// Update all dynamic lights
g_lightSystem.updateDLights(currentTime, deltaTime);
```

**Extended DLight with Quake features:**

```cpp
#include "QuakeLighting.h"

QuakeLight::ExtendedDLight rocketLight;
rocketLight.setAsRocket(position, 200.0f, glm::vec3(1.0f, 0.6f, 0.2f));

QuakeLight::ExtendedDLight powerupLight;
powerupLight.setAsPowerup(position, 300.0f, glm::vec3(0.2f, 0.5f, 1.0f), true);
```

### 3. Light Grid Loading

Load BSPX lightgrid for fast lighting queries:

```cpp
// Load from BSP file's BSPLIGHTGRID lump
void BSPLoader::loadLightGrid() {
    const uint8_t* data = getLumpData(LUMP_BSPX_LIGHTGRID);
    size_t size = getLumpSize(LUMP_BSPX_LIGHTGRID);
    g_lightSystem.loadLightGrid(data, size);
}

// Sample light at point (automatically uses lightgrid or falls back to lightmaps)
glm::vec3 lightColor = g_lightSystem.sampleLightAtPoint(playerPosition);
```

### 4. Configuration

```cpp
#include "QuakeLighting.h"

QuakeLight::LightingConfig config;
config.flatLightStyles = 0;           // 0=animated, 1=average, 2=peak
config.coloredPowerupGlow = true;     // Enable colored effects
config.rocketLightIntensity = 1.0f;   // Rocket light multiplier
config.explosionLightIntensity = 1.0f; // Explosion light multiplier
config.cshiftPercent = 100.0f;        // Color shift percentage
config.flashblend = false;            // Use fast flashblend rendering

config.applyTo(g_lightSystem);
```

## Integration with Existing Code

### Update BSPLoader.h

Add light system integration:

```cpp
#include "LightSystem.h"
#include "QuakeLighting.h"

class BSPLoader {
public:
    void setupLighting(LightManager& lightManager) {
        // Extract static lights from entities
        auto lights = extractLights();
        for (const auto& light : lights) {
            lightManager.addLight(light);
        }
        
        // Setup light environment
        setupLightEnvironment(lightManager);
        
        // Load lightgrid if available
        g_lightSystem.setWorldLightData(lightmapData.data(), lightmapData.size());
    }
};
```

### Update Renderer

Add lighting pass:

```cpp
void Renderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos, ShadowSystem* shadowSystem) {
    // Animate light styles
    extern float cl_time;  // Current game time
    g_lightSystem.animateLightStyles(cl_time);
    
    // Update dynamic lights
    g_lightSystem.updateDLights(cl_time, frameDelta);
    
    // Sample ambient light at camera position
    glm::vec3 ambientLight = g_lightSystem.sampleLightAtPoint(viewPos);
    
    // Pass to shader
    lightingShader->setUniform("ambientLight", ambientLight);
    
    // ... rest of rendering
}
```

### Parse Light Entities

Update `Light::fromBSPEntity()` to use Quake parsing:

```cpp
#include "QuakeLighting.h"

Light Light::fromBSPEntity(const BSPEntity& entity) {
    Light light;
    
    // Parse _light property using Quake format
    int intensity = 200;
    auto it = entity.properties.find("_light");
    if (it != entity.properties.end()) {
        glm::vec3 color = QuakeLight::parseLightColor(it->second, intensity);
        light.setColor(color);
        light.setIntensity(intensity / 200.0f);
        
        // Calculate radius from intensity
        float radius = QuakeLight::calculateLightDistance(intensity);
        light.setRadius(radius);
    }
    
    // ... rest of entity parsing
    return light;
}
```

## Data Structures

### LightStyle
```cpp
struct LightStyle {
    int length;                    // Pattern string length
    std::string map;               // Pattern (e.g., "abcdef...")
    char average;                  // Average brightness character
    char peak;                     // Peak brightness character
};
```

### DLight
```cpp
struct DLight {
    glm::vec3 origin;              // Light position
    float radius;                  // Current radius
    float spawn;                   // Spawn time (demo playback)
    double die;                    // Expiration time
    float decay;                   // Decay rate (units/sec)
    float minlight;                // Minimum contributing light
    int key;                       // Entity key for sync
    glm::vec3 color;               // RGB color
};
```

### LightGrid
```cpp
struct LightGrid {
    glm::vec3 gridScale;           // Scale factors (1/step)
    unsigned int count[3];         // Grid dimensions
    glm::vec3 mins;                // Minimum bounds
    unsigned int numStyles;        // Number of light styles
    
    unsigned int rootNode;         // Octree root index
    std::vector<LightGridNode> nodes;
    std::vector<LightGridLeaf> leafs;
};
```

## Constants

```cpp
constexpr int MAX_LIGHTSTYLES = 1024;   // Max animated light patterns
constexpr int MAX_DLIGHTS = 64;         // Max simultaneous dynamic lights
constexpr int MAXLIGHTMAPS = 4;         // Lightmaps per surface
constexpr unsigned int INVALID_LIGHTSTYLE = 0xFFFFu;
```

## Performance Notes

1. **Light Grid vs Lightmaps**: Light grid provides O(log n) sampling vs O(n) ray tracing for lightmaps
2. **Flashblend Mode**: When enabled (`flashblend=true`), dynamic lights render as screen-space blends instead of affecting geometry
3. **Flat Light Styles**: Setting `flatLightStyles=1` or `2` disables animation for better performance

## References

- Original source: https://github.com/dimashaa10/kabluk/tree/kuzbass/Quake/gl_rlight.c
- QuakeSpasm lighting: https://github.com/sezero/quakespasm
- LordHavoc's lighting improvements: https://github.com/DarkPlacesEngine/darkplaces
