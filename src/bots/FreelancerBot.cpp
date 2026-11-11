#include "bots/FreelancerBot.h"
#include "bots/TextUtils.h"
#include <math.h>

FreelancerBot::FreelancerBot(ITelegramClient& telegram,
                             IFreelancerClient& freelancer,
                             IJsonStore& store,
                             const char* seenFilePath,
                             const char* filtersFilePath,
                             const char* authorizedChatId,
                             int64_t authorizedUserId,
                             uint32_t fetchIntervalMs)
    : _telegram(telegram),
      _freelancer(freelancer),
      _store(store),
      _seenFilePath(seenFilePath),
      _filtersFilePath(filtersFilePath),
      _authorizedChatId(authorizedChatId),
      _authorizedUserId(authorizedUserId),
      _fetchIntervalMs(fetchIntervalMs) {}

void FreelancerBot::begin() {
  loadSeenProjects();
  loadDiscardKeywords();
}

void FreelancerBot::loadSeenProjects() {
  _seenProjects.clear();
  JsonDocument doc;
  if (_store.load(_seenFilePath, doc) && doc.is<JsonArray>()) {
    JsonArray arr = doc.as<JsonArray>();
    size_t total = arr.size();
    size_t startIdx = (total > MAX_SEEN_PROJECTS) ? (total - MAX_SEEN_PROJECTS) : 0;
    for (size_t i = startIdx; i < total; i++) {
      _seenProjects.push_back(String(arr[i].as<const char*>()));
    }
  }
  Serial.printf("[FREELANCER-BOT] %u projetos vistos carregados.\n",
                static_cast<unsigned>(_seenProjects.size()));
}

void FreelancerBot::saveSeenProjects() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const String& pid : _seenProjects) {
    arr.add(pid);
  }
  _store.save(_seenFilePath, doc);
}

void FreelancerBot::addSeenProject(const String& pid) {
  if (isProjectSeen(pid)) {
    return;
  }
  if (_seenProjects.size() >= MAX_SEEN_PROJECTS) {
    _seenProjects.erase(_seenProjects.begin());
  }
  _seenProjects.push_back(pid);
  saveSeenProjects();
}

bool FreelancerBot::isProjectSeen(const String& pid) const {
  for (const String& s : _seenProjects) {
    if (s == pid) return true;
  }
  return false;
}

void FreelancerBot::loadDiscardKeywords() {
  _discardKeywords.clear();
  JsonDocument doc;
  if (_store.load(_filtersFilePath, doc) && doc.is<JsonArray>()) {
    for (JsonVariant v : doc.as<JsonArray>()) {
      String kw = v.as<String>();
      kw.trim();
      kw.toLowerCase();
      if (kw.length() > 0) {
        _discardKeywords.push_back(kw);
      }
    }
  }
}

void FreelancerBot::saveDiscardKeywords() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const String& kw : _discardKeywords) {
    arr.add(kw);
  }
  _store.save(_filtersFilePath, doc);
}

bool FreelancerBot::containsDiscardKeyword(const String& title, const String& description) const {
  String combined = title + " " + description;
  combined.toLowerCase();
  for (const String& kw : _discardKeywords) {
    if (kw.length() > 0 && combined.indexOf(kw) != -1) {
      Serial.printf("[FREELANCER-BOT] Projeto descartado pela palavra-chave: %s\n", kw.c_str());
      return true;
    }
  }
  return false;
}

String FreelancerBot::escapeHtml(const String& input) {
  String s = input;
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  return s;
}

