#pragma once
#include "bots/IBot.h"
#include "bots/ITelegramClient.h"
#include "bots/IJsonStore.h"
#include "bots/ITimeProvider.h"

// Categorias de tarefas:
// - DAILY ("daily"): Tarefas recorrentes que ocorrem todos os dias, indefinidamente
// - IMMEDIATE ("immediate"): Tarefas pontuais que podem ser feitas imediatamente e eliminadas
// - SCHEDULED ("scheduled"): Tarefas pontuais que acontecerão no futuro (agendadas)
enum class TaskCategory {
  DAILY,
  IMMEDIATE,
  SCHEDULED
};

// Bot Gerenciador de Tarefas e Agenda do Dia (SRP / LSP / DIP)
class TaskManagerBot : public IBot {
public:
  TaskManagerBot(ITelegramClient& telegram,
                 IJsonStore& store,
                 ITimeProvider& timeProvider,
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

  void cmdAddTask(const String& chatId,
                  const String& taskName,
                  TaskCategory category = TaskCategory::IMMEDIATE,
                  const String& whenStr = "",
                  bool naturalStyle = false);
  void cmdDoneTask(const String& chatId, int taskId, bool naturalStyle = false);
  void cmdUndoneTask(const String& chatId, int taskId, bool naturalStyle = false);
  void cmdRemoveTask(const String& chatId, int taskId, bool naturalStyle = false);
  void cmdMoveTask(const String& chatId,
                   int taskId,
                   TaskCategory targetCategory,
                   const String& whenStr = "",
                   bool naturalStyle = false);
  void cmdListTasks(const String& chatId, int filterCategory = -1, bool naturalStyle = false);
  void cmdClearTasks(const String& chatId, const std::vector<String>& args);
  void cmdTodayAdd(const String& chatId, const std::vector<String>& args, bool naturalStyle = false);
  void cmdTodayList(const String& chatId, bool naturalStyle = false);
  void cmdTodayRemove(const String& chatId, int taskId);
  void cmdTodayClear(const String& chatId);
  void cmdHelp(const String& chatId);

  void loadTasksData(JsonDocument& doc);
  static const char* categoryToKey(TaskCategory cat);
  static TaskCategory keyToCategory(const String& key);
  static void extractOptionalDatePrefix(const String& rawInput, String& outWhen, String& outDesc);
  static bool isDateToken(const String& token);
  static bool isPositiveIntToken(const String& token);
  bool isScheduledForToday(const String& whenStr) const;
};

