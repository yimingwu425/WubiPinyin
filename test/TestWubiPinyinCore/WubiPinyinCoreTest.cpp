#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "WubiPinyinControlProtocol.h"
#include "WubiPinyinControlPayload.h"
#include "WubiPinyinCore.h"

namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void WriteUInt32LittleEndian(std::vector<std::uint8_t>* bytes,
                             std::size_t offset,
                             std::uint32_t value) {
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    (*bytes)[offset + index] = static_cast<std::uint8_t>(value & 0xff);
    value >>= 8;
  }
}

void TestControlProtocol() {
  using wubipinyin::ControlFrame;
  using wubipinyin::ControlMessageType;
  using wubipinyin::ControlProtocolError;

  ControlFrame frame;
  frame.type = ControlMessageType::kUpdateSetting;
  frame.request_id = 41;
  frame.session_id = 17;
  frame.generation = 9;
  frame.payload = {'t', 'h', 'e', 'm', 'e', '=', 'd', 'a', 'r', 'k'};

  ControlProtocolError error = ControlProtocolError::kNone;
  std::vector<std::uint8_t> encoded;
  assert(wubipinyin::EncodeControlFrame(frame, &encoded, &error));
  assert(error == ControlProtocolError::kNone);
  assert(encoded.size() == wubipinyin::kControlFrameHeaderBytes +
                               frame.payload.size());
  assert(encoded[0] == 'W');
  assert(encoded[1] == 'B');
  assert(encoded[2] == 'P');
  assert(encoded[3] == 'C');

  ControlFrame decoded;
  assert(wubipinyin::DecodeControlFrame(encoded.data(), encoded.size(),
                                         &decoded, &error));
  assert(decoded.type == frame.type);
  assert(decoded.request_id == frame.request_id);
  assert(decoded.session_id == frame.session_id);
  assert(decoded.generation == frame.generation);
  assert(decoded.payload == frame.payload);

  assert(wubipinyin::ValidateControlRequestSequence(40, frame.request_id,
                                                     &error));
  assert(!wubipinyin::ValidateControlRequestSequence(frame.request_id,
                                                      frame.request_id, &error));
  assert(error == ControlProtocolError::kRequestOutOfSequence);

  std::vector<std::uint8_t> invalid_magic = encoded;
  invalid_magic[0] ^= 0x01;
  assert(!wubipinyin::DecodeControlFrame(invalid_magic.data(),
                                          invalid_magic.size(), &decoded,
                                          &error));
  assert(error == ControlProtocolError::kInvalidMagic);

  std::vector<std::uint8_t> invalid_version = encoded;
  invalid_version[sizeof(std::uint32_t)] = 2;
  assert(!wubipinyin::DecodeControlFrame(invalid_version.data(),
                                          invalid_version.size(), &decoded,
                                          &error));
  assert(error == ControlProtocolError::kUnsupportedVersion);

  std::vector<std::uint8_t> invalid_type = encoded;
  invalid_type[sizeof(std::uint32_t) + sizeof(std::uint16_t)] = 0;
  invalid_type[sizeof(std::uint32_t) + sizeof(std::uint16_t) + 1] = 0;
  assert(!wubipinyin::DecodeControlFrame(invalid_type.data(),
                                          invalid_type.size(), &decoded,
                                          &error));
  assert(error == ControlProtocolError::kInvalidMessageType);

  std::vector<std::uint8_t> truncated_header = encoded;
  truncated_header.resize(wubipinyin::kControlFrameHeaderBytes - 1);
  assert(!wubipinyin::DecodeControlFrame(truncated_header.data(),
                                          truncated_header.size(), &decoded,
                                          &error));
  assert(error == ControlProtocolError::kTruncatedHeader);

  std::vector<std::uint8_t> truncated_payload = encoded;
  truncated_payload.pop_back();
  assert(!wubipinyin::DecodeControlFrame(truncated_payload.data(),
                                          truncated_payload.size(), &decoded,
                                          &error));
  assert(error == ControlProtocolError::kTruncatedPayload);

  std::vector<std::uint8_t> trailing_bytes = encoded;
  trailing_bytes.push_back(0);
  assert(!wubipinyin::DecodeControlFrame(trailing_bytes.data(),
                                          trailing_bytes.size(), &decoded,
                                          &error));
  assert(error == ControlProtocolError::kTrailingBytes);

  std::vector<std::uint8_t> oversized_payload = encoded;
  WriteUInt32LittleEndian(&oversized_payload,
                          wubipinyin::kControlFramePayloadLengthOffset,
                          static_cast<std::uint32_t>(
                              wubipinyin::kMaxControlPayloadBytes + 1));
  assert(!wubipinyin::DecodeControlFrame(oversized_payload.data(),
                                          oversized_payload.size(), &decoded,
                                          &error));
  assert(error == ControlProtocolError::kPayloadTooLarge);

  frame.payload.resize(wubipinyin::kMaxControlPayloadBytes + 1);
  const std::vector<std::uint8_t> valid_encoded = encoded;
  assert(!wubipinyin::EncodeControlFrame(frame, &encoded, &error));
  assert(error == ControlProtocolError::kPayloadTooLarge);
  assert(encoded == valid_encoded);
}

