#include "ConfigPortal.h"
#include <WiFi.h>

ConfigPortal::ConfigPortal(WiFiService& wifiService,
                           IStatusIndicator& indicator,
                           const char* apSsid,
                           uint32_t initialTimeoutMs,
                           uint32_t extendedTimeoutMs)
    : _wifiService(wifiService),
      _indicator(indicator),
      _apSsid(apSsid),
      _initialTimeoutMs(initialTimeoutMs),
      _extendedTimeoutMs(extendedTimeoutMs),
      _activeTimeoutMs(initialTimeoutMs),
      _server(80),
      _startTimeMs(0),
      _clientConnected(false),
      _finishEarly(false),
      _lastTestResultJson("{\"status\":\"idle\",\"message\":\"Nenhum teste realizado.\"}") {}

void ConfigPortal::runBlocking() {
  _indicator.setState(SystemState::CONFIG_MODE);
  _startTimeMs = millis();
  _activeTimeoutMs = _initialTimeoutMs;
  _clientConnected = false;
  _finishEarly = false;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(_apSsid);

  IPAddress apIP = WiFi.softAPIP();
  Serial.println();
  Serial.println("==========================================");
  Serial.printf("[PORTAL] Modo de configuracao iniciado (%lu s para conexao)\n", _initialTimeoutMs / 1000);
  Serial.printf("[PORTAL] Conecte-se no Wi-Fi: %s\n", _apSsid);
  Serial.printf("[PORTAL] Acesse no celular:   http://%s\n", apIP.toString().c_str());
  Serial.printf("[PORTAL] MD5 do Firmware:     %s\n", ESP.getSketchMD5().c_str());
  Serial.println("==========================================");

  _dnsServer.start(53, "*", apIP);
  setupRoutes();
  _server.begin();

  while (millis() - _startTimeMs < _activeTimeoutMs && !_finishEarly) {
    if (!_clientConnected && WiFi.softAPgetStationNum() > 0) {
      _clientConnected = true;
      _startTimeMs = millis();
      _activeTimeoutMs = _extendedTimeoutMs;
      Serial.printf("[PORTAL] Usuario conectado ao Wi-Fi AP! Estendendo modo de configuracao por mais %lu segundos.\n",
                    _extendedTimeoutMs / 1000);
    }

    _dnsServer.processNextRequest();
    _server.handleClient();
    delay(5);
  }

  if (!_clientConnected && !_finishEarly) {
    Serial.printf("[PORTAL] Nenhum dispositivo conectou em %lu segundos. Saindo do modo de configuracao e iniciando modo normal...\n",
                  _initialTimeoutMs / 1000);
  } else {
    Serial.println("[PORTAL] Encerrando portal AP e iniciando modo normal...");
  }
  _server.stop();
  _dnsServer.stop();
  WiFi.softAPdisconnect(true);
}

