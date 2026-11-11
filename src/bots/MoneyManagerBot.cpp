#include "bots/MoneyManagerBot.h"
#include "bots/TextUtils.h"
#include <math.h>

MoneyManagerBot::MoneyManagerBot(ITelegramClient& telegram,
                                 IJsonStore& store,
                                 ITimeProvider& timeProvider,
                                 const char* dataFilePath,
                                 int64_t authorizedUserId)
    : _telegram(telegram),
      _store(store),
      _timeProvider(timeProvider),
      _dataFilePath(dataFilePath),
      _authorizedUserId(authorizedUserId) {}

void MoneyManagerBot::begin() {
  JsonDocument doc;
  if (!_store.load(_dataFilePath, doc)) {
    doc["money"]["spendings"].to<JsonArray>();
    _store.save(_dataFilePath, doc);
  }
}

void MoneyManagerBot::loadMoneyData(JsonDocument& doc) {
  if (!_store.load(_dataFilePath, doc)) {
    doc.clear();
  }
  if (!doc["money"].is<JsonObject>()) {
    doc["money"].to<JsonObject>();
  }
  if (!doc["money"]["spendings"].is<JsonArray>()) {
    doc["money"]["spendings"].to<JsonArray>();
  }
}

String MoneyManagerBot::fmtMoney(float val) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%.2f", val);
  return String(buf);
}

void MoneyManagerBot::poll() {
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

void MoneyManagerBot::handleMessage(const TelegramMessage& msg) {
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

void MoneyManagerBot::handleSlashCommand(const String& chatId,
                                         const String& cmd,
                                         const String& rest,
                                         const std::vector<String>& args) {
  if (cmd == "config" || cmd == "configmoney") {
    cmdConfig(chatId, args);
  } else if (cmd == "spend") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "Use: /spend amount description", "");
      return;
    }
    String amtStr = args[0];
    amtStr.replace(",", ".");
    float amount = amtStr.toFloat();
    if (amount <= 0.0f) {
      _telegram.sendMessage(chatId, "Invalid amount.", "");
      return;
    }
    String desc;
    for (size_t i = 1; i < args.size(); i++) {
      if (i > 1) desc += " ";
      desc += args[i];
    }
    cmdSpend(chatId, amount, desc, false);
  } else if (cmd == "futurespend") {
    cmdFutureSpend(chatId, args);
  } else if (cmd == "allowance" || cmd == "saldo") {
    cmdAllowance(chatId, false);
  } else if (cmd == "allowanceplot" || cmd == "plot") {
    cmdAllowancePlot(chatId);
  } else if (cmd == "list" || cmd == "listspend" || cmd == "gastos") {
    cmdListSpend(chatId, false);
  } else if (cmd == "del" || cmd == "delspend" || cmd == "delete" || cmd == "remove") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "Use: /delspend id", "");
      return;
    }
    int sid = args[0].toInt();
    if (sid <= 0) {
      _telegram.sendMessage(chatId, "Invalid ID.", "");
      return;
    }
    cmdDeleteSpend(chatId, sid);
  } else if (cmd == "showconfig") {
    cmdShowConfig(chatId);
  } else if (cmd == "help" || cmd == "start" || cmd == "voicehelp") {
    cmdHelp(chatId);
  }
}

void MoneyManagerBot::cmdConfig(const String& chatId, const std::vector<String>& args) {
  if (args.size() != 4) {
    _telegram.sendMessage(
        chatId,
        "Use:\n/config start_money start_date end_money end_date\nExample:\n/config 3000 01/03/2026 500 31/03/2026",
        "");
    return;
  }

  String s0 = args[0];
  s0.replace(",", ".");
  float startMoney = s0.toFloat();
  String startDate = args[1];

  String s2 = args[2];
  s2.replace(",", ".");
  float endMoney = s2.toFloat();
  String endDate = args[3];

  int32_t dStart = 0, dEnd = 0;
  if (!_timeProvider.parseDateToEpochDays(startDate, dStart) ||
      !_timeProvider.parseDateToEpochDays(endDate, dEnd) || dEnd < dStart) {
    _telegram.sendMessage(chatId, "⚠️ Invalid date format or range. Use DD/MM/YYYY.", "");
    return;
  }

  JsonDocument doc;
  loadMoneyData(doc);
  JsonObject cfg = doc["money"]["config"].to<JsonObject>();
  cfg["start_money"] = startMoney;
  cfg["start_date"] = startDate;
  cfg["end_money"] = endMoney;
  cfg["end_date"] = endDate;

  _store.save(_dataFilePath, doc);
  _telegram.sendMessage(chatId, "💰 Budget configured.", "");
}

