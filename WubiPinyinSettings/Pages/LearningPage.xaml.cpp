#include "Pages/LearningPage.xaml.h"
#if __has_include("LearningPage.g.cpp")
#include "LearningPage.g.cpp"
#endif

#include "SettingsSession.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::WubiPinyinSettings::implementation {

LearningPage::LearningPage() {
  InitializeComponent();
}

void LearningPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&) {
  std::wstring error;
  const bool loaded = wubipinyin::settings::SettingsSession::Current().Refresh(&error);
  Populate();
  if (!loaded) {
    SetStatus(false, error);
  }
}

void LearningPage::Settings_Changed(IInspectable const&, IInspectable const&) {
  if (!m_loading) {
    Save();
  }
}

fire_and_forget LearningPage::ResetLearning_Click(IInspectable const&,
                                                  RoutedEventArgs const&) {
  ContentDialog dialog;
  dialog.XamlRoot(XamlRoot());
  dialog.Title(box_value(L"重置学习数据"));
  dialog.Content(box_value(L"这会清除本地学习排序，且无法恢复。"));
  dialog.PrimaryButtonText(L"重置");
  dialog.CloseButtonText(L"取消");

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  std::wstring error;
  if (wubipinyin::settings::SettingsSession::Current().ResetLearning(&error)) {
    SetStatus(true, L"学习数据已重置");
  } else {
    SetStatus(false, error);
  }
}

void LearningPage::Populate() {
  m_loading = true;
  const auto& settings = wubipinyin::settings::SettingsSession::Current().Snapshot();
  LearningToggle().IsOn(settings.learning_enabled);
  PasswordProtectionToggle().IsOn(settings.password_input_protection);
  m_loading = false;
}

void LearningPage::Save() {
  auto settings = wubipinyin::settings::SettingsSession::Current().Snapshot();
  settings.learning_enabled = LearningToggle().IsOn();
  settings.password_input_protection = PasswordProtectionToggle().IsOn();

  std::wstring error;
  if (wubipinyin::settings::SettingsSession::Current().Save(settings, &error)) {
    SetStatus(true, L"设置已保存");
  } else {
    SetStatus(false, error);
  }
}

void LearningPage::SetStatus(bool success, std::wstring const& message) {
  StatusBar().Severity(success ? InfoBarSeverity::Success : InfoBarSeverity::Error);
  StatusBar().Title(success ? L"已更新" : L"无法保存");
  StatusBar().Message(message);
  StatusBar().IsOpen(true);
}

}  // namespace winrt::WubiPinyinSettings::implementation
