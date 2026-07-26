#pragma once

#include "Pages/LearningPage.xaml.g.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>

namespace winrt::WubiPinyinSettings::implementation {

struct LearningPage : LearningPageT<LearningPage> {
  LearningPage();

  void Page_Loaded(IInspectable const& sender,
                   Microsoft::UI::Xaml::RoutedEventArgs const& args);
  void Settings_Changed(
      IInspectable const& sender,
      IInspectable const& args);
  winrt::fire_and_forget ResetLearning_Click(
      IInspectable const& sender,
      Microsoft::UI::Xaml::RoutedEventArgs const& args);

 private:
  void Populate();
  void Save();
  void SetStatus(bool success, std::wstring const& message);

  bool m_loading = true;
};

}  // namespace winrt::WubiPinyinSettings::implementation

namespace winrt::WubiPinyinSettings::factory_implementation {

struct LearningPage : LearningPageT<LearningPage, implementation::LearningPage> {};

}  // namespace winrt::WubiPinyinSettings::factory_implementation
