#include "bots/GymCalorieBot.h"
#include "bots/TextUtils.h"
#include <math.h>

GymCalorieBot::GymCalorieBot(ITelegramClient& telegram,
                             IJsonStore& store,
                             ITimeProvider& timeProvider,
                             const char* dataFilePath,
                             int64_t authorizedUserId)
    : _telegram(telegram),
      _store(store),
      _timeProvider(timeProvider),
      _dataFilePath(dataFilePath),
      _authorizedUserId(authorizedUserId) {}

void GymCalorieBot::begin() {
  JsonDocument doc;
  if (!_store.load(_dataFilePath, doc)) {
    doc["gym"]["plan"] = "";
    doc["calories"]["target"] = 0.0f;
    doc["calories"]["entries"].to<JsonArray>();
    _store.save(_dataFilePath, doc);
  }
}

void GymCalorieBot::loadGymData(JsonDocument& doc) {
  if (!_store.load(_dataFilePath, doc)) {
    doc.clear();
  }
  if (!doc["gym"].is<JsonObject>()) {
    doc["gym"].to<JsonObject>()["plan"] = "";
  }
  if (!doc["calories"].is<JsonObject>()) {
    JsonObject cal = doc["calories"].to<JsonObject>();
    cal["target"] = 0.0f;
    cal["entries"].to<JsonArray>();
  } else if (!doc["calories"]["entries"].is<JsonArray>()) {
    doc["calories"]["entries"].to<JsonArray>();
  }
}

String GymCalorieBot::fmtKcal(float val) {
  char buf[32];
  if (fabsf(val - roundf(val)) < 0.05f) {
    snprintf(buf, sizeof(buf), "%d", static_cast<int>(roundf(val)));
  } else {
    snprintf(buf, sizeof(buf), "%.1f", val);
  }
  return String(buf);
}

String GymCalorieBot::formatGymPlan(const String& plan) {
  String p = plan;
  p.trim();
  if (p.length() == 0) {
    return "🏋️‍♂️ *Plano de Treino Vazio*\n\n"
           "Você ainda não cadastrou o seu treino.\n\n"
           "💡 *Para salvar o treino inteiro, use:*\n"
           "`/gym set <seu treino completo>`";
  }
  return "🏋️‍♂️ *Seu Plano de Treino Atual:*\n\n" + p;
}

String GymCalorieBot::formatProgressMsg(JsonDocument& doc, const String& dateStr) const {
  float target = doc["calories"]["target"] | 0.0f;
  JsonArray entries = doc["calories"]["entries"].as<JsonArray>();

  float totalConsumed = 0.0f;
  for (JsonObject e : entries) {
    if (String(e["date"] | "") == dateStr) {
      totalConsumed += (e["calories"] | 0.0f);
    }
  }

  String consumedStr = fmtKcal(totalConsumed);
  if (target > 0.0f) {
    String targetStr = fmtKcal(target);
    float pct = (totalConsumed / target) * 100.0f;
    float remaining = target - totalConsumed;

    String statusLine;
    if (remaining > 0.05f) {
      statusLine = "🎯 Faltam *" + fmtKcal(remaining) + " kcal* para bater a sua meta!";
    } else if (fabsf(remaining) <= 0.05f) {
      statusLine = "🎉 *Meta calórica atingida perfeitamente!* 🎯";
    } else {
      statusLine = "⚠️ *Meta ultrapassada em " + fmtKcal(fabsf(remaining)) + " kcal!*";
    }

    char pctBuf[16];
    snprintf(pctBuf, sizeof(pctBuf), "%.1f%%", pct);

    return "📊 *Progresso de Hoje (" + dateStr + "):*\n"
           "🔥 Consumido: *" + consumedStr + " / " + targetStr + " kcal* (" + String(pctBuf) + ")\n" +
           statusLine;
  }

  return "📊 *Consumido hoje (" + dateStr + "):* *" + consumedStr + " kcal*\n"
         "💡 _Configure sua meta diária com `/calmeta <valor>` (ex: `/calmeta 2500`)_";
}