void ConfigPortal::setupRoutes() {
  _server.on("/", HTTP_GET, [this]() {
    _server.send(200, "text/html", buildHtmlPage());
  });

  _server.on("/save", HTTP_POST, [this]() {
    bool savedWifi = false;
    if (_server.hasArg("ssid") && _server.arg("ssid").length() > 0) {
      WiFiCredentials creds{_server.arg("ssid"), _server.arg("pass")};
      _wifiService.updateCredentials(creds);
      savedWifi = true;
    }

    ApiTokens tokens = _wifiService.getCurrentTokens();
    if (_server.hasArg("auth_uid")) tokens.authorizedUserId     = _server.arg("auth_uid");
    if (_server.hasArg("tg_free"))  tokens.telegramFreelancer   = _server.arg("tg_free");
    if (_server.hasArg("tg_gym"))   tokens.telegramGymCalorie   = _server.arg("tg_gym");
    if (_server.hasArg("tg_merc"))  tokens.telegramMercado      = _server.arg("tg_merc");
    if (_server.hasArg("tg_money")) tokens.telegramMoneyManager = _server.arg("tg_money");
    if (_server.hasArg("tg_task"))  tokens.telegramTaskManager  = _server.arg("tg_task");
    if (_server.hasArg("fl_oauth")) tokens.freelancerApiToken   = _server.arg("fl_oauth");
    _wifiService.updateTokens(tokens);

    String msg = savedWifi
        ? ("Configurações e tokens salvos (Wi-Fi: \"" + htmlEscape(_server.arg("ssid")) + "\")!")
        : String("Tokens de API salvos na NVS com sucesso!");
    _server.send(200, "text/html", buildHtmlPage(msg));
  });

  _server.on("/scan", HTTP_GET, [this]() {
    auto networks = _wifiService.scanNetworks();
    String json = "[";
    for (size_t i = 0; i < networks.size(); i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + jsonEscape(networks[i].ssid) + "\","
              "\"rssi\":" + String(networks[i].rssi) + ","
              "\"secure\":" + String(networks[i].secure ? "true" : "false") + "}";
    }
    json += "]";
    _server.send(200, "application/json", json);
  });

  _server.on("/test", HTTP_POST, [this]() {
    WiFiCredentials current = _wifiService.getCurrentCredentials();
    String testSsid = _server.hasArg("ssid") ? _server.arg("ssid") : current.ssid;
    String testPass = _server.hasArg("pass") ? _server.arg("pass") : current.password;
    testSsid.trim();

    if (testSsid.length() == 0) {
      _server.send(400, "application/json", "{\"success\":false,\"error\":\"SSID vazio\"}");
      return;
    }

    String obtainedIp;
    bool ok = _wifiService.testConnection({testSsid, testPass}, 10000, obtainedIp);

    if (ok) {
      _lastTestResultJson = "{\"status\":\"ok\",\"ip\":\"" + obtainedIp + "\"}";
      _server.send(200, "application/json", "{\"success\":true,\"ip\":\"" + obtainedIp + "\"}");
    } else {
      _lastTestResultJson = "{\"status\":\"fail\",\"ip\":\"\"}";
      _server.send(200, "application/json", "{\"success\":false}");
    }

    _indicator.setState(SystemState::CONFIG_MODE);
  });

  _server.on("/test-status", HTTP_GET, [this]() {
    _server.send(200, "application/json", _lastTestResultJson);
  });

  _server.on("/start-now", HTTP_POST, [this]() {
    if (_server.hasArg("ssid") && _server.arg("ssid").length() > 0) {
      _wifiService.updateCredentials({_server.arg("ssid"), _server.arg("pass")});
    }
    ApiTokens tokens = _wifiService.getCurrentTokens();
    if (_server.hasArg("auth_uid")) tokens.authorizedUserId     = _server.arg("auth_uid");
    if (_server.hasArg("tg_free"))  tokens.telegramFreelancer   = _server.arg("tg_free");
    if (_server.hasArg("tg_gym"))   tokens.telegramGymCalorie   = _server.arg("tg_gym");
    if (_server.hasArg("tg_merc"))  tokens.telegramMercado      = _server.arg("tg_merc");
    if (_server.hasArg("tg_money")) tokens.telegramMoneyManager = _server.arg("tg_money");
    if (_server.hasArg("tg_task"))  tokens.telegramTaskManager  = _server.arg("tg_task");
    if (_server.hasArg("fl_oauth")) tokens.freelancerApiToken   = _server.arg("fl_oauth");
    _wifiService.updateTokens(tokens);

    Serial.println("[PORTAL] Botao 'Iniciar Agora' acionado! Pulando tempo restante...");
    _server.send(200, "application/json", "{\"starting\":true}");
    delay(300);
    _finishEarly = true;
  });

  _server.onNotFound([this]() {
    _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    _server.send(302, "text/plain", "");
  });
}

