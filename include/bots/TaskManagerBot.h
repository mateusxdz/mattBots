#pragma once
#include "bots/IBot.h"
#include "bots/ITelegramClient.h"
#include "bots/IJsonStore.h"

// Bot Gerenciador de Tarefas e Agenda do Dia (SRP / LSP / DIP)
class TaskManagerBot : public IBot {
public:
  TaskManagerBot(ITelegramClient& telegram,
                 IJsonStore& store,
                 const char* dataFilePath,
                 int64_t authorizedUserId);
  ~TaskManagerBot() override = default;

  const char* name() const override { return "TaskManagerBot"; }
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

  void cmdAddTask(const String& chatId, const String& taskName, bool naturalStyle = false);
  void cmdRemoveTask(const String& chatId, int taskId, bool naturalStyle = false);
  void cmdListTasks(const String& chatId, bool naturalStyle = false);
  void cmdClearTasks(const String& chatId, const std::vector<String>& args);
  void cmdTodayAdd(const String& chatId, const std::vector<String>& args, bool naturalStyle = false);
  void cmdTodayList(const String& chatId, bool naturalStyle = false);
  void cmdTodayRemove(const String& chatId, int taskId);
  void cmdTodayClear(const String& chatId);
  void cmdHelp(const String& chatId);

  void loadTasksData(JsonDocument& doc);
};