void GymCalorieBot::poll() {
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

void GymCalorieBot::handleMessage(const TelegramMessage& msg) {
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

void GymCalorieBot::handleSlashCommand(const String& chatId,
                                       const String& cmd,
                                       const String& rest,
                                       const std::vector<String>& args) {
  if (cmd == "gym" || cmd == "treino") {
    cmdGym(chatId, rest, args);
  } else if (cmd == "comi" || cmd == "cal" || cmd == "eat") {
    if (args.empty()) {
      cmdShowStatus(chatId);
      return;
    }
    String calStr = args[0];
    calStr.replace(",", ".");
    calStr.toLowerCase();
    calStr.replace("kcal", "");
    float calVal = calStr.toFloat();
    if (calVal <= 0.0f) {
      _telegram.sendMessage(
          chatId,
          "⚠️ *Uso correto:*\n`/comi <calorias> [alimento]`\n\n*Exemplos:*\n• `/comi 300 arroz`");
      return;
    }
    String foodDesc;
    for (size_t i = 1; i < args.size(); i++) {
      if (i > 1) foodDesc += " ";
      foodDesc += args[i];
    }
    cmdAddCalories(chatId, calVal, foodDesc);
  } else if (cmd == "calmeta" || cmd == "caltarget" || cmd == "setcal") {
    cmdSetTarget(chatId, args);
  } else if (cmd == "callist" || cmd == "caltoday" || cmd == "calstatus" || cmd == "calorias") {
    cmdShowStatus(chatId);
  } else if (cmd == "caledit") {
    cmdEditCalorie(chatId, args);
  } else if (cmd == "caldel" || cmd == "calremove" || cmd == "removercal") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "⚠️ *Uso:* `/caldel <id>` (ex: `/caldel 3`)");
      return;
    }
    String idStr = args[0];
    idStr.replace("#", "");
    int id = idStr.toInt();
    if (id <= 0) {
      _telegram.sendMessage(chatId, "⚠️ ID inválido.");
      return;
    }
    cmdDeleteCalorie(chatId, id);
  } else if (cmd == "calreset" || cmd == "resetcal") {
    bool confirmed = (!args.empty() && args[0].equalsIgnoreCase("confirm"));
    cmdResetCalories(chatId, confirmed);
  } else if (cmd == "calhelp" || cmd == "help" || cmd == "start" || cmd == "voicehelp") {
    cmdHelp(chatId);
  }
}

void GymCalorieBot::cmdGym(const String& chatId,
                           const String& rest,
                           const std::vector<String>& args) {
  JsonDocument doc;
  loadGymData(doc);
  String plan = doc["gym"]["plan"] | "";

  if (args.empty() || args[0].equalsIgnoreCase("get") ||
      args[0].equalsIgnoreCase("view") || args[0].equalsIgnoreCase("ver")) {
    _telegram.sendMessage(chatId, formatGymPlan(plan));
    return;
  }

  if (args[0].equalsIgnoreCase("set")) {
    int setIdx = rest.indexOf(args[0]);
    String newPlan = (setIdx != -1) ? rest.substring(setIdx + args[0].length()) : "";
    newPlan.trim();
    if (newPlan.length() == 0) {
      _telegram.sendMessage(
          chatId,
          "⚠️ *Uso:* `/gym set <texto do seu treino completo>`");
      return;
    }
    doc["gym"]["plan"] = newPlan;
    _store.save(_dataFilePath, doc);
    _telegram.sendMessage(
        chatId,
        "✅ *Plano de treino salvo com sucesso!*\n\n🏋️‍♂️ *Seu treino:*\n" + newPlan);
    return;
  }

  _telegram.sendMessage(
      chatId,
      "💡 *Como usar o Gym:*\n\n"
      "• `/gym` ou `/gym get` : Visualiza seu plano de treino\n"
      "• `/gym set <texto>` : Salva/substitui seu plano de treino completo");
}

void GymCalorieBot::cmdAddCalories(const String& chatId, float calories, const String& foodDesc) {
  JsonDocument doc;
  loadGymData(doc);
  JsonArray entries = doc["calories"]["entries"].as<JsonArray>();

  int maxId = 0;
  for (JsonObject e : entries) {
    int id = e["id"] | 0;
    if (id > maxId) maxId = id;
  }
  int nextId = maxId + 1;

  String cleanFood = foodDesc;
  cleanFood.trim();
  if (cleanFood.length() == 0) {
    cleanFood = "Alimento sem descrição";
  } else {
    cleanFood = TextUtils::capitalizeFirst(cleanFood);
  }

  String todayStr = _timeProvider.todayStr();
  String timeStr = _timeProvider.nowTimeStr();

  JsonObject entry = entries.add<JsonObject>();
  entry["id"] = nextId;
  entry["calories"] = calories;
  entry["food"] = cleanFood;
  entry["date"] = todayStr;
  entry["time"] = timeStr;

  _store.save(_dataFilePath, doc);

  String resp = "🍽️ *Ingestão Registrada!* (ID `#" + String(nextId) + "`)\n"
                "➕ *+" + fmtKcal(calories) + " kcal* | " + cleanFood + " às " + timeStr + "\n\n" +
                formatProgressMsg(doc, todayStr);
  _telegram.sendMessage(chatId, resp);
}

