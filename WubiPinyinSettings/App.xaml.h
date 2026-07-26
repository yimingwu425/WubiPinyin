#pragma once

#include "App.xaml.g.h"
#include <winrt/Microsoft.UI.Xaml.h>

namespace winrt::WubiPinyinSettings::implementation {

struct App : AppT<App> {
  App();

 void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const& args);

 private:
  Microsoft::UI::Xaml::Window m_window{nullptr};
};

}  // namespace winrt::WubiPinyinSettings::implementation
