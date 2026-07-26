#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "WubiPinyinControlProtocol.h"
#include "WubiPinyinCore.h"

namespace wubipinyin {

enum class ControlPayloadError : std::uint8_t {
  kNone,
  kInvalidArgument,
  kPayloadTooLarge,
  kTruncated,
  kInvalidValue,
  kTrailingBytes,
};

std::string ControlPayloadErrorMessage(ControlPayloadError error);

// The payload codec is intentionally binary and length-prefixed. It is shared
// by the x64 Settings app and the Broker, while the frame codec protects the
// pipe boundary itself.
bool EncodeSettingsSnapshot(const SettingsSnapshot& settings,
                            std::vector<std::uint8_t>* payload,
                            ControlPayloadError* error);
bool DecodeSettingsSnapshot(const std::vector<std::uint8_t>& payload,
                            SettingsSnapshot* settings,
                            ControlPayloadError* error);

bool EncodeUserEntry(const UserEntry& entry,
                     std::vector<std::uint8_t>* payload,
                     ControlPayloadError* error);
bool DecodeUserEntry(const std::vector<std::uint8_t>& payload,
                     UserEntry* entry,
                     ControlPayloadError* error);
bool EncodeUserEntries(const std::vector<UserEntry>& entries,
                       std::vector<std::uint8_t>* payload,
                       ControlPayloadError* error);
bool DecodeUserEntries(const std::vector<std::uint8_t>& payload,
                       std::vector<UserEntry>* entries,
                       ControlPayloadError* error);

bool EncodeUserEntryId(std::int64_t id,
                       std::vector<std::uint8_t>* payload,
                       ControlPayloadError* error);
bool DecodeUserEntryId(const std::vector<std::uint8_t>& payload,
                       std::int64_t* id,
                       ControlPayloadError* error);
bool EncodeHybridRoute(HybridRoute route,
                       std::vector<std::uint8_t>* payload,
                       ControlPayloadError* error);
bool DecodeHybridRoute(const std::vector<std::uint8_t>& payload,
                       HybridRoute* route,
                       ControlPayloadError* error);

// Every response has a status byte followed by a bounded UTF-8 diagnostic and
// an optional command-specific result payload. Error frames use the same
// layout so clients have one parser for successful and failed requests.
bool EncodeControlReply(bool success,
                        const std::string& message,
                        const std::vector<std::uint8_t>& result,
                        std::vector<std::uint8_t>* payload,
                        ControlPayloadError* error);
bool DecodeControlReply(const std::vector<std::uint8_t>& payload,
                        bool* success,
                        std::string* message,
                        std::vector<std::uint8_t>* result,
                        ControlPayloadError* error);

struct BrokerControlCallbacks {
  // These callbacks run in the Broker's serialized control context. They must
  // not make a synchronous request back through the control pipe.
  std::function<bool(const SettingsSnapshot&, std::string* error)>
      apply_settings;
  std::function<bool(std::uint64_t session_id, HybridRoute route)> set_route;
  std::function<bool(std::uint64_t session_id)> commit_raw;
  std::function<bool(std::string* error)> reload_dictionaries;
  std::function<bool(std::string* error)> reset_learning;
};

// SettingsStore remains the only SQLite writer. This dispatcher contains no
// transport code, which permits focused Linux unit tests and keeps SQL off
// the TSF key IPC path.
class BrokerControlDispatcher {
 public:
  BrokerControlDispatcher(SettingsStore* settings_store,
                          std::filesystem::path rime_user_directory,
                          BrokerControlCallbacks callbacks);

  bool Dispatch(const ControlFrame& request,
                ControlFrame* response,
                std::string* error);

 private:
  bool ReloadDictionaries(std::string* error);
  bool MakeReply(const ControlFrame& request,
                 bool success,
                 const std::string& message,
                 const std::vector<std::uint8_t>& result,
                 ControlFrame* response,
                 std::string* error) const;

  SettingsStore* settings_store_;
  std::filesystem::path rime_user_directory_;
  BrokerControlCallbacks callbacks_;
};

}  // namespace wubipinyin