void TestControlPayload() {
  using wubipinyin::ControlPayloadError;
  using wubipinyin::HybridRoute;
  using wubipinyin::SettingsSnapshot;
  using wubipinyin::UserEntry;

  SettingsSnapshot settings;
  settings.default_route = HybridRoute::kPinyin;
  settings.learning_enabled = false;
  settings.password_input_protection = true;
  settings.show_source_labels = false;
  settings.candidate_page_size = 7;
  settings.theme = "dark";

  ControlPayloadError error = ControlPayloadError::kNone;
  std::vector<std::uint8_t> encoded;
  assert(wubipinyin::EncodeSettingsSnapshot(settings, &encoded, &error));
  SettingsSnapshot decoded;
  assert(wubipinyin::DecodeSettingsSnapshot(encoded, &decoded, &error));
  assert(decoded.default_route == settings.default_route);
  assert(decoded.learning_enabled == settings.learning_enabled);
  assert(decoded.password_input_protection ==
         settings.password_input_protection);
  assert(decoded.show_source_labels == settings.show_source_labels);
  assert(decoded.candidate_page_size == settings.candidate_page_size);
  assert(decoded.theme == settings.theme);

  std::vector<std::uint8_t> trailing = encoded;
  trailing.push_back(0);
  assert(!wubipinyin::DecodeSettingsSnapshot(trailing, &decoded, &error));
  assert(error == ControlPayloadError::kTrailingBytes);

  UserEntry entry;
  entry.id = 3;
  entry.text = "西安";
  entry.scheme = HybridRoute::kPinyin;
  entry.code = "xi'an";
  entry.weight = 2200;
  entry.enabled = false;
  assert(wubipinyin::EncodeUserEntry(entry, &encoded, &error));
  UserEntry decoded_entry;
  assert(wubipinyin::DecodeUserEntry(encoded, &decoded_entry, &error));
  assert(decoded_entry.id == entry.id);
  assert(decoded_entry.text == entry.text);
  assert(decoded_entry.scheme == entry.scheme);
  assert(decoded_entry.code == entry.code);
  assert(decoded_entry.weight == entry.weight);
  assert(decoded_entry.enabled == entry.enabled);

  const std::vector<UserEntry> entries = {entry, decoded_entry};
  assert(wubipinyin::EncodeUserEntries(entries, &encoded, &error));
  std::vector<UserEntry> decoded_entries;
  assert(wubipinyin::DecodeUserEntries(encoded, &decoded_entries, &error));
  assert(decoded_entries.size() == entries.size());

  std::vector<std::uint8_t> reply;
  assert(wubipinyin::EncodeControlReply(true, "", encoded, &reply, &error));
  bool reply_success = false;
  std::string reply_message;
  std::vector<std::uint8_t> reply_result;
  assert(wubipinyin::DecodeControlReply(reply, &reply_success, &reply_message,
                                         &reply_result, &error));
  assert(reply_success);
  assert(reply_message.empty());
  assert(reply_result == encoded);
}

