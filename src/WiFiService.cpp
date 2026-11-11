#include "WiFiService.h"
#include <WiFi.h>

WiFiService::WiFiService(ICredentialsStore& store, IStatusIndicator& indicator)
    : _store(store), _indicator(indicator) {}

void WiFiService::begin() {
  _credentials = _store.load();
  _tokens = _store.loadTokens();
}

bool WiFiService::connectWithSaved(uint32_t timeoutMs) {
  _credentials = _store.load();
  if (!_credentials.isValid()) {
    Serial.println("[WIFI] Nenhum SSID configurado. Estado: WIFI_ERROR");
    _indicator.setState(SystemState::WIFI_ERROR);
    return false;
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == _credentials.ssid) {
    _indicator.setState(SystemState::WIFI_CONNECTED);
    Serial.printf("[WIFI] Ja conectado! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  _indicator.setState(SystemState::WIFI_CONNECTING);
  Serial.printf("[WIFI] Conectando a \"%s\"...\n", _credentials.ssid.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(_credentials.ssid.c_str(), _credentials.password.c_str());

  uint32_t startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    _indicator.setState(SystemState::WIFI_CONNECTED);
    Serial.printf("[WIFI] Conectado com sucesso! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  _indicator.setState(SystemState::WIFI_ERROR);
  Serial.println("[WIFI] Falha ao conectar.");
  return false;
}

bool WiFiService::testConnection(const WiFiCredentials& creds, uint32_t timeoutMs, String& outIp) {
  updateCredentials(creds);
  if (!_credentials.isValid()) {
    return false;
  }

  Serial.printf("[TESTE] Testando conexao com \"%s\"...\n", _credentials.ssid.c_str());
  _indicator.setState(SystemState::WIFI_CONNECTING);

  WiFi.disconnect(false);
  WiFi.begin(_credentials.ssid.c_str(), _credentials.password.c_str());

  uint32_t startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeoutMs) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    outIp = WiFi.localIP().toString();
    Serial.printf("[TESTE] Sucesso! IP obtido: %s\n", outIp.c_str());
    _indicator.setState(SystemState::WIFI_CONNECTED);
    delay(1500);
    return true;
  }

  Serial.println("[TESTE] Falha ao conectar na rede informada.");
  _indicator.setState(SystemState::WIFI_ERROR);
  delay(1500);
  WiFi.disconnect(false);
  return false;
}

std::vector<ScannedNetwork> WiFiService::scanNetworks() {
  std::vector<ScannedNetwork> results;
  Serial.println("[WIFI] Escaneando redes Wi-Fi proximas...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    ScannedNetwork net;
    net.ssid = WiFi.SSID(i);
    net.rssi = WiFi.RSSI(i);
    net.secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    results.push_back(net);
  }
  WiFi.scanDelete();
  Serial.printf("[WIFI] %d redes encontradas.\n", n);
  return results;
}

void WiFiService::ensureConnected(uint32_t timeoutMs) {
  if (!_credentials.isValid()) {
    _indicator.setState(SystemState::WIFI_ERROR);
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (_indicator.getState() != SystemState::OTA_UPDATING) {
      _indicator.setState(SystemState::WIFI_CONNECTED);
    }
  } else {
    Serial.println("[WIFI] Conexao perdida. Reconectando...");
    connectWithSaved(timeoutMs);
  }
}

bool WiFiService::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

String WiFiService::localIp() const {
  return WiFi.localIP().toString();
}

WiFiCredentials WiFiService::getCurrentCredentials() {
  _credentials = _store.load();
  return _credentials;
}

void WiFiService::updateCredentials(const WiFiCredentials& creds) {
  _credentials = creds;
  _credentials.ssid.trim();
  _store.save(_credentials);
}

ApiTokens WiFiService::getCurrentTokens() {
  _tokens = _store.loadTokens();
  return _tokens;
}

void WiFiService::updateTokens(const ApiTokens& tokens) {
  _tokens = tokens;
  _store.saveTokens(_tokens);
}

