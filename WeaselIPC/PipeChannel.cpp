#include "stdafx.h"

#include <PipeChannel.h>

#include <cstring>
#include <memory>
#include <new>
#include <utility>
#include <vector>

using namespace weasel;
using namespace std;
using namespace boost;

#define _ThrowLastError throw ::GetLastError()
#define _ThrowCode(__c) throw __c
#define _ThrowIfNot(__c)                 \
  {                                      \
    DWORD err;                           \
    if ((err = ::GetLastError()) != __c) \
      throw err;                         \
  }

namespace {
constexpr DWORD kPipeConnectTimeoutMs = 100;
constexpr DWORD kPipeConnectRetryIntervalMs = 10;
constexpr DWORD kPipeIoTimeoutMs = 50;

static_assert(kPipeConnectTimeoutMs > 0);
static_assert(kPipeConnectTimeoutMs <= 100);
static_assert(kPipeConnectRetryIntervalMs > 0);
static_assert(kPipeConnectRetryIntervalMs <= kPipeConnectTimeoutMs);
static_assert(kPipeIoTimeoutMs > 0);

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

enum class IoDirection { kRead, kWrite };

// This object owns every value that Windows may still touch after an I/O
// deadline. It borrows the pipe until a timeout transfers that handle to the
// deferred reaper.
class PendingIo {
 public:
  PendingIo(HANDLE pipe, DWORD byte_count)
      : pipe_(pipe), buffer_(byte_count) {
    operation_.hEvent = event_.get();
  }

  ~PendingIo() {
    if (owns_pipe_ && pipe_ != INVALID_HANDLE_VALUE) {
      ::CloseHandle(pipe_);
    }
  }

  PendingIo(const PendingIo&) = delete;
  PendingIo& operator=(const PendingIo&) = delete;

  bool valid() const { return event_.get() != NULL; }
  HANDLE pipe() const { return pipe_; }
  OVERLAPPED* operation() { return &operation_; }
  BYTE* data() { return buffer_.data(); }
  DWORD size() const { return static_cast<DWORD>(buffer_.size()); }

  void AdoptPipe(HANDLE& pipe) {
    pipe_ = pipe;
    owns_pipe_ = true;
    pipe = INVALID_HANDLE_VALUE;
  }

  void SetReaperModule(HMODULE module) { reaper_module_ = module; }
  HMODULE TakeReaperModule() {
    HMODULE module = reaper_module_;
    reaper_module_ = NULL;
    return module;
  }

 private:
  HANDLE pipe_;
  EventHandle event_;
  OVERLAPPED operation_{};
  std::vector<BYTE> buffer_;
  bool owns_pipe_ = false;
  HMODULE reaper_module_ = NULL;
};

DWORD GetPendingResult(PendingIo* pending,
                       DWORD* transferred,
                       bool* completed) {
  DWORD result = 0;
  if (::GetOverlappedResult(pending->pipe(), pending->operation(), &result,
                            FALSE)) {
    *transferred = result;
    *completed = true;
    return ERROR_SUCCESS;
  }

  const DWORD error = ::GetLastError();
  *transferred = result;
  if (error == ERROR_IO_INCOMPLETE) {
    *completed = false;
    return error;
  }

  *completed = true;
  return error;
}

DWORD CopyReadResult(PendingIo* pending,
                     void* destination,
                     DWORD transferred,
                     DWORD error) {
  if (transferred > pending->size()) {
    return ERROR_INVALID_DATA;
  }
  if ((error == ERROR_SUCCESS || error == ERROR_MORE_DATA) &&
      transferred > 0) {
    memcpy(destination, pending->data(), transferred);
  }
  return error;
}

DWORD WINAPI ReapCancelledIo(LPVOID context) {
  std::unique_ptr<PendingIo> pending(static_cast<PendingIo*>(context));
  DWORD ignored = 0;
  ::GetOverlappedResult(pending->pipe(), pending->operation(), &ignored,
                        TRUE);

  HMODULE module = pending->TakeReaperModule();
  pending.reset();
  if (module != NULL) {
    ::FreeLibraryAndExitThread(module, 0);
  }
  return 0;
}

DWORD WINAPI ReapCancelledIoFromThreadPool(LPVOID context) {
  std::unique_ptr<PendingIo> pending(static_cast<PendingIo*>(context));
  DWORD ignored = 0;
  ::GetOverlappedResult(pending->pipe(), pending->operation(), &ignored,
                        TRUE);
  // The module reference remains pinned for this exceptional fallback so the
  // thread-pool callback can safely return through code in this module.
  return 0;
}

void StartCancelledIoReaper(std::unique_ptr<PendingIo> pending) {
  HMODULE module = NULL;
  if (!::GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
          (LPCWSTR)&ReapCancelledIo, &module)) {
    // The cancellation state must outlive the pending I/O. In the exceptional
    // case that this module cannot be pinned, retaining it is safer than
    // freeing memory Windows may still access.
    pending.release();
    return;
  }

