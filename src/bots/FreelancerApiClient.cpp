#include "bots/FreelancerApiClient.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

FreelancerApiClient::FreelancerApiClient(SharedHttpsTransport& transport,
                                         const String& oauthToken,
                                         const char* activeProjectsUrl)
    : _transport(transport),
      _oauthToken(oauthToken),
      _activeProjectsUrl(activeProjectsUrl) {
  _oauthToken.trim();
}

void FreelancerApiClient::setOAuthToken(const String& oauthToken) {
  _oauthToken = oauthToken;
  _oauthToken.trim();
  _cachedBidderId = 0;
}

String FreelancerApiClient::formatBudget(float minB, float maxB, const String& currency) {
  char buf[64];
  if (minB > 0.0f && maxB > 0.0f) {
    snprintf(buf, sizeof(buf), "%.0f-%.0f %s", minB, maxB, currency.c_str());
    return String(buf);
  }
  if (minB > 0.0f) {
    snprintf(buf, sizeof(buf), "From %.0f %s", minB, currency.c_str());
    return String(buf);
  }
  if (maxB > 0.0f) {
    snprintf(buf, sizeof(buf), "Up to %.0f %s", maxB, currency.c_str());
    return String(buf);
  }
  return "Not specified";
}

bool FreelancerApiClient::fetchActiveProjects(std::vector<FreelancerProject>& outProjects) {
  outProjects.clear();
  if (_oauthToken.length() == 0) {
    return false;
  }

  HTTPClient http;
  http.setTimeout(15000);
  http.useHTTP10(true);

  if (!http.begin(_transport.client(), _activeProjectsUrl)) {
    return false;
  }
  http.addHeader("Freelancer-OAuth-V1", _oauthToken);
  http.addHeader("Accept-Encoding", "identity");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[FREELANCER-API] GET active projects falhou: HTTP %d\n", code);
    http.end();
    return false;
  }

  JsonDocument filter;
  JsonObject projFilter = filter["result"]["projects"].add<JsonObject>();
  projFilter["id"] = true;
  projFilter["title"] = true;
  projFilter["preview_description"] = true;
  projFilter["description"] = true;
  projFilter["seo_url"] = true;
  projFilter["budget"]["minimum"] = true;
  projFilter["budget"]["maximum"] = true;
  projFilter["budget"]["currency"]["code"] = true;
  projFilter["currency"]["code"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("[FREELANCER-API] Erro JSON active projects: %s\n", err.c_str());
    return false;
  }

  JsonArray projects = doc["result"]["projects"].as<JsonArray>();
  for (JsonObject p : projects) {
    FreelancerProject fp;
    fp.id = String(p["id"].as<int64_t>());
    fp.title = p["title"] | "";
    fp.title.trim();

    fp.description = p["preview_description"] | (p["description"] | "");
    fp.description.trim();

    fp.seoUrl = p["seo_url"] | "";
    fp.minBudget = p["budget"]["minimum"] | 0.0f;
    fp.maxBudget = p["budget"]["maximum"] | 0.0f;

    String cur = p["currency"]["code"] | (p["budget"]["currency"]["code"] | "USD");
    if (cur.length() == 0) cur = "USD";
    fp.currencyCode = cur;
    fp.budgetFormatted = formatBudget(fp.minBudget, fp.maxBudget, fp.currencyCode);

    outProjects.push_back(fp);
  }

  return true;
}

