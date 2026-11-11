#pragma once
#include <Arduino.h>
#include <vector>
#include "ICredentialsStore.h"
#include "IStatusIndicator.h"

struct ScannedNetwork {
  String ssid;
  int32_t rssi;
  bool secure;
};

// Responsabilidade Única (SRP): Gerenciamento de conexão e varredura Wi-Fi
class WiFiService {
public:
  WiFiService(ICredentialsStore& store, IStatusIndicator& indicator);

  bool connectWithSaved(uint32_t timeoutMs);
  bool testConnection(const WiFiCredentials& creds, uint32_t timeoutMs, String& outIp);
  std::vector<ScannedNetwork> scanNetworks();
  void ensureConnected(uint32_t timeoutMs);
  bool isConnected() const;
  String localIp() const;
  WiFiCredentials getCurrentCredentials() const;
  void updateCredentials(const WiFiCredentials& creds);
  ApiTokens getCurrentTokens() const;
  void updateTokens(const ApiTokens& tokens);

private:
  ICredentialsStore& _store;
  IStatusIndicator& _indicator;
  WiFiCredentials _credentials;
  ApiTokens _tokens;
};

