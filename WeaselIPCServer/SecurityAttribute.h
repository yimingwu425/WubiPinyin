#pragma once
#include <winnt.h>   // for security attributes constants
#include <aclapi.h>  // for ACL
#include <string>

namespace weasel {
class SecurityAttribute {
 private:
  PSECURITY_DESCRIPTOR pd;
  SECURITY_ATTRIBUTES sa;
  std::wstring user_sid;
  DWORD session_id;
  void _Init();

 public:
  SecurityAttribute();
  ~SecurityAttribute();

  SecurityAttribute(const SecurityAttribute&) = delete;
  SecurityAttribute& operator=(const SecurityAttribute&) = delete;

  bool valid() const { return pd != NULL; }
  bool IsCurrentUserClient(HANDLE pipe) const;
  SECURITY_ATTRIBUTES* get_attr();
};
};  // namespace weasel
