#include "stdafx.h"
#include "WeaselServerApp.h"

#include <atomic>
#include <filesystem>
#include <limits>
#include <logging.h>

namespace {

std::string HybridRouteOption(wubipinyin::HybridRoute route) {
  switch (route) {
    case wubipinyin::HybridRoute::kWubi:
      return "wubi";
    case wubipinyin::HybridRoute::kPinyin:
      return "pinyin";
    case wubipinyin::HybridRoute::kAuto:
      return "auto";
  }
  return "auto";
}

}  // namespace

WeaselServerApp::WeaselServerApp()
    : m_handler(std::make_unique<RimeWithWeaselHandler>(&m_ui)),
      tray_icon(m_ui) {
  // m_handler.reset(new RimeWithWeaselHandler(&m_ui));
  m_server.SetRequestHandler(m_handler.get());
  SetupMenuHandlers();
}

WeaselServerApp::~WeaselServerApp() {}

int WeaselServerApp::Run() {
  InitializeSettingsStore();

  if (!m_server.Start())
    return -1;

  m_ui.Create(m_server.GetHWnd());

  m_handler->Initialize();
  m_handler->OnUpdateUI([this]() { tray_icon.Refresh(); });
  StartControlServer();

  tray_icon.Create(m_server.GetHWnd());
  tray_icon.Refresh();

  int ret = m_server.Run();

  StopControlServer();
  m_handler->Finalize();
  m_ui.Destroy();
  tray_icon.RemoveIcon();
  return ret;
}

void WeaselServerApp::InitializeSettingsStore() {
  std::string error;
  const std::filesystem::path user_data_dir = WeaselUserDataPath();
  if (!m_settings_store.Open(user_data_dir / L"wubipinyin.sqlite3", &error)) {
    LOG(WARNING) << "Unable to open WubiPinyin settings store: " << error;
    return;
  }

  if (!m_settings_store.MaterializeRimeDictionaries(user_data_dir, &error)) {
    LOG(WARNING) << "Unable to materialize WubiPinyin user dictionaries: "
                 << error;
  }

  const auto settings = m_settings_store.ReadSettings(&error);
  if (!settings) {
    LOG(WARNING) << "Unable to read WubiPinyin settings: " << error;
    return;
  }
  m_handler->SetDefaultHybridRoute(HybridRouteOption(settings->default_route));
  m_handler->SetLearningEnabled(settings->learning_enabled);
  m_handler->SetShowCandidateSourceLabels(settings->show_source_labels);
  m_handler->SetPasswordInputProtection(settings->password_input_protection);

  wubipinyin::BrokerControlCallbacks callbacks;
  callbacks.apply_settings = [this](const auto& updated,
                                    std::string* callback_error) {
    return ApplySettings(updated, callback_error);
  };
  callbacks.set_route = [this](std::uint64_t session_id,
                               wubipinyin::HybridRoute route) {
    return SetRoute(session_id, route, nullptr);
  };
  callbacks.commit_raw = [this](std::uint64_t session_id) {
    return CommitRaw(session_id, nullptr);
  };
  callbacks.reload_dictionaries = [this](std::string* callback_error) {
    return ReloadDictionaries(callback_error);
  };
  callbacks.reset_learning = [this](std::string* callback_error) {
    return ResetLearning(callback_error);
  };
  m_control_dispatcher =
      std::make_unique<wubipinyin::BrokerControlDispatcher>(
          &m_settings_store, user_data_dir, std::move(callbacks));
}

void WeaselServerApp::StartControlServer() {
  if (!m_control_dispatcher || m_control_server) {
    return;
  }
  m_control_server = std::make_unique<BrokerControlServer>(
      [this](const wubipinyin::ControlFrame& request,
             wubipinyin::ControlFrame* response,
             std::string* error) {
        return m_control_dispatcher &&
               m_control_dispatcher->Dispatch(request, response, error);
      });
  if (!m_control_server->Start()) {
    LOG(WARNING) << "Unable to start the WubiPinyin control pipe.";
    m_control_server.reset();
  }
}

void WeaselServerApp::StopControlServer() {
  if (m_control_server) {
    m_control_server->Stop();
    m_control_server.reset();
  }
}