  pending->SetReaperModule(module);
  PendingIo* raw_pending = pending.release();
  HANDLE thread = ::CreateThread(NULL, 0, ReapCancelledIo, raw_pending, 0,
                                 NULL);
  if (thread != NULL) {
    ::CloseHandle(thread);
    return;
  }

  if (::QueueUserWorkItem(ReapCancelledIoFromThreadPool, raw_pending,
                          WT_EXECUTELONGFUNCTION)) {
    return;
  }

  raw_pending->TakeReaperModule();
  ::FreeLibrary(module);
  // See the lifetime rule above. A failed thread creation must not turn a
  // timeout into a use-after-free on the TSF thread.
  (void)raw_pending;
}

DWORD RetirePendingIo(HANDLE& pipe,
                      std::unique_ptr<PendingIo> pending,
                      DWORD failure) {
  ::CancelIoEx(pending->pipe(), pending->operation());
  pending->AdoptPipe(pipe);

  DWORD ignored = 0;
  bool completed = false;
  GetPendingResult(pending.get(), &ignored, &completed);
  if (!completed) {
    StartCancelledIoReaper(std::move(pending));
  }
  return failure;
}

DWORD TransferWithTimeout(HANDLE& pipe,
                          IoDirection direction,
                          const void* source,
                          void* destination,
                          DWORD byte_count,
                          DWORD* transferred) {
  if (transferred == NULL ||
      (direction == IoDirection::kWrite && byte_count > 0 && source == NULL) ||
      (direction == IoDirection::kRead && byte_count > 0 &&
       destination == NULL)) {
    return ERROR_INVALID_PARAMETER;
  }

  std::unique_ptr<PendingIo> pending;
  try {
    pending = std::make_unique<PendingIo>(pipe, byte_count);
  } catch (const std::bad_alloc&) {
    return ERROR_NOT_ENOUGH_MEMORY;
  }
  if (!pending->valid()) {
    return ::GetLastError();
  }

  if (direction == IoDirection::kWrite && byte_count > 0) {
    memcpy(pending->data(), source, byte_count);
  }

  BOOL started = FALSE;
  if (direction == IoDirection::kWrite) {
    started = ::WriteFile(pipe, pending->data(), byte_count, NULL,
                          pending->operation());
  } else {
    started = ::ReadFile(pipe, pending->data(), byte_count, NULL,
                         pending->operation());
  }
  const DWORD start_error = started ? ERROR_SUCCESS : ::GetLastError();

  if (!started && start_error != ERROR_IO_PENDING &&
      start_error != ERROR_MORE_DATA) {
    return start_error;
  }

  DWORD result = 0;
  bool completed = false;
  DWORD completion_error =
      GetPendingResult(pending.get(), &result, &completed);
  if (completed) {
    *transferred = result;
    return direction == IoDirection::kRead
               ? CopyReadResult(pending.get(), destination, result,
                                completion_error)
               : completion_error;
  }
  if (!started && start_error != ERROR_IO_PENDING) {
    return RetirePendingIo(pipe, std::move(pending), start_error);
  }

  const DWORD wait =
      ::WaitForSingleObject(pending->operation()->hEvent, kPipeIoTimeoutMs);
  if (wait == WAIT_OBJECT_0) {
    completion_error = GetPendingResult(pending.get(), &result, &completed);
    if (completed) {
      *transferred = result;
      return direction == IoDirection::kRead
                 ? CopyReadResult(pending.get(), destination, result,
                                  completion_error)
                 : completion_error;
    }
    return RetirePendingIo(pipe, std::move(pending), ERROR_IO_INCOMPLETE);
  }

  const DWORD failure = wait == WAIT_TIMEOUT
                            ? ERROR_TIMEOUT
                            : (wait == WAIT_FAILED ? ::GetLastError()
                                                   : ERROR_GEN_FAILURE);
  // A completion can race with cancellation. Either way, the caller has
  // crossed its deadline, so discard its result and retire this connection.
  return RetirePendingIo(pipe, std::move(pending), failure);
}

