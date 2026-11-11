#include "LedStatusIndicator.h"

LedStatusIndicator::LedStatusIndicator(uint8_t ledPin)
    : _ledPin(ledPin), _currentState(SystemState::CONFIG_MODE) {}

void LedStatusIndicator::begin() {
  pinMode(_ledPin, OUTPUT);
  xTaskCreatePinnedToCore(taskEntry, "LedTask", 2048, this, 1, nullptr, 0);
}

void LedStatusIndicator::setState(SystemState state) {
  _currentState = state;
}

SystemState LedStatusIndicator::getState() const {
  return _currentState;
}

void LedStatusIndicator::taskEntry(void* pvParameters) {
  auto* self = static_cast<LedStatusIndicator*>(pvParameters);
  self->runLoop();
}

void LedStatusIndicator::runLoop() {
  while (true) {
    switch (_currentState) {
      case SystemState::CONFIG_MODE:
        // Padrão 1 (Modo Configuração - 2 min): Batimento cardíaco (dupla piscada + pausa)
        digitalWrite(_ledPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(_ledPin, LOW);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(_ledPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(_ledPin, LOW);
        vTaskDelay(pdMS_TO_TICKS(700));
        break;

      case SystemState::WIFI_CONNECTING:
        // Padrão 2 (Conectando no Wi-Fi): Piscada rápida contínua (5 Hz)
        digitalWrite(_ledPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(_ledPin, LOW);
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

      case SystemState::WIFI_CONNECTED:
        // Padrão 3 (Conectado no Wi-Fi): Piscada lenta e suave (0,5 Hz)
        digitalWrite(_ledPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(1000));
        digitalWrite(_ledPin, LOW);
        vTaskDelay(pdMS_TO_TICKS(1000));
        break;

      case SystemState::WIFI_ERROR:
        // Padrão 4 (Falha / Sem credenciais): Alerta triplo rápido + pausa longa
        for (int i = 0; i < 3; i++) {
          digitalWrite(_ledPin, HIGH);
          vTaskDelay(pdMS_TO_TICKS(80));
          digitalWrite(_ledPin, LOW);
          vTaskDelay(pdMS_TO_TICKS(80));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        break;

      case SystemState::OTA_UPDATING:
        // Padrão 5 (Atualizando via OTA): Piscada ultrarrápida (12,5 Hz)
        digitalWrite(_ledPin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(40));
        digitalWrite(_ledPin, LOW);
        vTaskDelay(pdMS_TO_TICKS(40));
        break;
    }
  }
}

