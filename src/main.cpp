#include <Arduino.h>
#include "AppConfig.h"
#include "LedStatusIndicator.h"
#include "NvsCredentialsStore.h"
#include "WiFiService.h"
#include "ConfigPortal.h"
#include "GitHubOtaUpdater.h"

// Infraestrutura e Serviços dos Bots (SOLID - DIP / SRP / OCP)
#include "bots/LittleFsJsonStore.h"
#include "bots/NtpTimeProvider.h"
#include "bots/TelegramHttpClient.h"
#include "bots/FreelancerApiClient.h"
#include "bots/BotOrchestrator.h"
//#include "bots/SeedData.h"

// Implementações dos 5 Bots (LSP / ISP)
#include "bots/FreelancerBot.h"
#include "bots/GymCalorieBot.h"
#include "bots/MercadoBot.h"
#include "bots/MoneyManagerBot.h"
#include "bots/TaskManagerBot.h"

// ============================================================================
// Composição das Dependências (Composition Root - Injeção de Dependência)
// ============================================================================
static LedStatusIndicator statusIndicator(AppConfig::LED_PIN);
static NvsCredentialsStore credentialsStore("wifi-cfg");
static WiFiService wifiService(credentialsStore, statusIndicator);
static GitHubOtaUpdater otaUpdater(statusIndicator,
                                   AppConfig::GITHUB_FIRMWARE_MD5_URL,
                                   AppConfig::GITHUB_FIRMWARE_BIN_URL,
                                   AppConfig::OTA_CHECK_INTERVAL_MS);

// Camada de Persistência (LittleFS) e Tempo (NTP)
static LittleFsJsonStore jsonStore;
static NtpTimeProvider timeProvider(AppConfig::NTP_GMT_OFFSET_SEC,
                                    AppConfig::NTP_DAYLIGHT_OFFSET_SEC);

// Contexto TLS único compartilhado por todos os bots (economia de RAM no ESP32-WROOM-32E)
static SharedHttpsTransport httpsTransport;

// Clientes HTTP injetados em cada Bot (tokens carregados da NVS)
static TelegramHttpClient tgFreelancer(httpsTransport, "", true);
static TelegramHttpClient tgGymCalorie(httpsTransport, "", false);
static TelegramHttpClient tgMercado(httpsTransport, "", false);
static TelegramHttpClient tgMoneyManager(httpsTransport, "", false);
static TelegramHttpClient tgTaskManager(httpsTransport, "", false);

static FreelancerApiClient freelancerApiClient(httpsTransport,
                                               "",
                                               AppConfig::FREELANCER_ACTIVE_PROJECTS_URL);

// Instâncias dos 5 Bots (User ID autorizado carregado da NVS)
static FreelancerBot freelancerBot(tgFreelancer,
                                   freelancerApiClient,
                                   jsonStore,
                                   AppConfig::FILE_FREELANCER_SEEN_JSON,
                                   AppConfig::FILE_FREELANCER_FILTERS_JSON,
                                   "",
                                   0,
                                   AppConfig::FREELANCER_FETCH_INTERVAL_MS);

static GymCalorieBot gymCalorieBot(tgGymCalorie,
                                   jsonStore,
                                   timeProvider,
                                   AppConfig::FILE_GYM_CALORIES_JSON,
                                   0);

static MercadoBot mercadoBot(tgMercado,
                             jsonStore,
                             AppConfig::FILE_MERCADO_JSON,
                             0);

static MoneyManagerBot moneyManagerBot(tgMoneyManager,
                                       jsonStore,
                                       timeProvider,
                                       AppConfig::FILE_MONEY_JSON,
                                       0);

static TaskManagerBot taskManagerBot(tgTaskManager,
                                     jsonStore,
                                     AppConfig::FILE_TASKS_JSON,
                                     0);

