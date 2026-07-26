#pragma once

#include "resource.h"
#include <resource.h>
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <RimeWithWeasel.h>
#include <WeaselConstants.h>
#include <WeaselUtility.h>
#include "../WubiPinyinCore/WubiPinyinCore.h"
#include "../WubiPinyinCore/WubiPinyinControlPayload.h"
#include <filesystem>
#include <functional>
#include <memory>

#include "WeaselTrayIcon.h"
#include "BrokerControlServer.h"

namespace fs = std::filesystem;

class WeaselServerApp {
 public:
  static bool execute(const fs::path& cmd, const std::wstring& args) {
    return (uintptr_t)ShellExecuteW(NULL, NULL, cmd.c_str(), args.c_str(), NULL,
                                    SW_SHOWNORMAL) > 32;
  }

  static bool explore(const fs::path& path) {
    std::wstring quoted_path(L"\"" + path.wstring() + L"\"");
    return (uintptr_t)ShellExecuteW(NULL, L"explore", quoted_path.c_str(), NULL,
                                    NULL, SW_SHOWNORMAL) > 32;
  }

  static bool open(const fs::path& path) {
    return (uintptr_t)ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL,
                                    SW_SHOWNORMAL) > 32;
  }

  static fs::path install_dir() {
    WCHAR exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(GetModuleHandle(NULL), exe_path, _countof(exe_path));
    return fs::path(exe_path).remove_filename();
  }

 public:
  WeaselServerApp();
  ~WeaselServerApp();
  int Run();

 protected:
  void SetupMenuHandlers();
  void InitializeSettingsStore();
  void StartControlServer();
  void StopControlServer();
  bool ApplySettings(const wubipinyin::SettingsSnapshot& settings,
                     std::string* error);
  bool SetRoute(std::uint64_t session_id,
                wubipinyin::HybridRoute route,
                std::string* error);
  bool CommitRaw(std::uint64_t session_id, std::string* error);
  bool ReloadDictionaries(std::string* error);
  bool ResetLearning(std::string* error);
  bool RunControlOperation(std::function<bool()> operation,
                           std::string* error);

  weasel::Server m_server;
  weasel::UI m_ui;
  WeaselTrayIcon tray_icon;
  std::unique_ptr<RimeWithWeaselHandler> m_handler;
  wubipinyin::SettingsStore m_settings_store;
  std::unique_ptr<wubipinyin::BrokerControlDispatcher> m_control_dispatcher;
  std::unique_ptr<BrokerControlServer> m_control_server;
};
