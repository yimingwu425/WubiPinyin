#pragma once
#include <WeaselIPCData.h>
#include <WeaselUtility.h>
#include <windows.h>
#include <Sddl.h>
#include <functional>
#include <memory>
#include <vector>
#include <KeyEvent.h>

#define WEASEL_IPC_WINDOW L"WeaselIPCWindow_1.0"
#define WEASEL_IPC_PIPE_NAME L"WubiPinyinControl"

#define WEASEL_IPC_METADATA_SIZE 1024
#define WEASEL_IPC_BUFFER_SIZE (4 * 1024)
#define WEASEL_IPC_BUFFER_LENGTH (WEASEL_IPC_BUFFER_SIZE / sizeof(WCHAR))
#define WEASEL_IPC_SHARED_MEMORY_SIZE \
  (sizeof(PipeMessage) + WEASEL_IPC_BUFFER_SIZE)

constexpr UINT WEASEL_IPC_SERVER_TASK_MESSAGE = WM_APP + 0x470;

enum WEASEL_IPC_COMMAND {
  WEASEL_IPC_ECHO = (WM_APP + 1),
  WEASEL_IPC_START_SESSION,
  WEASEL_IPC_END_SESSION,
  WEASEL_IPC_PROCESS_KEY_EVENT,
  WEASEL_IPC_SHUTDOWN_SERVER,
  WEASEL_IPC_FOCUS_IN,
  WEASEL_IPC_FOCUS_OUT,
  WEASEL_IPC_UPDATE_INPUT_POS,
  WEASEL_IPC_START_MAINTENANCE,
  WEASEL_IPC_END_MAINTENANCE,
  WEASEL_IPC_COMMIT_COMPOSITION,
  WEASEL_IPC_CLEAR_COMPOSITION,
  WEASEL_IPC_TRAY_COMMAND,
  WEASEL_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_CHANGE_PAGE,
  WEASEL_IPC_LAST_COMMAND
};

namespace weasel {
struct PipeMessage {
  WEASEL_IPC_COMMAND Msg;
  DWORD wParam;
  DWORD lParam;
};

struct IPCMetadata {
  enum { WINDOW_CLASS_LENGTH = 64 };
  UINT32 server_hwnd;
  WCHAR server_window_class[WINDOW_CLASS_LENGTH];
};

// 處理請求之物件
struct RequestHandler {
  using EatLine = std::function<bool(std::wstring&)>;
  RequestHandler() {}
  virtual ~RequestHandler() {}
  virtual void Initialize() {}
  virtual void Finalize() {}
  virtual DWORD FindSession(DWORD session_id) { return 0; }
  virtual DWORD AddSession(LPWSTR buffer, EatLine eat = 0) { return 0; }
  virtual DWORD RemoveSession(DWORD session_id) { return 0; }
  virtual BOOL ProcessKeyEvent(KeyEvent keyEvent,
                               DWORD session_id,
                               EatLine eat) {
    return FALSE;
  }
  virtual void CommitComposition(DWORD session_id) {}
  virtual void ClearComposition(DWORD session_id) {}
  virtual void SelectCandidateOnCurrentPage(size_t index, DWORD session_id) {}
  virtual bool HighlightCandidateOnCurrentPage(size_t index,
                                               DWORD session_id,
                                               EatLine eat) {
    return false;
  }
  virtual bool ChangePage(bool backward, DWORD session_id, EatLine eat) {
    return false;
  }
  virtual void FocusIn(DWORD param, DWORD session_id) {}
  virtual void FocusOut(DWORD param, DWORD session_id) {}
  virtual void UpdateInputPosition(RECT const& rc, DWORD session_id) {}
  virtual void StartMaintenance() {}
  virtual void EndMaintenance() {}
  virtual void SetOption(DWORD session_id, const std::string& opt, bool val) {}
  virtual void UpdateColorTheme(BOOL darkMode) {}
};

// 處理server端回應之物件
typedef std::function<bool(LPWSTR buffer, DWORD length)> ResponseHandler;

// 事件處理函數
typedef std::function<bool()> CommandHandler;

// 啟動服務進程之物件
typedef CommandHandler ServerLauncher;

// IPC實現類聲明

class ClientImpl;
class ServerImpl;

// IPC接口類

class Client {
 public:
  Client();
  virtual ~Client();