// Orquestrador Round-Robin para os 5 Bots
static BotOrchestrator botOrchestrator(timeProvider, AppConfig::BOT_ROUND_ROBIN_STEP_MS);

static void initializeBotStorageAndRoster() {
  /*if (jsonStore.begin()) {
    jsonStore.ensureSeedFile(AppConfig::FILE_FREELANCER_FILTERS_JSON, SeedData::DEFAULT_DISCARD_KEYWORDS_JSON);
    jsonStore.ensureSeedFile(AppConfig::FILE_MERCADO_JSON, SeedData::DEFAULT_MERCADO_JSON);
    jsonStore.ensureSeedFile(AppConfig::FILE_MONEY_JSON, SeedData::DEFAULT_MONEY_JSON);
    jsonStore.ensureSeedFile(AppConfig::FILE_GYM_CALORIES_JSON, SeedData::DEFAULT_GYM_CALORIES_JSON);
    jsonStore.ensureSeedFile(AppConfig::FILE_TASKS_JSON, SeedData::DEFAULT_TASKS_JSON);
  }*/

  botOrchestrator.registerBot(freelancerBot);
  botOrchestrator.registerBot(gymCalorieBot);
  botOrchestrator.registerBot(mercadoBot);
  botOrchestrator.registerBot(moneyManagerBot);
  botOrchestrator.registerBot(taskManagerBot);
}

void setup() {
  Serial.begin(115200);

  // Inicializa o controlador visual do LED (Task FreeRTOS no Core 0)
  statusIndicator.begin();

  // Carrega credenciais e tokens da NVS (após initArduino ter inicializado a partição NVS)
  wifiService.begin();

  // Prepara o armazenamento LittleFS e registra os 5 bots no orquestrador
  initializeBotStorageAndRoster();

  // 1) Abre o portal de configuração (ESP_MATEUS) por 30s (estende para +2 min se alguém conectar)
  ConfigPortal portal(wifiService,
                      statusIndicator,
                      AppConfig::AP_SSID,
                      AppConfig::CONFIG_PORTAL_INITIAL_TIMEOUT_MS,
                      AppConfig::CONFIG_PORTAL_EXTENDED_TIMEOUT_MS);
  portal.runBlocking();

  // Carrega os tokens e o User ID autorizado salvos na NVS (via portal cativo)
  ApiTokens savedTokens = wifiService.getCurrentTokens();
  tgFreelancer.setBotToken(savedTokens.telegramFreelancer);
  tgGymCalorie.setBotToken(savedTokens.telegramGymCalorie);
  tgMercado.setBotToken(savedTokens.telegramMercado);
  tgMoneyManager.setBotToken(savedTokens.telegramMoneyManager);
  tgTaskManager.setBotToken(savedTokens.telegramTaskManager);
  freelancerApiClient.setOAuthToken(savedTokens.freelancerApiToken);

  int64_t authUserId = strtoll(savedTokens.authorizedUserId.c_str(), nullptr, 10);
  freelancerBot.setAuthorizedUserId(authUserId);
  gymCalorieBot.setAuthorizedUserId(authUserId);
  mercadoBot.setAuthorizedUserId(authUserId);
  moneyManagerBot.setAuthorizedUserId(authUserId);
  taskManagerBot.setAuthorizedUserId(authUserId);

  // 2) Conecta na rede Wi-Fi configurada
  if (wifiService.connectWithSaved(AppConfig::WIFI_CONNECT_TIMEOUT_MS)) {
    botOrchestrator.begin();
  }
}

void loop() {
  // Garante que a conexão Wi-Fi seja mantida
  wifiService.ensureConnected(AppConfig::WIFI_CONNECT_TIMEOUT_MS);

  if (wifiService.isConnected()) {
    // Verifica a cada 60 segundos no GitHub se há um binário diferente do instalado
    otaUpdater.loop();

    // Executa os 5 bots em Round-Robin cooperativo
    botOrchestrator.loop();
  }

  delay(50);
}
