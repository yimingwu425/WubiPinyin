#include "SettingsSession.h"

#include <utility>

namespace wubipinyin::settings {

SettingsSession& SettingsSession::Current() {
  static SettingsSession session;
  return session;
}

bool SettingsSession::Refresh(std::wstring* error) {
  SettingsSnapshot loaded;
  if (!client_.GetSettings(&loaded, error)) {
    return false;
  }
  snapshot_ = std::move(loaded);
  return true;
}

bool SettingsSession::Save(const SettingsSnapshot& snapshot,
                           std::wstring* error) {
  if (!client_.UpdateSettings(snapshot, error)) {
    return false;
  }
  snapshot_ = snapshot;
  return true;
}

bool SettingsSession::ListEntries(std::vector<UserEntry>* entries,
                                  std::wstring* error) {
  return client_.ListUserEntries(entries, error);
}

bool SettingsSession::UpsertEntry(UserEntry* entry, std::wstring* error) {
  return client_.UpsertUserEntry(entry, error);
}

bool SettingsSession::DeleteEntry(std::int64_t id, std::wstring* error) {
  return client_.DeleteUserEntry(id, error);
}

bool SettingsSession::ResetLearning(std::wstring* error) {
  return client_.ResetLearning(error);
}

bool SettingsSession::ReloadDictionaries(std::wstring* error) {
  return client_.ReloadDictionaries(error);
}

}  // namespace wubipinyin::settings
