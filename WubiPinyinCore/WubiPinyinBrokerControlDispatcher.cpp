#include "WubiPinyinControlPayload.h"

#include <utility>

namespace wubipinyin {

BrokerControlDispatcher::BrokerControlDispatcher(
    SettingsStore* settings_store,
    std::filesystem::path rime_user_directory,
    BrokerControlCallbacks callbacks)
    : settings_store_(settings_store),
      rime_user_directory_(std::move(rime_user_directory)),
      callbacks_(std::move(callbacks)) {}

bool BrokerControlDispatcher::MakeReply(
    const ControlFrame& request,
    bool success,
    const std::string& message,
    const std::vector<std::uint8_t>& result,
    ControlFrame* response,
    std::string* error) const {
  if (!response) {
    if (error) {
      *error = "Missing control response";
    }
    return false;
  }
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  std::vector<std::uint8_t> payload;
  if (!EncodeControlReply(success, message, result, &payload, &payload_error)) {
    if (error) {
      *error = ControlPayloadErrorMessage(payload_error);
    }
    return false;
  }
  response->type = success ? ControlMessageType::kResponse
                           : ControlMessageType::kError;
  response->request_id = request.request_id;
  response->session_id = request.session_id;
  response->generation = request.generation;
  response->payload = std::move(payload);
  return true;
}

bool BrokerControlDispatcher::ReloadDictionaries(std::string* error) {
  if (!settings_store_->MaterializeRimeDictionaries(rime_user_directory_, error)) {
    return false;
  }
  if (callbacks_.reload_dictionaries) {
    return callbacks_.reload_dictionaries(error);
  }
  return true;
}

bool BrokerControlDispatcher::Dispatch(const ControlFrame& request,
                                       ControlFrame* response,
                                       std::string* error) {
  if (error) {
    error->clear();
  }
  if (!settings_store_ || !response) {
    if (error) {
      *error = "Control dispatcher is not initialized";
    }
    return false;
  }
  std::string operation_error;
  std::vector<std::uint8_t> result;
  bool success = false;

  switch (request.type) {
    case ControlMessageType::kGetSettings: {
      if (!request.payload.empty()) {
        operation_error = "GetSettings does not accept a payload";
        break;
      }
      const auto settings = settings_store_->ReadSettings(&operation_error);
      if (!settings) {
        break;
      }
      ControlPayloadError payload_error = ControlPayloadError::kNone;
      if (!EncodeSettingsSnapshot(*settings, &result, &payload_error)) {
        operation_error = ControlPayloadErrorMessage(payload_error);
        break;
      }
      success = true;
      break;
    }
    case ControlMessageType::kUpdateSetting: {
      SettingsSnapshot settings;
      ControlPayloadError payload_error = ControlPayloadError::kNone;
      if (!DecodeSettingsSnapshot(request.payload, &settings, &payload_error)) {
        operation_error = ControlPayloadErrorMessage(payload_error);
        break;
      }
      const auto previous = settings_store_->ReadSettings(&operation_error);
      if (!previous) {
        break;
      }
      if (!settings_store_->WriteSettings(settings, &operation_error)) {
        break;
      }
      const bool requires_deployment =
          previous->candidate_page_size != settings.candidate_page_size;
      if (requires_deployment &&
          !settings_store_->MaterializeRimeDictionaries(rime_user_directory_,
                                                         &operation_error)) {
        break;
      }
      if (callbacks_.apply_settings &&
          !callbacks_.apply_settings(settings, &operation_error)) {
        if (operation_error.empty()) {
          operation_error = "Unable to apply updated settings";
        }
        break;
      }
      if (requires_deployment && callbacks_.reload_dictionaries &&
          !callbacks_.reload_dictionaries(&operation_error)) {
        if (operation_error.empty()) {
          operation_error = "Unable to schedule the updated Rime schema";
        }
        break;
      }
      success = true;
      break;
    }
    case ControlMessageType::kListUserEntries: {
      if (!request.payload.empty()) {
        operation_error = "ListUserEntries does not accept a payload";
        break;
      }
      const auto entries = settings_store_->ListUserEntries(&operation_error);
      if (!operation_error.empty()) {
        break;
      }
      ControlPayloadError payload_error = ControlPayloadError::kNone;
      if (!EncodeUserEntries(entries, &result, &payload_error)) {
        operation_error = ControlPayloadErrorMessage(payload_error);
        break;
      }
      success = true;
      break;
    }
    case ControlMessageType::kUpsertUserEntry: {
      UserEntry entry;
      ControlPayloadError payload_error = ControlPayloadError::kNone;
      if (!DecodeUserEntry(request.payload, &entry, &payload_error)) {
        operation_error = ControlPayloadErrorMessage(payload_error);
        break;
      }
      if (!settings_store_->UpsertUserEntry(&entry, &operation_error) ||
          !ReloadDictionaries(&operation_error)) {
        break;
      }
      if (!EncodeUserEntry(entry, &result, &payload_error)) {
        operation_error = ControlPayloadErrorMessage(payload_error);
        break;
      }
      success = true;
      break;
    }
    case ControlMessageType::kDeleteUserEntry: {
      std::int64_t id = 0;
      ControlPayloadError payload_error = ControlPayloadError::kNone;
      if (!DecodeUserEntryId(request.payload, &id, &payload_error)) {
        operation_error = ControlPayloadErrorMessage(payload_error);
        break;
      }
      if (!settings_store_->DeleteUserEntry(id, &operation_error) ||
          !ReloadDictionaries(&operation_error)) {
        break;
      }
      success = true;
      break;
    }
    case ControlMessageType::kResetLearning: {
      if (!request.payload.empty()) {
        operation_error = "ResetLearning does not accept a payload";
        break;
      }
      if (!settings_store_->RequestLearningReset(&operation_error)) {
        break;
      }
      if (callbacks_.reset_learning && callbacks_.reset_learning(&operation_error)) {
        settings_store_->ConsumeLearningResetRequest(nullptr);
        success = true;
      } else if (operation_error.empty()) {
        operation_error = "Learning reset is unavailable";
      }
      break;
    }
    case ControlMessageType::kSetRoute: {
      HybridRoute route = HybridRoute::kAuto;
      ControlPayloadError payload_error = ControlPayloadError::kNone;
      if (request.session_id == 0 ||
          !DecodeHybridRoute(request.payload, &route, &payload_error)) {
        operation_error = request.session_id == 0
                              ? "SetRoute requires a session"
                              : ControlPayloadErrorMessage(payload_error);
        break;
      }
      if (callbacks_.set_route && callbacks_.set_route(request.session_id, route)) {
        success = true;
      } else {
        operation_error = "The requested input session is unavailable";
      }
      break;
    }
    case ControlMessageType::kCommitRaw: {
      if (!request.payload.empty() || request.session_id == 0) {
        operation_error = "CommitRaw requires an empty payload and a session";
        break;
      }
      if (callbacks_.commit_raw && callbacks_.commit_raw(request.session_id)) {
        success = true;
      } else {
        operation_error = "The requested input session is unavailable";
      }
      break;
    }
    case ControlMessageType::kReloadDictionaries: {
      if (!request.payload.empty()) {
        operation_error = "ReloadDictionaries does not accept a payload";
        break;
      }
      success = ReloadDictionaries(&operation_error);
      break;
    }
    case ControlMessageType::kResponse:
    case ControlMessageType::kError:
      operation_error = "Control requests cannot use a response message type";
      break;
  }

  return MakeReply(request, success, operation_error, result, response, error);
}

}  // namespace wubipinyin
