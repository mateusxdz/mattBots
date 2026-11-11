#include "GitHubOtaUpdater.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

GitHubOtaUpdater::GitHubOtaUpdater(IStatusIndicator& indicator,
                                   const char* md5Url,
                                   const char* binUrl,
                                   uint32_t checkIntervalMs)
    : _indicator(indicator),
      _md5Url(md5Url),
      _binUrl(binUrl),
      _checkIntervalMs(checkIntervalMs),
      _lastCheckMs(0),
      _firstCheckDone(false) {}

void GitHubOtaUpdater::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  uint32_t now = millis();
  if (!_firstCheckDone || (now - _lastCheckMs >= _checkIntervalMs)) {
    _firstCheckDone = true;
    _lastCheckMs = now;
    checkAndUpdateNow();
  }
}

bool GitHubOtaUpdater::checkAndUpdateNow() {
  String installedMd5 = ESP.getSketchMD5();
  installedMd5.toLowerCase();

  Serial.println("[GITHUB-OTA] Verificando se ha novo firmware no GitHub...");
  Serial.printf("[GITHUB-OTA] MD5 instalado atualmente: %s\n", installedMd5.c_str());

  String remoteMd5 = fetchRemoteMd5();
  if (remoteMd5.length() == 0) {
    Serial.println("[GITHUB-OTA] Nao foi possivel obter o arquivo firmware.md5 do GitHub.");
    return false;
  }

  Serial.printf("[GITHUB-OTA] MD5 disponivel no GitHub: %s\n", remoteMd5.c_str());

  if (remoteMd5.equalsIgnoreCase(installedMd5)) {
    Serial.println("[GITHUB-OTA] O firmware ja esta na versao mais recente.");
    return false;
  }

  Serial.println("[GITHUB-OTA] Novo firmware detectado! Iniciando download e atualizacao OTA...");
  return performHttpUpdate(remoteMd5);
}

String GitHubOtaUpdater::fetchRemoteMd5() const {
  WiFiClientSecure client;
  client.setInsecure(); // Permite HTTPS para raw.githubusercontent.com sem precisar atualizar certificado raiz

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  // Adiciona timestamp como query string para evitar cache do CDN do GitHub
  String urlWithNoCache = String(_md5Url) + "?t=" + String(millis());
  if (!http.begin(client, urlWithNoCache)) {
    return "";
  }

  int httpCode = http.GET();
  String payload = "";
  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString();
    payload.trim();
    // Se o arquivo tiver formato md5sum ("<hash>  firmware.bin"), extrai apenas os primeiros 32 caracteres
    if (payload.length() >= 32) {
      payload = payload.substring(0, 32);
    }
    payload.toLowerCase();
  } else {
    Serial.printf("[GITHUB-OTA] HTTP GET firmware.md5 retornou codigo: %d\n", httpCode);
  }

  http.end();
  return payload;
}

bool GitHubOtaUpdater::performHttpUpdate(const String& expectedMd5) {
  SystemState previousState = _indicator.getState();
  _indicator.setState(SystemState::OTA_UPDATING);

  WiFiClientSecure client;
  client.setInsecure();

  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(false); // Reiniciamos manualmente após logar sucesso

  String binUrlWithNoCache = String(_binUrl) + "?t=" + String(millis());
  t_httpUpdate_return ret = httpUpdate.update(client, binUrlWithNoCache);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[GITHUB-OTA] Falha na atualizacao (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      _indicator.setState(previousState);
      return false;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[GITHUB-OTA] Nenhuma atualizacao aplicada.");
      _indicator.setState(previousState);
      return false;

    case HTTP_UPDATE_OK:
      Serial.printf("[GITHUB-OTA] Firmware (%s) gravado com sucesso! Reiniciando ESP32...\n",
                    expectedMd5.c_str());
      delay(500);
      ESP.restart();
      return true;
  }

  _indicator.setState(previousState);
  return false;
}

