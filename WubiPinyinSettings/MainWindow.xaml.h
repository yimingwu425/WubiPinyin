#pragma once

#include "MainWindow.g.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace winrt::WubiPinyinSettings::implementation {

struct MainWindow : MainWindowT<MainWindow> {
  MainWindow();
  explicit MainWindow(winrt::hstring const& initial_page);

  void NavigationRoot_SelectionChanged(
      IInspectable const& sender,
      Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs
          const& args);

 private:
  void SelectInitialPage(winrt::hstring const& tag);
  void NavigateTo(winrt::hstring const& tag);
};

}  // namespace winrt::WubiPinyinSettings::implementation

namespace winrt::WubiPinyinSettings::factory_implementation {

struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};

}  // namespace winrt::WubiPinyinSettings::factory_implementation