bool WeaselServerApp::RunControlOperation(std::function<bool()> operation,
                                          std::string* error) {
  auto result = std::make_shared<std::atomic_bool>(false);
  const bool completed = m_server.RunOnServerThread(
      [operation = std::move(operation), result] {
        try {
          result->store(operation(), std::memory_order_release);
        } catch (...) {
          result->store(false, std::memory_order_release);
        }
      });
  if (!completed) {
    if (error) {
      *error = "The Broker did not complete the control request in time";
    }
    return false;
  }
  if (!result->load(std::memory_order_acquire) && error && error->empty()) {
    *error = "The Broker rejected the control request";
  }
  return result->load(std::memory_order_acquire);
}

bool WeaselServerApp::ApplySettings(
    const wubipinyin::SettingsSnapshot& settings,
    std::string* error) {
  return RunControlOperation(
      [this, settings] {
        m_handler->SetDefaultHybridRoute(HybridRouteOption(settings.default_route));
        m_handler->SetLearningEnabled(settings.learning_enabled);
        m_handler->SetShowCandidateSourceLabels(settings.show_source_labels);
        m_handler->SetPasswordInputProtection(
            settings.password_input_protection);
        return true;
      },
      error);
}

bool WeaselServerApp::SetRoute(std::uint64_t session_id,
                               wubipinyin::HybridRoute route,
                               std::string* error) {
  if (session_id > std::numeric_limits<DWORD>::max()) {
    if (error) {
      *error = "The input session id is invalid";
    }
    return false;
  }
  return RunControlOperation(
      [this, session_id, route] {
        return m_handler->SetHybridRoute(
            static_cast<DWORD>(session_id), HybridRouteOption(route));
      },
      error);
}

bool WeaselServerApp::CommitRaw(std::uint64_t session_id, std::string* error) {
  if (session_id > std::numeric_limits<DWORD>::max()) {
    if (error) {
      *error = "The input session id is invalid";
    }
    return false;
  }
  return RunControlOperation(
      [this, session_id] {
        return m_handler->CommitRawInput(static_cast<DWORD>(session_id));
      },
      error);
}

bool WeaselServerApp::ReloadDictionaries(std::string* error) {
  return RunControlOperation(
      [this] {
        m_handler->StartMaintenance();
        m_handler->EndMaintenance();
        return true;
      },
      error);
}

bool WeaselServerApp::ResetLearning(std::string* error) {
  return RunControlOperation(
      [this] {
        m_handler->StartMaintenance();
        std::error_code remove_error;
        const auto user_data_dir = WeaselUserDataPath();
        std::filesystem::remove_all(user_data_dir / "hybrid_auto_wubi.userdb",
                                    remove_error);
        if (!remove_error) {
          std::filesystem::remove_all(
              user_data_dir / "hybrid_auto_pinyin.userdb", remove_error);
        }
        m_handler->EndMaintenance();
        return !remove_error;
      },
      error);
}

void WeaselServerApp::SetupMenuHandlers() {
  std::filesystem::path dir = install_dir();
  m_server.AddMenuHandler(ID_WEASELTRAY_QUIT,
                          [this] { return m_server.Stop() == 0; });
  m_server.AddMenuHandler(ID_WEASELTRAY_DEPLOY,
                          std::bind(execute, dir / WUBIPINYIN_DEPLOYER_EXECUTABLE,
                                    std::wstring(L"/deploy")));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_SETTINGS,
      std::bind(execute, dir / WUBIPINYIN_SETTINGS_EXECUTABLE,
                std::wstring()));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_DICT_MANAGEMENT,
      std::bind(execute, dir / WUBIPINYIN_SETTINGS_EXECUTABLE,
                std::wstring(L"--page dictionary")));
  m_server.AddMenuHandler(ID_WEASELTRAY_WIKI,
                          std::bind(open, L"https://rime.im/docs/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_HOMEPAGE,
                          std::bind(open, L"https://rime.im/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_FORUM,
                          std::bind(open, L"https://rime.im/discuss/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_INSTALLDIR, std::bind(explore, dir));
  m_server.AddMenuHandler(ID_WEASELTRAY_USERCONFIG,
                          std::bind(explore, WeaselUserDataPath()));
  m_server.AddMenuHandler(ID_WEASELTRAY_LOGDIR,
                          std::bind(explore, WeaselLogPath()));
}
