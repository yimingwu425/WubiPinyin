#include "stdafx.h"
#include "SecurityAttribute.h"
#include <Sddl.h>
#include <string>
#include <vector>

#ifndef SDDL_ALL_APP_PACKAGES
#define SDDL_ALL_APP_PACKAGES TEXT("AC")
#endif

namespace {

std::wstring UserSidForToken(HANDLE token) {
  DWORD size = 0;
  if (::GetTokenInformation(token, TokenUser, NULL, 0, &size) ||
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) {
    return L"";
  }

  std::vector<BYTE> buffer(size);
  if (!::GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
    return L"";
  }

  const TOKEN_USER* user =
      reinterpret_cast<const TOKEN_USER*>(buffer.data());
  LPWSTR sid = NULL;
  if (!::ConvertSidToStringSidW(user->User.Sid, &sid)) {
    return L"";
  }

  std::wstring result(sid);
  ::LocalFree(sid);
  return result;
}

std::wstring CurrentUserSid() {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
    return L"";
  }

  std::wstring sid = UserSidForToken(token);
  ::CloseHandle(token);
  return sid;
}

}  // namespace

namespace weasel {

SecurityAttribute::SecurityAttribute() : pd(NULL), sa{}, session_id(0) {
  _Init();
}

SecurityAttribute::~SecurityAttribute() {
  if (pd != NULL) {
    ::LocalFree(pd);
  }
}

void SecurityAttribute::_Init() {
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = FALSE;

  user_sid = CurrentUserSid();
  if (user_sid.empty() ||
      !::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id)) {
    return;
  }

  // UWP hosts need the All App Packages ACE. Both client ACEs are restricted
  // to read/write access; only the server owns the pipe object.
  std::wstring sddl = L"D:(A;;GA;;;SY)(A;;GRGW;;;";
  sddl += user_sid;
  sddl += L")(A;;GRGW;;;" SDDL_ALL_APP_PACKAGES
          L")S:(ML;;NW;;;LW)";
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(
          sddl.c_str(), SDDL_REVISION_1, &pd, NULL)) {
    pd = NULL;
    return;
  }
  sa.lpSecurityDescriptor = pd;
}

bool SecurityAttribute::IsCurrentUserClient(HANDLE pipe) const {
  if (!valid() || user_sid.empty()) {
    return false;
  }

  ULONG process_id = 0;
  if (!::GetNamedPipeClientProcessId(pipe, &process_id) || process_id == 0) {
    return false;
  }

  DWORD client_session_id = 0;
  if (!::ProcessIdToSessionId(process_id, &client_session_id) ||
      client_session_id != session_id) {
    return false;
  }

  HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 process_id);
  if (process == NULL) {
    return false;
  }

  HANDLE token = NULL;
  const BOOL opened = ::OpenProcessToken(process, TOKEN_QUERY, &token);
  ::CloseHandle(process);
  if (!opened) {
    return false;
  }

  const std::wstring client_sid = UserSidForToken(token);
  ::CloseHandle(token);
  return client_sid == user_sid;
}

SECURITY_ATTRIBUTES* SecurityAttribute::get_attr() {
  return valid() ? &sa : NULL;
}
};  // namespace weasel
