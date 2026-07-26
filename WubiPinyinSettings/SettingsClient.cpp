#include "SettingsClient.h"

#include <Windows.h>
#include <Sddl.h>

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace wubipinyin::settings {
namespace {

constexpr wchar_t kBrokerControlPipePrefix[] =
    L"\\\\.\\pipe\\WubiPinyinBrokerControlV1\\";
constexpr DWORD kConnectTimeoutMilliseconds = 300;
constexpr DWORD kIoTimeoutMilliseconds = 500;
constexpr std::size_t kMaximumFrameBytes =
    kControlFrameHeaderBytes + kMaxControlPayloadBytes;

static_assert(kMaximumFrameBytes <= std::numeric_limits<DWORD>::max());

class Handle final {
 public:
  explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {}
  ~Handle() {
    if (valid()) {
      ::CloseHandle(value_);
    }
  }

  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;

  Handle(Handle&& other) noexcept : value_(std::exchange(other.value_, INVALID_HANDLE_VALUE)) {}
  Handle& operator=(Handle&& other) noexcept {
    if (this != &other) {
      if (valid()) {
        ::CloseHandle(value_);
      }
      value_ = std::exchange(other.value_, INVALID_HANDLE_VALUE);
    }
    return *this;
  }

  HANDLE get() const { return value_; }
  bool valid() const { return value_ != INVALID_HANDLE_VALUE && value_ != NULL; }

 private:
  HANDLE value_;
};

void SetError(std::wstring* error, std::wstring const& message) {
  if (error) {
    *error = message;
  }
}

std::wstring SystemError(DWORD code) {
  LPWSTR formatted = nullptr;
  const DWORD length = ::FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, 0, reinterpret_cast<LPWSTR>(&formatted), 0, nullptr);
  if (length == 0 || !formatted) {
    return L"Windows error " + std::to_wstring(code);
  }
  std::wstring result(formatted, length);
  ::LocalFree(formatted);
  while (!result.empty() &&
         (result.back() == L'\r' || result.back() == L'\n' ||
          result.back() == L' ')) {
    result.pop_back();
  }
  return result;
}

std::wstring Utf8ToWide(const std::string& value) {
  if (value.empty()) {
    return L"";
  }
  const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                             value.data(),
                                             static_cast<int>(value.size()),
                                             nullptr, 0);
  if (required <= 0) {
    return L"Invalid UTF-8 response from Broker";
  }
  std::wstring result(static_cast<std::size_t>(required), L'\0');
  if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(),
                            required) != required) {
    return L"Invalid UTF-8 response from Broker";
  }
  return result;
}

std::wstring CurrentUserSid() {
  HANDLE raw_token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
    return L"";
  }
  Handle token(raw_token);

  DWORD required = 0;
  if (::GetTokenInformation(token.get(), TokenUser, nullptr, 0, &required) ||
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0) {
    return L"";
  }
  std::vector<BYTE> buffer(required);
  if (!::GetTokenInformation(token.get(), TokenUser, buffer.data(), required,
                             &required)) {
    return L"";
  }

  const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
  LPWSTR sid = nullptr;
  if (!::ConvertSidToStringSidW(user->User.Sid, &sid) || !sid) {
    return L"";
  }
  std::wstring result(sid);
  ::LocalFree(sid);
  return result;
}

bool BrokerPipeName(std::wstring* pipe_name, std::wstring* error) {
  if (!pipe_name) {
    SetError(error, L"Missing Broker pipe name output");
    return false;
  }
  const std::wstring sid = CurrentUserSid();
  DWORD session_id = 0;
  if (sid.empty() ||
      !::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id)) {
    SetError(error, L"Could not resolve the current user pipe identity");
    return false;
  }
  *pipe_name = kBrokerControlPipePrefix + sid + L"\\" +
               std::to_wstring(session_id);
  return true;
}

