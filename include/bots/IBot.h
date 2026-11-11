#pragma once
#include <Arduino.h>

// Abstração base para todos os bots (ISP / LSP / OCP)
class IBot {
public:
  virtual ~IBot() = default;
  virtual const char* name() const = 0;
  virtual void begin() = 0;
  virtual void poll() = 0;
};

