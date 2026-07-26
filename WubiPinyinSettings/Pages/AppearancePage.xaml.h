#pragma once

#include "Pages/AppearancePage.xaml.g.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>

namespace winrt::WubiPinyinSettings::implementation {

struct AppearancePage : AppearancePageT<AppearancePage> {
  AppearancePage();

  void Page_Loaded(IInspectable const& sender,
                   Microsoft::UI::Xaml::RoutedEventArgs const& args);
  void Theme_Changed(
      IInspectable const& sender,
      Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
  void SourceLabels_Toggled(
      IInspectable const& sender,
      IInspectable const& args);

 private:
  void Populate();
  void Save();
  void SetStatus(bool success, std::wstring const& message);

  bool m_loading = true;
};

}  // namespace winrt::WubiPinyinSettings::implementation

namespace winrt::WubiPinyinSettings::factory_implementation {

struct AppearancePage : AppearancePageT<AppearancePage,
                                        implementation::AppearancePage> {};

}  // namespace winrt::WubiPinyinSettings::factory_implementation
