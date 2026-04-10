# Система рендеринга воды (Half-Life 1 Style)

## Обзор

Эта реализация воссоздает систему рендеринга воды из движка Half-Life 1 (GoldSrc), включая:
- Warp-эффект для анимации поверхности воды
- Разбиение полигонов для корректного наложения текстур (GL_SubdivideSurface)
- Подводный эффект с цветовым фильтром и туманом
- Поддержка различных типов жидкости (вода, лава/слизь)

## Файлы

### Water.h / Water.cpp
Базовый класс управления водой:
- `WaterManager` - основной класс для управления водными эффектами
- `TurbSinTable` - таблица синусов для быстрого расчета warp-эффекта
- `WaterParams` - параметры жидкости (цвет, плотность тумана, скорость волн)
- `WaterPoly` - структура для subdivided полигонов воды
- `SurfaceType` - типы поверхностей (Water, Slime, Sky, Underwater)

### WaterRenderer.h / WaterRenderer.cpp
Класс рендеринга водных поверхностей:
- `WaterRenderer` - основной класс рендеринга
- `WaterSurface` - структура для OpenGL меша воды
- Шейдеры для warp-эффекта и underwater пост-обработки

## Ключевые особенности реализации

### 1. Warp-эффект (из GL_WARP.C)
```cpp
// Расчет смещения вершины
float warp1 = sin(time * 160.0 + vertex.x + vertex.y) * 8.0 + 8.0;
float warp2 = sin(time * 171.0 + vertex.x * 5.0 - vertex.y) * 8.0 + 8.0;
float warpOffset = (warp1 + warp2 * 0.8);
```

### 2. Разбиение полигонов (из GL_SubdivideSurface)
Полигоны разбиваются на части размером 64 единицы для корректного warp:
```cpp
void WaterManager::subdividePolygon(...) {
    // Рекурсивное разбиение по осям X, Y, Z
    // пока размер полигона > SUBDIVIDE_SIZE (64.0)
}
```

### 3. Подводный эффект (из D_SetFadeColor)
Цветовой фильтр и туман при нахождении камеры под водой:
```cpp
void WaterRenderer::renderUnderwaterEffect(...) {
    // Смешивание цвета сцены с цветом воды
    vec3 finalColor = mix(sceneColor.rgb, fogColor, fogFactor);
}
```

## Интеграция в движок

### Шаг 1: Добавление файлов
Добавьте эти файлы в ваш проект:
- `Water.h`, `Water.cpp`
- `WaterRenderer.h`, `WaterRenderer.cpp`

### Шаг 2: Инициализация
В вашем основном классе рендерера или игры:
```cpp
#include "WaterRenderer.h"

class MyGame {
    WaterRenderer waterRenderer;
    
    bool init() {
        if (!waterRenderer.init()) {
            return false;
        }
        return true;
    }
};
```

### Шаг 3: Загрузка из BSP
После загрузки карты:
```cpp
bool loadMap(const std::string& mapName) {
    BSPLoader bsp;
    WADLoader wad;
    
    if (!bsp.load(mapName, wad)) {
        return false;
    }
    
    // Загружаем водные поверхности
    waterRenderer.loadFromBSP(bsp, wad);
    
    return true;
}
```

### Шаг 4: Рендеринг
В игровом цикле:
```cpp
void render(float deltaTime, float totalTime) {
    // Обновляем состояние воды
    waterRenderer.update(deltaTime, totalTime);
    
    // Рендерим основную сцену...
    
    // Рендерим воду (после прозрачных объектов)
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    waterRenderer.render(viewMatrix, projectionMatrix, cameraPos, totalTime);
    
    // Если камера под водой - применяем эффект
    if (waterRenderer.isCameraUnderwater(cameraPos)) {
        waterRenderer.renderUnderwaterEffect(screenWidth, screenHeight);
    }
}
```

## Настройка параметров

### Изменение цвета воды
```cpp
WaterManager& wm = waterRenderer.getWaterManager();
wm.setUnderwaterColor(glm::vec3(0.2f, 0.4f, 0.6f), 0.5f);
```

### Определение водных текстур
Функция `isWaterTexture()` проверяет имена текстур:
- Содержат "water", "slime", "lava"
- Начинаются с '!' (анимированные текстуры HL1)

Для вашей системы может потребоваться модификация этой функции.

## Требования к BSP

Система ожидает следующие данные из BSP:
- Флаг SURF_DRAWTURB (0x04) в texInfo.flags
- Оригинальные вершины для каждой грани
- Текстурные координаты через vecs[S] и vecs[T]

## Отличия от оригинала

1. **Современный OpenGL**: Используется GLSL вместо фиксированного конвейера
2. **Vertex Buffer Objects**: Вершины хранятся в VBO вместо динамического расчета
3. **Post-processing**: Underwater эффект через fullscreen quad
4. **Упрощенная работа с текстурами**: Требуется интеграция с вашей системой текстур

## Расширение

### Добавление новых типов жидкости
```cpp
enum class SurfaceType {
    Water,
    Slime,
    Lava,      // Добавьте новый тип
    Sky
};

// В WaterManager::getWaterParams():
waterParams[static_cast<int>(SurfaceType::Lava)].color = glm::vec3(0.8f, 0.2f, 0.1f);
waterParams[static_cast<int>(SurfaceType::Lava)].waveSpeed = 2.0f;
```

### Кастомные шейдеры
Можно изменить шейдеры в `getWaterVert()` и `getWaterFrag()` для более сложных эффектов.

## Производительность

- **Subdivide**: Выполняется один раз при загрузке карты
- **Warp расчет**: Делается в вершинном шейдере (GPU)
- **Blend**: Требует осторожности с сортировкой прозрачных объектов

## Известные ограничения

1. Нет поддержки animated textures (нужна доработка BSPLoader)
2. Упрощенная проверка "камера под водой"
3. Нет отражений/преломлений (можно добавить через cube maps)

## Ссылки на оригинальный код

- [GL_WARP.C](https://github.com/ScriptedSnark/half-life1_win32_722/blob/master/engine/GL_WARP.C) - warp эффекты
- [water.h](https://github.com/ScriptedSnark/half-life1_win32_722/blob/master/engine/water.h) - заголовки воды
- [GL_RSURF.C](https://github.com/ScriptedSnark/half-life1_win32_722/blob/master/engine/GL_RSURF.C) - рендеринг поверхностей
