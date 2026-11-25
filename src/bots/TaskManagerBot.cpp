#include "bots/TaskManagerBot.h"
#include "bots/TextUtils.h"

TaskManagerBot::TaskManagerBot(ITelegramClient& telegram,
                               IJsonStore& store,
                               ITimeProvider& timeProvider,
                               const char* dataFilePath,
                               int64_t authorizedUserId)
    : _telegram(telegram),
      _store(store),
      _timeProvider(timeProvider),
      _dataFilePath(dataFilePath),
      _authorizedUserId(authorizedUserId) {}

void TaskManagerBot::begin() {
  JsonDocument doc;
  if (!_store.load(_dataFilePath, doc)) {
    doc["tasks"].to<JsonArray>();
    doc["today"].to<JsonArray>();
    _store.save(_dataFilePath, doc);
  }
}

void TaskManagerBot::loadTasksData(JsonDocument& doc) {
  if (!_store.load(_dataFilePath, doc)) {
    doc.clear();
  }
  if (!doc["tasks"].is<JsonArray>()) {
    doc["tasks"].to<JsonArray>();
  }
  if (!doc["today"].is<JsonArray>()) {
    doc["today"].to<JsonArray>();
  }
}

const char* TaskManagerBot::categoryToKey(TaskCategory cat) {
  switch (cat) {
    case TaskCategory::DAILY:
      return "daily";
    case TaskCategory::SCHEDULED:
      return "scheduled";
    case TaskCategory::IMMEDIATE:
    default:
      return "immediate";
  }
}

TaskCategory TaskManagerBot::keyToCategory(const String& key) {
  if (key.equalsIgnoreCase("daily") || key.equalsIgnoreCase("diaria") || key.equalsIgnoreCase("diario")) {
    return TaskCategory::DAILY;
  }
  if (key.equalsIgnoreCase("scheduled") || key.equalsIgnoreCase("future") ||
      key.equalsIgnoreCase("futura") || key.equalsIgnoreCase("agendada")) {
    return TaskCategory::SCHEDULED;
  }
  return TaskCategory::IMMEDIATE;
}

bool TaskManagerBot::isDateToken(const String& token) {
  // Aceita DD/MM, DD/MM/AAAA, DD-MM, DD-MM-AAAA
  int len = token.length();
  if (len != 5 && len != 8 && len != 10) {
    return false;
  }
  int slashCount = 0;
  for (int i = 0; i < len; i++) {
    char c = token[i];
    if (c == '/' || c == '-') {
      slashCount++;
    } else if (!isDigit(c)) {
      return false;
    }
  }
  return (slashCount == 1 || slashCount == 2);
}

void TaskManagerBot::extractOptionalDatePrefix(const String& rawInput, String& outWhen, String& outDesc) {
  String s = rawInput;
  s.trim();

  // Remove conectivos iniciais opcionais ("para ", "em ", "dia ")
  String lower = s;
  lower.toLowerCase();
  if (lower.startsWith("para o dia ")) {
    s = s.substring(11);
  } else if (lower.startsWith("para dia ")) {
    s = s.substring(9);
  } else if (lower.startsWith("no dia ")) {
    s = s.substring(7);
  } else if (lower.startsWith("para ")) {
    s = s.substring(5);
  } else if (lower.startsWith("dia ")) {
    s = s.substring(4);
  } else if (lower.startsWith("em ")) {
    s = s.substring(3);
  }
  s.trim();

  int sp = s.indexOf(' ');
  if (sp > 0) {
    String firstTok = s.substring(0, sp);
    String remainder = s.substring(sp + 1);
    remainder = TextUtils::stripLeadingConnectors(remainder);
    if (isDateToken(firstTok) && remainder.length() > 0) {
      firstTok.replace("-", "/");
      outWhen = firstTok;
      outDesc = remainder;
      return;
    }
  }

  outWhen = "";
  outDesc = s;
}

bool TaskManagerBot::isScheduledForToday(const String& whenStr) const {
  if (whenStr.length() < 5) {
    return false;
  }
  String todayFull = _timeProvider.todayStr(); // "DD/MM/YYYY"
  String todayShort = todayFull.substring(0, 5); // "DD/MM"
  return (whenStr == todayFull || whenStr == todayShort);
}

void TaskManagerBot::poll() {
  std::vector<TelegramUpdate> updates;
  int64_t offset = (_lastUpdateId > 0) ? (_lastUpdateId + 1) : 0;
  if (!_telegram.getUpdates(offset, updates)) {
    return;
  }

  for (const TelegramUpdate& u : updates) {
    if (u.updateId > _lastUpdateId) {
      _lastUpdateId = u.updateId;
    }
    if (!u.isCallback) {
      handleMessage(u.message);
    }
  }
}

