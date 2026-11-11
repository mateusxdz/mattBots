#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Abstração para leitura e gravação de documentos JSON persistentes (DIP / SRP)
class IJsonStore {
public:
  virtual ~IJsonStore() = default;

  virtual bool begin() = 0;
  virtual bool exists(const char* path) const = 0;
  virtual bool load(const char* path, JsonDocument& doc) = 0;
  virtual bool save(const char* path, const JsonDocument& doc) = 0;
};

