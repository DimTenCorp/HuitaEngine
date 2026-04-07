#include "Entity.h"

// Конструктор по умолчанию
CBaseEntity::CBaseEntity() 
    : position(glm::vec3(0.0f, 0.0f, 0.0f)),
      rotation(glm::vec3(0.0f, 0.0f, 0.0f)),
      scale(glm::vec3(1.0f, 1.0f, 1.0f)),
      isActive(true) {
}

// Деструктор
CBaseEntity::~CBaseEntity() {
    // Очистка ресурсов если необходима
}

// Обновление состояния сущности
void CBaseEntity::Update(float deltaTime) {
    // Базовая логика обновления
    // Переопределяется в производных классах
}

// Рендеринг сущности
void CBaseEntity::Render() {
    // Базовая логика рендеринга
    // Переопределяется в производных классах
}
