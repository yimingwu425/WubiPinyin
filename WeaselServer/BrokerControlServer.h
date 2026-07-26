#pragma once

#include <functional>
#include <string>
#include <thread>

#include <windows.h>

#include "../WeaselIPCServer/SecurityAttribute.h"
#include "../WubiPinyinCore/WubiPinyinControlProtocol.h"

// The control pipe is deliberately separate from the TSF key pipe. It is
// owned by the Broker and handles settings/lexicon requests only.
class BrokerControlServer {
 public:
  using Dispatch = std::function<bool(const wubipinyin::ControlFrame& request,
                                      wubipinyin::ControlFrame* response,
                                      std::string* error)>;

  explicit BrokerControlServer(Dispatch dispatch);
  ~BrokerControlServer();

  BrokerControlServer(const BrokerControlServer&) = delete;
  BrokerControlServer& operator=(const BrokerControlServer&) = delete;

  bool Start();
  void Stop();

 private:
  void Listen();
  bool Connect(HANDLE pipe);
  bool ReadFrame(HANDLE pipe, wubipinyin::ControlFrame* frame);
  bool WriteFrame(HANDLE pipe, const wubipinyin::ControlFrame& frame);
  bool WaitForIo(HANDLE pipe,
                 OVERLAPPED* overlapped,
                 DWORD timeout_ms,
                 DWORD* transferred);
  void ProcessConnection(HANDLE pipe);
  bool IsStopping() const;

  weasel::SecurityAttribute security_;
  Dispatch dispatch_;
  HANDLE stop_event_ = NULL;
  std::thread listener_;
};
