#pragma once
#include "bots/IBot.h"
#include "bots/ITelegramClient.h"
#include "bots/IJsonStore.h"
#include "bots/ITimeProvider.h"

// Bot de Controle de Calorias e Plano de Academia (SRP / LSP / DIP)
class GymCalorieBot : public IBot {
public:
  GymCalorieBot(ITelegramClient& telegram,
                IJsonStore& store,
                ITimeProvider& timeProvider,
                const char* dataFilePath,
                int64_t authorizedUserId);
  ~GymCalorieBot() override = default;

  const char* name() const override { return "GymCalorieBot"; }
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

  void cmdGym(const String& chatId, const String& rest, const std::vector<String>& args);
  void cmdAddCalories(const String& chatId, float calories, const String& foodDesc);
  void cmdSetTarget(const String& chatId, const std::vector<String>& args);
  void cmdShowStatus(const String& chatId);
  void cmdEditCalorie(const String& chatId, const std::vector<String>& args);
  void cmdDeleteCalorie(const String& chatId, int entryId);
  void cmdResetCalories(const String& chatId, bool confirmed);
  void cmdHelp(const String& chatId);

  void loadGymData(JsonDocument& doc);
  String formatProgressMsg(JsonDocument& doc, const String& dateStr) const;
  static String fmtKcal(float val);
  static String formatGymPlan(const String& plan);
};

