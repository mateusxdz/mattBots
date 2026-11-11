#include "bots/TelegramHttpClient.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

TelegramHttpClient::TelegramHttpClient(SharedHttpsTransport& transport,
                                       const String& botToken,
                                       bool allowCallbacks)
    : _transport(transport),
      _botToken(botToken),
      _allowCallbacks(allowCallbacks) {
  _botToken.trim();
}

void TelegramHttpClient::setBotToken(const String& botToken) {
  _botToken = botToken;
  _botToken.trim();
}

String TelegramHttpClient::buildApiUrl(const char* method) const {
  String url = "https://api.telegram.org/bot";
  url += _botToken;
  url += "/";
  url += method;
  return url;
}

bool TelegramHttpClient::getUpdates(int64_t offset, std::vector<TelegramUpdate>& outUpdates) {
  outUpdates.clear();
  if (_botToken.length() == 0) {
    return false;
  }

  String url = buildApiUrl("getUpdates");
  url += "?timeout=0&limit=5";
  if (offset > 0) {
    url += "&offset=" + String(offset);
  }

  HTTPClient http;
  http.setTimeout(10000);
  http.useHTTP10(true); // Permite streaming direto para ArduinoJson sem chunked encoding

  if (!http.begin(_transport.client(), url)) {
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  // Filtro ArduinoJson para extrair apenas os campos essenciais e economizar SRAM
  JsonDocument filter;
  filter["ok"] = true;
  JsonObject resFilter = filter["result"].add<JsonObject>();
  resFilter["update_id"] = true;
  resFilter["message"]["message_id"] = true;
  resFilter["message"]["from"]["id"] = true;
  resFilter["message"]["chat"]["id"] = true;
  resFilter["message"]["text"] = true;
  resFilter["message"]["voice"]["file_id"] = true;
  resFilter["message"]["audio"]["file_id"] = true;

  if (_allowCallbacks) {
    resFilter["callback_query"]["id"] = true;
    resFilter["callback_query"]["from"]["id"] = true;
    resFilter["callback_query"]["message"]["chat"]["id"] = true;
    resFilter["callback_query"]["message"]["message_id"] = true;
    resFilter["callback_query"]["data"] = true;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err || !doc["ok"].as<bool>()) {
    return false;
  }

  JsonArray results = doc["result"].as<JsonArray>();
  for (JsonObject item : results) {
    TelegramUpdate upd;
    upd.updateId = item["update_id"].as<int64_t>();

    if (_allowCallbacks && item["callback_query"].is<JsonObject>()) {
      JsonObject cb = item["callback_query"].as<JsonObject>();
      upd.isCallback = true;
      upd.callback.updateId = upd.updateId;
      upd.callback.id = cb["id"] | "";
      upd.callback.userId = cb["from"]["id"].as<int64_t>();
      upd.callback.chatId = String(cb["message"]["chat"]["id"].as<int64_t>());
      upd.callback.messageId = cb["message"]["message_id"].as<int64_t>();
      upd.callback.data = cb["data"] | "";
      outUpdates.push_back(upd);
    } else if (item["message"].is<JsonObject>()) {
      JsonObject msg = item["message"].as<JsonObject>();
      upd.isCallback = false;
      upd.message.updateId = upd.updateId;
      upd.message.messageId = msg["message_id"].as<int64_t>();
      upd.message.userId = msg["from"]["id"].as<int64_t>();
      upd.message.chatId = String(msg["chat"]["id"].as<int64_t>());
      upd.message.text = msg["text"] | "";
      upd.message.hasVoiceOrAudio =
          msg["voice"].is<JsonObject>() || msg["audio"].is<JsonObject>();
      outUpdates.push_back(upd);
    }
  }

  return true;
}

int64_t TelegramHttpClient::sendMessage(const String& chatId,
                                        const String& text,
                                        const char* parseMode,
                                        const String& replyMarkupJson) {
  if (_botToken.length() == 0) {
    return 0;
  }
  HTTPClient http;
  http.setTimeout(12000);
  http.useHTTP10(true);

  if (!http.begin(_transport.client(), buildApiUrl("sendMessage"))) {
    return 0;
  }
  http.addHeader("Content-Type", "application/json");

  JsonDocument payloadDoc;
  payloadDoc["chat_id"] = chatId;
  payloadDoc["text"] = text;
  payloadDoc["disable_web_page_preview"] = true;
  if (parseMode && strlen(parseMode) > 0) {
    payloadDoc["parse_mode"] = parseMode;
  }
  if (replyMarkupJson.length() > 0) {
    JsonDocument markupDoc;
    if (!deserializeJson(markupDoc, replyMarkupJson)) {
      payloadDoc["reply_markup"] = markupDoc.as<JsonVariant>();
    }
  }

  String body;
  serializeJson(payloadDoc, body);

  int code = http.POST(body);
  int64_t sentMsgId = 0;

  if (code == HTTP_CODE_OK) {
    JsonDocument filter;
    filter["result"]["message_id"] = true;
    JsonDocument respDoc;
    if (!deserializeJson(respDoc, http.getStream(), DeserializationOption::Filter(filter))) {
      sentMsgId = respDoc["result"]["message_id"].as<int64_t>();
    }
  } else {
    Serial.printf("[TELEGRAM] sendMessage HTTP %d: %s\n", code, http.getString().c_str());
  }

  http.end();
  return sentMsgId;
}

bool TelegramHttpClient::editMessageText(const String& chatId,
                                         int64_t messageId,
                                         const String& text,
                                         const char* parseMode,
                                         const String& replyMarkupJson) {
  HTTPClient http;
  http.setTimeout(12000);

  if (!http.begin(_transport.client(), buildApiUrl("editMessageText"))) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  JsonDocument payloadDoc;
  payloadDoc["chat_id"] = chatId;
  payloadDoc["message_id"] = messageId;
  payloadDoc["text"] = text;
  payloadDoc["disable_web_page_preview"] = true;
  if (parseMode && strlen(parseMode) > 0) {
    payloadDoc["parse_mode"] = parseMode;
  }
  if (replyMarkupJson.length() > 0) {
    JsonDocument markupDoc;
    if (!deserializeJson(markupDoc, replyMarkupJson)) {
      payloadDoc["reply_markup"] = markupDoc.as<JsonVariant>();
    }
  }

  String body;
  serializeJson(payloadDoc, body);

  int code = http.POST(body);
  http.end();
  return code == HTTP_CODE_OK;
}

bool TelegramHttpClient::answerCallbackQuery(const String& callbackQueryId,
                                             const String& text) {
  HTTPClient http;
  http.setTimeout(8000);

  if (!http.begin(_transport.client(), buildApiUrl("answerCallbackQuery"))) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  JsonDocument payloadDoc;
  payloadDoc["callback_query_id"] = callbackQueryId;
  if (text.length() > 0) {
    payloadDoc["text"] = text;
  }

  String body;
  serializeJson(payloadDoc, body);

  int code = http.POST(body);
  http.end();
  return code == HTTP_CODE_OK;
}

