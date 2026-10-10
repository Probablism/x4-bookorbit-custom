#include "BookOrbitSyncStatusStore.h"

#include <Logging.h>

#include <mutex>

namespace {
constexpr uint32_t MIN_PLAUSIBLE_EPOCH = 1577836800u;  // 2020-01-01
constexpr uint32_t MAX_PLAUSIBLE_EPOCH = 4102444800u;  // 2100-01-01
}

void BookOrbitSyncStatusStore::toJson(JsonDocument& doc) const {
  doc["version"] = FORMAT_VERSION;
  doc["lastSuccessEpoch"] = lastSuccessfulSyncEpoch;
}

bool BookOrbitSyncStatusStore::fromJson(JsonVariantConst doc) {
  // Zero is the stable representation for a device that has not completed a
  // sync yet. Reject malformed values instead of displaying a false date.
  const uint32_t epoch = doc["lastSuccessEpoch"] | 0u;
  lastSuccessfulSyncEpoch =
      epoch >= MIN_PLAUSIBLE_EPOCH && epoch < MAX_PLAUSIBLE_EPOCH ? epoch : 0;
  return true;
}

bool BookOrbitSyncStatusStore::recordSuccess(const uint32_t epochSeconds) {
  if (epochSeconds < MIN_PLAUSIBLE_EPOCH || epochSeconds >= MAX_PLAUSIBLE_EPOCH) return false;
  ensureLoaded();

  std::lock_guard<std::mutex> lock(storeMutex);
  // Do not move a displayed success backwards if the clock was corrected
  // between two syncs. More importantly, never replace a known success with
  // an invalid/older value after an unsuccessful retry.
  if (epochSeconds <= lastSuccessfulSyncEpoch) return true;

  const uint32_t previousEpoch = lastSuccessfulSyncEpoch;
  lastSuccessfulSyncEpoch = epochSeconds;
  JsonDocument doc;
  toJson(doc);
  if (!writeDocToFileAtomically(getFilePath(), doc)) {
    lastSuccessfulSyncEpoch = previousEpoch;
    LOG_ERR("BookOrbit", "Could not persist last successful sync time");
    return false;
  }
  return true;
}