void FreelancerBot::generateTemplateProposal(const FreelancerProject& proj,
                                             String& outProposal,
                                             float& outAmount,
                                             int& outPeriodDays) {
  constexpr float BID_RATIO = 0.85f;
  if (proj.minBudget > 0.0f && proj.maxBudget > 0.0f) {
    outAmount = roundf((proj.minBudget + (proj.maxBudget - proj.minBudget) * BID_RATIO) * 100.0f) / 100.0f;
  } else if (proj.maxBudget > 0.0f) {
    outAmount = roundf((proj.maxBudget * BID_RATIO) * 100.0f) / 100.0f;
  } else if (proj.minBudget > 0.0f) {
    outAmount = proj.minBudget;
  } else {
    outAmount = 100.0f;
  }

  outPeriodDays = 3;

  String cleanTitle = proj.title;
  cleanTitle.trim();

  outProposal =
      "Hey! I can help you with '" + cleanTitle + "'. "
      "I have extensive experience working with Embedded Systems & Firmware (ESP32, STM32, C/C++), "
      "IoT & Communications (MQTT, WebSockets, REST APIs), Python & Node.js Backend & API Integrations, "
      "and I've built similar reliable systems before. "
      "This should be straightforward to complete for you. Let me know if you'd like to discuss the details!";
}

String FreelancerBot::buildBidCardText(const FreelancerBidDraft& draft, const String& errorMsg) {
  String out;
  if (errorMsg.length() > 0) {
    out += "❌ <b>BID SUBMISSION FAILED</b>\n⚠️ <b>Error:</b> " + escapeHtml(errorMsg) + "\n\n";
  }
  char priceBuf[32];
  snprintf(priceBuf, sizeof(priceBuf), "%.2f", draft.amount);

  out += "📝 <b>BID DRAFT</b>\n"
         "📌 <b>Project:</b> " + escapeHtml(draft.project.title) + "\n"
         "💰 <b>Your Price:</b> $" + String(priceBuf) + " " + escapeHtml(draft.project.currencyCode) + "\n"
         "⏱ <b>Delivery Time:</b> " + String(draft.periodDays) + " days\n\n"
         "📋 <b>Generated Proposal:</b>\n"
         "<i>" + escapeHtml(draft.proposal) + "</i>\n\n"
         "👇 <i>Tap a button below to adjust price/days/text or submit to Freelancer.com!</i>";
  return out;
}

String FreelancerBot::buildBidCardKeyboard(const String& pid) {
  return "{\"inline_keyboard\":["
         "[{\"text\":\"🚀 Confirm & Submit Bid\",\"callback_data\":\"submit_" + pid + "\"}],"
         "[{\"text\":\"💰 Edit Price\",\"callback_data\":\"editprice_" + pid + "\"},"
          "{\"text\":\"⏱ Edit Days\",\"callback_data\":\"editdays_" + pid + "\"}],"
         "[{\"text\":\"✏️ Edit Proposal Text\",\"callback_data\":\"edittext_" + pid + "\"}],"
         "[{\"text\":\"❌ Cancel Draft\",\"callback_data\":\"cancel_" + pid + "\"}]"
         "]}";
}

FreelancerBidDraft* FreelancerBot::findOrFetchDraft(const String& projectId) {
  for (FreelancerBidDraft& d : _pendingDrafts) {
    if (d.project.id == projectId) {
      return &d;
    }
  }

  FreelancerProject fetched;
  if (!_freelancer.fetchProjectDetails(projectId, fetched)) {
    return nullptr;
  }

  if (_pendingDrafts.size() >= MAX_PENDING_DRAFTS) {
    _pendingDrafts.erase(_pendingDrafts.begin());
  }

  FreelancerBidDraft newDraft;
  newDraft.project = fetched;
  generateTemplateProposal(fetched, newDraft.proposal, newDraft.amount, newDraft.periodDays);
  _pendingDrafts.push_back(newDraft);
  return &_pendingDrafts.back();
}

void FreelancerBot::removeDraft(const String& projectId) {
  for (size_t i = 0; i < _pendingDrafts.size(); i++) {
    if (_pendingDrafts[i].project.id == projectId) {
      _pendingDrafts.erase(_pendingDrafts.begin() + i);
      return;
    }
  }
}

void FreelancerBot::poll() {
  handleTelegramUpdates();

  uint32_t now = millis();
  if (!_firstFetchDone || (now - _lastProjectFetchMs >= _fetchIntervalMs)) {
    _firstFetchDone = true;
    _lastProjectFetchMs = now;
    fetchAndNotifyProjects();
  }
}

