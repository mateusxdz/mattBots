#pragma once
#include <Arduino.h>
#include <vector>

struct TelegramMessage {
  int64_t updateId = 0;
  int64_t messageId = 0;
  int64_t userId = 0;
  String chatId;
  String text;
  bool hasVoiceOrAudio = false;
};

struct TelegramCallbackQuery {
  int64_t updateId = 0;
  String id;
  int64_t userId = 0;
  String chatId;
  int64_t messageId = 0;
  String data;
};

struct TelegramUpdate {
  int64_t updateId = 0;
  bool isCallback = false;
  TelegramMessage message;
  TelegramCallbackQuery callback;
};

// Abstração de comunicação com a API do Telegram para um Bot específico (DIP / ISP)
class ITelegramClient {
public:
  virtual ~ITelegramClient() = default;

  virtual bool getUpdates(int64_t offset, std::vector<TelegramUpdate>& outUpdates) = 0;

  virtual int64_t sendMessage(const String& chatId,
                              const String& text,
                              const char* parseMode = "Markdown",
                              const String& replyMarkupJson = "") = 0;

  virtual bool editMessageText(const String& chatId,
                               int64_t messageId,
                               const String& text,
                               const char* parseMode = "HTML",
                               const String& replyMarkupJson = "") = 0;

  virtual bool answerCallbackQuery(const String& callbackQueryId,
                                   const String& text = "") = 0;
};

