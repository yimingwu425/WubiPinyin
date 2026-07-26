#pragma once

#include <winrt/WubiPinyinSettings.h>
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

namespace winrt::WubiPinyinSettings::factory_implementation {

struct App : AppT<App, implementation::App> {};

}  // namespace winrt::WubiPinyinSettings::factory_implementation