void FreelancerBot::handleTelegramUpdates() {
  std::vector<TelegramUpdate> updates;
  int64_t offset = (_lastUpdateId > 0) ? (_lastUpdateId + 1) : 0;
  if (!_telegram.getUpdates(offset, updates)) {
    return;
  }

  for (const TelegramUpdate& u : updates) {
    if (u.updateId > _lastUpdateId) {
      _lastUpdateId = u.updateId;
    }
    if (u.isCallback) {
      handleCallbackQuery(u.callback);
    } else {
      handleMessage(u.message);
    }
  }
}

void FreelancerBot::handleCallbackQuery(const TelegramCallbackQuery& cb) {
  if (cb.userId != _authorizedUserId) {
    _telegram.answerCallbackQuery(cb.id, "Unauthorized");
    return;
  }

  const String& data = cb.data;
  String chatId = (cb.chatId.length() > 0) ? cb.chatId : _authorizedChatId;

  if (data.startsWith("draft_")) {
    String pid = data.substring(6);
    _telegram.answerCallbackQuery(cb.id, "📝 Preparing Proposal Draft...");

    FreelancerBidDraft* draft = findOrFetchDraft(pid);
    if (!draft) {
      _telegram.sendMessage(
          chatId,
          "⚠️ Project data could not be retrieved from Freelancer.com. Please try again.",
          "HTML");
      return;
    }

    if (draft->proposal.length() == 0) {
      generateTemplateProposal(draft->project, draft->proposal, draft->amount, draft->periodDays);
    }

    String cardText = buildBidCardText(*draft);
    String cardKb = buildBidCardKeyboard(pid);
    int64_t msgId = _telegram.sendMessage(chatId, cardText, "HTML", cardKb);
    if (msgId > 0) {
      draft->cardMessageId = msgId;
    }
    return;
  }

  _telegram.answerCallbackQuery(cb.id);

  if (data.startsWith("editprice_")) {
    String pid = data.substring(10);
    if (!findOrFetchDraft(pid)) {
      _telegram.sendMessage(chatId, "⚠️ Project data not found. Please select 'Draft Bid' first.", "HTML");
      return;
    }
    _activePromptProjectId = pid;
    _activePromptField = PromptField::PRICE;
    _telegram.sendMessage(
        chatId,
        "💰 <b>Reply to this chat with your new bid amount in USD</b> (e.g. <code>600</code>):",
        "HTML");
  } else if (data.startsWith("editdays_")) {
    String pid = data.substring(9);
    if (!findOrFetchDraft(pid)) {
      _telegram.sendMessage(chatId, "⚠️ Project data not found. Please select 'Draft Bid' first.", "HTML");
      return;
    }
    _activePromptProjectId = pid;
    _activePromptField = PromptField::PERIOD;
    _telegram.sendMessage(
        chatId,
        "⏱ <b>Reply to this chat with your estimated delivery time in days</b> (e.g. <code>3</code>):",
        "HTML");
  } else if (data.startsWith("edittext_")) {
    String pid = data.substring(9);
    if (!findOrFetchDraft(pid)) {
      _telegram.sendMessage(chatId, "⚠️ Project data not found. Please select 'Draft Bid' first.", "HTML");
      return;
    }
    _activePromptProjectId = pid;
    _activePromptField = PromptField::PROPOSAL;
    _telegram.sendMessage(
        chatId,
        "✏️ <b>Reply to this chat with your updated proposal text:</b>",
        "HTML");
  } else if (data.startsWith("submit_")) {
    String pid = data.substring(7);
    FreelancerBidDraft* draft = findOrFetchDraft(pid);
    if (!draft || draft->proposal.length() == 0) {
      _telegram.sendMessage(
          chatId,
          "❌ Draft session missing or expired. Please click 'Draft Bid' to start a proposal.",
          "HTML");
      return;
    }

    if (cb.messageId > 0) {
      _telegram.editMessageText(
          chatId,
          cb.messageId,
          "⏳ <b>Submitting bid to Freelancer.com for '" + escapeHtml(draft->project.title) + "'...</b>",
          "HTML");
    }

    String respMsg;
    bool ok = _freelancer.placeBid(pid, draft->amount, draft->periodDays, draft->proposal, respMsg);
    if (ok) {
      char priceBuf[32];
      snprintf(priceBuf, sizeof(priceBuf), "%.2f", draft->amount);
      String statusText =
          "✅ <b>BID SUBMITTED SUCCESSFULLY!</b>\n\n"
          "📌 <b>Project:</b> " + escapeHtml(draft->project.title) + "\n"
          "💰 <b>Amount:</b> $" + String(priceBuf) + " " + escapeHtml(draft->project.currencyCode) + "\n"
          "⏱ <b>Period:</b> " + String(draft->periodDays) + " days\n"
          "ℹ️ " + escapeHtml(respMsg);

      if (cb.messageId > 0) {
        _telegram.editMessageText(chatId, cb.messageId, statusText, "HTML");
      } else {
        _telegram.sendMessage(chatId, statusText, "HTML");
      }
      removeDraft(pid);
    } else {
      String cardText = buildBidCardText(*draft, respMsg);
      String cardKb = buildBidCardKeyboard(pid);
      if (cb.messageId > 0) {
        _telegram.editMessageText(chatId, cb.messageId, cardText, "HTML", cardKb);
        draft->cardMessageId = cb.messageId;
      } else {
        int64_t sentId = _telegram.sendMessage(chatId, cardText, "HTML", cardKb);
        if (sentId > 0) draft->cardMessageId = sentId;
      }
    }
  } else if (data.startsWith("cancel_")) {
    String pid = data.substring(7);
    removeDraft(pid);
    if (cb.messageId > 0) {
      _telegram.editMessageText(chatId, cb.messageId, "❌ <b>Bid draft canceled.</b>", "HTML");
    }
  }
}