bool WaitForIo(HANDLE pipe,
               OVERLAPPED* operation,
               DWORD timeout,
               DWORD* transferred,
               std::wstring* error) {
  const DWORD wait = ::WaitForSingleObject(operation->hEvent, timeout);
  if (wait == WAIT_OBJECT_0) {
    if (::GetOverlappedResult(pipe, operation, transferred, FALSE)) {
      return true;
    }
    SetError(error, SystemError(::GetLastError()));
    return false;
  }
  if (wait == WAIT_TIMEOUT) {
    // The OVERLAPPED structure, event, and (for reads) buffer are owned by
    // the caller's stack. Reap the cancelled operation before returning so
    // that none of them can be accessed after their lifetime ends.
    ::CancelIoEx(pipe, operation);
    DWORD cancelled_transferred = 0;
    ::GetOverlappedResult(pipe, operation, &cancelled_transferred, TRUE);
    SetError(error, L"The WubiPinyin Broker did not respond in time");
    return false;
  }
  SetError(error, SystemError(::GetLastError()));
  return false;
}

bool WriteWithTimeout(HANDLE pipe,
                      const std::vector<std::uint8_t>& buffer,
                      std::wstring* error) {
  Handle event(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (!event.valid()) {
    SetError(error, SystemError(::GetLastError()));
    return false;
  }
  OVERLAPPED operation{};
  operation.hEvent = event.get();
  DWORD transferred = 0;
  const BOOL started = ::WriteFile(
      pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &transferred,
      &operation);
  if (!started) {
    const DWORD write_error = ::GetLastError();
    if (write_error != ERROR_IO_PENDING ||
        !WaitForIo(pipe, &operation, kIoTimeoutMilliseconds, &transferred,
                   error)) {
      if (write_error != ERROR_IO_PENDING && error && error->empty()) {
        *error = SystemError(write_error);
      }
      return false;
    }
  }
  if (transferred != buffer.size()) {
    SetError(error, L"The control request was only partially written");
    return false;
  }
  return true;
}

bool ReadWithTimeout(HANDLE pipe,
                     std::vector<std::uint8_t>* response,
                     std::wstring* error) {
  if (!response) {
    SetError(error, L"Missing control response output");
    return false;
  }
  std::array<std::uint8_t, kMaximumFrameBytes> buffer{};
  Handle event(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (!event.valid()) {
    SetError(error, SystemError(::GetLastError()));
    return false;
  }
  OVERLAPPED operation{};
  operation.hEvent = event.get();
  DWORD transferred = 0;
  const BOOL started = ::ReadFile(pipe, buffer.data(),
                                  static_cast<DWORD>(buffer.size()),
                                  &transferred, &operation);
  if (!started) {
    const DWORD read_error = ::GetLastError();
    if (read_error != ERROR_IO_PENDING ||
        !WaitForIo(pipe, &operation, kIoTimeoutMilliseconds, &transferred,
                   error)) {
      if (read_error == ERROR_MORE_DATA) {
        SetError(error, L"The Broker response exceeds the control frame limit");
      } else if (read_error != ERROR_IO_PENDING && error && error->empty()) {
        *error = SystemError(read_error);
      }
      return false;
    }
  }
  if (transferred == 0) {
    SetError(error, L"The Broker closed the control pipe without a response");
    return false;
  }
  response->assign(buffer.begin(),
                   buffer.begin() + static_cast<std::ptrdiff_t>(transferred));
  return true;
}

Handle ConnectToBroker(std::wstring* error) {
  std::wstring name;
  if (!BrokerPipeName(&name, error)) {
    return Handle{};
  }

  const ULONGLONG deadline = ::GetTickCount64() + kConnectTimeoutMilliseconds;
  for (;;) {
    Handle pipe(::CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
                              nullptr));
    if (pipe.valid()) {
      DWORD mode = PIPE_READMODE_MESSAGE;
      if (::SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr)) {
        return pipe;
      }
      SetError(error, SystemError(::GetLastError()));
      return Handle{};
    }

    const DWORD connect_error = ::GetLastError();
    if (connect_error != ERROR_PIPE_BUSY) {
      if (connect_error == ERROR_FILE_NOT_FOUND) {
        SetError(error, L"WubiPinyin Broker is not running");
      } else {
        SetError(error, SystemError(connect_error));
      }
      return Handle{};
    }
    const ULONGLONG now = ::GetTickCount64();
    if (now >= deadline) {
      SetError(error, L"The WubiPinyin Broker is busy");
      return Handle{};
    }
    const DWORD remaining = static_cast<DWORD>(deadline - now);
    if (!::WaitNamedPipeW(name.c_str(), remaining)) {
      SetError(error, SystemError(::GetLastError()));
      return Handle{};
    }
  }
}