void MoneyManagerBot::cmdSpend(const String& chatId,
                               float amount,
                               const String& desc,
                               bool naturalStyle) {
  JsonDocument doc;
  loadMoneyData(doc);
  JsonArray spendings = doc["money"]["spendings"].as<JsonArray>();

  int maxId = 0;
  for (JsonObject s : spendings) {
    int id = s["id"] | 0;
    if (id > maxId) maxId = id;
  }
  int nextId = maxId + 1;

  String cleanDesc = desc;
  cleanDesc.trim();
  if (cleanDesc.length() == 0) {
    cleanDesc = "Gasto sem descrição";
  }

  JsonObject item = spendings.add<JsonObject>();
  item["id"] = nextId;
  item["amount"] = amount;
  item["description"] = cleanDesc;
  item["date"] = _timeProvider.todayStr();
  item["is_future"] = false;

  _store.save(_dataFilePath, doc);

  if (naturalStyle) {
    _telegram.sendMessage(
        chatId,
        "🧾 *Gasto registrado:* R$ " + fmtMoney(amount) + "\n📝 *Descrição:* " + cleanDesc);
  } else {
    _telegram.sendMessage(chatId, "🧾 Spending recorded: " + fmtMoney(amount), "");
  }
}

void MoneyManagerBot::cmdFutureSpend(const String& chatId, const std::vector<String>& args) {
  if (args.size() < 3) {
    _telegram.sendMessage(chatId, "Use: /futurespend amount DD/MM/YYYY description", "");
    return;
  }

  String amtStr = args[0];
  amtStr.replace(",", ".");
  float amount = amtStr.toFloat();
  if (amount <= 0.0f) {
    _telegram.sendMessage(chatId, "Invalid amount.", "");
    return;
  }

  String dateStr = args[1];
  int32_t spendDays = 0;
  if (!_timeProvider.parseDateToEpochDays(dateStr, spendDays)) {
    _telegram.sendMessage(chatId, "Invalid date format. Use DD/MM/YYYY.", "");
    return;
  }

  int32_t todayDays = _timeProvider.todayEpochDays();
  if (spendDays < todayDays) {
    _telegram.sendMessage(
        chatId,
        "⚠️ Date cannot be in the past. For past spendings, use /spend.",
        "");
    return;
  }

  String desc;
  for (size_t i = 2; i < args.size(); i++) {
    if (i > 2) desc += " ";
    desc += args[i];
  }

  JsonDocument doc;
  loadMoneyData(doc);
  JsonArray spendings = doc["money"]["spendings"].as<JsonArray>();

  int maxId = 0;
  for (JsonObject s : spendings) {
    int id = s["id"] | 0;
    if (id > maxId) maxId = id;
  }
  int nextId = maxId + 1;

  JsonObject item = spendings.add<JsonObject>();
  item["id"] = nextId;
  item["amount"] = amount;
  item["description"] = desc;
  item["date"] = dateStr;
  item["is_future"] = true;

  _store.save(_dataFilePath, doc);

  _telegram.sendMessage(
      chatId,
      "📅 Scheduled future spending: " + fmtMoney(amount) + " on " + dateStr + " (" + desc + ")",
      "");
}

