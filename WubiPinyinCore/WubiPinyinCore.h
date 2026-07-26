#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace wubipinyin {

enum class HybridRoute : std::uint8_t {
  kAuto,
  kWubi,
  kPinyin,
};

enum CandidateSourceMask : std::uint8_t {
  kCandidateSourceNone = 0,
  kCandidateSourceWubi = 1 << 0,
  kCandidateSourcePinyin = 1 << 1,
};

struct UserEntry {
  std::int64_t id = 0;
  std::string text;
  HybridRoute scheme = HybridRoute::kAuto;
  std::string code;
  int weight = 1000;
  bool enabled = true;
};

struct SettingsSnapshot {
  HybridRoute default_route = HybridRoute::kAuto;
  bool learning_enabled = true;
  bool password_input_protection = true;
  bool show_source_labels = true;
  int candidate_page_size = 5;
  std::string theme = "system";
};

// The Broker is the only owner of this store.  All strings use UTF-8 so the
// same values can be written directly into Rime's UTF-8 user dictionaries.
class SettingsStore {
 public:
  SettingsStore() = default;
  ~SettingsStore();

  SettingsStore(const SettingsStore&) = delete;
  SettingsStore& operator=(const SettingsStore&) = delete;

  bool Open(const std::filesystem::path& database_path, std::string* error);
  void Close();

  std::optional<SettingsSnapshot> ReadSettings(std::string* error) const;
  bool WriteSettings(const SettingsSnapshot& settings, std::string* error);

  std::vector<UserEntry> ListUserEntries(std::string* error) const;
  bool UpsertUserEntry(UserEntry* entry, std::string* error);
  bool DeleteUserEntry(std::int64_t id, std::string* error);

  // Rebuilds route-specific Rime dictionary sources and the generated schema
  // override from the SQLite source of truth. The caller schedules Rime
  // maintenance after this returns true.
  bool MaterializeRimeDictionaries(const std::filesystem::path& directory,
                                   std::string* error) const;

  bool RequestLearningReset(std::string* error);
  bool ConsumeLearningResetRequest(std::string* error);

 private:
  bool CreateSchema(std::string* error);
  bool SetValue(const std::string& key,
                const std::string& value,
                std::string* error);
  bool SetDefaultValue(const std::string& key,
                       const std::string& value,
                       std::string* error);
  std::optional<std::string> GetValue(const std::string& key,
                                      std::string* error) const;

  static bool ValidateUserEntry(const UserEntry& entry, std::string* error);
  static std::string RouteName(HybridRoute route);
  static std::optional<HybridRoute> ParseRoute(const std::string& value);

  mutable std::mutex mutex_;
  sqlite3* database_ = nullptr;
};

}  // namespace wubipinyin
