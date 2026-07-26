#include "WubiPinyinControlPayload.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace wubipinyin {
namespace {

constexpr std::size_t kMaximumStringBytes = 8 * 1024;
constexpr std::size_t kMaximumEntryCount = 4096;

void SetError(ControlPayloadError* error, ControlPayloadError value) {
  if (error) {
    *error = value;
  }
}

template <typename UInt>
void AppendLittleEndian(std::vector<std::uint8_t>* payload, UInt value) {
  for (std::size_t index = 0; index < sizeof(UInt); ++index) {
    payload->push_back(static_cast<std::uint8_t>(value & 0xff));
    if constexpr (sizeof(UInt) > 1) {
      value = static_cast<UInt>(value >> 8);
    }
  }
}

template <typename UInt>
bool ReadLittleEndian(const std::vector<std::uint8_t>& payload,
                      std::size_t* offset,
                      UInt* value) {
  if (!offset || !value || *offset > payload.size() ||
      payload.size() - *offset < sizeof(UInt)) {
    return false;
  }
  UInt result = 0;
  for (std::size_t index = 0; index < sizeof(UInt); ++index) {
    result |= static_cast<UInt>(payload[*offset + index]) << (index * 8);
  }
  *offset += sizeof(UInt);
  *value = result;
  return true;
}

bool AppendString(std::vector<std::uint8_t>* payload,
                  const std::string& value,
                  ControlPayloadError* error) {
  if (value.size() > kMaximumStringBytes ||
      value.size() > std::numeric_limits<std::uint32_t>::max()) {
    SetError(error, ControlPayloadError::kPayloadTooLarge);
    return false;
  }
  AppendLittleEndian(payload, static_cast<std::uint32_t>(value.size()));
  payload->insert(payload->end(), value.begin(), value.end());
  return true;
}

bool ReadString(const std::vector<std::uint8_t>& payload,
                std::size_t* offset,
                std::string* value,
                ControlPayloadError* error) {
  std::uint32_t size = 0;
  if (!ReadLittleEndian(payload, offset, &size)) {
    SetError(error, ControlPayloadError::kTruncated);
    return false;
  }
  if (size > kMaximumStringBytes || !offset || *offset > payload.size() ||
      payload.size() - *offset < size) {
    SetError(error, size > kMaximumStringBytes
                        ? ControlPayloadError::kPayloadTooLarge
                        : ControlPayloadError::kTruncated);
    return false;
  }
  if (value) {
    value->assign(reinterpret_cast<const char*>(payload.data() + *offset),
                  size);
  }
  *offset += size;
  return true;
}

bool IsValidRouteByte(std::uint8_t route) {
  return route <= static_cast<std::uint8_t>(HybridRoute::kPinyin);
}

bool IsValidBoolByte(std::uint8_t value) {
  return value == 0 || value == 1;
}

bool DecodeSettings(const std::vector<std::uint8_t>& payload,
                    std::size_t* offset,
                    SettingsSnapshot* settings,
                    ControlPayloadError* error) {
  std::uint8_t route = 0;
  std::uint8_t learning_enabled = 0;
  std::uint8_t show_source_labels = 0;
  std::uint8_t password_input_protection = 0;
  std::uint32_t candidate_page_size = 0;
  if (!ReadLittleEndian(payload, offset, &route) ||
      !ReadLittleEndian(payload, offset, &learning_enabled) ||
      !ReadLittleEndian(payload, offset, &show_source_labels) ||
      !ReadLittleEndian(payload, offset, &password_input_protection) ||
      !ReadLittleEndian(payload, offset, &candidate_page_size)) {
    SetError(error, ControlPayloadError::kTruncated);
    return false;
  }
  if (!IsValidRouteByte(route) || !IsValidBoolByte(learning_enabled) ||
      !IsValidBoolByte(show_source_labels) ||
      !IsValidBoolByte(password_input_protection) || candidate_page_size < 5 ||
      candidate_page_size > 9) {
    SetError(error, ControlPayloadError::kInvalidValue);
    return false;
  }
  SettingsSnapshot decoded;
  decoded.default_route = static_cast<HybridRoute>(route);
  decoded.learning_enabled = learning_enabled != 0;
  decoded.show_source_labels = show_source_labels != 0;
  decoded.password_input_protection = password_input_protection != 0;
  decoded.candidate_page_size = static_cast<int>(candidate_page_size);
  if (!ReadString(payload, offset, &decoded.theme, error)) {
    return false;
  }
  if (decoded.theme != "system" && decoded.theme != "light" &&
      decoded.theme != "dark") {
    SetError(error, ControlPayloadError::kInvalidValue);
    return false;
  }
  *settings = std::move(decoded);
  return true;
}

bool AppendSettings(const SettingsSnapshot& settings,
                    std::vector<std::uint8_t>* payload,
                    ControlPayloadError* error) {
  if (!payload || settings.candidate_page_size < 5 ||
      settings.candidate_page_size > 9 ||
      (settings.theme != "system" && settings.theme != "light" &&
       settings.theme != "dark")) {
    SetError(error, ControlPayloadError::kInvalidValue);
    return false;
  }
  AppendLittleEndian(payload,
                     static_cast<std::uint8_t>(settings.default_route));
  AppendLittleEndian(payload,
                     static_cast<std::uint8_t>(settings.learning_enabled));
  AppendLittleEndian(payload,
                     static_cast<std::uint8_t>(settings.show_source_labels));
  AppendLittleEndian(
      payload, static_cast<std::uint8_t>(settings.password_input_protection));
  AppendLittleEndian(payload,
                     static_cast<std::uint32_t>(settings.candidate_page_size));
  return AppendString(payload, settings.theme, error);
}

bool DecodeEntry(const std::vector<std::uint8_t>& payload,
                 std::size_t* offset,
                 UserEntry* entry,
                 ControlPayloadError* error) {
  std::uint64_t id = 0;
  std::uint8_t route = 0;
  std::uint32_t weight = 0;
  std::uint8_t enabled = 0;
  if (!ReadLittleEndian(payload, offset, &id) ||
      !ReadString(payload, offset, &entry->text, error) ||
      !ReadLittleEndian(payload, offset, &route) ||
      !ReadString(payload, offset, &entry->code, error) ||
      !ReadLittleEndian(payload, offset, &weight) ||
      !ReadLittleEndian(payload, offset, &enabled)) {
    if (error && *error == ControlPayloadError::kNone) {
      *error = ControlPayloadError::kTruncated;
    }
    return false;
  }
  if (id > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
      !IsValidRouteByte(route) || route == static_cast<std::uint8_t>(
                                               HybridRoute::kAuto) ||
      weight > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      !IsValidBoolByte(enabled)) {
    SetError(error, ControlPayloadError::kInvalidValue);
    return false;
  }
  entry->id = static_cast<std::int64_t>(id);
  entry->scheme = static_cast<HybridRoute>(route);
  entry->weight = static_cast<int>(weight);
  entry->enabled = enabled != 0;
  return true;
}

bool AppendEntry(const UserEntry& entry,
                 std::vector<std::uint8_t>* payload,
                 ControlPayloadError* error) {
  if (!payload || entry.id < 0 ||
      entry.scheme == HybridRoute::kAuto ||
      entry.weight < 0) {
    SetError(error, ControlPayloadError::kInvalidValue);
    return false;
  }
  AppendLittleEndian(payload, static_cast<std::uint64_t>(entry.id));
  if (!AppendString(payload, entry.text, error)) {
    return false;
  }
  AppendLittleEndian(payload, static_cast<std::uint8_t>(entry.scheme));
  if (!AppendString(payload, entry.code, error)) {
    return false;
  }
  AppendLittleEndian(payload, static_cast<std::uint32_t>(entry.weight));
  AppendLittleEndian(payload, static_cast<std::uint8_t>(entry.enabled));
  return true;
}

bool EnsureAtEnd(const std::vector<std::uint8_t>& payload,
                 std::size_t offset,
                 ControlPayloadError* error) {
  if (offset == payload.size()) {
    SetError(error, ControlPayloadError::kNone);
    return true;
  }
  SetError(error, ControlPayloadError::kTrailingBytes);
  return false;
}

}  // namespace

