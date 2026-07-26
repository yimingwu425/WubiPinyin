#include "MainWindow.xaml.h"

#include "Pages/AboutPage.xaml.h"
#include "Pages/AppearancePage.xaml.h"
#include "Pages/DictionaryPage.xaml.h"
#include "Pages/InputPage.xaml.h"
#include "Pages/LearningPage.xaml.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;

namespace winrt::WubiPinyinSettings::implementation {

MainWindow::MainWindow() : MainWindow(L"input") {}

MainWindow::MainWindow(hstring const& initial_page) {
  InitializeComponent();
  SystemBackdrop(MicaBackdrop{});
  ExtendsContentIntoTitleBar(true);
  SetTitleBar(AppTitleBar());
  SelectInitialPage(initial_page);
}

void MainWindow::NavigationRoot_SelectionChanged(
    NavigationView const&,
    NavigationViewSelectionChangedEventArgs const& args) {
  const auto item = args.SelectedItem().try_as<NavigationViewItem>();
  if (item) {
    NavigateTo(unbox_value<hstring>(item.Tag()));
  }
}

void MainWindow::SelectInitialPage(hstring const& tag) {
  const hstring selected_tag = tag == L"dictionary" ? tag : L"input";
  const auto items = NavigationRoot().MenuItems();
  for (uint32_t index = 0; index < items.Size(); ++index) {
    const auto item = items.GetAt(index).try_as<NavigationViewItem>();
    if (item && unbox_value<hstring>(item.Tag()) == selected_tag) {
      NavigationRoot().SelectedItem(item);
      NavigateTo(selected_tag);
      return;
    }
  }
  NavigateTo(L"input");
}

void MainWindow::NavigateTo(hstring const& tag) {
  if (tag == L"input") {
    ContentFrame().Navigate(
        xaml_typename<winrt::WubiPinyinSettings::InputPage>());
  } else if (tag == L"appearance") {
    ContentFrame().Navigate(
        xaml_typename<winrt::WubiPinyinSettings::AppearancePage>());
  } else if (tag == L"dictionary") {
    ContentFrame().Navigate(
        xaml_typename<winrt::WubiPinyinSettings::DictionaryPage>());
  } else if (tag == L"learning") {
    ContentFrame().Navigate(
        xaml_typename<winrt::WubiPinyinSettings::LearningPage>());
  } else if (tag == L"about") {
    ContentFrame().Navigate(
        xaml_typename<winrt::WubiPinyinSettings::AboutPage>());
  }
}

}  // namespace winrt::WubiPinyinSettings::implementation