DWORD WriteWithTimeout(HANDLE& pipe,
                       const void* buffer,
                       DWORD bytes_to_write,
                       DWORD* written) {
  return TransferWithTimeout(pipe, IoDirection::kWrite, buffer, NULL,
                             bytes_to_write, written);
}

DWORD ReadWithTimeout(HANDLE& pipe,
                      void* buffer,
                      DWORD bytes_to_read,
                      DWORD* read) {
  return TransferWithTimeout(pipe, IoDirection::kRead, NULL, buffer,
                             bytes_to_read, read);
}
}  // namespace

PipeChannelBase::PipeChannelBase(std::wstring&& pn_cmd,
                                 size_t bs = 4 * 1024,
                                 SECURITY_ATTRIBUTES* s = NULL)
    : pname(pn_cmd),
      buff_size(bs),
      use_overlapped_io(s == NULL),
      sa(s) {};

PipeChannelBase::~PipeChannelBase() {
  // Thread-specific pointers are cleaned up automatically
}

bool PipeChannelBase::_Ensure() {
  try {
    HANDLE* phandle = _GetPipeHandle();
    if (_Invalid(*phandle)) {
      *phandle = _Connect(pname.c_str());
      return !_Invalid(*phandle);
    }
  } catch (...) {
    return false;
  }

  return true;
}

HANDLE PipeChannelBase::_Connect(const wchar_t* name) {
  HANDLE pipe = INVALID_HANDLE_VALUE;
  const ULONGLONG deadline = ::GetTickCount64() + kPipeConnectTimeoutMs;
  while (_Invalid(pipe = _TryConnect())) {
    const ULONGLONG now = ::GetTickCount64();
    if (now >= deadline) {
      _ThrowCode(ERROR_SEM_TIMEOUT);
    }

    const DWORD remaining = static_cast<DWORD>(deadline - now);
    const DWORD wait_time = remaining < kPipeConnectRetryIntervalMs
                                ? remaining
                                : kPipeConnectRetryIntervalMs;
    if (!::WaitNamedPipe(name, wait_time)) {
      const DWORD error = ::GetLastError();
      if (error != ERROR_SEM_TIMEOUT && error != ERROR_PIPE_BUSY) {
        _ThrowCode(error);
      }
    }
  }

  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL)) {
    const DWORD error = ::GetLastError();
    ::CloseHandle(pipe);
    _ThrowCode(error);
  }
  return pipe;
}

bool PipeChannelBase::_Reconnect() {
  HANDLE* phandle = _GetPipeHandle();
  _FinalizePipe(*phandle);
  return _Ensure();
}

