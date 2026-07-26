#pragma once

#include "Pages/InputPage.xaml.g.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace winrt::WubiPinyinSettings::implementation {

struct InputPage : InputPageT<InputPage> {
  InputPage();

  void Page_Loaded(IInspectable const& sender,
                   Microsoft::UI::Xaml::RoutedEventArgs const& args);
  void Settings_Changed(
      IInspectable const& sender,
      Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

 private:
  void Populate();
  void Save();
  void SetStatus(bool success, std::wstring const& message);

  bool m_loading = true;
};

}  // namespace winrt::WubiPinyinSettings::implementation

namespace winrt::WubiPinyinSettings::factory_implementation {

struct InputPage : InputPageT<InputPage, implementation::InputPage> {};

}  // namespace winrt::WubiPinyinSettings::factory_implementation
