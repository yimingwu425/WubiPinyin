// WeaselSetup.cpp : main source file for WeaselSetup.exe
//

#include "stdafx.h"

#include "resource.h"
#include "WeaselUtility.h"
#include <thread>

#include "InstallOptionsDlg.h"

#include <ShellScalingApi.h>
#pragma comment(lib, "Shcore.lib")
CAppModule _Module;

static int Run(LPTSTR lpCmdLine);
static bool IsProcAdmin();
static int RestartAsAdmin(LPTSTR lpCmdLine);

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR lpstrCmdLine,
                     int /*nCmdShow*/) {
  HRESULT hRes = ::CoInitialize(NULL);
  ATLASSERT(SUCCEEDED(hRes));

  AtlInitCommonControls(
      ICC_BAR_CLASSES);  // add flags to support other controls

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  LANGID langId = get_language_id();
  SetThreadUILanguage(langId);
  SetThreadLocale(langId);

  int nRet = Run(lpstrCmdLine);

  _Module.Term();
  ::CoUninitialize();

  return nRet;
}
int install(bool hant, bool silent);
int uninstall(bool silent);
bool has_installed();

static std::wstring install_dir() {
  WCHAR exe_path[MAX_PATH] = {0};
  GetModuleFileNameW(GetModuleHandle(NULL), exe_path, _countof(exe_path));
  std::wstring dir(exe_path);
  size_t pos = dir.find_last_of(L"\\");
  dir.resize(pos);
  return dir;
}

static int CustomInstall(bool installing) {
  bool hant = false;
  bool silent = false;

  const WCHAR* KEY = WEASEL_REG_KEY;
  HKEY hKey;
  LSTATUS ret = RegOpenKey(HKEY_CURRENT_USER, KEY, &hKey);
  if (ret == ERROR_SUCCESS) {
    DWORD type = 0;
    DWORD data = 0;
    DWORD len = sizeof(data);
    ret = RegQueryValueEx(hKey, L"Hant", NULL, &type, (LPBYTE)&data, &len);
    if (ret == ERROR_SUCCESS && type == REG_DWORD) {
      hant = (data != 0);
      if (installing)
        silent = true;
    }
    RegCloseKey(hKey);
  }
  bool _has_installed = has_installed();
  if (!silent) {
    InstallOptionsDialog dlg;
    dlg.installed = _has_installed;
    dlg.hant = hant;
    if (IDOK != dlg.DoModal()) {
      if (!installing)
        return 1;  // aborted by user
    } else {
      hant = dlg.hant;
      _has_installed = dlg.installed;
    }
  }
  if (!_has_installed)
    if (0 != install(hant, silent))
      return 1;

  ret = SetRegKeyValue(HKEY_CURRENT_USER, KEY, L"Hant", 0,
                       REG_DWORD, false);
  if (FAILED(HRESULT_FROM_WIN32(ret))) {
    MSG_BY_IDS(IDS_STR_ERR_WRITE_HANT, IDS_STR_INSTALL_FAILED,
               MB_ICONERROR | MB_OK);
    return 1;
  }
  if (_has_installed) {
    std::wstring dir(install_dir());
    std::thread th([dir]() {
      ShellExecuteW(NULL, NULL,
                    (dir + L"\\" + WUBIPINYIN_SERVER_EXECUTABLE).c_str(),
                    L"/q", NULL, SW_SHOWNORMAL);
      Sleep(500);
      ShellExecuteW(NULL, NULL,
                    (dir + L"\\" + WUBIPINYIN_SERVER_EXECUTABLE).c_str(),
                    L"", NULL, SW_SHOWNORMAL);
      Sleep(500);
      ShellExecuteW(NULL, NULL,
                    (dir + L"\\" + WUBIPINYIN_DEPLOYER_EXECUTABLE).c_str(),
                    L"/deploy", NULL, SW_SHOWNORMAL);
    });
    th.detach();
    MSG_BY_IDS(IDS_STR_MODIFY_SUCCESS_INFO, IDS_STR_MODIFY_SUCCESS_CAP,
               MB_ICONINFORMATION | MB_OK);
  }

  return 0;
}

