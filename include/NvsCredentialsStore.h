#pragma once
#include <Preferences.h>
#include "ICredentialsStore.h"

// Responsabilidade Única (SRP): Persistência em memória não-volátil (NVS)
class NvsCredentialsStore : public ICredentialsStore {
public:
  explicit NvsCredentialsStore(const char* namespaceName = "wifi-cfg");
  WiFiCredentials load() override;
  void save(const WiFiCredentials& credentials) override;
  ApiTokens loadTokens() override;
  void saveTokens(const ApiTokens& tokens) override;

private:
  const char* _namespace;
  Preferences _prefs;
};

