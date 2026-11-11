#include "bots/MercadoBot.h"
#include "bots/TextUtils.h"

MercadoBot::MercadoBot(ITelegramClient& telegram,
                       IJsonStore& store,
                       const char* dataFilePath,
                       int64_t authorizedUserId)
    : _telegram(telegram),
      _store(store),
      _dataFilePath(dataFilePath),
      _authorizedUserId(authorizedUserId) {}

void MercadoBot::begin() {
  JsonDocument doc;
  if (!_store.load(_dataFilePath, doc)) {
    doc["mercado"].to<JsonArray>();
    _store.save(_dataFilePath, doc);
  }
}

bool MercadoBot::loadMercadoData(JsonDocument& doc) {
  if (!_store.load(_dataFilePath, doc)) {
    doc.clear();
    doc["mercado"].to<JsonArray>();
  }
  if (!doc["mercado"].is<JsonArray>()) {
    doc["mercado"].to<JsonArray>();
  }
  return true;
}

void MercadoBot::poll() {
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

void MercadoBot::handleMessage(const TelegramMessage& msg) {
  if (msg.userId != _authorizedUserId) {
    _telegram.sendMessage(msg.chatId, "❌ You are not allowed to use this bot.");
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

void MercadoBot::handleSlashCommand(const String& chatId,
                                    const String& cmd,
                                    const String& rest,
                                    const std::vector<String>& args) {
  if (cmd == "add" || cmd == "addmercado") {
    if (rest.length() == 0) {
      _telegram.sendMessage(chatId, "Use: /add <item_name>", "");
      return;
    }
    cmdAdd(chatId, rest, false);
  } else if (cmd == "remove" || cmd == "removemercado") {
    if (args.empty()) {
      _telegram.sendMessage(chatId, "Use: /remove <id>", "");
      return;
    }
    int id = args[0].toInt();
    if (id <= 0 && args[0] != "0") {
      _telegram.sendMessage(chatId, "Invalid ID. Use a number (e.g. /remove 1).", "");
      return;
    }
    cmdRemove(chatId, id, false);
  } else if (cmd == "list" || cmd == "listmercado") {
    cmdList(chatId, false);
  } else if (cmd == "clear" || cmd == "clearmercado") {
    cmdClear(chatId, args);
  } else if (cmd == "help" || cmd == "start" || cmd == "voicehelp") {
    cmdHelp(chatId);
  }
}

void MercadoBot::cmdAdd(const String& chatId, const String& itemName, bool naturalStyle) {
  JsonDocument doc;
  loadMercadoData(doc);
  JsonArray arr = doc["mercado"].as<JsonArray>();

  int maxId = 0;
  for (JsonObject item : arr) {
    int id = item["id"] | 0;
    if (id > maxId) maxId = id;
  }
  int nextId = maxId + 1;

  JsonObject newItem = arr.add<JsonObject>();
  newItem["id"] = nextId;
  newItem["name"] = itemName;
  _store.save(_dataFilePath, doc);

  if (naturalStyle) {
    _telegram.sendMessage(chatId, "🛒 *Item #" + String(nextId) + " adicionado ao mercado:* " + itemName);
  } else {
    _telegram.sendMessage(chatId, "🛒 Added item #" + String(nextId) + ": " + itemName, "");
  }
}

void MercadoBot::cmdRemove(const String& chatId, int itemId, bool naturalStyle) {
  JsonDocument doc;
  loadMercadoData(doc);
  JsonArray arr = doc["mercado"].as<JsonArray>();

  if (arr.size() == 0) {
    _telegram.sendMessage(chatId, "🛒 Shopping list is empty.", "");
    return;
  }

  bool found = false;
  for (size_t i = 0; i < arr.size(); i++) {
    if ((arr[i]["id"] | 0) == itemId) {
      arr.remove(i);
      found = true;
      break;
    }
  }

  if (found) {
    _store.save(_dataFilePath, doc);
    if (naturalStyle) {
      _telegram.sendMessage(chatId, "🗑️ *Item #" + String(itemId) + " removido do mercado!*");
    } else {
      _telegram.sendMessage(chatId, "🗑️ Removed item #" + String(itemId) + ".", "");
    }
  } else {
    if (naturalStyle) {
      _telegram.sendMessage(chatId, "⚠️ Item #" + String(itemId) + " não encontrado na lista de compras.");
    } else {
      _telegram.sendMessage(chatId, "Item not found.", "");
    }
  }
}

void MercadoBot::cmdList(const String& chatId, bool naturalStyle) {
  JsonDocument doc;
  loadMercadoData(doc);
  JsonArray arr = doc["mercado"].as<JsonArray>();

  if (arr.size() == 0) {
    _telegram.sendMessage(chatId, naturalStyle ? "🛒 Lista de compras vazia." : "🛒 Shopping list is empty.", "");
    return;
  }

  String out = naturalStyle ? "🛍️ *Lista de Mercado:*\n" : "🛍️ *Shopping List:*\n";
  for (JsonObject item : arr) {
    out += "[" + String(item["id"] | 0) + "] " + String(item["name"] | "") + "\n";
  }
  _telegram.sendMessage(chatId, out);
}

void MercadoBot::cmdClear(const String& chatId, const std::vector<String>& args) {
  if (args.empty() || !args[0].equalsIgnoreCase("confirm")) {
    _telegram.sendMessage(chatId, "⚠️ To clear the shopping list, type: /clear confirm", "");
    return;
  }
  JsonDocument doc;
  doc["mercado"].to<JsonArray>();
  _store.save(_dataFilePath, doc);
  _telegram.sendMessage(chatId, "🧹 Cleared shopping list.", "");
}

void MercadoBot::cmdHelp(const String& chatId) {
  String helpText =
      "🛒 *Comandos do Mercado / Lista de Compras:*\n\n"
      "• `/add <item>` : Adiciona um item à lista\n"
      "• `/remove <id>` : Remove um item pelo número\n"
      "• `/list` : Exibe todos os itens da lista\n"
      "• `/clear confirm` : Limpa toda a lista de compras\n\n"
      "💬 *Você também pode digitar livremente (ex: \"comprar café\", \"listar mercado\", \"remover 2\").*";
  _telegram.sendMessage(chatId, helpText);
}

void MercadoBot::handleNaturalLanguage(const String& chatId, const String& rawText) {
  String clean = TextUtils::normalizeLower(rawText);

  if (clean == "ajuda" || clean == "comandos") {
    cmdHelp(chatId);
    return;
  }

  // Listar
  if (clean == "listar" || clean == "lista" || clean == "compras" || clean == "ver" ||
      clean == "mercado" || clean == "listar mercado" || clean == "lista de compras" ||
      clean == "ver lista" || clean == "ver mercado" || clean == "o que comprar") {
    cmdList(chatId, true);
    return;
  }

  // Remover: "remover 2", "tirar 2", "remover do mercado 2", "apagar 2"
  const char* remPrefixes[] = {"remover ", "remove ", "remova ", "tirar ", "tira ", "apagar ", "apaga ", "deletar ", "deleta "};
  for (const char* p : remPrefixes) {
    if (clean.startsWith(p)) {
      String tail = clean.substring(strlen(p));
      tail.trim();
      for (int i = tail.length() - 1; i >= 0; i--) {
        if (isDigit(tail[i])) {
          int endIdx = i + 1;
          int startIdx = i;
          while (startIdx > 0 && isDigit(tail[startIdx - 1])) {
            startIdx--;
          }
          int itemId = tail.substring(startIdx, endIdx).toInt();
          if (itemId > 0) {
            cmdRemove(chatId, itemId, true);
            return;
          }
          break;
        }
      }
    }
  }

  // Adicionar: "comprar ...", "adicionar ...", "colocar ...", "incluir ...", "anotar ...", "mercado ..."
  const char* addPrefixes[] = {
      "adicionar ", "adiciona ", "adicione ", "colocar ", "coloca ",
      "bota ", "botar ", "incluir ", "anotar ", "anota ", "comprar ", "mercado "};
  String rawClean = TextUtils::sanitizeText(rawText);
  String rawLower = rawClean;
  rawLower.toLowerCase();

  for (const char* p : addPrefixes) {
    if (rawLower.startsWith(p)) {
      String rest = rawClean.substring(strlen(p));
      rest = TextUtils::stripLeadingConnectors(rest);
      if (rest.length() > 0) {
        cmdAdd(chatId, TextUtils::capitalizeFirst(rest), true);
        return;
      }
    }
  }

  _telegram.sendMessage(
      chatId,
      "❓ *Não reconheci esse comando de mercado.*\n\n"
      "💡 *Exemplos:*\n"
      "• 🛒 _\"Comprar leite desnatado\"_\n"
      "• 🛒 _\"Listar mercado\"_\n"
      "• 🛒 _\"Remover 2\"_");
}