void GymCalorieBot::cmdSetTarget(const String& chatId, const std::vector<String>& args) {
  JsonDocument doc;
  loadGymData(doc);

  if (args.empty()) {
    float cur = doc["calories"]["target"] | 0.0f;
    _telegram.sendMessage(
        chatId,
        "🎯 *Meta calórica atual:* " + fmtKcal(cur) + " kcal\n\n"
        "Para alterar, use: `/calmeta <valor>` (ex: `/calmeta 2500`)");
    return;
  }

  String valStr = args[0];
  valStr.replace(",", ".");
  valStr.toLowerCase();
  valStr.replace("kcal", "");
  float target = valStr.toFloat();
  if (target <= 0.0f) {
    _telegram.sendMessage(chatId, "⚠️ Insira um valor numérico válido para a meta (ex: `/calmeta 2200`).");
    return;
  }

  doc["calories"]["target"] = target;
  _store.save(_dataFilePath, doc);

  String todayStr = _timeProvider.todayStr();
  _telegram.sendMessage(
      chatId,
      "🎯 *Meta calórica diária definida para " + fmtKcal(target) + " kcal!*\n\n" +
          formatProgressMsg(doc, todayStr));
}

void GymCalorieBot::cmdShowStatus(const String& chatId) {
  JsonDocument doc;
  loadGymData(doc);
  String todayStr = _timeProvider.todayStr();
  JsonArray entries = doc["calories"]["entries"].as<JsonArray>();

  String listLines;
  int todayCount = 0;
  for (JsonObject e : entries) {
    if (String(e["date"] | "") == todayStr) {
      todayCount++;
      String t = e["time"] | "";
      String timeTag = (t.length() > 0) ? (" às " + t) : "";
      listLines += "• `[#" + String(e["id"] | 0) + "]` *" +
                   fmtKcal(e["calories"] | 0.0f) + " kcal* — " +
                   String(e["food"] | "") + timeTag + "\n";
    }
  }

  if (todayCount == 0) {
    String msg = "🍽️ *Nenhuma caloria anotada hoje (" + todayStr + ").*\n\n" +
                 formatProgressMsg(doc, todayStr) + "\n\n"
                 "💡 *Para registrar refeições:*\n"
                 "• `/comi 300 arroz e feijão`\n"
                 "• Ou digite: _\"comi 300 calorias de arroz\"_";
    _telegram.sendMessage(chatId, msg);
    return;
  }

  String out = "🍽️ *Refeições de Hoje (" + todayStr + "):*\n\n" +
               listLines + "\n" +
               formatProgressMsg(doc, todayStr) + "\n\n"
               "────────────────────\n"
               "💡 _Editar: `/caledit <id> <kcal> [alimento]` | Remover: `/caldel <id>`_";
  _telegram.sendMessage(chatId, out);
}

void GymCalorieBot::cmdEditCalorie(const String& chatId, const std::vector<String>& args) {
  if (args.size() < 2) {
    _telegram.sendMessage(
        chatId,
        "⚠️ *Uso correto:*\n`/caledit <id> <novas_calorias> [novo alimento]`");
    return;
  }

  String idStr = args[0];
  idStr.replace("#", "");
  int entryId = idStr.toInt();

  String calStr = args[1];
  calStr.replace(",", ".");
  calStr.toLowerCase();
  calStr.replace("kcal", "");
  float newCal = calStr.toFloat();
  if (entryId <= 0 || newCal <= 0.0f) {
    _telegram.sendMessage(chatId, "⚠️ ID ou valor calórico inválido.");
    return;
  }

  JsonDocument doc;
  loadGymData(doc);
  JsonArray entries = doc["calories"]["entries"].as<JsonArray>();

  for (JsonObject e : entries) {
    if ((e["id"] | 0) == entryId) {
      float oldCal = e["calories"] | 0.0f;
      String oldFood = e["food"] | "";
      e["calories"] = newCal;

      if (args.size() > 2) {
        String newFood;
        for (size_t i = 2; i < args.size(); i++) {
          if (i > 2) newFood += " ";
          newFood += args[i];
        }
        e["food"] = TextUtils::capitalizeFirst(newFood);
      }

      String entryDate = e["date"] | _timeProvider.todayStr();
      String updatedFood = e["food"] | "";
      _store.save(_dataFilePath, doc);

      String msg = "✏️ *Registro `#" + String(entryId) + "` atualizado!*\n"
                   "De: " + fmtKcal(oldCal) + " kcal (" + oldFood + ")\n"
                   "Para: *" + fmtKcal(newCal) + " kcal* (" + updatedFood + ")\n\n" +
                   formatProgressMsg(doc, entryDate);
      _telegram.sendMessage(chatId, msg);
      return;
    }
  }

  _telegram.sendMessage(chatId, "⚠️ Registro de caloria `#" + String(entryId) + "` não encontrado.");
}