HANDLE PipeChannelBase::_TryConnect() {
  auto pipe = ::CreateFile(pname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  if (!_Invalid(pipe)) {
    // connected to the pipe
    return pipe;
  }
  // being busy is not really an error since we just need to wait.
  _ThrowIfNot(ERROR_PIPE_BUSY);
  // All pipe instances are busy
  return INVALID_HANDLE_VALUE;
}

size_t PipeChannelBase::_WritePipe(HANDLE& pipe, size_t s, char* b) {
  if (s > MAXDWORD) {
    _ThrowCode(ERROR_INVALID_PARAMETER);
  }

  const DWORD bytes_to_write = static_cast<DWORD>(s);
  DWORD lwritten = 0;
  if (use_overlapped_io) {
    const DWORD error =
        WriteWithTimeout(pipe, b, bytes_to_write, &lwritten);
    if (error != ERROR_SUCCESS) {
      _ThrowCode(error);
    }
  } else if (!::WriteFile(pipe, b, bytes_to_write, &lwritten, NULL)) {
    _ThrowLastError;
  }
  if (lwritten != bytes_to_write) {
    _ThrowCode(ERROR_WRITE_FAULT);
  }

  // The message remains available to the peer without a synchronous flush.
  // Flushing here can block forever when a peer disconnects or stops reading.
  return lwritten;
}

void PipeChannelBase::_FinalizePipe(HANDLE& p) {
  if (!_Invalid(p)) {
    DisconnectNamedPipe(p);
    CloseHandle(p);
  }
  p = INVALID_HANDLE_VALUE;
}

void PipeChannelBase::_Receive(HANDLE& pipe, LPVOID msg, size_t rec_len) {
  if (rec_len > MAXDWORD || buff_size > MAXDWORD) {
    _ThrowCode(ERROR_INVALID_PARAMETER);
  }

  const DWORD bytes_to_read = static_cast<DWORD>(rec_len);
  DWORD lread = 0;
  memset(msg, 0, rec_len);
  DWORD error = ERROR_SUCCESS;
  if (use_overlapped_io) {
    error = ReadWithTimeout(pipe, msg, bytes_to_read, &lread);
  } else if (!::ReadFile(pipe, msg, bytes_to_read, &lread, NULL)) {
    error = ::GetLastError();
  }
  if (error == ERROR_SUCCESS && lread != bytes_to_read) {
    _ThrowCode(ERROR_INVALID_DATA);
  }
  if (error != ERROR_SUCCESS) {
    if (error != ERROR_MORE_DATA || lread != bytes_to_read) {
      _ThrowCode(error == ERROR_MORE_DATA ? ERROR_INVALID_DATA : error);
    }

    auto ctx = _GetContext();
    memset(ctx->buffer.get(), 0, buff_size);
    if (use_overlapped_io) {
      error = ReadWithTimeout(pipe, ctx->buffer.get(),
                              static_cast<DWORD>(buff_size), &lread);
    } else if (!::ReadFile(pipe, ctx->buffer.get(),
                           static_cast<DWORD>(buff_size), &lread, NULL)) {
      error = ::GetLastError();
    } else {
      error = ERROR_SUCCESS;
    }
    if (error != ERROR_SUCCESS) {
      _ThrowCode(error);
    }
  }
  _GetContext()->has_body = false;
}

HANDLE PipeChannelBase::_ConnectServerPipe(std::wstring& pn) {
  HANDLE pipe = CreateNamedPipe(
      pn.c_str(), PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
          PIPE_REJECT_REMOTE_CLIENTS,
      PIPE_UNLIMITED_INSTANCES, buff_size, buff_size, 0, sa);
  if (pipe == INVALID_HANDLE_VALUE) {
    _ThrowLastError;
  }

  if (!::ConnectNamedPipe(pipe, NULL)) {
    const DWORD error = ::GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      ::CloseHandle(pipe);
      _ThrowCode(error);
    }
  }
  return pipe;
}
