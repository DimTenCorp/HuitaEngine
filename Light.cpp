#include "Light.h"
#include "Entity.h"
#include <iostream>

// Конструктор по умолчанию
CLight::CLight() 
    : lightType(POINT_LIGHT),
      color(glm::vec3(1.0f, 1.0f, 1.0f)),
      brightness(1.0f),
      radius(10.0f),
      constant(1.0f),
      linear(0.09f),
      quadratic(0.032f),
      spotCutoff(12.5f),
      spotOuterCutoff(17.5f),
      direction(glm::vec3(0.0f, -1.0f, 0.0f)) {
    isActive = true;
}

// Деструктор
CLight::~CLight() {
    // Очистка ресурсов если необходима
}

// Обновление состояния света
void CLight::Update(float deltaTime) {
    // Базовая логика обновления
    // Может быть расширена для анимации или динамического изменения параметров
    if (!isActive) {
        return;
    }

    // Здесь можно добавить логику изменения яркости, цвета и т.д.
}

// Рендеринг света
void CLight::Render() {
    // Базовая логика рендеринга
    // В реальном движке здесь была бы отправка данных о свете в шейдеры
    if (!isActive) {
        return;
    }

    // Отладочный вывод
    // std::cout << "Rendering light at position: " 
    //           << position.x << ", " << position.y << ", " << position.z << std::endl;
}

// Получение позиции (переопределение метода базового класса)
vec3_t CLight::getPosition() const {
    return position;
}

// Установка позиции (переопределение метода базового класса)
void CLight::setPosition(const vec3_t& pos) {
    position = pos;
}