std::string ControlPayloadErrorMessage(ControlPayloadError error) {
  switch (error) {
    case ControlPayloadError::kInvalidArgument:
      return "Invalid control payload argument";
    case ControlPayloadError::kPayloadTooLarge:
      return "Control payload is too large";
    case ControlPayloadError::kTruncated:
      return "Control payload is truncated";
    case ControlPayloadError::kInvalidValue:
      return "Control payload contains an invalid value";
    case ControlPayloadError::kTrailingBytes:
      return "Control payload has trailing bytes";
    case ControlPayloadError::kNone:
      return "";
  }
  return "Invalid control payload";
}

bool EncodeSettingsSnapshot(const SettingsSnapshot& settings,
                            std::vector<std::uint8_t>* payload,
                            ControlPayloadError* error) {
  if (!payload) {
    SetError(error, ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::vector<std::uint8_t> encoded;
  if (!AppendSettings(settings, &encoded, error)) {
    return false;
  }
  *payload = std::move(encoded);
  SetError(error, ControlPayloadError::kNone);
  return true;
}

bool DecodeSettingsSnapshot(const std::vector<std::uint8_t>& payload,
                            SettingsSnapshot* settings,
                            ControlPayloadError* error) {
  if (!settings) {
    SetError(error, ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::size_t offset = 0;
  SettingsSnapshot decoded;
  if (!DecodeSettings(payload, &offset, &decoded, error) ||
      !EnsureAtEnd(payload, offset, error)) {
    return false;
  }
  *settings = std::move(decoded);
  return true;
}

bool EncodeUserEntry(const UserEntry& entry,
                     std::vector<std::uint8_t>* payload,
                     ControlPayloadError* error) {
  if (!payload) {
    SetError(error, ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::vector<std::uint8_t> encoded;
  if (!AppendEntry(entry, &encoded, error)) {
    return false;
  }
  *payload = std::move(encoded);
  SetError(error, ControlPayloadError::kNone);
  return true;
}

bool DecodeUserEntry(const std::vector<std::uint8_t>& payload,
                     UserEntry* entry,
                     ControlPayloadError* error) {
  if (!entry) {
    SetError(error, ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::size_t offset = 0;
  UserEntry decoded;
  if (!DecodeEntry(payload, &offset, &decoded, error) ||
      !EnsureAtEnd(payload, offset, error)) {
    return false;
  }
  *entry = std::move(decoded);
  return true;
}

bool EncodeUserEntries(const std::vector<UserEntry>& entries,
                       std::vector<std::uint8_t>* payload,
                       ControlPayloadError* error) {
  if (!payload || entries.size() > kMaximumEntryCount) {
    SetError(error, payload ? ControlPayloadError::kPayloadTooLarge
                            : ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::vector<std::uint8_t> encoded;
  AppendLittleEndian(&encoded, static_cast<std::uint32_t>(entries.size()));
  for (const UserEntry& entry : entries) {
    if (!AppendEntry(entry, &encoded, error) ||
        encoded.size() > kMaxControlPayloadBytes) {
      if (error && *error == ControlPayloadError::kNone) {
        *error = ControlPayloadError::kPayloadTooLarge;
      }
      return false;
    }
  }
  *payload = std::move(encoded);
  SetError(error, ControlPayloadError::kNone);
  return true;
}

bool DecodeUserEntries(const std::vector<std::uint8_t>& payload,
                       std::vector<UserEntry>* entries,
                       ControlPayloadError* error) {
  if (!entries) {
    SetError(error, ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::size_t offset = 0;
  std::uint32_t count = 0;
  if (!ReadLittleEndian(payload, &offset, &count)) {
    SetError(error, ControlPayloadError::kTruncated);
    return false;
  }
  if (count > kMaximumEntryCount) {
    SetError(error, ControlPayloadError::kPayloadTooLarge);
    return false;
  }
  std::vector<UserEntry> decoded;
  decoded.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    UserEntry entry;
    if (!DecodeEntry(payload, &offset, &entry, error)) {
      return false;
    }
    decoded.push_back(std::move(entry));
  }
  if (!EnsureAtEnd(payload, offset, error)) {
    return false;
  }
  *entries = std::move(decoded);
  return true;
}

bool EncodeUserEntryId(std::int64_t id,
                       std::vector<std::uint8_t>* payload,
                       ControlPayloadError* error) {
  if (!payload || id <= 0) {
    SetError(error, payload ? ControlPayloadError::kInvalidValue
                            : ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::vector<std::uint8_t> encoded;
  AppendLittleEndian(&encoded, static_cast<std::uint64_t>(id));
  *payload = std::move(encoded);
  SetError(error, ControlPayloadError::kNone);
  return true;
}

bool DecodeUserEntryId(const std::vector<std::uint8_t>& payload,
                       std::int64_t* id,
                       ControlPayloadError* error) {
  if (!id) {
    SetError(error, ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::size_t offset = 0;
  std::uint64_t decoded = 0;
  if (!ReadLittleEndian(payload, &offset, &decoded)) {
    SetError(error, ControlPayloadError::kTruncated);
    return false;
  }
  if (decoded == 0 || decoded > static_cast<std::uint64_t>(
                           std::numeric_limits<std::int64_t>::max()) ||
      !EnsureAtEnd(payload, offset, error)) {
    if (error && *error == ControlPayloadError::kNone) {
      *error = ControlPayloadError::kInvalidValue;
    }
    return false;
  }
  *id = static_cast<std::int64_t>(decoded);
  return true;
}

bool EncodeHybridRoute(HybridRoute route,
                       std::vector<std::uint8_t>* payload,
                       ControlPayloadError* error) {
  if (!payload || !IsValidRouteByte(static_cast<std::uint8_t>(route))) {
    SetError(error, payload ? ControlPayloadError::kInvalidValue
                            : ControlPayloadError::kInvalidArgument);
    return false;
  }
  *payload = {static_cast<std::uint8_t>(route)};
  SetError(error, ControlPayloadError::kNone);
  return true;
}

bool DecodeHybridRoute(const std::vector<std::uint8_t>& payload,
                       HybridRoute* route,
                       ControlPayloadError* error) {
  if (!route) {
    SetError(error, ControlPayloadError::kInvalidArgument);
    return false;
  }
  if (payload.size() != 1) {
    SetError(error, payload.empty() ? ControlPayloadError::kTruncated
                                    : ControlPayloadError::kTrailingBytes);
    return false;
  }
  if (!IsValidRouteByte(payload[0])) {
    SetError(error, ControlPayloadError::kInvalidValue);
    return false;
  }
  *route = static_cast<HybridRoute>(payload[0]);
  SetError(error, ControlPayloadError::kNone);
  return true;
}

bool EncodeControlReply(bool success,
                        const std::string& message,
                        const std::vector<std::uint8_t>& result,
                        std::vector<std::uint8_t>* payload,
                        ControlPayloadError* error) {
  if (!payload || result.size() > kMaxControlPayloadBytes) {
    SetError(error, payload ? ControlPayloadError::kPayloadTooLarge
                            : ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::vector<std::uint8_t> encoded;
  encoded.push_back(success ? 1 : 0);
  if (!AppendString(&encoded, message, error) ||
      result.size() > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  AppendLittleEndian(&encoded, static_cast<std::uint32_t>(result.size()));
  encoded.insert(encoded.end(), result.begin(), result.end());
  if (encoded.size() > kMaxControlPayloadBytes) {
    SetError(error, ControlPayloadError::kPayloadTooLarge);
    return false;
  }
  *payload = std::move(encoded);
  SetError(error, ControlPayloadError::kNone);
  return true;
}

bool DecodeControlReply(const std::vector<std::uint8_t>& payload,
                        bool* success,
                        std::string* message,
                        std::vector<std::uint8_t>* result,
                        ControlPayloadError* error) {
  if (!success || !message || !result) {
    SetError(error, ControlPayloadError::kInvalidArgument);
    return false;
  }
  std::size_t offset = 0;
  std::uint8_t status = 0;
  if (!ReadLittleEndian(payload, &offset, &status) || !IsValidBoolByte(status)) {
    SetError(error, ControlPayloadError::kInvalidValue);
    return false;
  }
  std::string decoded_message;
  if (!ReadString(payload, &offset, &decoded_message, error)) {
    return false;
  }
  std::uint32_t result_size = 0;
  if (!ReadLittleEndian(payload, &offset, &result_size) ||
      offset > payload.size() || payload.size() - offset < result_size) {
    SetError(error, ControlPayloadError::kTruncated);
    return false;
  }
  std::vector<std::uint8_t> decoded_result(
      payload.begin() + static_cast<std::ptrdiff_t>(offset),
      payload.begin() + static_cast<std::ptrdiff_t>(offset + result_size));
  offset += result_size;
  if (!EnsureAtEnd(payload, offset, error)) {
    return false;
  }
  *success = status != 0;
  *message = std::move(decoded_message);
  *result = std::move(decoded_result);
  return true;
}

}  // namespace wubipinyin