void TaskManagerBot::handleMessage(const TelegramMessage& msg) {
  if (msg.userId != _authorizedUserId) {
    _telegram.sendMessage(msg.chatId, "❌ You are not allowed to use this bot.", "");
    return;
  }

  if (msg.hasVoiceOrAudio) {
    _telegram.sendMessage(
        msg.chatId,
        "ℹ️ Comandos de áudio foram desativados no ESP32. Envie o comando em texto (ou use o ditado do teclado)!");
    return;
  }

  String text = msg.text;
  text.trim();
  if (text.length() == 0) {
    return;
  }

  String cmd, rest;
  std::vector<String> args;
  if (TextUtils::parseSlashCommand(text, cmd, rest, args)) {
    handleSlashCommand(msg.chatId, cmd, rest, args);
  } else {
    handleNaturalLanguage(msg.chatId, text);
  }
}

void TaskManagerBot::handleSlashCommand(const String& chatId,
                                        const String& cmd,
                                        const String& rest,
                                        const std::vector<String>& args) {
  if (cmd == "add" || cmd == "now" || cmd == "addnow" || cmd == "immediate") {
    if (rest.length() == 0) {
      _telegram.sendMessage(chatId, "Use: /add <descrição da tarefa pontual imediata>", "");
      return;
    }
    // Se o usuário digitou `/now <id>`, move a tarefa existente para imediata
    if (cmd == "now" && args.size() == 1 && args[0].toInt() > 0) {
      cmdMoveTask(chatId, args[0].toInt(), TaskCategory::IMMEDIATE, "", false);
      return;
    }
    cmdAddTask(chatId, rest, TaskCategory::IMMEDIATE, "", false);

  } else if (cmd == "daily" || cmd == "adddaily" || cmd == "diaria") {
    if (rest.length() == 0) {
      _telegram.sendMessage(chatId, "Use: /daily <descrição da tarefa diária recorrente>", "");
      return;
    }
    if (args.size() == 1 && args[0].toInt() > 0) {
      cmdMoveTask(chatId, args[0].toInt(), TaskCategory::DAILY, "", false);
      return;
    }
    cmdAddTask(chatId, rest, TaskCategory::DAILY, "", false);

  } else if (cmd == "schedule" || cmd == "scheduled" || cmd == "future" ||
             cmd == "addfuture" || cmd == "agendar" || cmd == "futura") {
    if (rest.length() == 0) {
      _telegram.sendMessage(
          chatId,
          "Use: /schedule [DD/MM] <descrição da tarefa futura/agendada>\n"
          "Ex: /schedule 15/12 Consulta médica\n"
          "Ex: /schedule Declarar imposto de renda",
          "");
      return;
    }
    // Se o primeiro argumento for um ID de tarefa existente e o usuário quiser mover para futura
    if (args.size() >= 1 && args[0].toInt() > 0 && (args.size() == 1 || isDateToken(args[1]))) {
      String when = (args.size() >= 2) ? args[1] : "";
      when.replace("-", "/");
      cmdMoveTask(chatId, args[0].toInt(), TaskCategory::SCHEDULED, when, false);
      return;
    }
    String whenStr, desc;
    extractOptionalDatePrefix(rest, whenStr, desc);
    cmdAddTask(chatId, desc, TaskCategory::SCHEDULED, whenStr, false);

  } else if (cmd == "done" || cmd == "concluir" || cmd == "check") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "Use: /done <id>", "");
      return;
    }
    int tid = args[0].toInt();
    if (tid <= 0) {
      _telegram.sendMessage(chatId, "ID inválido.", "");
      return;
    }
    cmdDoneTask(chatId, tid, false);

  } else if (cmd == "undone" || cmd == "uncheck") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "Use: /undone <id>", "");
      return;
    }
    int tid = args[0].toInt();
    if (tid <= 0) {
      _telegram.sendMessage(chatId, "ID inválido.", "");
      return;
    }
    cmdUndoneTask(chatId, tid, false);

  } else if (cmd == "remove" || cmd == "delete") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "Use: /remove <id>", "");
      return;
    }
    int tid = args[0].toInt();
    if (tid <= 0) {
      _telegram.sendMessage(chatId, "Invalid ID.", "");
      return;
    }
    cmdRemoveTask(chatId, tid, false);

  } else if (cmd == "move") {
    if (args.size() < 2) {
      _telegram.sendMessage(chatId, "Use: /move <id> <daily|now|future> [DD/MM]", "");
      return;
    }
    int tid = args[0].toInt();
    TaskCategory cat = keyToCategory(args[1]);
    String when = (args.size() >= 3) ? args[2] : "";
    when.replace("-", "/");
    cmdMoveTask(chatId, tid, cat, when, false);

  } else if (cmd == "list") {
    if (!args.empty()) {
      String f = args[0];
      if (f.equalsIgnoreCase("daily") || f.equalsIgnoreCase("diarias")) {
        cmdListTasks(chatId, static_cast<int>(TaskCategory::DAILY), false);
        return;
      } else if (f.equalsIgnoreCase("now") || f.equalsIgnoreCase("immediate") || f.equalsIgnoreCase("imediatas")) {
        cmdListTasks(chatId, static_cast<int>(TaskCategory::IMMEDIATE), false);
        return;
      } else if (f.equalsIgnoreCase("future") || f.equalsIgnoreCase("scheduled") ||
                 f.equalsIgnoreCase("futuras") || f.equalsIgnoreCase("agendadas")) {
        cmdListTasks(chatId, static_cast<int>(TaskCategory::SCHEDULED), false);
        return;
      }
    }
    cmdListTasks(chatId, -1, false);

  } else if (cmd == "listdaily") {
    cmdListTasks(chatId, static_cast<int>(TaskCategory::DAILY), false);
  } else if (cmd == "listnow" || cmd == "listimmediate") {
    cmdListTasks(chatId, static_cast<int>(TaskCategory::IMMEDIATE), false);
  } else if (cmd == "listfuture" || cmd == "listscheduled") {
    cmdListTasks(chatId, static_cast<int>(TaskCategory::SCHEDULED), false);

  } else if (cmd == "clear" || cmd == "cleartasks") {
    cmdClearTasks(chatId, args);
  } else if (cmd == "today") {
    if (args.empty()) {
      cmdTodayList(chatId, false);
      return;
    }
    cmdTodayAdd(chatId, args, false);
  } else if (cmd == "todaylist") {
    cmdTodayList(chatId, false);
  } else if (cmd == "todayremove") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "Use: /todayremove <id>", "");
      return;
    }
    int tid = args[0].toInt();
    if (tid <= 0) {
      _telegram.sendMessage(chatId, "Invalid ID.", "");
      return;
    }
    cmdTodayRemove(chatId, tid);
  } else if (cmd == "todayclear") {
    cmdTodayClear(chatId);
  } else if (cmd == "help" || cmd == "start" || cmd == "voicehelp") {
    cmdHelp(chatId);
  }
}

