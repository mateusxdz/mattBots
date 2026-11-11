#include "NvsCredentialsStore.h"

NvsCredentialsStore::NvsCredentialsStore(const char* namespaceName)
    : _namespace(namespaceName) {}

WiFiCredentials NvsCredentialsStore::load() {
  WiFiCredentials creds;
  _prefs.begin(_namespace, true);
  creds.ssid = _prefs.getString("ssid", "");
  creds.password = _prefs.getString("pass", "");
  _prefs.end();
  return creds;
}

void NvsCredentialsStore::save(const WiFiCredentials& credentials) {
  WiFiCredentials clean = credentials;
  clean.ssid.trim();

  _prefs.begin(_namespace, false);
  _prefs.putString("ssid", clean.ssid);
  _prefs.putString("pass", clean.password);
  _prefs.end();

  Serial.printf("[NVS] Credenciais salvas para SSID: \"%s\"\n", clean.ssid.c_str());
}

ApiTokens NvsCredentialsStore::loadTokens() {
  ApiTokens tokens;
  _prefs.begin(_namespace, true);
  tokens.authorizedUserId     = _prefs.getString("auth_uid", "");
  tokens.telegramFreelancer   = _prefs.getString("tg_free", "");
  tokens.telegramGymCalorie   = _prefs.getString("tg_gym", "");
  tokens.telegramMercado      = _prefs.getString("tg_merc", "");
  tokens.telegramMoneyManager = _prefs.getString("tg_money", "");
  tokens.telegramTaskManager  = _prefs.getString("tg_task", "");
  tokens.freelancerApiToken   = _prefs.getString("fl_oauth", "");
  _prefs.end();
  return tokens;
}

void NvsCredentialsStore::saveTokens(const ApiTokens& tokens) {
  ApiTokens clean = tokens;
  clean.authorizedUserId.trim();
  clean.telegramFreelancer.trim();
  clean.telegramGymCalorie.trim();
  clean.telegramMercado.trim();
  clean.telegramMoneyManager.trim();
  clean.telegramTaskManager.trim();
  clean.freelancerApiToken.trim();

  _prefs.begin(_namespace, false);
  _prefs.putString("auth_uid", clean.authorizedUserId);
  _prefs.putString("tg_free", clean.telegramFreelancer);
  _prefs.putString("tg_gym", clean.telegramGymCalorie);
  _prefs.putString("tg_merc", clean.telegramMercado);
  _prefs.putString("tg_money", clean.telegramMoneyManager);
  _prefs.putString("tg_task", clean.telegramTaskManager);
  _prefs.putString("fl_oauth", clean.freelancerApiToken);
  _prefs.end();

  Serial.println("[NVS] Tokens de API salvos na NVS.");
}

