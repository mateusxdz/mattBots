#pragma once
#include <Arduino.h>
#include "IOtaUpdater.h"
#include "IStatusIndicator.h"

// Responsabilidade Única (SRP): Verifica periodicamente no GitHub se o hash MD5
// do binário remoto difere do firmware instalado no ESP32 (ESP.getSketchMD5())
// e realiza o download + flash automático.
class GitHubOtaUpdater : public IOtaUpdater {
public:
  GitHubOtaUpdater(IStatusIndicator& indicator,
                   const char* md5Url,
                   const char* binUrl,
                   uint32_t checkIntervalMs);

  void loop() override;
  bool checkAndUpdateNow() override;

private:
  IStatusIndicator& _indicator;
  const char* _md5Url;
  const char* _binUrl;
  uint32_t _checkIntervalMs;
  uint32_t _lastCheckMs;
  bool _firstCheckDone;

  String fetchRemoteMd5() const;
  bool performHttpUpdate(const String& expectedMd5);
};