void TestControlDispatcher(wubipinyin::SettingsStore* store,
                           const std::filesystem::path& rime_directory) {
  using wubipinyin::BrokerControlCallbacks;
  using wubipinyin::BrokerControlDispatcher;
  using wubipinyin::ControlFrame;
  using wubipinyin::ControlMessageType;
  using wubipinyin::ControlPayloadError;
  using wubipinyin::HybridRoute;

  bool settings_applied = false;
  bool learning_reset = false;
  std::uint64_t routed_session = 0;
  HybridRoute routed_to = HybridRoute::kAuto;
  int dictionary_reloads = 0;
  BrokerControlCallbacks callbacks;
  callbacks.apply_settings = [&settings_applied](const auto&, std::string*) {
    settings_applied = true;
    return true;
  };
  callbacks.set_route = [&routed_session, &routed_to](std::uint64_t session,
                                                       HybridRoute route) {
    routed_session = session;
    routed_to = route;
    return true;
  };
  callbacks.commit_raw = [](std::uint64_t) { return true; };
  callbacks.reload_dictionaries = [&dictionary_reloads](std::string*) {
    ++dictionary_reloads;
    return true;
  };
  callbacks.reset_learning = [&learning_reset](std::string*) {
    learning_reset = true;
    return true;
  };
  BrokerControlDispatcher dispatcher(store, rime_directory, callbacks);

  ControlFrame request;
  request.type = ControlMessageType::kGetSettings;
  request.request_id = 1;
  ControlFrame response;
  std::string error;
  assert(dispatcher.Dispatch(request, &response, &error));
  assert(response.type == ControlMessageType::kResponse);

  bool response_success = false;
  std::string response_message;
  std::vector<std::uint8_t> result;
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  assert(wubipinyin::DecodeControlReply(response.payload, &response_success,
                                         &response_message, &result,
                                         &payload_error));
  assert(response_success);
  wubipinyin::SettingsSnapshot settings;
  assert(wubipinyin::DecodeSettingsSnapshot(result, &settings, &payload_error));
  settings.password_input_protection = false;
  settings.show_source_labels = false;
  assert(wubipinyin::EncodeSettingsSnapshot(settings, &request.payload,
                                             &payload_error));
  request.type = ControlMessageType::kUpdateSetting;
  request.request_id = 2;
  assert(dispatcher.Dispatch(request, &response, &error));
  assert(settings_applied);

  wubipinyin::UserEntry entry;
  entry.text = "输入法";
  entry.scheme = HybridRoute::kWubi;
  entry.code = "kqny";
  assert(wubipinyin::EncodeUserEntry(entry, &request.payload, &payload_error));
  request.type = ControlMessageType::kUpsertUserEntry;
  request.request_id = 3;
  assert(dispatcher.Dispatch(request, &response, &error));
  assert(dictionary_reloads == 1);
  assert(wubipinyin::DecodeControlReply(response.payload, &response_success,
                                         &response_message, &result,
                                         &payload_error));
  assert(response_success);
  wubipinyin::UserEntry stored_entry;
  assert(wubipinyin::DecodeUserEntry(result, &stored_entry, &payload_error));
  assert(stored_entry.id > 0);

  request.payload.clear();
  request.type = ControlMessageType::kResetLearning;
  request.request_id = 4;
  assert(dispatcher.Dispatch(request, &response, &error));
  assert(learning_reset);

  request.session_id = 101;
  assert(wubipinyin::EncodeHybridRoute(HybridRoute::kWubi, &request.payload,
                                        &payload_error));
  request.type = ControlMessageType::kSetRoute;
  request.request_id = 5;
  assert(dispatcher.Dispatch(request, &response, &error));
  assert(routed_session == 101);
  assert(routed_to == HybridRoute::kWubi);
}

}  // namespace

