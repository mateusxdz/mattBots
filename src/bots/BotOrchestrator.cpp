#include "bots/BotOrchestrator.h"

BotOrchestrator::BotOrchestrator(ITimeProvider& timeProvider, uint32_t stepIntervalMs)
    : _timeProvider(timeProvider), _stepIntervalMs(stepIntervalMs) {}

void BotOrchestrator::registerBot(IBot& bot) {
  _bots.push_back(&bot);
}

void BotOrchestrator::begin() {
  if (_initialized) {
    return;
  }
  _timeProvider.begin();
  for (IBot* bot : _bots) {
    Serial.printf("[ORCHESTRATOR] Inicializando bot: %s\n", bot->name());
    bot->begin();
  }
  _initialized = true;
  _lastStepMs = millis();
}

void BotOrchestrator::loop() {
  if (!_initialized) {
    begin();
  }
  if (_bots.empty()) {
    return;
  }

  uint32_t now = millis();
  if (now - _lastStepMs < _stepIntervalMs) {
    return;
  }
  _lastStepMs = now;

  _timeProvider.ensureSynced();

  IBot* currentBot = _bots[_currentIndex];
  currentBot->poll();

  _currentIndex = (_currentIndex + 1) % _bots.size();
}

