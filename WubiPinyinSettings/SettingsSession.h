#pragma once

#include "SettingsClient.h"

namespace wubipinyin::settings {

// Keeps a UI-local snapshot only. All reads and writes still go through the
// Broker so this process never opens the product SQLite database.
class SettingsSession {
 public:
  static SettingsSession& Current();

  const SettingsSnapshot& Snapshot() const { return snapshot_; }

  bool Refresh(std::wstring* error);
  bool Save(const SettingsSnapshot& snapshot, std::wstring* error);
  bool ListEntries(std::vector<UserEntry>* entries, std::wstring* error);
  bool UpsertEntry(UserEntry* entry, std::wstring* error);
  bool DeleteEntry(std::int64_t id, std::wstring* error);
  bool ResetLearning(std::wstring* error);
  bool ReloadDictionaries(std::wstring* error);

 private:
  SettingsSession() = default;

  SettingsClient client_;
  SettingsSnapshot snapshot_;
};

}  // namespace wubipinyin::settings
