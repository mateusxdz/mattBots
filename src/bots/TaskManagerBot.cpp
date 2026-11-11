#include "bots/TaskManagerBot.h"
#include "bots/TextUtils.h"

TaskManagerBot::TaskManagerBot(ITelegramClient& telegram,
                               IJsonStore& store,
                               const char* dataFilePath,
                               int64_t authorizedUserId)
    : _telegram(telegram),
      _store(store),
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
  if (cmd == "add") {
    if (rest.length() == 0) {
      _telegram.sendMessage(chatId, "Use: /add <task_description>", "");
      return;
    }
    cmdAddTask(chatId, rest, false);
  } else if (cmd == "remove") {
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
  } else if (cmd == "list") {
    cmdListTasks(chatId, false);
  } else if (cmd == "clear" || cmd == "cleartasks") {
    cmdClearTasks(chatId, args);
  } else if (cmd == "today") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "Use: /today <id1> <id2> ...", "");
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

void TaskManagerBot::cmdAddTask(const String& chatId, const String& taskName, bool naturalStyle) {
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
  _store.save(_dataFilePath, doc);

  if (naturalStyle) {
    _telegram.sendMessage(chatId, "✅ *Tarefa #" + String(nextId) + " adicionada:* " + taskName);
  } else {
    _telegram.sendMessage(chatId, "✅ Added task #" + String(nextId) + ": " + taskName, "");
  }
}

void TaskManagerBot::cmdRemoveTask(const String& chatId, int taskId, bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();
  JsonArray today = doc["today"].as<JsonArray>();

  bool found = false;
  for (size_t i = 0; i < tasks.size(); i++) {
    if ((tasks[i]["id"] | 0) == taskId) {
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

  if (naturalStyle) {
    if (found) {
      _telegram.sendMessage(chatId, "🗑️ *Tarefa #" + String(taskId) + " removida!*");
    } else {
      _telegram.sendMessage(chatId, "⚠️ Tarefa #" + String(taskId) + " não encontrada.");
    }
  } else {
    _telegram.sendMessage(chatId, "🗑️ Removed task #" + String(taskId), "");
  }
}

void TaskManagerBot::cmdListTasks(const String& chatId, bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();

  if (tasks.size() == 0) {
    _telegram.sendMessage(chatId, naturalStyle ? "📭 Nenhuma tarefa pendente." : "No tasks yet.", "");
    return;
  }

  String out = naturalStyle ? "📋 *Tarefas:*\n" : "📋 *Tasks:*\n";
  for (JsonObject t : tasks) {
    out += "[" + String(t["id"] | 0) + "] " + String(t["name"] | "") + "\n";
  }
  _telegram.sendMessage(chatId, out);
}

void TaskManagerBot::cmdClearTasks(const String& chatId, const std::vector<String>& args) {
  if (args.empty() || !args[0].equalsIgnoreCase("confirm")) {
    _telegram.sendMessage(chatId, "⚠️ To clear all tasks, type: /clear confirm", "");
    return;
  }
  JsonDocument doc;
  loadTasksData(doc);
  doc["tasks"].to<JsonArray>();
  _store.save(_dataFilePath, doc);
  _telegram.sendMessage(chatId, "🧹 Cleared all tasks.", "");
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
    if (naturalStyle) {
      _telegram.sendMessage(chatId, "📌 *Tarefa #" + listStr + " adicionada para hoje!*");
    } else {
      _telegram.sendMessage(chatId, "📌 Added to today: " + listStr, "");
    }
  } else {
    _telegram.sendMessage(chatId, naturalStyle ? "⚠️ Tarefa não encontrada ou já está na lista de hoje." : "Nothing added.", "");
  }
}

void TaskManagerBot::cmdTodayList(const String& chatId, bool naturalStyle) {
  JsonDocument doc;
  loadTasksData(doc);
  JsonArray tasks = doc["tasks"].as<JsonArray>();
  JsonArray today = doc["today"].as<JsonArray>();

  if (today.size() == 0) {
    _telegram.sendMessage(
        chatId,
        naturalStyle ? "📭 Nenhuma tarefa agendada para hoje." : "📭 No tasks for today.",
        "");
    return;
  }

  String out = naturalStyle ? "📅 *Tarefas de Hoje:*\n" : "📅 *Today:*\n";
  for (JsonVariant v : today) {
    int tid = v.as<int>();
    String taskName = "❓ Unknown task";
    for (JsonObject t : tasks) {
      if ((t["id"] | 0) == tid) {
        taskName = t["name"] | "";
        break;
      }
    }
    out += "[" + String(tid) + "] " + taskName + "\n";
  }
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
    _telegram.sendMessage(chatId, "➖ Removed #" + String(taskId) + " from today.", "");
  } else {
    _telegram.sendMessage(chatId, "Task not in today list.", "");
  }
}

