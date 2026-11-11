#pragma once
#include "bots/IBot.h"
#include "bots/ITelegramClient.h"
#include "bots/IJsonStore.h"
#include "bots/ITimeProvider.h"

// Bot de Controle Financeiro e Orçamento Diário (SRP / LSP / DIP)
class MoneyManagerBot : public IBot {
public:
  MoneyManagerBot(ITelegramClient& telegram,
                  IJsonStore& store,
                  ITimeProvider& timeProvider,
                  const char* dataFilePath,
                  int64_t authorizedUserId);
  ~MoneyManagerBot() override = default;

  const char* name() const override { return "MoneyManagerBot"; }
  void begin() override;
  void poll() override;
  void setAuthorizedUserId(int64_t userId) { _authorizedUserId = userId; }

private:
  ITelegramClient& _telegram;
  IJsonStore& _store;
  ITimeProvider& _timeProvider;
  const char* _dataFilePath;
  int64_t _authorizedUserId;
  int64_t _lastUpdateId = 0;

  void handleMessage(const TelegramMessage& msg);
  void handleSlashCommand(const String& chatId,
                          const String& cmd,
                          const String& rest,
                          const std::vector<String>& args);
  void handleNaturalLanguage(const String& chatId, const String& rawText);

  void cmdConfig(const String& chatId, const std::vector<String>& args);
  void cmdSpend(const String& chatId, float amount, const String& desc, bool naturalStyle = false);
  void cmdFutureSpend(const String& chatId, const std::vector<String>& args);
  void cmdAllowance(const String& chatId, bool naturalStyle = false);
  void cmdAllowancePlot(const String& chatId);
  void cmdListSpend(const String& chatId, bool naturalStyle = false);
  void cmdDeleteSpend(const String& chatId, int spendId);
  void cmdShowConfig(const String& chatId);
  void cmdHelp(const String& chatId);

  void loadMoneyData(JsonDocument& doc);
  static String fmtMoney(float val);
};

