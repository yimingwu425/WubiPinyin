#include "Pages/InputPage.xaml.h"
#if __has_include("InputPage.g.cpp")
#include "InputPage.g.cpp"
#endif

#include "SettingsSession.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::WubiPinyinSettings::implementation {
namespace {

wubipinyin::HybridRoute RouteFromTag(hstring const& tag) {
  if (tag == L"wubi") {
    return wubipinyin::HybridRoute::kWubi;
  }
  if (tag == L"pinyin") {
    return wubipinyin::HybridRoute::kPinyin;
  }
  return wubipinyin::HybridRoute::kAuto;
}

hstring RouteTag(wubipinyin::HybridRoute route) {
  switch (route) {
    case wubipinyin::HybridRoute::kWubi:
      return L"wubi";
    case wubipinyin::HybridRoute::kPinyin:
      return L"pinyin";
    case wubipinyin::HybridRoute::kAuto:
      return L"auto";
  }
  return L"auto";
}

hstring SelectedTag(ComboBox const& box) {
  const auto item = box.SelectedItem().try_as<ComboBoxItem>();
  return item ? unbox_value<hstring>(item.Tag()) : hstring{};
}

void SelectTag(ComboBox const& box, hstring const& tag) {
  for (uint32_t index = 0; index < box.Items().Size(); ++index) {
    const auto item = box.Items().GetAt(index).try_as<ComboBoxItem>();
    if (item && unbox_value<hstring>(item.Tag()) == tag) {
      box.SelectedIndex(static_cast<int32_t>(index));
      return;
    }
  }
}

int PageSizeFromTag(hstring const& tag) {
  if (tag == L"6") {
    return 6;
  }
  if (tag == L"7") {
    return 7;
  }
  if (tag == L"8") {
    return 8;
  }
  if (tag == L"9") {
    return 9;
  }
  return 5;
}

}  // namespace

InputPage::InputPage() {
  InitializeComponent();
}

void InputPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&) {
  std::wstring error;
  const bool loaded = wubipinyin::settings::SettingsSession::Current().Refresh(&error);
  Populate();
  if (!loaded) {
    SetStatus(false, error);
  }
}

void InputPage::Settings_Changed(IInspectable const&,
                                 SelectionChangedEventArgs const&) {
  if (!m_loading) {
    Save();
  }
}

void InputPage::Populate() {
  m_loading = true;
  const auto& settings = wubipinyin::settings::SettingsSession::Current().Snapshot();
  SelectTag(RouteBox(), RouteTag(settings.default_route));
  SelectTag(PageSizeBox(), to_hstring(settings.candidate_page_size));
  m_loading = false;
}

void InputPage::Save() {
  auto settings = wubipinyin::settings::SettingsSession::Current().Snapshot();
  settings.default_route = RouteFromTag(SelectedTag(RouteBox()));
  settings.candidate_page_size = PageSizeFromTag(SelectedTag(PageSizeBox()));

  std::wstring error;
  if (wubipinyin::settings::SettingsSession::Current().Save(settings, &error)) {
    SetStatus(true, L"设置已保存");
  } else {
    SetStatus(false, error);
  }
}

void InputPage::SetStatus(bool success, std::wstring const& message) {
  StatusBar().Severity(success ? InfoBarSeverity::Success : InfoBarSeverity::Error);
  StatusBar().Title(success ? L"已更新" : L"无法保存");
  StatusBar().Message(message);
  StatusBar().IsOpen(true);
}

}  // namespace winrt::WubiPinyinSettings::implementation
