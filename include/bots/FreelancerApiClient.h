#pragma once
#include "bots/IFreelancerClient.h"
#include "bots/TelegramHttpClient.h"

// Implementação concreta do cliente da API do Freelancer.com (SRP / DIP)
class FreelancerApiClient : public IFreelancerClient {
public:
  FreelancerApiClient(SharedHttpsTransport& transport,
                      const String& oauthToken,
                      const char* activeProjectsUrl);
  ~FreelancerApiClient() override = default;

  void setOAuthToken(const String& oauthToken);

  bool fetchActiveProjects(std::vector<FreelancerProject>& outProjects) override;
  bool fetchProjectDetails(const String& projectId, FreelancerProject& outProject) override;
  bool placeBid(const String& projectId,
                float amount,
                int periodDays,
                const String& description,
                String& outMessage) override;

private:
  SharedHttpsTransport& _transport;
  String _oauthToken;
  const char* _activeProjectsUrl;
  int64_t _cachedBidderId = 0;

  int64_t fetchSelfBidderId();
  static String formatBudget(float minB, float maxB, const String& currency);
};