void TaskManagerBot::cmdAddTask(const String& chatId,
                                const String& taskName,
                                TaskCategory category,
                                const String& whenStr,
                                bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();

  int maxId = 0;
  for (JsonObject t : tasks) {
    int id = t["id"] | 0;
    if (id > maxId) maxId = id;
  }
  int nextId = maxId + 1;

  JsonObject newTask = tasks.add<JsonObject>();
  newTask["id"] = nextId;
  newTask["name"] = taskName;
  newTask["type"] = categoryToKey(category);
  if (category == TaskCategory::SCHEDULED && whenStr.length() > 0) {
    newTask["when"] = whenStr;
  }
  _store.save(_dataFilePath, doc);

  String msg;
  if (category == TaskCategory::DAILY) {
    msg = "🔄 *Tarefa Diária #" + String(nextId) + " adicionada:* " + taskName +
          "\n_(Repete todos os dias indefinidamente)_";
  } else if (category == TaskCategory::SCHEDULED) {
    String whenLabel = (whenStr.length() > 0) ? (" para *" + whenStr + "*") : "";
    msg = "🗓️ *Tarefa Agendada/Futura #" + String(nextId) + " adicionada" + whenLabel + ":* " + taskName;
  } else {
    msg = "⚡ *Tarefa Pontual Imediata #" + String(nextId) + " adicionada:* " + taskName;
  }
  _telegram.sendMessage(chatId, msg);
}

void TaskManagerBot::cmdDoneTask(const String& chatId, int taskId, bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();
  JsonArray today = doc["today"].as<JsonArray>();

  for (size_t i = 0; i < tasks.size(); i++) {
    JsonObject t = tasks[i];
    if ((t["id"] | 0) == taskId) {
      String name = t["name"] | "";
      TaskCategory cat = keyToCategory(t["type"] | "immediate");

      if (cat == TaskCategory::DAILY) {
        String todayStr = _timeProvider.todayStr();
        t["last_done"] = todayStr;
        _store.save(_dataFilePath, doc);
        _telegram.sendMessage(
            chatId,
            "✅ *Tarefa Diária #" + String(taskId) + " concluída hoje!* (" + name + ")\n"
            "🔄 _Como ela é diária, continuará ativa para os próximos dias._");
        return;
      }

      // Tarefa pontual (imediata ou agendada): concluir remove definitivamente
      tasks.remove(i);
      for (size_t j = 0; j < today.size(); j++) {
        if (today[j].as<int>() == taskId) {
          today.remove(j);
          break;
        }
      }
      _store.save(_dataFilePath, doc);
      _telegram.sendMessage(
          chatId,
          "✅ *Tarefa Pontual #" + String(taskId) + " concluída e finalizada!* (" + name + ")");
      return;
    }
  }

  _telegram.sendMessage(chatId, "⚠️ Tarefa #" + String(taskId) + " não encontrada.");
}

