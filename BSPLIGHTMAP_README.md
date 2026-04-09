# BSPLightmap - Адаптация системы стандартных BSP лайтмапов

Этот код адаптирован из репозитория [BSPRenderer](https://github.com/tmp64/BSPRenderer/tree/master/src/renderer/src) 
и реализует систему стандартных BSP лайтмапов для GoldSrc/Half-Life движков.

## Основные возможности

- **Упаковка в атлас**: Все поверхности карты упаковываются в одну текстуру-атлас с использованием алгоритма stb_rect_pack
- **Поддержка 4 световых стилей (lightstyles)**: Для анимированного освещения (мерцающий свет, мигалки и т.д.)
- **Gamma коррекция**: Значение gamma = 2.5 для корректного отображения
- **Padding**: Автоматическое добавление отступов для предотвращения артефактов фильтрации

## Файлы

### BSPLightmap.h / BSPLightmap.cpp
Основной класс системы лайтмапов.

### Зависимости
- `stb_rect_pack.h` - библиотека для упаковки прямоугольников (уже включена в .cpp файл)
- `BSPLoader.h` - загрузчик BSP файлов (нужен getter `getLightmapData()`)
- OpenGL с поддержкой текстур-массивов (`GL_TEXTURE_2D_ARRAY`)

## Интеграция в движок

### 1. Добавьте getter в BSPLoader.h

```cpp
// В private секции BSPLoader:
const std::vector<unsigned char>& getLightmapData() const { return lightmapData; }
```

### 2. Создайте экземпляр BSPLightmap в Renderer

```cpp
// В Renderer.h:
#include "BSPLightmap.h"

class Renderer {
private:
    std::unique_ptr<BSPLightmap> bspLightmap;
    // ...
};
```

### 3. Инициализируйте после загрузки BSP

```cpp
// В Renderer.cpp после loadWorld():
bool Renderer::loadWorld(BSPLoader& bsp) {
    // ... загрузка меша ...
    
    // Создаём лайтмапы
    bspLightmap = std::make_unique<BSPLightmap>();
    
    // Получаем доступ к граням через BSPLoader (нужно добавить getter)
    // Или передайте данные напрямую
    bspLightmap->buildFromBSP(bsp, bsp.getMeshVertices(), /* faces */);
    
    return true;
}
```

### 4. Используйте в шейдере

Пример фрагментного шейдера для освещения от лайтмапа:

```glsl
#version 330 core

in vec3 TexCoord;
in vec3 LightmapUV;  // xyz: u, v, слой (lightstyle index)

uniform sampler2DArray lightmapTexture;
uniform float lightstyleScales[4];

out vec4 FragColor;

void main() {
    // Сэмплим все 4 световых стиля
    vec3 lighting = vec3(0.0);
    
    for (int i = 0; i < 4; i++) {
        vec3 sample = texture(lightmapTexture, vec3(LightmapUV.xy, i)).rgb;
        lighting += sample * lightstyleScales[i];
    }
    
    // Применяем gamma коррекцию
    lighting = pow(lighting, vec3(2.5));
    
    // Комбинируем с альбедо
    vec3 albedo = texture(albedoTexture, TexCoord).rgb;
    FragColor = vec4(albedo * lighting, 1.0);
}
```

### 5. Обновляйте lightstyle scales для анимации

```cpp
// В игровом цикле:
void updateLighting(float time) {
    // Пример: мерцание света
    float flicker = 0.8f + 0.2f * sin(time * 10.0f);
    renderer.getBspLightmap()->setLightstyleScale(1, flicker);
}
```

## Структура данных BSP лайтмапов

Формат данных в BSP файле (GoldSrc):

```
[offset]: width (1 байт)
[offset+1]: height (1 байт)
[offset+2]: RGB пиксели для style 0 (width * height * 3 байт)
[...]: RGB пиксели для style 1
[...]: RGB пиксели для style 2
[...]: RGB пиксели для style 3
```

Каждая грань имеет:
- `lightOffset`: смещение к данным лайтмапа (-1 если нет освещения)
- `styles[4]`: индексы световых стилей (255 = не используется)

## Отличия от оригинала

1. **Упрощённая интеграция**: Код адаптирован для использования в вашем движке
2. **Прямой доступ к данным**: Используется getter вместо внутренних структур SceneRenderer
3. **Самодостаточность**: Класс не зависит от других компонентов BSPRenderer

## Параметры настройки

В `BSPLightmap.h` можно изменить:

```cpp
static constexpr int LIGHTMAP_DIVISOR = 16;        // Делитель для UV координат
static constexpr int MAX_LIGHTMAP_BLOCK_SIZE = 2048;  // Максимальный размер атласа
static constexpr float LIGHTMAP_BLOCK_WASTED = 0.40f; // Запас места для упаковки
static constexpr int LIGHTMAP_PADDING = 2;         // Padding вокруг каждой грани
static constexpr float LIGHTMAP_GAMMA = 2.5f;      // Gamma коррекция
```

## Лицензия

Оригинальный код из BSPRenderer распространяется под лицензией репозитория tmp64/BSPRenderer.
Данный адапптированный код следует той же лицензии.
