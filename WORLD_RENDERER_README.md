# Адаптация рендерера мира из BSPRenderer

Этот документ описывает адаптированные компоненты рендеринга BSP карт из репозитория [BSPRenderer](https://github.com/tmp64/BSPRenderer) для движка HuitaEngine.

## Созданные файлы

### 1. WorldRenderer.h/cpp
Основной рендерер мира с поддержкой:
- **PVS (Potentially Visible Set)** - система определения видимости узлов BSP
- **Texture chaining** - группировка поверхностей по текстурам для батчинга
- **Front-to-back rendering** - обход BSP дерева для правильной сортировки
- **Skybox surfaces** - отдельная обработка неба
- **Lightstyles** - поддержка 4 стилей освещения для анимации

#### Ключевые структуры:
```cpp
struct RenderSurface {
    unsigned int faceIndex;       // Индекс грани
    unsigned int materialIdx;     // Индекс материала
    unsigned int vertexOffset;    // Смещение вершин
    unsigned int vertexCount;     // Количество вершин
    glm::vec3 origin;             // Центр поверхности
    unsigned int flags;           // Флаги (SURF_DRAWSKY и др.)
    glm::uvec2 lightmapCoord;     // Координаты в lightmap атласе
};

struct WorldSurfaceList {
    std::vector<std::vector<unsigned>> textureChain;  // Цепочки по текстурам
    std::vector<unsigned> textureChainFrames;         // Фреймы цепочек
    std::vector<unsigned> skySurfaces;                // Небесные поверхности
    unsigned visFrame;                                // Фрейм видимости
    int viewLeaf;                                     // Лист просмотра
    std::vector<unsigned> nodeVisFrame;               // Висфреймы узлов
    std::vector<unsigned> leafVisFrame;               // Висфреймы листьев
};
```

#### Основные методы:
- `getVisibleSurfaces(camera, surfList)` - сбор видимых поверхностей
- `drawTexturedWorld(surfList, shader)` - рендеринг текстурного мира
- `drawSkybox(surfList, shader)` - рендеринг неба
- `markLeaves(camera, surfList)` - отметка видимых листьев (PVS)
- `recursiveWorldNodesTextured(...)` - рекурсивный обход BSP дерева

### 2. BrushRenderer.h/cpp
Рендерер brush моделей (дверей, лифтов, вращающихся объектов):
- **Batching** - группировка поверхностей по материалам
- **Sorting** - сортировка для прозрачных объектов (back-to-front)
- **Frustum culling** - отсечение невидимых объектов
- **EBO dynamic update** - динамическое обновление индексного буфера

#### Основные методы:
- `drawBrushEntity(...)` - рендеринг без сортировки (непрозрачные)
- `drawSortedBrushEntity(...)` - рендеринг с сортировкой (прозрачные)
- `batchAddSurface(...)` - добавление поверхности в батч
- `batchFlush(...)` - отправка батча на рендеринг
- `getBrushTransform(...)` - матрица трансформации модели

## Интеграция с вашим движком

### Шаг 1: Добавьте getters в BSPLoader.h

Для работы рендереров нужно добавить методы доступа к данным BSP:

```cpp
// В public секции BSPLoader:
const std::vector<BSPPlane>& getPlanes() const { return planes; }
const std::vector<BSPFace>& getFaces() const { return faces; }
// Добавьте структуры BSPLeaf и методы getLeaves(), getVisData()
```

### Шаг 2: Определите структуру BSPLeaf

```cpp
struct BSPLeaf {
    int contents;
    int visOffset;
    // ... другие поля из BSP формата
};
```

### Шаг 3: Подключите рендереры в основной цикл

```cpp
// В main.cpp или игровом цикле:
WorldRenderer worldRenderer;
BrushRenderer brushRenderer;

// После загрузки карты:
worldRenderer.initialize(bspLoader);
brushRenderer.initialize(bspLoader);

// В цикле рендеринга:
WorldSurfaceList surfList;
worldRenderer.getVisibleSurfaces(camera, surfList);

// Рендеринг мира
worldRenderer.drawTexturedWorld(surfList, worldShader);

// Рендеринг brush entities
brushRenderer.beginRendering();
for (auto& entity : brushEntities) {
    if (entity.isTransparent) {
        brushRenderer.drawSortedBrushEntity(...);
    } else {
        brushRenderer.drawBrushEntity(...);
    }
}
```

### Шаг 4: Настройте шейдеры

Вам понадобятся шейдеры с поддержкой:
- Lightmap координат
- Lightstyles (4 канала освещения)
- Моделей матриц для brush entities
- Render modes и render fx для эффектов

## Отличия от оригинального BSPRenderer

1. **Упрощенная структура** - удалены зависимости от специфичных классов SceneRenderer
2. **Адаптированные типы** - используются стандартные glm::vec3 вместо vec_t
3. **Заглушки для интеграции** - комментарии с указанием где нужна интеграция
4. **Отсутствие консольных переменных** - ConVar заменены на простые bool флаги

## Требования

- OpenGL 3.3+ (для GL_TRIANGLE_FAN и primitive restart)
- GLM библиотека
- Собственная система материалов/текстур
- VAO для хранения атрибутов вершин

## Пример использования PVS

```cpp
// PVS автоматически обновляется при изменении позиции камеры
WorldSurfaceList surfList;
worldRenderer.getVisibleSurfaces(camera, surfList);

// surfList.textureChain[i] содержит поверхности с текстурой i
// surfList.textureChainFrames[i] показывает актуальность цепочки
// Рисовать только если textureChainFrames[i] == textureChainFrame

for (size_t i = 0; i < surfList.textureChain.size(); i++) {
    if (surfList.textureChainFrames[i] != surfList.textureChainFrame)
        continue;
    
    // Биндим текстуру i
    // Рисуем surfList.textureChain[i]
}
```

## Производительность

Оригинальный BSPRenderer оптимизирован для:
- Минимального количества draw calls через батчинг
- Эффективного PVS для быстрой отсечки
- Front-to-back rendering для early-Z optimization

Эта адаптация сохраняет ключевые оптимизации, но требует настройки под вашу систему материалов.
