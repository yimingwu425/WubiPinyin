#include "stdafx.h"
#include "WeaselServerImpl.h"
#include <atomic>
#include <mutex>
#include <Windows.h>
#include <resource.h>
#include <WeaselConstants.h>
#include <WeaselUtility.h>

namespace weasel {
class PipeServer : public PipeChannel<DWORD, PipeMessage> {
 public:
  using ServerRunner = std::function<void()>;
  using Respond = std::function<void(Msg)>;
  using ServerHandler = std::function<void(PipeMessage, Respond)>;

  PipeServer(std::wstring&& pn_cmd, SecurityAttribute& security);

 public:
  void Listen(ServerHandler const& handler);
  /* Get a server runner */
  ServerRunner GetServerRunner(ServerHandler const& handler);

 private:
  void _ProcessPipeThread(HANDLE pipe, ServerHandler const& handler);

  SecurityAttribute& security;
};
}  // namespace weasel

using namespace weasel;

extern CAppModule _Module;

namespace {

struct PendingServerTask {
  explicit PendingServerTask(std::function<void()> callback)
      : callback(std::move(callback)), completed(::CreateEventW(
                                       NULL, TRUE, FALSE, NULL)) {}

  ~PendingServerTask() {
    if (completed != NULL) {
      ::CloseHandle(completed);
    }
  }

  std::function<void()> callback;
  HANDLE completed;
  std::atomic_bool cancelled = false;
};

std::mutex g_api_mutex;

}  // namespace

ServerImpl::ServerImpl()
    : sa(),
      channel(std::make_unique<PipeServer>(GetPipeName(), sa)),
      m_pRequestHandler(NULL),
      m_hUser32Module(NULL),
      m_darkMode(IsUserDarkMode()),
      m_server_thread_id(0) {
  m_hUser32Module = GetModuleHandle(_T("user32.dll"));
}

ServerImpl::~ServerImpl() {
  _Finailize();
}

void ServerImpl::_Finailize() {
  if (pipeThread != nullptr) {
    pipeThread->interrupt();
    pipeThread = nullptr;
  } else {
    // avoid finalize again
    return;
  }

  if (IsWindow()) {
    DestroyWindow();
  }
}

LRESULT ServerImpl::OnColorChange(UINT uMsg,
                                  WPARAM wParam,
                                  LPARAM lParam,
                                  BOOL& bHandled) {
  if (IsUserDarkMode() != m_darkMode) {
    m_darkMode = IsUserDarkMode();
    m_pRequestHandler->UpdateColorTheme(m_darkMode);
  }
  return 0;
}

LRESULT ServerImpl::OnCreate(UINT uMsg,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL& bHandled) {
  // not neccessary...
  ::SetWindowText(m_hWnd, WEASEL_IPC_WINDOW);
  return 0;
}

LRESULT ServerImpl::OnClose(UINT uMsg,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL& bHandled) {
  Stop();
  return 0;
}

LRESULT ServerImpl::OnDestroy(UINT uMsg,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL& bHandled) {
  bHandled = FALSE;
  return 1;
}

LRESULT ServerImpl::OnQueryEndSystemSession(UINT uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam,
                                            BOOL& bHandled) {
  return TRUE;
}

LRESULT ServerImpl::OnEndSystemSession(UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam,
                                       BOOL& bHandled) {
  if (m_pRequestHandler) {
    m_pRequestHandler->Finalize();
    m_pRequestHandler = nullptr;
  }
  return 0;
}

LRESULT ServerImpl::OnCommand(UINT uMsg,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL& bHandled) {
  UINT uID = LOWORD(wParam);
  switch (uID) {
    case ID_WEASELTRAY_ENABLE_ASCII:
      m_pRequestHandler->SetOption(lParam, "ascii_mode", true);
      return 0;
    case ID_WEASELTRAY_DISABLE_ASCII:
      m_pRequestHandler->SetOption(lParam, "ascii_mode", false);
      return 0;
    default:;
  }

  std::map<UINT, CommandHandler>::iterator it = m_MenuHandlers.find(uID);
  if (it == m_MenuHandlers.end()) {
    bHandled = FALSE;
    return 0;
  }
  it->second();  // execute command
  return 0;
}

