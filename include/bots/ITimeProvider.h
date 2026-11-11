#pragma once
#include <Arduino.h>

// Abstração para obtenção de data/hora e cálculos de calendário (DIP / SRP)
class ITimeProvider {
public:
  virtual ~ITimeProvider() = default;

  virtual void begin() = 0;
  virtual void ensureSynced() = 0;
  virtual String todayStr() const = 0;    // Formato "DD/MM/YYYY"
  virtual String nowTimeStr() const = 0;  // Formato "HH:MM"

  // Converte "DD/MM/YYYY" em número de dias desde 1970-01-01 (retorna false se inválido)
  virtual bool parseDateToEpochDays(const String& dateStr, int32_t& outDays) const = 0;
  virtual String formatEpochDays(int32_t epochDays, bool includeYear = true) const = 0;
  virtual int32_t todayEpochDays() const = 0;
};

