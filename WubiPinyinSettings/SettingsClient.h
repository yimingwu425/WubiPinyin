#pragma once

#include "WubiPinyinControlPayload.h"

#include <cstdint>
#include <string>
#include <vector>

namespace wubipinyin::settings {

// The Settings app's only path to mutable product state. It has no SQLite
// dependency: the Broker remains the single database writer.
class SettingsClient {
 public:
  SettingsClient();

  bool GetSettings(SettingsSnapshot* settings, std::wstring* error);
  bool UpdateSettings(const SettingsSnapshot& settings, std::wstring* error);
  bool ListUserEntries(std::vector<UserEntry>* entries, std::wstring* error);
  bool UpsertUserEntry(UserEntry* entry, std::wstring* error);
  bool DeleteUserEntry(std::int64_t id, std::wstring* error);
  bool ResetLearning(std::wstring* error);
  bool ReloadDictionaries(std::wstring* error);

  // These calls support future session-aware UI surfaces. The current
  // settings pages do not bind them to a composition session.
  bool SetRoute(std::uint64_t session_id,
                HybridRoute route,
                std::wstring* error);
  bool CommitRaw(std::uint64_t session_id, std::wstring* error);

 private:
  bool Request(ControlMessageType type,
               std::uint64_t session_id,
               const std::vector<std::uint8_t>& payload,
               std::vector<std::uint8_t>* result,
               std::wstring* error);

  std::uint64_t next_request_id_ = 1;
  std::uint64_t generation_ = 1;
};

}  // namespace wubipinyin::settings
