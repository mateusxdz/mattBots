#pragma once
#include "bots/IBot.h"
#include "bots/ITelegramClient.h"
#include "bots/IJsonStore.h"

// Bot de Lista de Compras / Mercado (SRP / LSP / DIP)
class MercadoBot : public IBot {
public:
  MercadoBot(ITelegramClient& telegram,
             IJsonStore& store,
             const char* dataFilePath,
             int64_t authorizedUserId);
  ~MercadoBot() override = default;

  const char* name() const override { return "MercadoBot"; }
  void begin() override;
  void poll() override;
  void setAuthorizedUserId(int64_t userId) { _authorizedUserId = userId; }

private:
  ITelegramClient& _telegram;
  IJsonStore& _store;
  const char* _dataFilePath;
  int64_t _authorizedUserId;
  int64_t _lastUpdateId = 0;

  void handleMessage(const TelegramMessage& msg);
  void handleSlashCommand(const String& chatId,
                          const String& cmd,
                          const String& rest,
                          const std::vector<String>& args);
  void handleNaturalLanguage(const String& chatId, const String& rawText);

  void cmdAdd(const String& chatId, const String& itemName, bool naturalStyle = false);
  void cmdRemove(const String& chatId, int itemId, bool naturalStyle = false);
  void cmdList(const String& chatId, bool naturalStyle = false);
  void cmdClear(const String& chatId, const std::vector<String>& args);
  void cmdHelp(const String& chatId);

  bool loadMercadoData(JsonDocument& doc);
};