void GymCalorieBot::cmdDeleteCalorie(const String& chatId, int entryId) {
  JsonDocument doc;
  loadGymData(doc);
  JsonArray entries = doc["calories"]["entries"].as<JsonArray>();

  for (size_t i = 0; i < entries.size(); i++) {
    if ((entries[i]["id"] | 0) == entryId) {
      float removedCal = entries[i]["calories"] | 0.0f;
      String removedFood = entries[i]["food"] | "";
      String entryDate = entries[i]["date"] | _timeProvider.todayStr();
      entries.remove(i);
      _store.save(_dataFilePath, doc);

      String msg = "🗑️ *Registro `#" + String(entryId) + "` removido!* (-" +
                   fmtKcal(removedCal) + " kcal de " + removedFood + ")\n\n" +
                   formatProgressMsg(doc, entryDate);
      _telegram.sendMessage(chatId, msg);
      return;
    }
  }

  _telegram.sendMessage(chatId, "⚠️ Registro `#" + String(entryId) + "` não encontrado.");
}

void GymCalorieBot::cmdResetCalories(const String& chatId, bool confirmed) {
  if (!confirmed) {
    _telegram.sendMessage(
        chatId,
        "⚠️ *Atenção:* Isso irá apagar todas as refeições registradas no dia de HOJE.\n\n"
        "Para confirmar, envie:\n`/calreset confirm`");
    return;
  }

  JsonDocument doc;
  loadGymData(doc);
  String todayStr = _timeProvider.todayStr();
  JsonArray entries = doc["calories"]["entries"].as<JsonArray>();

  int removed = 0;
  for (int i = static_cast<int>(entries.size()) - 1; i >= 0; i--) {
    if (String(entries[i]["date"] | "") == todayStr) {
      entries.remove(i);
      removed++;
    }
  }

  _store.save(_dataFilePath, doc);
  _telegram.sendMessage(
      chatId,
      "🧹 *Calorias de hoje zeradas com sucesso!* (" + String(removed) + " refeições removidas).");
}

void GymCalorieBot::cmdHelp(const String& chatId) {
  String helpText =
      "🔥 *Comandos do Contador de Calorias & Academia*\n\n"
      "• `/comi <calorias> [alimento]` : Registra uma refeição (ex: `/comi 300 arroz`)\n"
      "• `/callist` : Mostra as refeições e progresso de hoje\n"
      "• `/calmeta <valor>` : Configura sua meta calórica diária (ex: `/calmeta 2500`)\n"
      "• `/caledit <id> <calorias> [alimento]` : Edita um registro por ID\n"
      "• `/caldel <id>` : Remove um registro por ID\n"
      "• `/calreset confirm` : Zera as refeições de hoje\n"
      "• `/gym` ou `/gym set <texto>` : Visualiza ou salva seu treino\n\n"
      "💬 *Dica:* Você também pode digitar livremente:\n"
      "_\"comi 300 calorias de arroz\"_\n"
      "_\"quantas calorias faltam?\"_\n"
      "_\"meu treino\"_";
  _telegram.sendMessage(chatId, helpText);
}