LRESULT ServerImpl::OnServerTask(UINT uMsg,
                                 WPARAM wParam,
                                 LPARAM lParam,
                                 BOOL& bHandled) {
  auto posted_task = std::unique_ptr<std::shared_ptr<PendingServerTask>>(
      reinterpret_cast<std::shared_ptr<PendingServerTask>*>(lParam));
  if (!posted_task || !*posted_task) {
    bHandled = TRUE;
    return 0;
  }
  const std::shared_ptr<PendingServerTask> task = *posted_task;
  try {
    std::lock_guard guard(g_api_mutex);
    if (!task->cancelled.load(std::memory_order_acquire) && task->callback) {
      task->callback();
    }
  } catch (...) {
    // The control-pipe callback owns its reply state and turns exceptions into
    // a bounded error response. Never allow a task exception into WTL.
  }
  if (task->completed != NULL) {
    ::SetEvent(task->completed);
  }
  bHandled = TRUE;
  return 0;
}

DWORD ServerImpl::OnCommand(WEASEL_IPC_COMMAND uMsg,
                            DWORD wParam,
                            DWORD lParam) {
  BOOL handled = TRUE;
  OnCommand(uMsg, wParam, lParam, handled);
  return handled;
}

HWND ServerImpl::Start() {
  if (!sa.valid()) {
    return 0;
  }

  std::wstring instanceName = WUBIPINYIN_SERVER_MUTEX_PREFIX;
  instanceName += getUsername();
  HANDLE hMutexOneInstance = ::CreateMutex(NULL, FALSE, instanceName.c_str());
  bool areYouOK = (::GetLastError() == ERROR_ALREADY_EXISTS ||
                   ::GetLastError() == ERROR_ACCESS_DENIED);

  if (areYouOK) {
    return 0;  // assure single instance
  }

  m_server_thread_id = ::GetCurrentThreadId();
  HWND hwnd = Create(NULL);

  return hwnd;
}

int ServerImpl::Stop() {
  // DO NOT exit process or finalize here
  // Let WeaselServer handle this
  PostMessage(WM_QUIT);
  return 0;
}

int ServerImpl::Run() {
  // This workaround causes a VC internal error:
  // void PipeServer::Listen(ServerHandler handler);
  //
  // auto handler = boost::bind(&ServerImpl::HandlePipeMessage, this);
  // auto listener = boost::bind(&PipeServer::Listen, channel.get(), handler);
  //
  auto listener = [this](PipeMessage msg, PipeServer::Respond resp) -> void {
    std::lock_guard guard(g_api_mutex);
    HandlePipeMessage(msg, resp);
  };
  pipeThread = std::make_unique<boost::thread>(
      [this, &listener]() { channel->Listen(listener); });

  CMessageLoop theLoop;
  _Module.AddMessageLoop(&theLoop);
  int nRet = theLoop.Run();
  _Module.RemoveMessageLoop();
  return nRet;
}

bool ServerImpl::RunOnServerThread(std::function<void()> task,
                                   DWORD timeout_ms) {
  if (!task || !IsWindow() || timeout_ms == 0) {
    return false;
  }
  if (::GetCurrentThreadId() == m_server_thread_id) {
    std::lock_guard guard(g_api_mutex);
    task();
    return true;
  }

  auto pending = std::make_shared<PendingServerTask>(std::move(task));
  if (pending->completed == NULL) {
    return false;
  }
  auto* posted_task = new std::shared_ptr<PendingServerTask>(pending);
  if (!::PostMessage(m_hWnd, WEASEL_IPC_SERVER_TASK_MESSAGE, 0,
                     reinterpret_cast<LPARAM>(posted_task))) {
    delete posted_task;
    return false;
  }
  const bool completed =
      ::WaitForSingleObject(pending->completed, timeout_ms) == WAIT_OBJECT_0;
  if (!completed) {
    pending->cancelled.store(true, std::memory_order_release);
  }
  return completed;
}

DWORD ServerImpl::OnEcho(WEASEL_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->FindSession(lParam);
}

DWORD ServerImpl::OnStartSession(WEASEL_IPC_COMMAND uMsg,
                                 DWORD wParam,
                                 DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->AddSession(
      reinterpret_cast<LPWSTR>(channel->ReceiveBuffer()),
      [this](std::wstring& msg) -> bool {
        *channel << msg;
        return true;
      });
}

DWORD ServerImpl::OnEndSession(WEASEL_IPC_COMMAND uMsg,
                               DWORD wParam,
                               DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->RemoveSession(lParam);
}

DWORD ServerImpl::OnKeyEvent(WEASEL_IPC_COMMAND uMsg,
                             DWORD wParam,
                             DWORD lParam) {
  if (!m_pRequestHandler /* || !m_pSharedMemory*/)
    return 0;

  auto eat = [this](std::wstring& msg) -> bool {
    *channel << msg;
    return true;
  };
  return m_pRequestHandler->ProcessKeyEvent(KeyEvent(wParam), lParam, eat);
}

