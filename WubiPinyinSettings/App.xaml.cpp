#include "App.xaml.h"

#include "MainWindow.xaml.h"
#include <shellapi.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <cwchar>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::WubiPinyinSettings::implementation {
namespace {

hstring InitialPageFromCommandLine() {
  int argument_count = 0;
  LPWSTR* arguments = ::CommandLineToArgvW(::GetCommandLineW(),
                                            &argument_count);
  if (!arguments) {
    return L"input";
  }

  bool open_dictionary = false;
  for (int index = 1; index + 1 < argument_count; ++index) {
    if (_wcsicmp(arguments[index], L"--page") == 0 &&
        _wcsicmp(arguments[index + 1], L"dictionary") == 0) {
      open_dictionary = true;
      break;
    }
  }
  ::LocalFree(arguments);
  return open_dictionary ? L"dictionary" : L"input";
}

}  // namespace

App::App() {
  InitializeComponent();
}

void App::OnLaunched(LaunchActivatedEventArgs const&) {
  m_window = make<MainWindow>(InitialPageFromCommandLine());
  m_window.Activate();
}

}  // namespace winrt::WubiPinyinSettings::implementation