std::wstring PayloadError(ControlPayloadError error) {
  return Utf8ToWide(ControlPayloadErrorMessage(error));
}

std::wstring FrameError(ControlProtocolError error) {
  switch (error) {
    case ControlProtocolError::kNone:
      return L"";
    case ControlProtocolError::kInvalidArgument:
      return L"Invalid control frame argument";
    case ControlProtocolError::kInvalidMagic:
      return L"The Broker returned an invalid control frame";
    case ControlProtocolError::kUnsupportedVersion:
      return L"The Broker uses an unsupported control protocol version";
    case ControlProtocolError::kInvalidMessageType:
      return L"The Broker returned an invalid control response type";
    case ControlProtocolError::kInvalidRequestId:
      return L"The Broker returned an invalid request id";
    case ControlProtocolError::kPayloadTooLarge:
      return L"The Broker returned an oversized response";
    case ControlProtocolError::kTruncatedHeader:
    case ControlProtocolError::kTruncatedPayload:
    case ControlProtocolError::kTrailingBytes:
      return L"The Broker returned a truncated control frame";
    case ControlProtocolError::kRequestOutOfSequence:
      return L"The Broker returned an out-of-sequence response";
  }
  return L"Invalid control frame";
}

}  // namespace

SettingsClient::SettingsClient() = default;

bool SettingsClient::Request(ControlMessageType type,
                             std::uint64_t session_id,
                             const std::vector<std::uint8_t>& payload,
                             std::vector<std::uint8_t>* result,
                             std::wstring* error) {
  if (error) {
    error->clear();
  }
  if (!result) {
    SetError(error, L"Missing control result output");
    return false;
  }
  if (next_request_id_ == 0) {
    SetError(error, L"Control request id space is exhausted");
    return false;
  }

  ControlFrame request;
  request.type = type;
  request.request_id = next_request_id_++;
  request.session_id = session_id;
  request.generation = generation_;
  request.payload = payload;

  ControlProtocolError frame_error = ControlProtocolError::kNone;
  std::vector<std::uint8_t> encoded;
  if (!EncodeControlFrame(request, &encoded, &frame_error)) {
    SetError(error, FrameError(frame_error));
    return false;
  }

  Handle pipe = ConnectToBroker(error);
  if (!pipe.valid() || !WriteWithTimeout(pipe.get(), encoded, error)) {
    return false;
  }

  std::vector<std::uint8_t> response_bytes;
  if (!ReadWithTimeout(pipe.get(), &response_bytes, error)) {
    return false;
  }

  ControlFrame response;
  frame_error = ControlProtocolError::kNone;
  if (!DecodeControlFrame(response_bytes.data(), response_bytes.size(),
                          &response, &frame_error)) {
    SetError(error, FrameError(frame_error));
    return false;
  }
  if ((response.type != ControlMessageType::kResponse &&
       response.type != ControlMessageType::kError) ||
      response.request_id != request.request_id ||
      response.session_id != request.session_id ||
      response.generation != request.generation) {
    SetError(error, L"The Broker response does not match the control request");
    return false;
  }

  bool success = false;
  std::string message;
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  if (!DecodeControlReply(response.payload, &success, &message, result,
                          &payload_error)) {
    SetError(error, PayloadError(payload_error));
    return false;
  }
  if (!success || response.type == ControlMessageType::kError) {
    SetError(error, message.empty() ? L"The Broker rejected the request"
                                    : Utf8ToWide(message));
    return false;
  }
  return true;
}

