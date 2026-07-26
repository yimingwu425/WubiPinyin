#include "stdafx.h"
#include "BrokerControlServer.h"

#include <array>
#include <memory>
#include <vector>

#include <WeaselIPC.h>
#include <logging.h>

#include "../WubiPinyinCore/WubiPinyinControlPayload.h"

namespace {

constexpr DWORD kControlIoTimeoutMs = 500;
constexpr DWORD kControlRetryDelayMs = 100;
constexpr std::size_t kMaximumFrameBytes =
    wubipinyin::kControlFrameHeaderBytes +
    wubipinyin::kMaxControlPayloadBytes;

class EventHandle {
 public:
  EventHandle() : handle_(::CreateEventW(NULL, TRUE, FALSE, NULL)) {}
  ~EventHandle() {
    if (handle_ != NULL) {
      ::CloseHandle(handle_);
    }
  }

  EventHandle(const EventHandle&) = delete;
  EventHandle& operator=(const EventHandle&) = delete;

  HANDLE get() const { return handle_; }

 private:
  HANDLE handle_;
};

wubipinyin::ControlFrame MakeErrorResponse(
    const wubipinyin::ControlFrame& request,
    const std::string& message) {
  wubipinyin::ControlFrame response;
  response.type = wubipinyin::ControlMessageType::kError;
  response.request_id = request.request_id;
  response.session_id = request.session_id;
  response.generation = request.generation;
  wubipinyin::ControlPayloadError payload_error =
      wubipinyin::ControlPayloadError::kNone;
  if (!wubipinyin::EncodeControlReply(false, message, {}, &response.payload,
                                      &payload_error)) {
    response.payload.clear();
  }
  return response;
}

}  // namespace

BrokerControlServer::BrokerControlServer(Dispatch dispatch)
    : dispatch_(std::move(dispatch)) {}

BrokerControlServer::~BrokerControlServer() {
  Stop();
}

bool BrokerControlServer::Start() {
  if (!dispatch_ || !security_.valid() || listener_.joinable()) {
    return false;
  }
  stop_event_ = ::CreateEventW(NULL, TRUE, FALSE, NULL);
  if (stop_event_ == NULL) {
    return false;
  }
  listener_ = std::thread([this] { Listen(); });
  return true;
}

void BrokerControlServer::Stop() {
  if (stop_event_ != NULL) {
    ::SetEvent(stop_event_);
  }
  if (listener_.joinable()) {
    listener_.join();
  }
  if (stop_event_ != NULL) {
    ::CloseHandle(stop_event_);
    stop_event_ = NULL;
  }
}

bool BrokerControlServer::IsStopping() const {
  return stop_event_ != NULL &&
         ::WaitForSingleObject(stop_event_, 0) == WAIT_OBJECT_0;
}

bool BrokerControlServer::WaitForIo(HANDLE pipe,
                                    OVERLAPPED* overlapped,
                                    DWORD timeout_ms,
                                    DWORD* transferred) {
  if (!overlapped || overlapped->hEvent == NULL || stop_event_ == NULL) {
    return false;
  }
  HANDLE wait_handles[] = {overlapped->hEvent, stop_event_};
  const DWORD wait =
      ::WaitForMultipleObjects(2, wait_handles, FALSE, timeout_ms);
  if (wait != WAIT_OBJECT_0) {
    ::CancelIoEx(pipe, overlapped);

    // Keep the event and OVERLAPPED alive until the cancelled operation has
    // completed. Closing either immediately after CancelIoEx risks a late
    // completion referencing invalid stack storage.
    DWORD ignored = 0;
    ::GetOverlappedResult(pipe, overlapped, &ignored, TRUE);
    return false;
  }
  return ::GetOverlappedResult(pipe, overlapped, transferred, FALSE) != FALSE;
}

bool BrokerControlServer::Connect(HANDLE pipe) {
  EventHandle event;
  if (event.get() == NULL) {
    return false;
  }
  OVERLAPPED overlapped{};
  overlapped.hEvent = event.get();
  if (::ConnectNamedPipe(pipe, &overlapped)) {
    return true;
  }
  const DWORD error = ::GetLastError();
  if (error == ERROR_PIPE_CONNECTED) {
    return true;
  }
  if (error != ERROR_IO_PENDING) {
    return false;
  }
  DWORD ignored = 0;
  return WaitForIo(pipe, &overlapped, INFINITE, &ignored);
}