DWORD ServerImpl::OnShutdownServer(WEASEL_IPC_COMMAND uMsg,
                                   DWORD wParam,
                                   DWORD lParam) {
  Stop();
  return 0;
}

DWORD ServerImpl::OnFocusIn(WEASEL_IPC_COMMAND uMsg,
                            DWORD wParam,
                            DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  m_pRequestHandler->FocusIn(wParam, lParam);
  return 0;
}

DWORD ServerImpl::OnFocusOut(WEASEL_IPC_COMMAND uMsg,
                             DWORD wParam,
                             DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  m_pRequestHandler->FocusOut(wParam, lParam);
  return 0;
}

DWORD ServerImpl::OnUpdateInputPosition(WEASEL_IPC_COMMAND uMsg,
                                        DWORD wParam,
                                        DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  /*
   * 移位标志 = 1bit == 0
   * height: 0~127 = 7bit
   * top:-2048~2047 = 12bit（有符号）
   * left:-2048~2047 = 12bit（有符号）
   *
   * 高解析度下：
   * 移位标志 = 1bit == 1
   * height: 0~254 = 7bit（舍弃低1位）
   * top: -4096~4094 = 12bit（有符号，舍弃低1位）
   * left: -4096~4094 = 12bit（有符号，舍弃低1位）
   */
  RECT rc;
  int hi_res = (wParam >> 31) & 0x01;
  rc.left = ((wParam & 0x7ff) - (wParam & 0x800)) << hi_res;
  rc.top = (((wParam >> 12) & 0x7ff) - ((wParam >> 12) & 0x800)) << hi_res;
  const int width = 6;
  int height = ((wParam >> 24) & 0x7f) << hi_res;
  rc.right = rc.left + width;
  rc.bottom = rc.top + height;

  {
    using PPTLPFPMDPI = BOOL(WINAPI*)(HWND, LPPOINT);
    PPTLPFPMDPI PhysicalToLogicalPointForPerMonitorDPI =
        (PPTLPFPMDPI)::GetProcAddress(m_hUser32Module,
                                      "PhysicalToLogicalPointForPerMonitorDPI");
    POINT lt = {rc.left, rc.top};
    POINT rb = {rc.right, rc.bottom};
    PhysicalToLogicalPointForPerMonitorDPI(NULL, &lt);
    PhysicalToLogicalPointForPerMonitorDPI(NULL, &rb);
    rc = {lt.x, lt.y, rb.x, rb.y};
  }

  m_pRequestHandler->UpdateInputPosition(rc, lParam);
  return 0;
}

DWORD ServerImpl::OnStartMaintenance(WEASEL_IPC_COMMAND uMsg,
                                     DWORD wParam,
                                     DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->StartMaintenance();
  return 0;
}

DWORD ServerImpl::OnEndMaintenance(WEASEL_IPC_COMMAND uMsg,
                                   DWORD wParam,
                                   DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->EndMaintenance();
  return 0;
}

DWORD ServerImpl::OnCommitComposition(WEASEL_IPC_COMMAND uMsg,
                                      DWORD wParam,
                                      DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->CommitComposition(lParam);
  return 0;
}

DWORD ServerImpl::OnClearComposition(WEASEL_IPC_COMMAND uMsg,
                                     DWORD wParam,
                                     DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->ClearComposition(lParam);
  return 0;
}

DWORD ServerImpl::OnSelectCandidateOnCurrentPage(WEASEL_IPC_COMMAND uMsg,
                                                 DWORD wParam,
                                                 DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->SelectCandidateOnCurrentPage(wParam, lParam);
  return 0;
}

DWORD ServerImpl::OnHighlightCandidateOnCurrentPage(WEASEL_IPC_COMMAND uMsg,
                                                    DWORD wParam,
                                                    DWORD lParam) {
  if (m_pRequestHandler) {
    auto eat = [this](std::wstring& msg) -> bool {
      *channel << msg;
      return true;
    };
    m_pRequestHandler->HighlightCandidateOnCurrentPage(wParam, lParam, eat);
  }
  return 0;
}

DWORD ServerImpl::OnChangePage(WEASEL_IPC_COMMAND uMsg,
                               DWORD wParam,
                               DWORD lParam) {
  if (m_pRequestHandler) {
    auto eat = [this](std::wstring& msg) -> bool {
      *channel << msg;
      return true;
    };
    m_pRequestHandler->ChangePage(wParam, lParam, eat);
  }
  return 0;
}

#define MAP_PIPE_MSG_HANDLE(__msg, __wParam, __lParam) \
  {                                                    \
    auto lParam = __lParam;                            \
    auto wParam = __wParam;                            \
    LRESULT _result = 0;                               \
    switch (__msg) {
#define PIPE_MSG_HANDLE(__msg, __func)       \
  case __msg:                                \
    _result = __func(__msg, wParam, lParam); \
    break;

#define END_MAP_PIPE_MSG_HANDLE(__result) \
  }                                       \
  __result = _result;                     \
  }