bool FreelancerApiClient::fetchProjectDetails(const String& projectId, FreelancerProject& outProject) {
  String url = "https://www.freelancer.com/api/projects/0.1/projects/" + projectId + "/";

  HTTPClient http;
  http.setTimeout(12000);
  http.useHTTP10(true);

  if (!http.begin(_transport.client(), url)) {
    return false;
  }
  http.addHeader("Freelancer-OAuth-V1", _oauthToken);
  http.addHeader("Accept-Encoding", "identity");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  JsonDocument filter;
  JsonObject resFilter = filter["result"].to<JsonObject>();
  resFilter["id"] = true;
  resFilter["title"] = true;
  resFilter["preview_description"] = true;
  resFilter["description"] = true;
  resFilter["seo_url"] = true;
  resFilter["budget"]["minimum"] = true;
  resFilter["budget"]["maximum"] = true;
  resFilter["budget"]["currency"]["code"] = true;
  resFilter["currency"]["code"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err || !doc["result"].is<JsonObject>()) {
    return false;
  }

  JsonObject p = doc["result"].as<JsonObject>();
  outProject.id = projectId;
  outProject.title = p["title"] | "";
  outProject.title.trim();
  outProject.description = p["description"] | (p["preview_description"] | "");
  outProject.description.trim();
  outProject.seoUrl = p["seo_url"] | "";
  outProject.minBudget = p["budget"]["minimum"] | 0.0f;
  outProject.maxBudget = p["budget"]["maximum"] | 0.0f;

  String cur = p["currency"]["code"] | (p["budget"]["currency"]["code"] | "USD");
  if (cur.length() == 0) cur = "USD";
  outProject.currencyCode = cur;
  outProject.budgetFormatted = formatBudget(outProject.minBudget, outProject.maxBudget, outProject.currencyCode);
  return true;
}

int64_t FreelancerApiClient::fetchSelfBidderId() {
  if (_cachedBidderId > 0) {
    return _cachedBidderId;
  }

  HTTPClient http;
  http.setTimeout(12000);
  http.useHTTP10(true);

  if (!http.begin(_transport.client(), "https://www.freelancer.com/api/users/0.1/self/")) {
    return 0;
  }
  http.addHeader("Freelancer-OAuth-V1", _oauthToken);
  http.addHeader("Accept-Encoding", "identity");

  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    JsonDocument filter;
    filter["result"]["id"] = true;
    JsonDocument doc;
    if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
      _cachedBidderId = doc["result"]["id"].as<int64_t>();
    }
  }
  http.end();
  return _cachedBidderId;
}

bool FreelancerApiClient::placeBid(const String& projectId,
                                   float amount,
                                   int periodDays,
                                   const String& description,
                                   String& outMessage) {
  int64_t bidderId = fetchSelfBidderId();
  if (bidderId <= 0) {
    outMessage = "Could not determine bidder_id from Freelancer OAuth token.";
    return false;
  }

  HTTPClient http;
  http.setTimeout(15000);
  http.useHTTP10(true);

  if (!http.begin(_transport.client(), "https://www.freelancer.com/api/projects/0.1/bids/")) {
    outMessage = "Failed to initialize HTTP connection.";
    return false;
  }

  http.addHeader("Freelancer-OAuth-V1", _oauthToken);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept-Encoding", "identity");

  JsonDocument payloadDoc;
  payloadDoc["project_id"] = projectId.toInt();
  payloadDoc["bidder_id"] = bidderId;
  payloadDoc["amount"] = amount;
  payloadDoc["period"] = periodDays;
  payloadDoc["description"] = description;
  payloadDoc["milestone_percentage"] = 100;

  String body;
  serializeJson(payloadDoc, body);

  int code = http.POST(body);
  JsonDocument respDoc;
  deserializeJson(respDoc, http.getStream());
  http.end();

  if (code == 200 || code == 201) {
    String status = respDoc["status"] | "";
    if (status == "success") {
      int64_t bidId = respDoc["result"]["id"] | 0;
      outMessage = "Bid placed successfully! Bid ID: " + String(bidId);
      return true;
    }
    String msg = respDoc["message"] | "Unknown error from Freelancer API";
    outMessage = "API returned status '" + status + "': " + msg;
    return false;
  }

  String errMsg = respDoc["message"] | (respDoc["error"]["message"] | "HTTP Error");
  outMessage = "Freelancer API Error (" + String(code) + "): " + errMsg;
  return false;
}