bool BrokerControlServer::ReadFrame(HANDLE pipe,
                                    wubipinyin::ControlFrame* frame) {
  if (!frame) {
    return false;
  }
  std::array<std::uint8_t, kMaximumFrameBytes> bytes{};
  EventHandle event;
  if (event.get() == NULL) {
    return false;
  }
  OVERLAPPED overlapped{};
  overlapped.hEvent = event.get();
  DWORD read = 0;
  if (!::ReadFile(pipe, bytes.data(), static_cast<DWORD>(bytes.size()), &read,
                  &overlapped)) {
    const DWORD error = ::GetLastError();
    if (error != ERROR_IO_PENDING ||
        !WaitForIo(pipe, &overlapped, kControlIoTimeoutMs, &read)) {
      return false;
    }
  }
  wubipinyin::ControlProtocolError protocol_error =
      wubipinyin::ControlProtocolError::kNone;
  return wubipinyin::DecodeControlFrame(bytes.data(), read, frame,
                                         &protocol_error);
}

bool BrokerControlServer::WriteFrame(HANDLE pipe,
                                     const wubipinyin::ControlFrame& frame) {
  wubipinyin::ControlProtocolError protocol_error =
      wubipinyin::ControlProtocolError::kNone;
  std::vector<std::uint8_t> encoded;
  if (!wubipinyin::EncodeControlFrame(frame, &encoded, &protocol_error)) {
    return false;
  }
  EventHandle event;
  if (event.get() == NULL) {
    return false;
  }
  OVERLAPPED overlapped{};
  overlapped.hEvent = event.get();
  DWORD written = 0;
  if (!::WriteFile(pipe, encoded.data(), static_cast<DWORD>(encoded.size()),
                   &written, &overlapped)) {
    const DWORD error = ::GetLastError();
    if (error != ERROR_IO_PENDING ||
        !WaitForIo(pipe, &overlapped, kControlIoTimeoutMs, &written)) {
      return false;
    }
  }
  return written == encoded.size();
}

void BrokerControlServer::ProcessConnection(HANDLE pipe) {
  std::uint64_t previous_request_id = 0;
  std::uint64_t previous_generation = 0;
  while (!IsStopping()) {
    wubipinyin::ControlFrame request;
    if (!ReadFrame(pipe, &request)) {
      return;
    }
    wubipinyin::ControlProtocolError protocol_error =
        wubipinyin::ControlProtocolError::kNone;
    if (!wubipinyin::ValidateControlRequestSequence(previous_request_id,
                                                     request.request_id,
                                                     &protocol_error)) {
      return;
    }
    if (request.generation == 0 ||
        (previous_generation != 0 &&
         request.generation < previous_generation)) {
      return;
    }
    previous_request_id = request.request_id;
    previous_generation = request.generation;

    wubipinyin::ControlFrame response;
    std::string error;
    if (!dispatch_(request, &response, &error)) {
      response = MakeErrorResponse(request, error.empty()
                                                ? "Broker control dispatch failed"
                                                : error);
    }
    if (!WriteFrame(pipe, response)) {
      return;
    }
  }
}

void BrokerControlServer::Listen() {
  while (!IsStopping()) {
    HANDLE pipe = ::CreateNamedPipeW(
        weasel::GetBrokerControlPipeName().c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
            PIPE_REJECT_REMOTE_CLIENTS,
        1, static_cast<DWORD>(kMaximumFrameBytes),
        static_cast<DWORD>(kMaximumFrameBytes), 0, security_.get_attr());
    if (pipe == INVALID_HANDLE_VALUE) {
      LOG(WARNING) << "Unable to create WubiPinyin control pipe: "
                   << ::GetLastError();
      ::WaitForSingleObject(stop_event_, kControlRetryDelayMs);
      continue;
    }
    if (Connect(pipe) && security_.IsCurrentUserClient(pipe)) {
      ProcessConnection(pipe);
    } else if (!IsStopping()) {
      LOG(WARNING) << "Rejected WubiPinyin control-pipe client.";
    }
    ::DisconnectNamedPipe(pipe);
    ::CloseHandle(pipe);
  }
}