template <typename _Resp>
void ServerImpl::HandlePipeMessage(PipeMessage pipe_msg, _Resp resp) {
  DWORD result;

  MAP_PIPE_MSG_HANDLE(pipe_msg.Msg, pipe_msg.wParam, pipe_msg.lParam)
  PIPE_MSG_HANDLE(WEASEL_IPC_ECHO, OnEcho)
  PIPE_MSG_HANDLE(WEASEL_IPC_START_SESSION, OnStartSession)
  PIPE_MSG_HANDLE(WEASEL_IPC_END_SESSION, OnEndSession)
  PIPE_MSG_HANDLE(WEASEL_IPC_PROCESS_KEY_EVENT, OnKeyEvent)
  PIPE_MSG_HANDLE(WEASEL_IPC_SHUTDOWN_SERVER, OnShutdownServer)
  PIPE_MSG_HANDLE(WEASEL_IPC_FOCUS_IN, OnFocusIn)
  PIPE_MSG_HANDLE(WEASEL_IPC_FOCUS_OUT, OnFocusOut)
  PIPE_MSG_HANDLE(WEASEL_IPC_UPDATE_INPUT_POS, OnUpdateInputPosition)
  PIPE_MSG_HANDLE(WEASEL_IPC_START_MAINTENANCE, OnStartMaintenance)
  PIPE_MSG_HANDLE(WEASEL_IPC_END_MAINTENANCE, OnEndMaintenance)
  PIPE_MSG_HANDLE(WEASEL_IPC_COMMIT_COMPOSITION, OnCommitComposition)
  PIPE_MSG_HANDLE(WEASEL_IPC_CLEAR_COMPOSITION, OnClearComposition);
  PIPE_MSG_HANDLE(WEASEL_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE,
                  OnSelectCandidateOnCurrentPage);
  PIPE_MSG_HANDLE(WEASEL_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
                  OnHighlightCandidateOnCurrentPage);
  PIPE_MSG_HANDLE(WEASEL_IPC_CHANGE_PAGE, OnChangePage);
  PIPE_MSG_HANDLE(WEASEL_IPC_TRAY_COMMAND, OnCommand);
  END_MAP_PIPE_MSG_HANDLE(result);

  resp(result);
}

PipeServer::PipeServer(std::wstring&& pn_cmd,
                       SecurityAttribute& security)
    : PipeChannel(std::move(pn_cmd), security.get_attr()),
      security(security) {}

void PipeServer::Listen(ServerHandler const& handler) {
  for (;;) {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    try {
      boost::this_thread::interruption_point();
      pipe = _ConnectServerPipe(pname);
      if (!security.IsCurrentUserClient(pipe)) {
        _FinalizePipe(pipe);
        continue;
      }
      boost::thread th(
          [&handler, pipe, this] { _ProcessPipeThread(pipe, handler); });
    } catch (DWORD ex) {
      _FinalizePipe(pipe);
    }
    boost::this_thread::interruption_point();
  }
}

PipeServer::ServerRunner PipeServer::GetServerRunner(
    ServerHandler const& handler) {
  return [&handler, this]() { Listen(handler); };
}

void PipeServer::_ProcessPipeThread(HANDLE pipe, ServerHandler const& handler) {
  try {
    for (;;) {
      Res msg;
      _Receive(pipe, &msg, sizeof(msg));
      handler(msg, [this, pipe](Msg resp) { _Send(pipe, resp); });
    }
  } catch (...) {
    _FinalizePipe(pipe);
  }
}

// weasel::Server

Server::Server() : m_pImpl(new ServerImpl) {}

Server::~Server() {
  if (m_pImpl)
    delete m_pImpl;
}

HWND Server::Start() {
  return m_pImpl->Start();
}

int Server::Stop() {
  return m_pImpl->Stop();
}

int Server::Run() {
  return m_pImpl->Run();
}

bool Server::RunOnServerThread(std::function<void()> task, DWORD timeout_ms) {
  return m_pImpl->RunOnServerThread(std::move(task), timeout_ms);
}

void Server::SetRequestHandler(RequestHandler* pHandler) {
  m_pImpl->SetRequestHandler(pHandler);
}

void Server::AddMenuHandler(UINT uID, CommandHandler handler) {
  m_pImpl->AddMenuHandler(uID, handler);
}

HWND Server::GetHWnd() {
  return m_pImpl->m_hWnd;
}
