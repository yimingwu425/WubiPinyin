#include "Pages/AppearancePage.xaml.h"
#if __has_include("AppearancePage.g.cpp")
#include "AppearancePage.g.cpp"
#endif

#include "SettingsSession.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::WubiPinyinSettings::implementation {
namespace {

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

}  // namespace

AppearancePage::AppearancePage() {
  InitializeComponent();
}

void AppearancePage::Page_Loaded(IInspectable const&, RoutedEventArgs const&) {
  std::wstring error;
  const bool loaded = wubipinyin::settings::SettingsSession::Current().Refresh(&error);
  Populate();
  if (!loaded) {
    SetStatus(false, error);
  }
}

void AppearancePage::Theme_Changed(IInspectable const&,
                                   SelectionChangedEventArgs const&) {
  if (!m_loading) {
    Save();
  }
}

void AppearancePage::SourceLabels_Toggled(IInspectable const&,
                                          IInspectable const&) {
  if (!m_loading) {
    Save();
  }
}

void AppearancePage::Populate() {
  m_loading = true;
  const auto& settings = wubipinyin::settings::SettingsSession::Current().Snapshot();
  SelectTag(ThemeBox(), to_hstring(settings.theme));
  SourceLabelsToggle().IsOn(settings.show_source_labels);
  m_loading = false;
}

void AppearancePage::Save() {
  auto settings = wubipinyin::settings::SettingsSession::Current().Snapshot();
  settings.theme = to_string(SelectedTag(ThemeBox()));
  settings.show_source_labels = SourceLabelsToggle().IsOn();

  std::wstring error;
  if (wubipinyin::settings::SettingsSession::Current().Save(settings, &error)) {
    SetStatus(true, L"设置已保存");
  } else {
    SetStatus(false, error);
  }
}

void AppearancePage::SetStatus(bool success, std::wstring const& message) {
  StatusBar().Severity(success ? InfoBarSeverity::Success : InfoBarSeverity::Error);
  StatusBar().Title(success ? L"已更新" : L"无法保存");
  StatusBar().Message(message);
  StatusBar().IsOpen(true);
}

}  // namespace winrt::WubiPinyinSettings::implementation