void MoneyManagerBot::cmdAllowance(const String& chatId, bool naturalStyle) {
  JsonDocument doc;
  loadMoneyData(doc);

  if (!doc["money"]["config"].is<JsonObject>()) {
    _telegram.sendMessage(chatId, "⚠️ Configure budget first with /config", "");
    return;
  }

  JsonObject cfg = doc["money"]["config"].as<JsonObject>();
  float startMoney = cfg["start_money"] | 0.0f;
  float endMoney = cfg["end_money"] | 0.0f;
  String startDateStr = cfg["start_date"] | "";
  String endDateStr = cfg["end_date"] | "";

  int32_t startDays = 0, endDays = 0;
  if (!_timeProvider.parseDateToEpochDays(startDateStr, startDays) ||
      !_timeProvider.parseDateToEpochDays(endDateStr, endDays)) {
    _telegram.sendMessage(chatId, "⚠️ Invalid budget dates.", "");
    return;
  }

  int32_t todayDays = _timeProvider.todayEpochDays();
  if (todayDays > endDays) {
    _telegram.sendMessage(chatId, "⚠️ Budget period already ended.", "");
    return;
  }

  JsonArray spendings = doc["money"]["spendings"].as<JsonArray>();
  float totalFutureReserved = 0.0f;
  float totalSpentBeforeToday = 0.0f;
  float spentToday = 0.0f;

  for (JsonObject s : spendings) {
    float amt = s["amount"] | 0.0f;
    bool isFuture = s["is_future"] | false;
    int32_t sDays = 0;
    bool validDate = _timeProvider.parseDateToEpochDays(String(s["date"] | ""), sDays);

    if (isFuture) {
      totalFutureReserved += amt;
    } else if (validDate) {
      if (sDays < todayDays) {
        totalSpentBeforeToday += amt;
      } else if (sDays == todayDays) {
        spentToday += amt;
      }
    }
  }

  float totalBudget = startMoney - endMoney;
  float effectiveBudget = totalBudget - totalFutureReserved;
  float availablePool = effectiveBudget - totalSpentBeforeToday;

  int32_t remainingDays = (endDays - todayDays) + 1;
  if (remainingDays <= 0) {
    _telegram.sendMessage(chatId, "⚠️ Budget period already ended.", "");
    return;
  }

  float todayAllowance = availablePool / static_cast<float>(remainingDays);
  float remainingToday = todayAllowance - spentToday;

  String msg = naturalStyle
                   ? ("💰 *Disponível para hoje:* R$ " + fmtMoney(remainingToday))
                   : ("💰 Remaining today: " + fmtMoney(remainingToday));

  if (totalFutureReserved > 0.0f) {
    msg += naturalStyle
               ? ("\n🔒 Reservado para gastos futuros: R$ " + fmtMoney(totalFutureReserved))
               : ("\n🔒 Reserved for future spendings: " + fmtMoney(totalFutureReserved));
  }

  _telegram.sendMessage(chatId, msg, naturalStyle ? "Markdown" : "");
}

void MoneyManagerBot::cmdAllowancePlot(const String& chatId) {
  JsonDocument doc;
  loadMoneyData(doc);

  if (!doc["money"]["config"].is<JsonObject>()) {
    _telegram.sendMessage(chatId, "⚠️ Configure budget first with /config", "");
    return;
  }

  JsonObject cfg = doc["money"]["config"].as<JsonObject>();
  float startMoney = cfg["start_money"] | 0.0f;
  float endMoney = cfg["end_money"] | 0.0f;
  String endDateStr = cfg["end_date"] | "";

  int32_t endDays = 0;
  if (!_timeProvider.parseDateToEpochDays(endDateStr, endDays)) {
    _telegram.sendMessage(chatId, "⚠️ Invalid budget end date.", "");
    return;
  }

  int32_t todayDays = _timeProvider.todayEpochDays();
  if (todayDays > endDays) {
    _telegram.sendMessage(chatId, "⚠️ Budget period already ended.", "");
    return;
  }

  JsonArray spendings = doc["money"]["spendings"].as<JsonArray>();
  float totalFutureReserved = 0.0f;
  float totalSpentBeforeToday = 0.0f;
  float spentToday = 0.0f;

  for (JsonObject s : spendings) {
    float amt = s["amount"] | 0.0f;
    bool isFuture = s["is_future"] | false;
    int32_t sDays = 0;
    bool validDate = _timeProvider.parseDateToEpochDays(String(s["date"] | ""), sDays);

    if (isFuture) {
      totalFutureReserved += amt;
    } else if (validDate) {
      if (sDays < todayDays) {
        totalSpentBeforeToday += amt;
      } else if (sDays == todayDays) {
        spentToday += amt;
      }
    }
  }

  float totalBudget = startMoney - endMoney;
  float effectiveBudget = totalBudget - totalFutureReserved;
  float availablePool = effectiveBudget - totalSpentBeforeToday;
  float remainingPoolAfterToday = availablePool - spentToday;

  int32_t totalSpanDays = (endDays - todayDays) + 1;
  int32_t step = (totalSpanDays > 14) ? (totalSpanDays / 10) : 1;

  float firstVal = (availablePool / static_cast<float>(totalSpanDays)) - spentToday;
  float finalVal = fmaxf(0.0f, remainingPoolAfterToday);
  float maxVal = fmaxf(fabsf(firstVal), finalVal);
  if (maxVal < 1.0f) maxVal = 1.0f;

  String out = "📈 *Allowance Growth Projection (ESP32)*\n\n";
  for (int32_t curr = todayDays; curr <= endDays; curr += step) {
    int32_t daysLeft = (endDays - curr) + 1;
    float val = (curr == todayDays)
                    ? ((availablePool / static_cast<float>(daysLeft)) - spentToday)
                    : fmaxf(0.0f, remainingPoolAfterToday / static_cast<float>(daysLeft));

    int bars = static_cast<int>(roundf((fmaxf(0.0f, val) / maxVal) * 8.0f));
    String barStr;
    for (int b = 0; b < bars; b++) barStr += "█";
    if (barStr.length() == 0) barStr = "▏";

    out += "`" + _timeProvider.formatEpochDays(curr, false) + "` " + barStr + " *$" + fmtMoney(val) + "*\n";
  }

  out += "\n💰 *Today's remaining:* $" + fmtMoney(firstVal) +
         "\n🎯 *Final day potential max:* $" + fmtMoney(finalVal);
  if (totalFutureReserved > 0.0f) {
    out += "\n🔒 *Reserved future spendings:* $" + fmtMoney(totalFutureReserved);
  }

  _telegram.sendMessage(chatId, out);
}

