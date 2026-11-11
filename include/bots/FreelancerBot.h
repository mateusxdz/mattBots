#pragma once
#include <vector>
#include "bots/IBot.h"
#include "bots/ITelegramClient.h"
#include "bots/IFreelancerClient.h"
#include "bots/IJsonStore.h"

// Bot de Monitoramento e Envio Interativo de Propostas no Freelancer.com (SRP / LSP / DIP)
class FreelancerBot : public IBot {
public:
  FreelancerBot(ITelegramClient& telegram,
                IFreelancerClient& freelancer,
                IJsonStore& store,
                const char* seenFilePath,
                const char* filtersFilePath,
                const char* authorizedChatId,
                int64_t authorizedUserId,
                uint32_t fetchIntervalMs);
  ~FreelancerBot() override = default;

  const char* name() const override { return "FreelancerBot"; }
  void begin() override;
  void poll() override;
  void setAuthorizedUserId(int64_t userId) {
    _authorizedUserId = userId;
    _authorizedChatId = (userId > 0) ? String(userId) : "";
  }

private:
  enum class PromptField {
    NONE,
    PRICE,
    PERIOD,
    PROPOSAL
  };

  ITelegramClient& _telegram;
  IFreelancerClient& _freelancer;
  IJsonStore& _store;
  const char* _seenFilePath;
  const char* _filtersFilePath;
  String _authorizedChatId;
  int64_t _authorizedUserId;
  uint32_t _fetchIntervalMs;

  int64_t _lastUpdateId = 0;
  uint32_t _lastProjectFetchMs = 0;
  bool _firstFetchDone = false;

  std::vector<String> _seenProjects;
  std::vector<String> _discardKeywords;
  std::vector<FreelancerBidDraft> _pendingDrafts;

  PromptField _activePromptField = PromptField::NONE;
  String _activePromptProjectId;

  static constexpr size_t MAX_SEEN_PROJECTS = 250;
  static constexpr size_t MAX_PENDING_DRAFTS = 10;

  void loadSeenProjects();
  void saveSeenProjects();
  void addSeenProject(const String& pid);
  bool isProjectSeen(const String& pid) const;

  void loadDiscardKeywords();
  void saveDiscardKeywords();
  bool containsDiscardKeyword(const String& title, const String& description) const;

  void handleTelegramUpdates();
  void handleCallbackQuery(const TelegramCallbackQuery& cb);
  void handleMessage(const TelegramMessage& msg);
  void fetchAndNotifyProjects();

  FreelancerBidDraft* findOrFetchDraft(const String& projectId);
  void removeDraft(const String& projectId);

  static void generateTemplateProposal(const FreelancerProject& proj,
                                       String& outProposal,
                                       float& outAmount,
                                       int& outPeriodDays);
  static String buildBidCardText(const FreelancerBidDraft& draft, const String& errorMsg = "");
  static String buildBidCardKeyboard(const String& pid);
  static String escapeHtml(const String& input);
};

