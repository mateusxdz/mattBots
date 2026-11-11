#pragma once
#include "bots/IJsonStore.h"

// Implementação concreta de persistência JSON em LittleFS (SRP / LSP)
class LittleFsJsonStore : public IJsonStore {
public:
  LittleFsJsonStore() = default;
  ~LittleFsJsonStore() override = default;

  bool begin() override;
  bool exists(const char* path) const override;
  bool load(const char* path, JsonDocument& doc) override;
  bool save(const char* path, const JsonDocument& doc) override;

  // Grava um conteúdo JSON inicial caso o arquivo ainda não exista na Flash
  bool ensureSeedFile(const char* path, const char* defaultJsonContent);

private:
  bool _mounted = false;
};

