#pragma once
#include <Arduino.h>

struct WiFiCredentials {
  String ssid;
  String password;

  bool isValid() const {
    return ssid.length() > 0;
  }
};

struct ApiTokens {
  String authorizedUserId;
  String telegramFreelancer;
  String telegramGymCalorie;
  String telegramMercado;
  String telegramMoneyManager;
  String telegramTaskManager;
  String freelancerApiToken;
};

// Abstração para persistência de configurações (DIP / ISP)
class ICredentialsStore {
public:
  virtual ~ICredentialsStore() = default;
  virtual WiFiCredentials load() = 0;
  virtual void save(const WiFiCredentials& credentials) = 0;
  virtual ApiTokens loadTokens() = 0;
  virtual void saveTokens(const ApiTokens& tokens) = 0;
};

