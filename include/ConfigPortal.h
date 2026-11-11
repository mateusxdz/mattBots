#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "WiFiService.h"
#include "IStatusIndicator.h"

// Responsabilidade Única (SRP): Portal Web Captive para configuração inicial
class ConfigPortal {
public:
  ConfigPortal(WiFiService& wifiService,
               IStatusIndicator& indicator,
               const char* apSsid,
               uint32_t initialTimeoutMs,
               uint32_t extendedTimeoutMs);
  void runBlocking();

private:
  WiFiService& _wifiService;
  IStatusIndicator& _indicator;
  const char* _apSsid;
  uint32_t _initialTimeoutMs;
  uint32_t _extendedTimeoutMs;
  uint32_t _activeTimeoutMs;

  WebServer _server;
  DNSServer _dnsServer;
  uint32_t _startTimeMs;
  bool _clientConnected;
  bool _finishEarly;
  String _lastTestResultJson;

  void setupRoutes();
  String buildHtmlPage(const String& message = "") const;
  static String htmlEscape(const String& str);
  static String jsonEscape(const String& str);
};

