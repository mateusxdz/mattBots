#pragma once
#include <Arduino.h>
#include <vector>
#include "bots/IBot.h"
#include "bots/ITimeProvider.h"

// Orquestrador Round-Robin para execução cooperativa dos bots (SRP / OCP / DIP)
class BotOrchestrator {
public:
  BotOrchestrator(ITimeProvider& timeProvider, uint32_t stepIntervalMs);

  void registerBot(IBot& bot);
  void begin();
  void loop();

private:
  ITimeProvider& _timeProvider;
  uint32_t _stepIntervalMs;
  std::vector<IBot*> _bots;
  size_t _currentIndex = 0;
  uint32_t _lastStepMs = 0;
  bool _initialized = false;
};