void TaskManagerBot::cmdUndoneTask(const String& chatId, int taskId, bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();

  for (JsonObject t : tasks) {
    if ((t["id"] | 0) == taskId) {
      t["last_done"] = "";
      _store.save(_dataFilePath, doc);
      _telegram.sendMessage(chatId, "⬜ *Tarefa #" + String(taskId) + " marcada como pendente novamente para hoje.*");
      return;
    }
  }
  _telegram.sendMessage(chatId, "⚠️ Tarefa #" + String(taskId) + " não encontrada.");
}

void TaskManagerBot::cmdRemoveTask(const String& chatId, int taskId, bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();
  JsonArray today = doc["today"].as<JsonArray>();

  bool found = false;
  String removedName;
  for (size_t i = 0; i < tasks.size(); i++) {
    if ((tasks[i]["id"] | 0) == taskId) {
      removedName = tasks[i]["name"] | "";
      tasks.remove(i);
      found = true;
      break;
    }
  }

  for (size_t i = 0; i < today.size(); i++) {
    if (today[i].as<int>() == taskId) {
      today.remove(i);
      break;
    }
  }

  _store.save(_dataFilePath, doc);

  if (found) {
    _telegram.sendMessage(chatId, "🗑️ *Tarefa #" + String(taskId) + " removida permanentemente!* (" + removedName + ")");
  } else {
    _telegram.sendMessage(chatId, "⚠️ Tarefa #" + String(taskId) + " não encontrada.");
  }
}

void TaskManagerBot::cmdMoveTask(const String& chatId,
                                 int taskId,
                                 TaskCategory targetCategory,
                                 const String& whenStr,
                                 bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();

  for (JsonObject t : tasks) {
    if ((t["id"] | 0) == taskId) {
      String name = t["name"] | "";
      t["type"] = categoryToKey(targetCategory);
      if (targetCategory == TaskCategory::SCHEDULED && whenStr.length() > 0) {
        t["when"] = whenStr;
      } else if (targetCategory != TaskCategory::SCHEDULED) {
        t.remove("when");
      }
      _store.save(_dataFilePath, doc);

      String catLabel;
      if (targetCategory == TaskCategory::DAILY) {
        catLabel = "🔄 *Diária (Todos os dias)*";
      } else if (targetCategory == TaskCategory::SCHEDULED) {
        catLabel = "🗓️ *Futura / Agendada*" + (whenStr.length() > 0 ? (" (" + whenStr + ")") : String(""));
      } else {
        catLabel = "⚡ *Pontual Imediata (Fazer Agora)*";
      }
      _telegram.sendMessage(chatId,
                            "🔀 *Tarefa #" + String(taskId) + "* (" + name + ") movida para: " + catLabel);
      return;
    }
  }

  _telegram.sendMessage(chatId, "⚠️ Tarefa #" + String(taskId) + " não encontrada.");
}

