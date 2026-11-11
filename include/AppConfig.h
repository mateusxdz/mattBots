#pragma once
#include <Arduino.h>

namespace AppConfig {
  // Hardware
  constexpr uint8_t LED_PIN = 2;

  // Portal de Configuração (Access Point)
  constexpr char AP_SSID[] = "ESP_MATEUS";
  constexpr uint32_t CONFIG_PORTAL_INITIAL_TIMEOUT_MS = 30000;   // 30 segundos aguardando conexão no AP
  constexpr uint32_t CONFIG_PORTAL_EXTENDED_TIMEOUT_MS = 120000; // +2 minutos se alguém se conectar ao AP

  // Wi-Fi Station
  constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;   // 15 segundos
  constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 5000; // 5 segundos

  // GitHub Pull-OTA (Verificação periódica a cada 60 segundos)
  constexpr uint32_t OTA_CHECK_INTERVAL_MS = 60000;     // 60 segundos (60.000 ms)

  // URLs do seu repositório no GitHub (altere para o seu usuário/repositório)
  // Dica: O arquivo firmware.md5 (ou version.txt) contém o hash MD5 ou versão do binário no GitHub,
  // evitando baixar o firmware.bin inteiro a cada 60 segundos sem necessidade.
  constexpr char GITHUB_FIRMWARE_MD5_URL[] =
      "https://github.com/mateusxdz/mattBots/raw/refs/heads/master/ota/firmware.md5";
  constexpr char GITHUB_FIRMWARE_BIN_URL[] =
      "https://github.com/mateusxdz/mattBots/raw/refs/heads/master/ota/firmware.bin";

  // ============================================================================
  // Configurações Globais dos Bots Telegram & Freelancer
  // ============================================================================
  // Intervalo entre passos do Round-Robin de polling dos bots (ms)
  constexpr uint32_t BOT_ROUND_ROBIN_STEP_MS = 500;

  // Configuração da API do Freelancer.com
  constexpr char FREELANCER_ACTIVE_PROJECTS_URL[] =
      "https://www.freelancer.com/api/projects/0.1/projects/active/?limit=10&compact=true&query=esp32%20iot%20mqtt%20firmware%20raspberry%20raspberrypi%20telegram";
  constexpr uint32_t FREELANCER_FETCH_INTERVAL_MS = 30000; // 30 segundos

  // Caminhos dos arquivos JSON persistidos no LittleFS
  constexpr char FILE_TASKS_JSON[] = "/tasks.json";
  constexpr char FILE_MERCADO_JSON[] = "/mercado.json";
  constexpr char FILE_MONEY_JSON[] = "/money.json";
  constexpr char FILE_GYM_CALORIES_JSON[] = "/gym_calories.json";
  constexpr char FILE_FREELANCER_SEEN_JSON[] = "/seen_projects.json";
  constexpr char FILE_FREELANCER_FILTERS_JSON[] = "/discard_keywords.json";

  // Configuração NTP (Horário de Brasília UTC-3)
  constexpr long NTP_GMT_OFFSET_SEC = -3 * 3600;
  constexpr int NTP_DAYLIGHT_OFFSET_SEC = 0;
}


