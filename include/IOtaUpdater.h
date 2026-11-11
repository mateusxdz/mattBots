#pragma once
#include <Arduino.h>

// Abstração para mecanismos de atualização OTA (Open/Closed Principle - OCP & DIP)
class IOtaUpdater {
public:
  virtual ~IOtaUpdater() = default;
  virtual void loop() = 0;
  virtual bool checkAndUpdateNow() = 0;
};

