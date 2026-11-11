#pragma once
#include <Arduino.h>
#include "IStatusIndicator.h"

// Princípio da Responsabilidade Única (SRP):
// Classe dedicada exclusivamente a sinalizar o estado do sistema via LED em FreeRTOS.
class LedStatusIndicator : public IStatusIndicator {
public:
  explicit LedStatusIndicator(uint8_t ledPin);
  void begin() override;
  void setState(SystemState state) override;
  SystemState getState() const override;

private:
  uint8_t _ledPin;
  volatile SystemState _currentState;
  static void taskEntry(void* pvParameters);
  void runLoop();
};

