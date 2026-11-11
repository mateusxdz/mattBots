#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "bots/ITelegramClient.h"

// Contexto TLS compartilhado entre todos os clientes HTTPS para economizar SRAM no ESP32-WROOM-32E
class SharedHttpsTransport {
public:
  SharedHttpsTransport() {
    _client.setInsecure();
    _client.setTimeout(15);
  }

  WiFiClientSecure& client() {
    return _client;
  }

private:
  WiFiClientSecure _client;
};

// Implementação concreta do cliente Telegram reutilizando transporte TLS único (SRP / DIP)
class TelegramHttpClient : public ITelegramClient {
public:
  TelegramHttpClient(SharedHttpsTransport& transport,
                     const String& botToken = "",
                     bool allowCallbacks = false);
  ~TelegramHttpClient() override = default;

  void setBotToken(const String& botToken);

  bool getUpdates(int64_t offset, std::vector<TelegramUpdate>& outUpdates) override;

  int64_t sendMessage(const String& chatId,
                      const String& text,
                      const char* parseMode = "Markdown",
                      const String& replyMarkupJson = "") override;

  bool editMessageText(const String& chatId,
                       int64_t messageId,
                       const String& text,
                       const char* parseMode = "HTML",
                       const String& replyMarkupJson = "") override;

  bool answerCallbackQuery(const String& callbackQueryId,
                           const String& text = "") override;

private:
  SharedHttpsTransport& _transport;
  String _botToken;
  bool _allowCallbacks;

  String buildApiUrl(const char* method) const;
};