int main() {
  TestControlProtocol();
  TestControlPayload();

  const auto root = std::filesystem::temp_directory_path() /
                    "wubipinyin-core-test";
  std::error_code cleanup_error;
  std::filesystem::remove_all(root, cleanup_error);

  std::string error;
  wubipinyin::SettingsStore store;
  assert(store.Open(root / "settings.sqlite3", &error));
  assert(error.empty());

  auto settings = store.ReadSettings(&error);
  assert(settings);
  assert(settings->default_route == wubipinyin::HybridRoute::kAuto);
  settings->theme = "dark";
  settings->candidate_page_size = 7;
  settings->password_input_protection = false;
  assert(store.WriteSettings(*settings, &error));

  wubipinyin::UserEntry wubi;
  wubi.text = "你好";
  wubi.scheme = wubipinyin::HybridRoute::kWubi;
  wubi.code = "wgki";
  assert(store.UpsertUserEntry(&wubi, &error));
  assert(wubi.id > 0);

  wubipinyin::UserEntry duplicate_wubi = wubi;
  duplicate_wubi.id = 0;
  duplicate_wubi.weight = 2000;
  assert(store.UpsertUserEntry(&duplicate_wubi, &error));
  assert(duplicate_wubi.id == wubi.id);

  wubipinyin::UserEntry pinyin;
  pinyin.text = "西安";
  pinyin.scheme = wubipinyin::HybridRoute::kPinyin;
  pinyin.code = "xi'an";
  assert(store.UpsertUserEntry(&pinyin, &error));
  assert(pinyin.id > 0);

  wubipinyin::UserEntry invalid_pinyin = pinyin;
  invalid_pinyin.id = 0;
  invalid_pinyin.code = "xi''an";
  assert(!store.UpsertUserEntry(&invalid_pinyin, &error));

  wubipinyin::UserEntry missing = pinyin;
  missing.id = 999999;
  assert(!store.UpsertUserEntry(&missing, &error));

  assert(store.MaterializeRimeDictionaries(root / "rime", &error));
  const auto wubi_dictionary =
      ReadFile(root / "rime" / "hybrid_auto_wubi_user.dict.yaml");
  const auto pinyin_dictionary =
      ReadFile(root / "rime" / "hybrid_auto_pinyin_user.dict.yaml");
  const auto schema_override =
      ReadFile(root / "rime" / "hybrid_auto.custom.yaml");
  assert(wubi_dictionary.find("你好\twgki") != std::string::npos);
  assert(pinyin_dictionary.find("西安\txi'an") != std::string::npos);
  assert(schema_override.find("\"menu/page_size\": 7") !=
         std::string::npos);

  pinyin.enabled = false;
  assert(store.UpsertUserEntry(&pinyin, &error));
  assert(store.MaterializeRimeDictionaries(root / "rime", &error));
  assert(ReadFile(root / "rime" / "hybrid_auto_pinyin_user.dict.yaml")
             .find("西安\txi'an") == std::string::npos);

  assert(store.RequestLearningReset(&error));
  assert(store.ConsumeLearningResetRequest(&error));
  assert(!store.ConsumeLearningResetRequest(&error));
  assert(store.DeleteUserEntry(wubi.id, &error));
  assert(store.ListUserEntries(&error).size() == 1);

  TestControlDispatcher(&store, root / "control-rime");

  store.Close();
  assert(store.Open(root / "settings.sqlite3", &error));
  settings = store.ReadSettings(&error);
  assert(settings);
  assert(settings->theme == "dark");
  assert(settings->candidate_page_size == 7);
  assert(!settings->password_input_protection);
  store.Close();
  assert(std::filesystem::remove_all(root, cleanup_error) > 0);
  return 0;
}