void FreelancerBot::handleMessage(const TelegramMessage& msg) {
  if (msg.userId != _authorizedUserId) {
    return;
  }

  String text = msg.text;
  text.trim();
  if (text.length() == 0) {
    return;
  }

  String chatId = (msg.chatId.length() > 0) ? msg.chatId : _authorizedChatId;

  // Se o usuário estiver respondendo a uma edição de preço/prazo/texto
  if (_activePromptField != PromptField::NONE && !text.startsWith("/")) {
    PromptField field = _activePromptField;
    String pid = _activePromptProjectId;
    _activePromptField = PromptField::NONE;
    _activePromptProjectId = "";

    FreelancerBidDraft* draft = findOrFetchDraft(pid);
    if (!draft) {
      _telegram.sendMessage(chatId, "⚠️ Draft not found.", "HTML");
      return;
    }

    if (field == PromptField::PRICE) {
      String cleanPrice = text;
      cleanPrice.replace("$", "");
      cleanPrice.replace(",", ".");
      cleanPrice.trim();
      float val = cleanPrice.toFloat();
      if (val > 0.0f) {
        draft->amount = val;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", val);
        _telegram.sendMessage(chatId, "✅ Price updated to <b>$" + String(buf) + "</b>!", "HTML");
      } else {
        _telegram.sendMessage(chatId, "⚠️ Invalid amount entered. Keeping previous price.", "HTML");
      }
    } else if (field == PromptField::PERIOD) {
      int days = text.toInt();
      if (days > 0) {
        draft->periodDays = days;
        _telegram.sendMessage(
            chatId,
            "✅ Delivery timeframe updated to <b>" + String(days) + " days</b>!",
            "HTML");
      } else {
        _telegram.sendMessage(chatId, "⚠️ Invalid days entered. Keeping previous timeframe.", "HTML");
      }
    } else if (field == PromptField::PROPOSAL) {
      draft->proposal = text;
      _telegram.sendMessage(chatId, "✅ Proposal text updated!", "HTML");
    }

    if (draft->cardMessageId > 0) {
      _telegram.editMessageText(
          chatId,
          draft->cardMessageId,
          buildBidCardText(*draft),
          "HTML",
          buildBidCardKeyboard(pid));
    }
    return;
  }

  if (text.startsWith("/discard ")) {
    String kw = text.substring(9);
    kw.trim();
    kw.toLowerCase();
    if (kw.length() > 0) {
      bool exists = false;
      for (const String& k : _discardKeywords) {
        if (k == kw) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        _discardKeywords.push_back(kw);
        saveDiscardKeywords();
      }
      _telegram.sendMessage(chatId, "❌ Added discard keyword:\n<code>" + escapeHtml(kw) + "</code>", "HTML");
    }
  } else if (text.startsWith("/allow ")) {
    String kw = text.substring(7);
    kw.trim();
    kw.toLowerCase();
    for (size_t i = 0; i < _discardKeywords.size(); i++) {
      if (_discardKeywords[i] == kw) {
        _discardKeywords.erase(_discardKeywords.begin() + i);
        saveDiscardKeywords();
        _telegram.sendMessage(
            chatId,
            "✅ Removed discard keyword:\n<code>" + escapeHtml(kw) + "</code>",
            "HTML");
        break;
      }
    }
  } else if (text.startsWith("/filters")) {
    if (_discardKeywords.empty()) {
      _telegram.sendMessage(chatId, "No active discard keywords.", "HTML");
    } else {
      String out = "🚫 <b>Active discard keywords:</b>\n\n";
      for (const String& k : _discardKeywords) {
        out += "• <code>" + escapeHtml(k) + "</code>\n";
      }
      _telegram.sendMessage(chatId, out, "HTML");
    }
  } else if (text.startsWith("/help") || text.startsWith("/start")) {
    _telegram.sendMessage(
        chatId,
        "<b>Commands:</b>\n\n"
        "/discard keyword → block projects\n"
        "/allow keyword → remove block\n"
        "/filters → list blocked keywords\n"
        "/help → show commands",
        "HTML");
  }
}