void TaskManagerBot::cmdListTasks(const String& chatId, int filterCategory, bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();

  if (tasks.size() == 0) {
    _telegram.sendMessage(chatId, "📭 Nenhuma tarefa cadastrada.");
    return;
  }

  String todayStr = _timeProvider.todayStr();
  String dailyLines;
  String immediateLines;
  String scheduledLines;

  for (JsonObject t : tasks) {
    int id = t["id"] | 0;
    String name = t["name"] | "";
    TaskCategory cat = keyToCategory(t["type"] | "immediate");

    if (cat == TaskCategory::DAILY) {
      String lastDone = t["last_done"] | "";
      String statusIcon = (lastDone == todayStr) ? "✅" : "⬜";
      dailyLines += "  " + statusIcon + " `[" + String(id) + "]` " + name + "\n";
    } else if (cat == TaskCategory::SCHEDULED) {
      String when = t["when"] | "";
      String whenBadge = (when.length() > 0) ? (" _(" + when + ")_") : "";
      if (isScheduledForToday(when)) {
        whenBadge += " 🔔 *HOJE!*";
      }
      scheduledLines += "  🗓️ `[" + String(id) + "]` " + name + whenBadge + "\n";
    } else {
      immediateLines += "  ⚡ `[" + String(id) + "]` " + name + "\n";
    }
  }

  String out;
  if (filterCategory == static_cast<int>(TaskCategory::DAILY)) {
    out = "🔄 *Tarefas Diárias (Todos os dias):*\n";
    out += (dailyLines.length() > 0) ? dailyLines : "  _Nenhuma tarefa diária cadastrada._\n";
  } else if (filterCategory == static_cast<int>(TaskCategory::IMMEDIATE)) {
    out = "⚡ *Tarefas Pontuais — Fazer Agora (Imediatas):*\n";
    out += (immediateLines.length() > 0) ? immediateLines : "  _Nenhuma tarefa imediata pendente._\n";
  } else if (filterCategory == static_cast<int>(TaskCategory::SCHEDULED)) {
    out = "🗓️ *Tarefas Pontuais — Futuras / Agendadas:*\n";
    out += (scheduledLines.length() > 0) ? scheduledLines : "  _Nenhuma tarefa futura agendada._\n";
  } else {
    out = "📋 *Painel de Tarefas (" + todayStr + "):*\n\n";

    out += "🔄 *Diárias (Todos os dias):*\n";
    out += (dailyLines.length() > 0) ? dailyLines : "  _Nenhuma_\n";

    out += "\n⚡ *Pontuais — Fazer Agora (Imediatas):*\n";
    out += (immediateLines.length() > 0) ? immediateLines : "  _Nenhuma_\n";

    out += "\n🗓️ *Pontuais — Futuras / Agendadas:*\n";
    out += (scheduledLines.length() > 0) ? scheduledLines : "  _Nenhuma_\n";

    out += "\n💡 _Use `/done <id>` para concluir (diárias marcam ✅ hoje; pontuais são eliminadas)._";
  }

  _telegram.sendMessage(chatId, out);
}

void TaskManagerBot::cmdClearTasks(const String& chatId, const std::vector<String>& args) {
  if (args.empty() || !args[0].equalsIgnoreCase("confirm")) {
    _telegram.sendMessage(
        chatId,
        "⚠️ Para limpar as tarefas, digite:\n"
        "• `/clear confirm` (limpa todas)\n"
        "• `/clear confirm now` (limpa apenas imediatas)\n"
        "• `/clear confirm future` (limpa apenas futuras)\n"
        "• `/clear confirm daily` (limpa apenas diárias)");
    return;
  }

  JsonDocument doc;
  loadTasksData(doc);

  if (args.size() >= 2) {
    TaskCategory targetCat = keyToCategory(args[1]);
    JsonArray tasks = doc["tasks"].as<JsonArray>();
    size_t i = 0;
    while (i < tasks.size()) {
      TaskCategory cat = keyToCategory(tasks[i]["type"] | "immediate");
      if (cat == targetCat) {
        tasks.remove(i);
      } else {
        i++;
      }
    }
    _store.save(_dataFilePath, doc);
    _telegram.sendMessage(chatId, "🧹 Tarefas da categoria selecionada foram limpas.");
    return;
  }

  doc["tasks"].to<JsonArray>();
  doc["today"].to<JsonArray>();
  _store.save(_dataFilePath, doc);
  _telegram.sendMessage(chatId, "🧹 Todas as tarefas foram limpas.");
}

void TaskManagerBot::cmdTodayAdd(const String& chatId,
                                 const std::vector<String>& args,
                                 bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();
  JsonArray today = doc["today"].as<JsonArray>();

  std::vector<int> addedIds;
  for (const String& arg : args) {
    int tid = arg.toInt();
    if (tid <= 0) continue;

    bool existsInTasks = false;
    for (JsonObject t : tasks) {
      if ((t["id"] | 0) == tid) {
        existsInTasks = true;
        break;
      }
    }
    if (!existsInTasks) continue;

    bool alreadyInToday = false;
    for (JsonVariant v : today) {
      if (v.as<int>() == tid) {
        alreadyInToday = true;
        break;
      }
    }

    if (!alreadyInToday) {
      today.add(tid);
      addedIds.push_back(tid);
    }
  }

  _store.save(_dataFilePath, doc);

  if (!addedIds.empty()) {
    String listStr;
    for (size_t i = 0; i < addedIds.size(); i++) {
      if (i > 0) listStr += ", ";
      listStr += String(addedIds[i]);
    }
    _telegram.sendMessage(chatId, "📌 *Tarefa #" + listStr + " adicionada ao foco de hoje!*");
  } else {
    _telegram.sendMessage(chatId, "⚠️ Tarefa não encontrada ou já está na lista de hoje.");
  }
}

