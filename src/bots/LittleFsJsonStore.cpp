#include "bots/LittleFsJsonStore.h"
#include <LittleFS.h>

bool LittleFsJsonStore::begin() {
  if (_mounted) {
    return true;
  }
  if (!LittleFS.begin(true)) {
    Serial.println("[STORAGE] Erro ao montar LittleFS!");
    return false;
  }
  _mounted = true;
  Serial.printf("[STORAGE] LittleFS montado com sucesso (Total: %u bytes, Usado: %u bytes)\n",
                LittleFS.totalBytes(),
                LittleFS.usedBytes());
  return true;
}

bool LittleFsJsonStore::exists(const char* path) const {
  if (!_mounted) {
    return false;
  }
  return LittleFS.exists(path);
}

bool LittleFsJsonStore::load(const char* path, JsonDocument& doc) {
  if (!_mounted && !begin()) {
    return false;
  }
  if (!LittleFS.exists(path)) {
    doc.clear();
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("[STORAGE] Falha ao abrir %s para leitura.\n", path);
    return false;
  }

  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    Serial.printf("[STORAGE] Erro ao desserializar %s: %s\n", path, err.c_str());
    return false;
  }
  return true;
}

bool LittleFsJsonStore::save(const char* path, const JsonDocument& doc) {
  if (!_mounted && !begin()) {
    return false;
  }

  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.printf("[STORAGE] Falha ao abrir %s para escrita.\n", path);
    return false;
  }

  size_t written = serializeJson(doc, file);
  file.close();
  return written > 0;
}

bool LittleFsJsonStore::ensureSeedFile(const char* path, const char* defaultJsonContent) {
  if (!_mounted && !begin()) {
    return false;
  }
  if (LittleFS.exists(path)) {
    return true;
  }

  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }
  file.print(defaultJsonContent);
  file.close();
  Serial.printf("[STORAGE] Arquivo inicial criado: %s\n", path);
  return true;
}