void GymCalorieBot::handleNaturalLanguage(const String& chatId, const String& rawText) {
  String clean = TextUtils::normalizeLower(rawText);

  if (clean == "ajuda" || clean == "comandos") {
    cmdHelp(chatId);
    return;
  }

  // Status de calorias
  if (clean == "calorias" || clean == "minhas calorias" || clean == "ver calorias" ||
      clean == "quantas calorias" || clean == "quantas calorias faltam" ||
      clean == "quanto falta de calorias" || clean == "saldo de calorias" ||
      clean == "calorias de hoje" || clean == "listar calorias") {
    cmdShowStatus(chatId);
    return;
  }

  // Ver treino
  if (clean == "treino" || clean == "meu treino" || clean == "ver treino" ||
      clean == "plano de treino" || clean == "mostrar treino") {
    std::vector<String> emptyArgs;
    cmdGym(chatId, "", emptyArgs);
    return;
  }

  // Salvar treino: "salvar treino ..."
  if (clean.startsWith("salvar treino ") || clean.startsWith("cadastrar treino ") ||
      clean.startsWith("atualizar treino ") || clean.startsWith("definir treino ")) {
    int idx = clean.indexOf("treino");
    if (idx != -1) {
      String newPlan = rawText.substring(idx + 6);
      newPlan = TextUtils::stripLeadingConnectors(newPlan);
      if (newPlan.length() > 0) {
        JsonDocument doc;
        loadGymData(doc);
        doc["gym"]["plan"] = newPlan;
        _store.save(_dataFilePath, doc);
        _telegram.sendMessage(
            chatId,
            "✅ *Plano de treino salvo com sucesso!*\n\n🏋️‍♂️ *Seu treino:*\n" + newPlan);
        return;
      }
    }
  }

  // Resetar calorias
  if (clean == "resetar calorias" || clean == "zerar calorias" || clean == "limpar calorias" ||
      clean == "resetar as calorias" || clean == "zerar as calorias") {
    cmdResetCalories(chatId, true);
    return;
  }

  // Remover caloria: "remover caloria 3"
  if (clean.indexOf("caloria") != -1 &&
      (clean.startsWith("remover ") || clean.startsWith("remove ") ||
       clean.startsWith("apagar ") || clean.startsWith("deletar ") ||
       clean.startsWith("excluir "))) {
    for (int i = clean.length() - 1; i >= 0; i--) {
      if (isDigit(clean[i])) {
        int endIdx = i + 1;
        int startIdx = i;
        while (startIdx > 0 && isDigit(clean[startIdx - 1])) startIdx--;
        int cid = clean.substring(startIdx, endIdx).toInt();
        if (cid > 0) {
          cmdDeleteCalorie(chatId, cid);
          return;
        }
        break;
      }
    }
  }

  // Meta calórica: "meta calorica 2200"
  if (clean.indexOf("meta") != -1) {
    for (unsigned int i = 0; i < clean.length(); i++) {
      if (isDigit(clean[i])) {
        unsigned int j = i;
        while (j < clean.length() && (isDigit(clean[j]) || clean[j] == '.' || clean[j] == ',')) j++;
        String numStr = clean.substring(i, j);
        std::vector<String> args = {numStr};
        cmdSetTarget(chatId, args);
        return;
      }
    }
  }

  // Registrar calorias: "comi 300 calorias de arroz", "300 kcal de frango"
  const char* eatVerbs[] = {
      "comi ", "ingeri ", "consumi ", "anotar ", "anota ",
      "adicione ", "adicionar ", "adiciona ", "registrar ", "registra "};
  bool hasEatVerb = false;
  for (const char* v : eatVerbs) {
    if (clean.startsWith(v)) {
      hasEatVerb = true;
      break;
    }
  }

  if (hasEatVerb || clean.indexOf("caloria") != -1 || clean.indexOf("kcal") != -1) {
    int numStart = -1;
    int numEnd = -1;
    for (unsigned int i = 0; i < clean.length(); i++) {
      if (isDigit(clean[i])) {
        numStart = i;
        unsigned int j = i;
        while (j < clean.length() && (isDigit(clean[j]) || clean[j] == '.' || clean[j] == ',')) j++;
        numEnd = j;
        break;
      }
    }

    if (numStart != -1 && numEnd != -1) {
      String numStr = clean.substring(numStart, numEnd);
      numStr.replace(",", ".");
      float calVal = numStr.toFloat();
      if (calVal > 0.0f) {
        String tail = rawText.substring(numEnd);
        tail.trim();
        String tailLower = TextUtils::normalizeLower(tail);
        if (tailLower.startsWith("calorias")) {
          tail = tail.substring(8);
        } else if (tailLower.startsWith("caloria")) {
          tail = tail.substring(7);
        } else if (tailLower.startsWith("kcal")) {
          tail = tail.substring(4);
        }
        tail = TextUtils::stripLeadingConnectors(tail);
        cmdAddCalories(chatId, calVal, tail);
        return;
      }
    }
  }

  _telegram.sendMessage(
      chatId,
      "❓ *Não reconheci esse comando de academia/calorias.*\n\n"
      "💡 *Exemplos:*\n"
      "• 🔥 _\"Comi 300 calorias de arroz\"_\n"
      "• 🔥 _\"Quantas calorias faltam?\"_\n"
      "• 🏋️‍♂️ _\"Meu treino\"_");
}