void TaskManagerBot::cmdTodayList(const String& chatId, bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();
  JsonArray today = doc["today"].as<JsonArray>();

  String todayStr = _timeProvider.todayStr();
  String dailySection;
  String scheduledTodaySection;
  String manualTodaySection;

  for (JsonObject t : tasks) {
    int id = t["id"] | 0;
    String name = t["name"] | "";
    TaskCategory cat = keyToCategory(t["type"] | "immediate");

    if (cat == TaskCategory::DAILY) {
      String lastDone = t["last_done"] | "";
      String icon = (lastDone == todayStr) ? "✅" : "⬜";
      dailySection += "  " + icon + " `[" + String(id) + "]` " + name + "\n";
    } else if (cat == TaskCategory::SCHEDULED && isScheduledForToday(t["when"] | "")) {
      scheduledTodaySection += "  🔔 `[" + String(id) + "]` " + name + " _(" + String(t["when"] | "") + ")_\n";
    }
  }

  for (JsonVariant v : today) {
    int tid = v.as<int>();
    for (JsonObject t : tasks) {
      if ((t["id"] | 0) == tid) {
        TaskCategory cat = keyToCategory(t["type"] | "immediate");
        if (cat != TaskCategory::DAILY) {
          manualTodaySection += "  ⚡ `[" + String(tid) + "]` " + String(t["name"] | "") + "\n";
        }
        break;
      }
    }
  }

  if (dailySection.length() == 0 && scheduledTodaySection.length() == 0 && manualTodaySection.length() == 0) {
    _telegram.sendMessage(chatId, "📭 Nenhuma tarefa diária ou agendada para hoje (" + todayStr + ").");
    return;
  }

  String out = "📅 *Agenda de Hoje (" + todayStr + "):*\n";
  if (dailySection.length() > 0) {
    out += "\n🔄 *Diárias:*\n" + dailySection;
  }
  if (scheduledTodaySection.length() > 0) {
    out += "\n🔔 *Agendadas para Hoje:*\n" + scheduledTodaySection;
  }
  if (manualTodaySection.length() > 0) {
    out += "\n📌 *Pontuais Selecionadas para Hoje:*\n" + manualTodaySection;
  }
  out += "\n💡 _Use `/done <id>` para concluir uma tarefa._";
  _telegram.sendMessage(chatId, out);
}

void TaskManagerBot::cmdTodayRemove(const String& chatId, int taskId) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray today = doc["today"].as<JsonArray>();

  bool removed = false;
  for (size_t i = 0; i < today.size(); i++) {
    if (today[i].as<int>() == taskId) {
      today.remove(i);
      removed = true;
      break;
    }
  }

  if (removed) {
    _store.save(_dataFilePath, doc);
    _telegram.sendMessage(chatId, "➖ Tarefa #" + String(taskId) + " removida da seleção de hoje.");
  } else {
    _telegram.sendMessage(chatId, "⚠️ Essa tarefa não está na seleção manual de hoje.");
  }
}

void TaskManagerBot::cmdTodayClear(const String& chatId) {
  JsonDocument doc;
  loadTasksData(doc);
  doc["today"].to<JsonArray>();
  _store.save(_dataFilePath, doc);
  _telegram.sendMessage(chatId, "🧹 Seleção manual de tarefas de hoje foi limpa.");
}

void TaskManagerBot::cmdHelp(const String& chatId) {
  String helpText =
      "📋 *Gerenciador de Tarefas (3 Categorias):*\n\n"
      "🔄 *1. Tarefas Diárias (Todos os dias, indefinidamente):*\n"
      "• `/daily <tarefa>` : Cria tarefa diária recorrente\n"
      "• `/listdaily` : Lista apenas as tarefas diárias\n\n"
      "⚡ *2. Tarefas Pontuais — Fazer Agora (Imediatas):*\n"
      "• `/add <tarefa>` (ou `/now <tarefa>`) : Cria tarefa imediata\n"
      "• `/listnow` : Lista apenas as tarefas imediatas\n\n"
      "🗓️ *3. Tarefas Pontuais — Futuras / Agendadas:*\n"
      "• `/schedule [DD/MM] <tarefa>` (ou `/future <tarefa>`) : Cria tarefa futura/agendada\n"
      "• `/listfuture` : Lista apenas as tarefas futuras\n\n"
      "✅ *Concluir, Mover e Gerenciar:*\n"
      "• `/list` : Painel completo segregado nas 3 categorias\n"
      "• `/done <id>` : Conclui tarefa (diária = marca ✅ hoje; pontual = elimina)\n"
      "• `/undone <id>` : Desmarca tarefa diária feita hoje\n"
      "• `/move <id> <daily|now|future> [DD/MM]` : Muda categoria da tarefa\n"
      "• `/remove <id>` : Exclui permanentemente qualquer tarefa\n"
      "• `/todaylist` : Mostra as diárias + agendadas para hoje + `/today <id>`\n\n"
      "💬 *Linguagem Natural:*\n"
      "• _\"Tarefa diária tomar creatina\"_\n"
      "• _\"Adicionar tarefa trocar lâmpada\"_\n"
      "• _\"Agendar tarefa 15/12 consulta médica\"_\n"
      "• _\"Tarefa futura renovar CNH\"_\n"
      "• _\"Concluir tarefa 3\"_";
  _telegram.sendMessage(chatId, helpText);
}

