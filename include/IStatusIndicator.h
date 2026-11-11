#pragma once

enum class SystemState {
  CONFIG_MODE,     // Portal Web AP ativo (2 min)
  WIFI_CONNECTING, // Tentando conectar ao Wi-Fi salvo
  WIFI_CONNECTED,  // Conectado ao Wi-Fi com sucesso
  WIFI_ERROR,      // Sem credenciais salvas ou falha ao conectar
  OTA_UPDATING     // Baixando e gravando atualização OTA do GitHub
};

// Princípio da Inversão de Dependência (DIP):
// Os serviços dependem desta abstração e não diretamente do hardware do LED.
class IStatusIndicator {
public:
  virtual ~IStatusIndicator() = default;
  virtual void begin() = 0;
  virtual void setState(SystemState state) = 0;
  virtual SystemState getState() const = 0;
};