void FreelancerBot::fetchAndNotifyProjects() {
  if (_authorizedChatId.length() == 0 || _authorizedUserId <= 0) {
    return;
  }
  std::vector<FreelancerProject> projects;
  if (!_freelancer.fetchActiveProjects(projects)) {
    return;
  }

  for (const FreelancerProject& p : projects) {
    if (p.id.length() == 0 || isProjectSeen(p.id)) {
      continue;
    }

    if (containsDiscardKeyword(p.title, p.description)) {
      addSeenProject(p.id);
      continue;
    }

    String descPreview = p.description;
    if (descPreview.length() > 900) {
      descPreview = descPreview.substring(0, 900) + "...";
    }

    String link = "https://www.freelancer.com/projects/" + p.seoUrl;
    String msg =
        "🚀 <b>New Project!</b>\n\n"
        "<b>" + escapeHtml(p.title) + "</b>\n\n"
        "💰 <b>Budget:</b> " + escapeHtml(p.budgetFormatted) + "\n\n"
        "📝 <b>Description:</b>\n" +
        escapeHtml(descPreview) + "\n\n"
        "🔗 <a href=\"" + link + "\">View Project</a>";

    if (_pendingDrafts.size() >= MAX_PENDING_DRAFTS) {
      _pendingDrafts.erase(_pendingDrafts.begin());
    }
    FreelancerBidDraft draft;
    draft.project = p;
    generateTemplateProposal(p, draft.proposal, draft.amount, draft.periodDays);
    _pendingDrafts.push_back(draft);

    String replyMarkup =
        "{\"inline_keyboard\":[[{\"text\":\"📝 Draft Bid\",\"callback_data\":\"draft_" + p.id + "\"}]]}";

    int64_t sentId = _telegram.sendMessage(_authorizedChatId, msg, "HTML", replyMarkup);
    if (sentId > 0) {
      addSeenProject(p.id);
      Serial.printf("[FREELANCER-BOT] Novo projeto notificado: %s (ID: %s)\n",
                    p.title.c_str(),
                    p.id.c_str());
    }
  }
}