void TaskManagerBot::handleNaturalLanguage(const String& chatId, const String& rawText) {
  String clean = TextUtils::normalizeLower(rawText);

  if (clean == "ajuda" || clean == "comandos") {
    cmdHelp(chatId);
    return;
  }

  // Listagens por categoria em linguagem natural
  if (clean == "tarefas diarias" || clean == "listar diarias" || clean == "minhas tarefas diarias" ||
      clean == "diarias" || clean == "ver diarias") {
    cmdListTasks(chatId, static_cast<int>(TaskCategory::DAILY), true);
    return;
  }

  if (clean == "tarefas imediatas" || clean == "tarefas para agora" || clean == "listar imediatas" ||
      clean == "tarefas pontuais" || clean == "fazer agora") {
    cmdListTasks(chatId, static_cast<int>(TaskCategory::IMMEDIATE), true);
    return;
  }

  if (clean == "tarefas futuras" || clean == "tarefas agendadas" || clean == "listar futuras" ||
      clean == "listar agendadas" || clean == "agendadas" || clean == "futuras") {
    cmdListTasks(chatId, static_cast<int>(TaskCategory::SCHEDULED), true);
    return;
  }

  // Listar todas as tarefas segregadas
  if (clean == "tarefas" || clean == "listar tarefas" || clean == "minhas tarefas" ||
      clean == "ver tarefas" || clean == "mostrar tarefas" || clean == "lista de tarefas" ||
      clean == "tarefas pendentes") {
    cmdListTasks(chatId, -1, true);
    return;
  }

  // Listar Hoje
  if (clean == "hoje" || clean == "tarefas de hoje" || clean == "ver hoje" ||
      clean == "lista de hoje" || clean == "o que tem para hoje" ||
      clean == "o que temos para hoje" || clean == "quais as tarefas de hoje") {
    cmdTodayList(chatId, true);
    return;
  }

  // Adicionar tarefa para hoje: "adicionar para hoje 20", "hoje 20", "colocar tarefa 20 para hoje"
  if (clean.indexOf("hoje") != -1 &&
      (clean.startsWith("adicionar ") || clean.startsWith("adiciona ") ||
       clean.startsWith("colocar ") || clean.startsWith("coloca ") ||
       clean.startsWith("marcar ") || clean.startsWith("marca ") ||
       clean.startsWith("para hoje ") || clean.startsWith("hoje "))) {
    for (unsigned int i = 0; i < clean.length(); i++) {
      if (isDigit(clean[i])) {
        unsigned int j = i;
        while (j < clean.length() && isDigit(clean[j])) j++;
        String idStr = clean.substring(i, j);
        std::vector<String> singleArg = {idStr};
        cmdTodayAdd(chatId, singleArg, true);
        return;
      }
    }
  }

  // Concluir tarefa vs Remover permanentemente
  if (clean.startsWith("concluir ") || clean.startsWith("conclui ") ||
      clean.startsWith("fiz ") || clean.startsWith("feita ") ||
      clean.startsWith("finalizar ") || clean.startsWith("finaliza ") ||
      clean.startsWith("remover ") || clean.startsWith("remove ") ||
      clean.startsWith("deletar ") || clean.startsWith("deleta ") ||
      clean.startsWith("apagar ") || clean.startsWith("apaga ") ||
      clean.startsWith("excluir ") || clean.startsWith("exclui ") ||
      clean.startsWith("tarefa ")) {
    for (int i = clean.length() - 1; i >= 0; i--) {
      if (isDigit(clean[i])) {
        int endIdx = i + 1;
        int startIdx = i;
        while (startIdx > 0 && isDigit(clean[startIdx - 1])) startIdx--;
        int tid = clean.substring(startIdx, endIdx).toInt();
        if (tid > 0) {
          bool isDoneAction =
              clean.indexOf("conclui") != -1 || clean.indexOf("feita") != -1 ||
              clean.indexOf("feito") != -1 || clean.indexOf("pronta") != -1 ||
              clean.indexOf("finaliz") != -1 || clean.startsWith("fiz ");
          bool isRemoveAction =
              clean.indexOf("remover") != -1 || clean.indexOf("remove") != -1 ||
              clean.indexOf("apaga") != -1 || clean.indexOf("deleta") != -1 ||
              clean.indexOf("exclui") != -1;
          if (isDoneAction) {
            cmdDoneTask(chatId, tid, true);
            return;
          }
          if (isRemoveAction) {
            cmdRemoveTask(chatId, tid, true);
            return;
          }
        }
        break;
      }
    }
  }

  String rawClean = TextUtils::sanitizeText(rawText);

  // 1) Tarefa Diária em linguagem natural: "tarefa diária ...", "adicionar tarefa diária ..."
  int diariaIdx = clean.indexOf("diaria");
  if (diariaIdx == -1) diariaIdx = clean.indexOf("diario");
  if (diariaIdx != -1) {
    int spaceAfter = rawClean.indexOf(' ', diariaIdx);
    if (spaceAfter != -1 && spaceAfter + 1 < static_cast<int>(rawClean.length())) {
      String desc = TextUtils::stripLeadingConnectors(rawClean.substring(spaceAfter + 1));
      if (desc.length() > 0) {
        cmdAddTask(chatId, TextUtils::capitalizeFirst(desc), TaskCategory::DAILY, "", true);
        return;
      }
    }
  }

  // 2) Tarefa Futura / Agendada em linguagem natural: "agendar tarefa ...", "tarefa futura ...", "tarefa agendada ..."
  int futuraIdx = clean.indexOf("tarefa futura");
  int agendadaIdx = clean.indexOf("tarefa agendada");
  if (futuraIdx != -1 || agendadaIdx != -1) {
    int baseIdx = (futuraIdx != -1) ? (futuraIdx + 7) : (agendadaIdx + 7);
    int spaceAfter = rawClean.indexOf(' ', baseIdx);
    if (spaceAfter != -1 && spaceAfter + 1 < static_cast<int>(rawClean.length())) {
      String restDesc = TextUtils::stripLeadingConnectors(rawClean.substring(spaceAfter + 1));
      String whenStr, desc;
      extractOptionalDatePrefix(restDesc, whenStr, desc);
      if (desc.length() > 0) {
        cmdAddTask(chatId, TextUtils::capitalizeFirst(desc), TaskCategory::SCHEDULED, whenStr, true);
        return;
      }
    }
  }
  if (clean.startsWith("agendar ")) {
    int firstSp = rawClean.indexOf(' ');
    String restDesc = (firstSp != -1) ? rawClean.substring(firstSp + 1) : "";
    restDesc.trim();
    String restLower = TextUtils::normalizeLower(restDesc);
    if (restLower.startsWith("tarefa ")) {
      int tIdx = restDesc.indexOf(' ');
      if (tIdx != -1) restDesc = restDesc.substring(tIdx + 1);
    }
    restDesc = TextUtils::stripLeadingConnectors(restDesc);
    String whenStr, desc;
    extractOptionalDatePrefix(restDesc, whenStr, desc);
    if (desc.length() > 0) {
      cmdAddTask(chatId, TextUtils::capitalizeFirst(desc), TaskCategory::SCHEDULED, whenStr, true);
      return;
    }
  }

  // 3) Tarefa Pontual Imediata: "adicionar tarefa ...", "nova tarefa ...", "tarefa ..."
  int tarefaIdx = clean.indexOf("tarefa");
  if (tarefaIdx != -1 && !clean.endsWith("tarefas")) {
    int spaceAfter = rawClean.indexOf(' ', tarefaIdx);
    if (spaceAfter != -1 && spaceAfter + 1 < static_cast<int>(rawClean.length())) {
      String desc = rawClean.substring(spaceAfter + 1);
      desc = TextUtils::stripLeadingConnectors(desc);
      if (desc.length() > 0) {
        cmdAddTask(chatId, TextUtils::capitalizeFirst(desc), TaskCategory::IMMEDIATE, "", true);
        return;
      }
    }
  }

  _telegram.sendMessage(
      chatId,
      "❓ *Não reconheci esse comando de tarefas.*\n\n"
      "💡 *Exemplos:*\n"
      "• 🔄 _\"Tarefa diária tomar creatina\"_ (`/daily`)\n"
      "• ⚡ _\"Adicionar tarefa comprar lâmpada\"_ (`/add`)\n"
      "• 🗓️ _\"Agendar tarefa 15/12 consulta médica\"_ (`/schedule`)\n"
      "• ✅ _\"Concluir tarefa 1\"_ (`/done 1`)\n"
      "• 📋 _\"Listar tarefas\"_ (`/list`)");
}