void MoneyManagerBot::cmdListSpend(const String& chatId, bool naturalStyle) {
  JsonDocument doc;
  loadMoneyData(doc);
  JsonArray spendings = doc["money"]["spendings"].as<JsonArray>();

  if (spendings.size() == 0) {
    _telegram.sendMessage(
        chatId,
        naturalStyle ? "🧾 Nenhum gasto registrado." : "No spending records.",
        "");
    return;
  }

  String out = naturalStyle ? "🧾 *Últimos Gastos:*\n" : "🧾 Spendings:\n";
  size_t total = spendings.size();
  size_t startIdx = (naturalStyle && total > 10) ? (total - 10) : 0;

  for (size_t i = startIdx; i < total; i++) {
    JsonObject s = spendings[i].as<JsonObject>();
    bool isFuture = s["is_future"] | false;
    String tag = isFuture ? " 📅 [FUTURE]" : " 🧾 [DAILY]";
    out += "[" + String(s["id"] | 0) + "] " + fmtMoney(s["amount"] | 0.0f) + " | " +
           String(s["description"] | "") + " | " + String(s["date"] | "") + tag + "\n";
  }

  _telegram.sendMessage(chatId, out, naturalStyle ? "Markdown" : "");
}

void MoneyManagerBot::cmdDeleteSpend(const String& chatId, int spendId) {
  JsonDocument doc;
  loadMoneyData(doc);
  JsonArray spendings = doc["money"]["spendings"].as<JsonArray>();

  bool found = false;
  for (size_t i = 0; i < spendings.size(); i++) {
    if ((spendings[i]["id"] | 0) == spendId) {
      spendings.remove(i);
      found = true;
      break;
    }
  }

  if (found) {
    _store.save(_dataFilePath, doc);
    _telegram.sendMessage(chatId, "🗑️ Spending " + String(spendId) + " deleted.", "");
  } else {
    _telegram.sendMessage(chatId, "Spending not found.", "");
  }
}