bool SettingsClient::GetSettings(SettingsSnapshot* settings,
                                 std::wstring* error) {
  if (!settings) {
    SetError(error, L"Missing settings output");
    return false;
  }
  std::vector<std::uint8_t> result;
  if (!Request(ControlMessageType::kGetSettings, 0, {}, &result, error)) {
    return false;
  }
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  if (!DecodeSettingsSnapshot(result, settings, &payload_error)) {
    SetError(error, PayloadError(payload_error));
    return false;
  }
  return true;
}

bool SettingsClient::UpdateSettings(const SettingsSnapshot& settings,
                                    std::wstring* error) {
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  std::vector<std::uint8_t> payload;
  if (!EncodeSettingsSnapshot(settings, &payload, &payload_error)) {
    SetError(error, PayloadError(payload_error));
    return false;
  }
  std::vector<std::uint8_t> result;
  return Request(ControlMessageType::kUpdateSetting, 0, payload, &result,
                 error);
}

bool SettingsClient::ListUserEntries(std::vector<UserEntry>* entries,
                                     std::wstring* error) {
  if (!entries) {
    SetError(error, L"Missing user entry output");
    return false;
  }
  std::vector<std::uint8_t> result;
  if (!Request(ControlMessageType::kListUserEntries, 0, {}, &result, error)) {
    return false;
  }
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  if (!DecodeUserEntries(result, entries, &payload_error)) {
    SetError(error, PayloadError(payload_error));
    return false;
  }
  return true;
}

bool SettingsClient::UpsertUserEntry(UserEntry* entry, std::wstring* error) {
  if (!entry) {
    SetError(error, L"Missing user entry input");
    return false;
  }
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  std::vector<std::uint8_t> payload;
  if (!EncodeUserEntry(*entry, &payload, &payload_error)) {
    SetError(error, PayloadError(payload_error));
    return false;
  }
  std::vector<std::uint8_t> result;
  if (!Request(ControlMessageType::kUpsertUserEntry, 0, payload, &result,
               error)) {
    return false;
  }
  if (!DecodeUserEntry(result, entry, &payload_error)) {
    SetError(error, PayloadError(payload_error));
    return false;
  }
  return true;
}

bool SettingsClient::DeleteUserEntry(std::int64_t id, std::wstring* error) {
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  std::vector<std::uint8_t> payload;
  if (!EncodeUserEntryId(id, &payload, &payload_error)) {
    SetError(error, PayloadError(payload_error));
    return false;
  }
  std::vector<std::uint8_t> result;
  return Request(ControlMessageType::kDeleteUserEntry, 0, payload, &result,
                 error);
}

bool SettingsClient::ResetLearning(std::wstring* error) {
  std::vector<std::uint8_t> result;
  return Request(ControlMessageType::kResetLearning, 0, {}, &result, error);
}

bool SettingsClient::ReloadDictionaries(std::wstring* error) {
  std::vector<std::uint8_t> result;
  return Request(ControlMessageType::kReloadDictionaries, 0, {}, &result,
                 error);
}

bool SettingsClient::SetRoute(std::uint64_t session_id,
                              HybridRoute route,
                              std::wstring* error) {
  ControlPayloadError payload_error = ControlPayloadError::kNone;
  std::vector<std::uint8_t> payload;
  if (!EncodeHybridRoute(route, &payload, &payload_error)) {
    SetError(error, PayloadError(payload_error));
    return false;
  }
  std::vector<std::uint8_t> result;
  return Request(ControlMessageType::kSetRoute, session_id, payload, &result,
                 error);
}

bool SettingsClient::CommitRaw(std::uint64_t session_id,
                               std::wstring* error) {
  std::vector<std::uint8_t> result;
  return Request(ControlMessageType::kCommitRaw, session_id, {}, &result,
                 error);
}

}  // namespace wubipinyin::settings
