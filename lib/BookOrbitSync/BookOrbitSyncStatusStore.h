#pragma once

#include <PersistableStore.h>

#include <cstdint>

/**
 * Durable, device-wide BookOrbit sync status.
 *
 * This is intentionally separate from credentials: a successful sync may be
 * recorded from the minimal sleep-sync boot, while settings remain untouched.
 * The status is only advanced after the progress exchange has completed; a
 * failed or cancelled attempt therefore keeps the last known-good time.
 */
class BookOrbitSyncStatusStore : public PersistableStore<BookOrbitSyncStatusStore> {
 private:
  static constexpr uint8_t FORMAT_VERSION = 1;

  uint32_t lastSuccessfulSyncEpoch = 0;

  BookOrbitSyncStatusStore() = default;
  ~BookOrbitSyncStatusStore() = default;

  friend class PersistableStore<BookOrbitSyncStatusStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/bookorbit-status.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  uint32_t getLastSuccessfulSyncEpoch() const {
    ensureLoaded();
    return lastSuccessfulSyncEpoch;
  }

  // A persistence failure is reported to the caller but does not invalidate
  // the network sync that already completed successfully.
  bool recordSuccess(uint32_t epochSeconds);
};

#define BOOKORBIT_SYNC_STATUS BookOrbitSyncStatusStore::getInstance()
