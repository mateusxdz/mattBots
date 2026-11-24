#include "bots/BotOrchestrator.h"
#include "bots/TelegramHttpClient.h"

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

  IBot* botToPoll = nullptr;

  // Prioridade de conversa (afinidade): se o usuário interagiu com um bot nos últimos 45s,
  // intercala esse bot ativo entre cada um dos demais bots (reduzindo a latência pela metade)
  if (_activeBot != nullptr && now < _activeBotUntilMs && _bots.size() > 1) {
    if (_pollActiveTurn) {
      botToPoll = _activeBot;
      _pollActiveTurn = false;
    } else {
      if (_bots[_currentIndex] == _activeBot) {
        _currentIndex = (_currentIndex + 1) % _bots.size();
      }
      botToPoll = _bots[_currentIndex];
      _currentIndex = (_currentIndex + 1) % _bots.size();
      _pollActiveTurn = true;
    }
  } else {
    _activeBot = nullptr;
    botToPoll = _bots[_currentIndex];
    _currentIndex = (_currentIndex + 1) % _bots.size();
  }

  SharedHttpsTransport::consumeBotActivity();
  botToPoll->poll();
  if (SharedHttpsTransport::consumeBotActivity()) {
    _activeBot = botToPoll;
    _activeBotUntilMs = millis() + 45000; // Mantém prioridade máxima por 45s após qualquer mensagem
    _pollActiveTurn = true;
  }
}