void MoneyManagerBot::cmdShowConfig(const String& chatId) {
  JsonDocument doc;
  loadMoneyData(doc);

  if (!doc["money"]["config"].is<JsonObject>()) {
    _telegram.sendMessage(chatId, "⚠️ No budget configured.", "");
    return;
  }

  JsonObject cfg = doc["money"]["config"].as<JsonObject>();
  float startMoney = cfg["start_money"] | 0.0f;
  float endMoney = cfg["end_money"] | 0.0f;
  String startDate = cfg["start_date"] | "";
  String endDate = cfg["end_date"] | "";

  int32_t dStart = 0, dEnd = 0;
  _timeProvider.parseDateToEpochDays(startDate, dStart);
  _timeProvider.parseDateToEpochDays(endDate, dEnd);

  int32_t totalDays = (dEnd >= dStart) ? ((dEnd - dStart) + 1) : 1;
  float totalBudget = startMoney - endMoney;
  float baseDaily = totalBudget / static_cast<float>(totalDays);

  String msg = "⚙️ Budget configuration\n\n"
               "Start money: " + fmtMoney(startMoney) + "\n"
               "Target end money: " + fmtMoney(endMoney) + "\n"
               "Start date: " + startDate + "\n"
               "End date: " + endDate + "\n\n"
               "Total budget: " + fmtMoney(totalBudget) + "\n"
               "Total days: " + String(totalDays) + "\n"
               "Base daily allowance: " + fmtMoney(baseDaily);

  _telegram.sendMessage(chatId, msg, "");
}

void MoneyManagerBot::cmdHelp(const String& chatId) {
  String helpText =
      "💰 *Comandos do Money Manager (Finanças):*\n\n"
      "• `/config <início> <data_início> <fim> <data_fim>` : Configura o orçamento do período\n"
      "• `/spend <valor> <descrição>` : Registra um gasto hoje (ex: `/spend 35 almoço`)\n"
      "• `/futurespend <valor> <DD/MM/AAAA> <desc>` : Agenda um gasto futuro reservado\n"
      "• `/allowance` : Consulta o saldo disponível para gastar hoje\n"
      "• `/allowanceplot` : Exibe a projeção do orçamento\n"
      "• `/list` ou `/listspend` : Lista os gastos registrados\n"
      "• `/del <id>` ou `/delspend <id>` : Remove um gasto pelo ID\n"
      "• `/showconfig` : Exibe o resumo do orçamento ativo\n\n"
      "💬 *Você também pode digitar livremente (ex: \"gastei 50 no mercado\", \"quanto posso gastar hoje?\").*";
  _telegram.sendMessage(chatId, helpText);
}

void MoneyManagerBot::handleNaturalLanguage(const String& chatId, const String& rawText) {
  String clean = TextUtils::normalizeLower(rawText);

  if (clean == "ajuda" || clean == "comandos") {
    cmdHelp(chatId);
    return;
  }

  // Saldo / Allowance
  if (clean == "saldo" || clean == "meu saldo" || clean == "saldo de hoje" ||
      clean == "saldo diario" || clean == "quanto posso gastar" ||
      clean == "quanto posso gastar hoje" || clean == "quanto ainda posso gastar" ||
      clean == "allowance") {
    cmdAllowance(chatId, true);
    return;
  }

  // Listar gastos
  if (clean == "gastos" || clean == "despesas" || clean == "listar gastos" ||
      clean == "ver gastos" || clean == "mostrar gastos" || clean == "listar despesas") {
    cmdListSpend(chatId, true);
    return;
  }

  // Registrar gasto: "gastei 50 no almoço", "paguei 120 de luz", "gasto de 80 mercado"
  if (clean.startsWith("gastei ") || clean.startsWith("gasto ") ||
      clean.startsWith("paguei ") || clean.startsWith("pagamento ") ||
      clean.startsWith("despesa ")) {
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
      float amount = numStr.toFloat();
      if (amount > 0.0f) {
        String tail = rawText.substring(numEnd);
        tail.trim();
        String tailLower = TextUtils::normalizeLower(tail);
        if (tailLower.startsWith("reais")) {
          tail = tail.substring(5);
        } else if (tailLower.startsWith("real")) {
          tail = tail.substring(4);
        }
        tail = TextUtils::stripLeadingConnectors(tail);
        cmdSpend(chatId, amount, TextUtils::capitalizeFirst(tail), true);
        return;
      }
    }
  }

  _telegram.sendMessage(
      chatId,
      "❓ *Não reconheci esse comando financeiro.*\n\n"
      "💡 *Exemplos:*\n"
      "• 💰 _\"Gastei 45 no almoço\"_\n"
      "• 💰 _\"Quanto posso gastar hoje?\"_\n"
      "• 💰 _\"Listar gastos\"_");
}