String ConfigPortal::buildHtmlPage(const String& message) const {
  uint32_t elapsed = millis() - _startTimeMs;
  uint32_t remainingSec = (elapsed < _activeTimeoutMs) ? (_activeTimeoutMs - elapsed) / 1000 : 0;
  WiFiCredentials creds = _wifiService.getCurrentCredentials();
  ApiTokens tokens = _wifiService.getCurrentTokens();

  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>ESP_MATEUS - Configurar Wi-Fi</title>
  <style>
    * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { margin: 0; padding: 14px; background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: flex-start; min-height: 100vh; }
    .card { width: 100%; max-width: 420px; background: #1e293b; border-radius: 16px; padding: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.4); border: 1px solid #334155; }
    h1 { margin: 0 0 8px 0; font-size: 1.35rem; color: #38bdf8; text-align: center; }
    .timer { font-size: 0.9rem; color: #94a3b8; margin-bottom: 16px; padding: 8px 12px; background: #0f172a; border-radius: 8px; text-align: center; }
    .timer strong { color: #facc15; }
    .msg { padding: 12px; border-radius: 8px; margin-bottom: 14px; font-size: 0.9rem; line-height: 1.4; }
    .msg.ok { background: #065f46; color: #d1fae5; border: 1px solid #059669; }
    .msg.err { background: #7f1d1d; color: #fee2e2; border: 1px solid #dc2626; }
    .msg.info { background: #1e3a8a; color: #dbeafe; border: 1px solid #3b82f6; }
    label { display: block; font-size: 0.85rem; color: #cbd5e1; margin-bottom: 6px; font-weight: 600; }
    input[type="text"], input[type="password"] {
      width: 100%; padding: 12px; margin-bottom: 14px; border-radius: 8px;
      border: 1px solid #475569; background: #0f172a; color: #f8fafc; font-size: 1rem;
    }
    input:focus { outline: none; border-color: #38bdf8; }
    .btn {
      width: 100%; padding: 13px; border: none; border-radius: 8px;
      font-size: 0.95rem; font-weight: 600; cursor: pointer; margin-bottom: 10px;
      display: flex; align-items: center; justify-content: center; gap: 6px; text-decoration: none;
    }
    .btn:disabled { opacity: 0.6; cursor: not-allowed; }
    .btn-primary { background: #0284c7; color: white; }
    .btn-secondary { background: #334155; color: #f8fafc; border: 1px solid #475569; }
    .btn-test { background: #d97706; color: white; }
    .btn-start { background: #16a34a; color: white; margin-top: 6px; }
    .net-list { margin-bottom: 14px; max-height: 190px; overflow-y: auto; border-radius: 8px; background: #0f172a; border: 1px solid #334155; display: none; }
    .net-item { padding: 10px 12px; border-bottom: 1px solid #1e293b; display: flex; justify-content: space-between; align-items: center; cursor: pointer; font-size: 0.9rem; }
    .net-item:last-child { border-bottom: none; }
    .net-item:hover, .net-item:active { background: #1e3a8a; }
    .net-rssi { font-size: 0.78rem; color: #94a3b8; }
    .divider { height: 1px; background: #334155; margin: 14px 0; }
    .current { margin-top: 8px; font-size: 0.78rem; color: #94a3b8; text-align: center; }
  </style>
</head>
<body>
  <div class="card">
    <h1>ESP_MATEUS - Wi-Fi</h1>
    <div class="timer">Tempo restante no portal: <strong id="t">)rawliteral" +
                String(remainingSec) + R"rawliteral(s</strong></div>

    <div id="statusBox">)rawliteral" +
                (message.length() > 0 ? ("<div class=\"msg ok\">" + message + "</div>") : "") + R"rawliteral(</div>

    <button type="button" class="btn btn-secondary" id="btnScan" onclick="scanNetworks()">
      🔍 Buscar Redes Wi-Fi Próximas
    </button>
    <div id="netList" class="net-list"></div>

    <form id="wifiForm" action="/save" method="POST">
      <label for="ssid">Nome da Rede (SSID)</label>
      <input type="text" id="ssid" name="ssid" placeholder="Selecione acima ou digite o SSID" value=")rawliteral" +
                htmlEscape(creds.ssid) + R"rawliteral(">

      <label for="pass">Senha do Wi-Fi</label>
      <input type="password" id="pass" name="pass" placeholder="Digite a senha (vazio se aberta)" value=")rawliteral" +
                htmlEscape(creds.password) + R"rawliteral(">

      <button type="button" class="btn btn-test" id="btnTest" onclick="testConnection()">
        ⚡ Testar Conexão Wi-Fi
      </button>

      <div class="divider"></div>
      <h1 style="font-size: 1.1rem; margin-bottom: 12px;">🔑 Tokens & Autorização</h1>

      <label for="auth_uid">Telegram Authorized User ID</label>
      <input type="text" id="auth_uid" name="auth_uid" placeholder="Ex: 1234567890" value=")rawliteral" +
                htmlEscape(tokens.authorizedUserId) + R"rawliteral(">

      <label for="tg_free">Telegram Token — Freelancer Bot</label>
      <input type="password" id="tg_free" name="tg_free" placeholder="Ex: 123456:ABC-DEF..." value=")rawliteral" +
                htmlEscape(tokens.telegramFreelancer) + R"rawliteral(">

      <label for="tg_gym">Telegram Token — Gym Calorie Bot</label>
      <input type="password" id="tg_gym" name="tg_gym" placeholder="Ex: 123456:ABC-DEF..." value=")rawliteral" +
                htmlEscape(tokens.telegramGymCalorie) + R"rawliteral(">

      <label for="tg_merc">Telegram Token — Mercado Bot</label>
      <input type="password" id="tg_merc" name="tg_merc" placeholder="Ex: 123456:ABC-DEF..." value=")rawliteral" +
                htmlEscape(tokens.telegramMercado) + R"rawliteral(">

      <label for="tg_money">Telegram Token — Money Manager Bot</label>
      <input type="password" id="tg_money" name="tg_money" placeholder="Ex: 123456:ABC-DEF..." value=")rawliteral" +
                htmlEscape(tokens.telegramMoneyManager) + R"rawliteral(">

      <label for="tg_task">Telegram Token — Task Manager Bot</label>
      <input type="password" id="tg_task" name="tg_task" placeholder="Ex: 123456:ABC-DEF..." value=")rawliteral" +
                htmlEscape(tokens.telegramTaskManager) + R"rawliteral(">

      <label for="fl_oauth">Freelancer.com API OAuth Token</label>
      <input type="password" id="fl_oauth" name="fl_oauth" placeholder="Token OAuth do Freelancer.com" value=")rawliteral" +
                htmlEscape(tokens.freelancerApiToken) + R"rawliteral(">

      <button type="submit" class="btn btn-primary">💾 Salvar Wi-Fi e Tokens</button>
    </form>

    <div class="divider"></div>

    <button type="button" class="btn btn-start" id="btnStart" onclick="startImmediately()">
      🚀 Iniciar Agora (Pular 2 min)
    </button>

    <div class="current">Rede salva: <strong id="savedLabel">)rawliteral" +
                (creds.isValid() ? htmlEscape(creds.ssid) : String("Nenhuma")) + R"rawliteral(</strong><br>MD5 atual: <code>)rawliteral" +
                ESP.getSketchMD5().substring(0, 12) + R"rawliteral(...</code></div>
  </div>

  <script>
    let sec = )rawliteral" +
                String(remainingSec) + R"rawliteral(;
    const timerEl = document.getElementById('t');
    const statusBox = document.getElementById('statusBox');

    setInterval(() => {
      if (sec > 0) {
        sec--;
        timerEl.textContent = sec + 's';
      } else {
        timerEl.textContent = 'Iniciando...';
      }
    }, 1000);

    function showStatus(text, type) {
      statusBox.innerHTML = '<div class="msg ' + type + '">' + text + '</div>';
    }

    async function scanNetworks() {
      const btn = document.getElementById('btnScan');
      const list = document.getElementById('netList');
      btn.disabled = true;
      btn.textContent = '⏳ Buscando redes...';
      list.style.display = 'none';

      try {
        const res = await fetch('/scan');
        const networks = await res.json();
        list.innerHTML = '';

        if (networks.length === 0) {
          list.innerHTML = '<div class="net-item">Nenhuma rede encontrada</div>';
        } else {
          networks.forEach(n => {
            const div = document.createElement('div');
            div.className = 'net-item';
            const lock = n.secure ? '🔒 ' : '🔓 ';
            div.innerHTML = '<span>' + lock + n.ssid + '</span><span class="net-rssi">' + n.rssi + ' dBm</span>';
            div.onclick = () => {
              document.getElementById('ssid').value = n.ssid;
              document.getElementById('pass').focus();
            };
            list.appendChild(div);
          });
        }
        list.style.display = 'block';
      } catch (e) {
        showStatus('Erro ao buscar redes. Verifique se está conectado ao Wi-Fi ESP_MATEUS.', 'err');
      } finally {
        btn.disabled = false;
        btn.textContent = '🔍 Buscar Redes Wi-Fi Próximas';
      }
    }

    async function testConnection() {
      const ssid = document.getElementById('ssid').value.trim();
      const pass = document.getElementById('pass').value;
      if (!ssid) {
        showStatus('Informe o SSID antes de testar a conexão.', 'err');
        return;
      }

      const btn = document.getElementById('btnTest');
      btn.disabled = true;
      btn.textContent = '⏳ Testando conexão (aguarde ~10s)...';
      showStatus('Testando conexão na rede <b>' + ssid + '</b>... Observe o LED da placa.', 'info');

      try {
        const body = new URLSearchParams({ ssid, pass });
        const res = await fetch('/test', { method: 'POST', body });
        const data = await res.json();
        document.getElementById('savedLabel').textContent = ssid;

        if (data.success) {
          showStatus('✅ Sucesso! Conectou em <b>' + ssid + '</b> (IP: ' + data.ip + ').', 'ok');
        } else {
          showStatus('❌ Falha ao conectar em <b>' + ssid + '</b>. Verifique a senha.', 'err');
        }
      } catch (e) {
        setTimeout(async () => {
          try {
            const r = await fetch('/test-status');
            const d = await r.json();
            if (d.status === 'ok') {
              showStatus('✅ Sucesso! Conectou em <b>' + ssid + '</b> (IP: ' + d.ip + ').', 'ok');
            } else if (d.status === 'fail') {
              showStatus('❌ Falha ao conectar em <b>' + ssid + '</b>. Verifique a senha.', 'err');
            }
          } catch (_) {
            showStatus('O teste mudou o canal Wi-Fi. Reconecte no Wi-Fi ESP_MATEUS se necessário.', 'info');
          }
        }, 2500);
      } finally {
        btn.disabled = false;
        btn.textContent = '⚡ Testar Conexão Wi-Fi';
      }
    }

    async function startImmediately() {
      const btn = document.getElementById('btnStart');
      btn.disabled = true;
      btn.textContent = '🚀 Iniciando...';

      try {
        const form = document.getElementById('wifiForm');
        const body = new URLSearchParams(new FormData(form));
        await fetch('/start-now', { method: 'POST', body });
      } catch (_) {}

      timerEl.textContent = '0s';
      sec = 0;
      showStatus('🚀 Tempo restante pulado! O portal foi encerrado e o ESP32 está conectando ao Wi-Fi.', 'ok');
    }
  </script>
</body>
</html>
)rawliteral";
  return html;
}

String ConfigPortal::htmlEscape(const String& str) {
  String out = str;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

String ConfigPortal::jsonEscape(const String& str) {
  String out = str;
  out.replace("\\", "\\\\");
  out.replace("\"", "\\\"");
  out.replace("\n", "\\n");
  out.replace("\r", "\\r");
  return out;
}

