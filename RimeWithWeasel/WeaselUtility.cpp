#include "stdafx.h"
#include <filesystem>
#include <string>
#include <WeaselUtility.h>

fs::path WeaselUserDataPath() {
  WCHAR _path[MAX_PATH] = {0};
  // Keep settings and learning in one fixed, product-owned location. Do not
  // accept a user-writable registry path in a process the installer can launch
  // elevated.
  ExpandEnvironmentStringsW(L"%AppData%\\WubiPinyin", _path,
                            _countof(_path));
  return fs::path(_path);
}

fs::path WeaselSharedDataPath() {
  wchar_t _path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, _path, _countof(_path));
  return fs::path(_path).remove_filename().append("data");
}

std::string GetCustomResource(const char* name, const char* type) {
  const HINSTANCE module = 0;  // main executable
  HRSRC hRes = FindResourceA(module, name, type);
  if (hRes) {
    HGLOBAL hData = LoadResource(module, hRes);
    if (hData) {
      const char* data = (const char*)::LockResource(hData);
      size_t size = ::SizeofResource(module, hRes);

      if (data && size) {
        if (data[size - 1] == '\0')  // null-terminated string
          size--;
        return std::string(data, size);
      }
    }
  }

  return std::string();
}
