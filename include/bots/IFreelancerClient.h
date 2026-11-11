#pragma once
#include <Arduino.h>
#include <vector>

struct FreelancerProject {
  String id;
  String title;
  String description;
  String seoUrl;
  String currencyCode = "USD";
  float minBudget = 0.0f;
  float maxBudget = 0.0f;
  String budgetFormatted;
};

struct FreelancerBidDraft {
  FreelancerProject project;
  String proposal;
  float amount = 100.0f;
  int periodDays = 3;
  int64_t cardMessageId = 0;
};

// Abstração para integração com a API REST do Freelancer.com (DIP / ISP)
class IFreelancerClient {
public:
  virtual ~IFreelancerClient() = default;

  virtual bool fetchActiveProjects(std::vector<FreelancerProject>& outProjects) = 0;
  virtual bool fetchProjectDetails(const String& projectId, FreelancerProject& outProject) = 0;
  virtual bool placeBid(const String& projectId,
                        float amount,
                        int periodDays,
                        const String& description,
                        String& outMessage) = 0;
};

