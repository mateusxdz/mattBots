#pragma once
#include "bots/ITimeProvider.h"

// Implementação de sincronização NTP e calendário (SRP / LSP)
class NtpTimeProvider : public ITimeProvider {
public:
  NtpTimeProvider(long gmtOffsetSec, int daylightOffsetSec);
  ~NtpTimeProvider() override = default;

  void begin() override;
  void ensureSynced() override;
  String todayStr() const override;
  String nowTimeStr() const override;

  bool parseDateToEpochDays(const String& dateStr, int32_t& outDays) const override;
  String formatEpochDays(int32_t epochDays, bool includeYear = true) const override;
  int32_t todayEpochDays() const override;

private:
  long _gmtOffsetSec;
  int _daylightOffsetSec;
  bool _configured = false;
  uint32_t _lastSyncAttemptMs = 0;

  static int32_t daysFromCivil(int y, unsigned m, unsigned d);
  static void civilFromDays(int32_t z, int& outY, unsigned& outM, unsigned& outD);
};