void TaskManagerBot::cmdTodayClear(const String& chatId) {
  JsonDocument doc;
  loadTasksData(doc);
  doc["today"].to<JsonArray>();
  _store.save(_dataFilePath, doc);
  _telegram.sendMessage(chatId, "🧹 Cleared today's tasks.", "");
}

void TaskManagerBot::cmdHelp(const String& chatId) {
  String helpText =
      "📋 *Comandos do Gerenciador de Tarefas (Tasks):*\n\n"
      "• `/add <tarefa>` : Adiciona uma nova tarefa\n"
      "• `/remove <id>` : Remove uma tarefa pelo ID\n"
      "• `/list` : Lista todas as tarefas pendentes\n"
      "• `/clear confirm` : Limpa todas as tarefas\n\n"
      "📅 *Tarefas de Hoje:*\n"
      "• `/today <id1> <id2>` : Adiciona tarefas para a lista de hoje\n"
      "• `/todaylist` : Exibe tarefas agendadas para hoje\n"
      "• `/todayremove <id>` : Remove tarefa da lista de hoje\n"
      "• `/todayclear` : Limpa as tarefas de hoje\n\n"
      "💬 *Você também pode digitar livremente (ex: \"tarefas de hoje\", \"adicionar tarefa pagar boleto\").*";
  _telegram.sendMessage(chatId, helpText);
}

void TaskManagerBot::handleNaturalLanguage(const String& chatId, const String& rawText) {
  String clean = TextUtils::normalizeLower(rawText);

  if (clean == "ajuda" || clean == "comandos") {
    cmdHelp(chatId);
    return;
  }

  // Listar Tarefas
  if (clean == "tarefas" || clean == "listar tarefas" || clean == "minhas tarefas" ||
      clean == "ver tarefas" || clean == "mostrar tarefas" || clean == "lista de tarefas" ||
      clean == "tarefas pendentes") {
    cmdListTasks(chatId, true);
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

  // Remover tarefa: "remover tarefa 5", "concluir tarefa 5", "apagar tarefa 5"
  if (clean.startsWith("remover ") || clean.startsWith("remove ") ||
      clean.startsWith("deletar ") || clean.startsWith("deleta ") ||
      clean.startsWith("apagar ") || clean.startsWith("apaga ") ||
      clean.startsWith("concluir ") || clean.startsWith("conclui ") ||
      clean.startsWith("excluir ") || clean.startsWith("exclui ") ||
      clean.startsWith("tarefa ")) {
    for (int i = clean.length() - 1; i >= 0; i--) {
      if (isDigit(clean[i])) {
        int endIdx = i + 1;
        int startIdx = i;
        while (startIdx > 0 && isDigit(clean[startIdx - 1])) startIdx--;
        int tid = clean.substring(startIdx, endIdx).toInt();
        if (tid > 0 && (clean.indexOf("remover") != -1 || clean.indexOf("remove") != -1 ||
                        clean.indexOf("apaga") != -1 || clean.indexOf("deleta") != -1 ||
                        clean.indexOf("conclui") != -1 || clean.indexOf("feita") != -1 ||
                        clean.indexOf("pronta") != -1 || clean.indexOf("exclui") != -1)) {
          cmdRemoveTask(chatId, tid, true);
          return;
        }
        break;
      }
    }
  }

  // Adicionar tarefa: "adicionar tarefa ...", "nova tarefa ...", "tarefa ..."
  String rawClean = TextUtils::sanitizeText(rawText);
  int tarefaIdx = clean.indexOf("tarefa");
  if (tarefaIdx != -1 && !clean.endsWith("tarefas")) {
    int afterWord = tarefaIdx + 6; // strlen("tarefa")
    if (afterWord < static_cast<int>(rawClean.length())) {
      String desc = rawClean.substring(afterWord);
      desc = TextUtils::stripLeadingConnectors(desc);
      if (desc.length() > 0) {
        cmdAddTask(chatId, TextUtils::capitalizeFirst(desc), true);
        return;
      }
    }
  }

  _telegram.sendMessage(
      chatId,
      "❓ *Não reconheci esse comando de tarefas.*\n\n"
      "💡 *Exemplos:*\n"
      "• 📋 _\"Adicionar tarefa comprar pão\"_\n"
      "• 📋 _\"Tarefas de hoje\"_\n"
      "• 📋 _\"Listar tarefas\"_");
}

