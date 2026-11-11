#include "bots/NtpTimeProvider.h"
#include <time.h>

NtpTimeProvider::NtpTimeProvider(long gmtOffsetSec, int daylightOffsetSec)
    : _gmtOffsetSec(gmtOffsetSec), _daylightOffsetSec(daylightOffsetSec) {}

void NtpTimeProvider::begin() {
  configTime(_gmtOffsetSec, _daylightOffsetSec, "a.st1.ntp.br", "pool.ntp.org", "time.nist.gov");
  _configured = true;
  _lastSyncAttemptMs = millis();
  ensureSynced();
}

void NtpTimeProvider::ensureSynced() {
  if (!_configured) {
    begin();
    return;
  }
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 200)) {
    return;
  }
  uint32_t now = millis();
  if (now - _lastSyncAttemptMs > 30000) {
    _lastSyncAttemptMs = now;
    configTime(_gmtOffsetSec, _daylightOffsetSec, "a.st1.ntp.br", "pool.ntp.org", "time.nist.gov");
  }
}

String NtpTimeProvider::todayStr() const {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return "01/01/2026";
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
           timeinfo.tm_mday,
           timeinfo.tm_mon + 1,
           timeinfo.tm_year + 1900);
  return String(buf);
}

String NtpTimeProvider::nowTimeStr() const {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return "00:00";
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  return String(buf);
}

int32_t NtpTimeProvider::daysFromCivil(int y, unsigned m, unsigned d) {
  y -= (m <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int>(doe) - 719468;
}

void NtpTimeProvider::civilFromDays(int32_t z, int& outY, unsigned& outM, unsigned& outD) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int y = static_cast<int>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  outD = doy - (153 * mp + 2) / 5 + 1;
  outM = mp + (mp < 10 ? 3 : -9);
  outY = y + (outM <= 2);
}

bool NtpTimeProvider::parseDateToEpochDays(const String& dateStr, int32_t& outDays) const {
  int d = 0, m = 0, y = 0;
  if (sscanf(dateStr.c_str(), "%d/%d/%d", &d, &m, &y) != 3) {
    return false;
  }
  if (y < 1970 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) {
    return false;
  }
  outDays = daysFromCivil(y, static_cast<unsigned>(m), static_cast<unsigned>(d));
  return true;
}

String NtpTimeProvider::formatEpochDays(int32_t epochDays, bool includeYear) const {
  int y = 0;
  unsigned m = 0, d = 0;
  civilFromDays(epochDays, y, m, d);
  char buf[16];
  if (includeYear) {
    snprintf(buf, sizeof(buf), "%02u/%02u/%04d", d, m, y);
  } else {
    snprintf(buf, sizeof(buf), "%02u/%02u", d, m);
  }
  return String(buf);
}

int32_t NtpTimeProvider::todayEpochDays() const {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return daysFromCivil(2026, 1, 1);
  }
  return daysFromCivil(timeinfo.tm_year + 1900,
                       static_cast<unsigned>(timeinfo.tm_mon + 1),
                       static_cast<unsigned>(timeinfo.tm_mday));
}