  // 连接到服务，必要时启动服务进程
  bool Connect(ServerLauncher launcher = 0);
  // 断开连接
  void Disconnect();
  // 终止服务
  void ShutdownServer();
  // 發起會話
  void StartSession();
  // 結束會話
  void EndSession();
  // 進入維護模式
  void StartMaintenance();
  // 退出維護模式
  void EndMaintenance();
  // 测试连接
  bool Echo();
  // 请求服务处理按键消息
  bool ProcessKeyEvent(KeyEvent const& keyEvent);
  // 上屏正在編輯的文字
  bool CommitComposition();
  // 清除正在編輯的文字
  bool ClearComposition();
  // 选择当前页面编号为index的候选
  bool SelectCandidateOnCurrentPage(size_t index);
  // 高亮当前页面编号为index的候选
  bool HighlightCandidateOnCurrentPage(size_t index);
  // 翻页，backward = true 向前翻，false向后翻
  bool ChangePage(bool backward);
  // 更新输入位置
  void UpdateInputPosition(RECT const& rc);
  // 输入窗口获得焦点
  void FocusIn();
  // 输入窗口失去焦点
  void FocusOut();
  // 托盤菜單
  void TrayCommand(UINT menuId);
  // 读取server返回的数据
  bool GetResponseData(ResponseHandler handler);

 private:
  ClientImpl* m_pImpl;
};

class Server {
 public:
  Server();
  virtual ~Server();

  // 初始化服务
  HWND Start();
  // 结束服务
  int Stop();
  // 消息循环
  int Run();

  // Runs a short control-plane operation on the server window thread. The
  // caller owns any result state captured by `task`; a timeout never blocks
  // TSF key processing.
  bool RunOnServerThread(std::function<void()> task,
                         DWORD timeout_ms = 1000);

  void SetRequestHandler(RequestHandler* pHandler);
  void AddMenuHandler(UINT uID, CommandHandler handler);
  HWND GetHWnd();

 private:
  ServerImpl* m_pImpl;
};

inline std::wstring GetPipeUserSid() {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
    return L"";
  }

  DWORD token_user_size = 0;
  const BOOL initial_query =
      ::GetTokenInformation(token, TokenUser, NULL, 0, &token_user_size);
  if (initial_query || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
      token_user_size == 0) {
    ::CloseHandle(token);
    return L"";
  }

  std::vector<BYTE> token_user(token_user_size);
  if (!::GetTokenInformation(token, TokenUser, token_user.data(),
                             token_user_size, &token_user_size)) {
    ::CloseHandle(token);
    return L"";
  }
  ::CloseHandle(token);

  const TOKEN_USER* user =
      reinterpret_cast<const TOKEN_USER*>(token_user.data());
  LPWSTR sid = NULL;
  if (!::ConvertSidToStringSidW(user->User.Sid, &sid)) {
    return L"";
  }

  std::wstring value(sid);
  ::LocalFree(sid);
  return value;
}

inline std::wstring GetUserSessionPipeName(const wchar_t* channel_name) {
  const std::wstring sid = GetPipeUserSid();
  DWORD session_id = 0;
  ::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id);

  std::wstring pipe_name = L"\\\\.\\pipe\\";
  pipe_name += channel_name;
  pipe_name += L"\\";
  if (!sid.empty()) {
    pipe_name += sid;
  } else {
    // The normal path always uses the SID. Keep a stable per-user fallback so
    // a token-query failure does not make the server spin on an invalid name.
    const std::wstring username = getUsername();
    pipe_name += username.empty() ? L"unknown" : username;
  }
  pipe_name += L"\\";
  pipe_name += std::to_wstring(session_id);
  return pipe_name;
}

inline std::wstring GetPipeName() {
  return GetUserSessionPipeName(WEASEL_IPC_PIPE_NAME);
}

inline std::wstring GetBrokerControlPipeName() {
  return GetUserSessionPipeName(L"WubiPinyinBrokerControlV1");
}
}  // namespace weasel