static int Run(LPTSTR lpCmdLine) {
  constexpr bool silent = true;
  // parameter /? or /help to show commandline args
  if (!wcscmp(L"/?", lpCmdLine) || !wcscmp(L"/help", lpCmdLine)) {
    WCHAR msg[1024] = {0};
    if (LoadString(GetModuleHandle(NULL), IDS_STR_HELP, msg,
                   sizeof(msg) / sizeof(TCHAR))) {
      MessageBox(NULL, msg, WUBIPINYIN_PRODUCT_NAME,
                 MB_ICONINFORMATION | MB_OK);
    } else {
      MessageBox(
          NULL,
          L"Usage: WubiPinyinSetup.exe [options]\n"
          L"/? or /help    - Show this help message\n"
          L"/u             - Uninstall WubiPinyin\n"
          L"/i             - Install WubiPinyin\n"
          L"/s             - Install WubiPinyin\n"
          L"/toggleime     - Toggle IME on open/close(ctrl+space)\n"
          L"/toggleascii   - Toggle ASCII on open/close(ctrl+space)\n",
          WUBIPINYIN_PRODUCT_NAME, MB_ICONINFORMATION | MB_OK);
    }
    return 0;
  }
  bool uninstalling = !wcscmp(L"/u", lpCmdLine);
  if (uninstalling) {
    if (IsProcAdmin())
      return uninstall(silent);
    else
      return RestartAsAdmin(lpCmdLine);
  }

  if (!wcscmp(L"/ls", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, WEASEL_REG_KEY,
                          L"Language", L"chs", REG_SZ);
  } else if (!wcscmp(L"/lt", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, WEASEL_REG_KEY,
                          L"Language", L"cht", REG_SZ);
  } else if (!wcscmp(L"/le", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, WEASEL_REG_KEY,
                          L"Language", L"eng", REG_SZ);
  }

  if (!wcscmp(L"/toggleime", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, WEASEL_REG_KEY,
                          L"ToggleImeOnOpenClose", L"yes", REG_SZ);
  }
  if (!wcscmp(L"/toggleascii", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, WEASEL_REG_KEY,
                          L"ToggleImeOnOpenClose", L"no", REG_SZ);
  }
  if (!IsProcAdmin()) {
    return RestartAsAdmin(lpCmdLine);
  }

  bool hans = !wcscmp(L"/s", lpCmdLine);
  if (hans)
    return install(false, silent);
  if (!wcscmp(L"/t", lpCmdLine))
    return install(false, silent);
  bool installing = !wcscmp(L"/i", lpCmdLine);
  return CustomInstall(installing);
}

// https://learn.microsoft.com/zh-cn/windows/win32/api/securitybaseapi/nf-securitybaseapi-checktokenmembership
bool IsProcAdmin() {
  BOOL b = FALSE;
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  PSID AdministratorsGroup;
  b = AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &AdministratorsGroup);

  if (b) {
    if (!CheckTokenMembership(NULL, AdministratorsGroup, &b)) {
      b = FALSE;
    }
    FreeSid(AdministratorsGroup);
  }

  return (b);
}

int RestartAsAdmin(LPTSTR lpCmdLine) {
  SHELLEXECUTEINFO execInfo{0};
  TCHAR path[MAX_PATH];
  GetModuleFileName(GetModuleHandle(NULL), path, _countof(path));
  execInfo.lpFile = path;
  execInfo.lpParameters = lpCmdLine;
  execInfo.lpVerb = _T("runas");
  execInfo.cbSize = sizeof(execInfo);
  execInfo.nShow = SW_SHOWNORMAL;
  execInfo.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
  execInfo.hwnd = NULL;
  execInfo.hProcess = NULL;
  if (::ShellExecuteEx(&execInfo) && execInfo.hProcess != NULL) {
    ::WaitForSingleObject(execInfo.hProcess, INFINITE);
    DWORD dwExitCode = 0;
    ::GetExitCodeProcess(execInfo.hProcess, &dwExitCode);
    ::CloseHandle(execInfo.hProcess);
    return dwExitCode;
  }
  return -1;
}
