#include "Pages/DictionaryPage.xaml.h"

#include "SettingsSession.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <string>
#include <vector>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::WubiPinyinSettings::implementation {
namespace {

std::wstring AsWide(hstring const& value) {
  return std::wstring(value.c_str(), value.size());
}

hstring Utf8ToHstring(std::string const& value) {
  if (value.empty()) {
    return {};
  }
  const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                             value.data(),
                                             static_cast<int>(value.size()),
                                             nullptr, 0);
  if (required <= 0) {
    return L"[invalid UTF-8]";
  }
  std::wstring converted(static_cast<std::size_t>(required), L'\0');
  if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), converted.data(),
                            required) != required) {
    return L"[invalid UTF-8]";
  }
  return hstring(converted);
}

std::string HstringToUtf8(hstring const& value) {
  if (value.empty()) {
    return {};
  }
  const int required = ::WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, value.c_str(),
      static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (required <= 0) {
    return {};
  }
  std::string converted(static_cast<std::size_t>(required), '\0');
  if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.c_str(),
                            static_cast<int>(value.size()), converted.data(),
                            required, nullptr, nullptr) != required) {
    return {};
  }
  return converted;
}

std::wstring Lower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return value;
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

std::int64_t ItemId(ListViewItem const& item) {
  try {
    return unbox_value<std::int64_t>(item.Tag());
  } catch (hresult_error const&) {
    return 0;
  }
}

}  // namespace

DictionaryPage::DictionaryPage() {
  InitializeComponent();
}

void DictionaryPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&) {
  ReloadEntries();
}

void DictionaryPage::SearchBox_TextChanged(TextBox const&,
                                           TextChangedEventArgs const&) {
  RenderEntries();
}

void DictionaryPage::EntriesList_SelectionChanged(
    IInspectable const&,
    SelectionChangedEventArgs const&) {
  const auto item = EntriesList().SelectedItem().try_as<ListViewItem>();
  if (!item) {
    m_selected_id = 0;
    DeleteButton().IsEnabled(false);
    return;
  }
  m_selected_id = ItemId(item);
  const auto selected = SelectedEntry();
  if (selected) {
    EditEntry(*selected);
  }
}

void DictionaryPage::NewEntry_Click(IInspectable const&, RoutedEventArgs const&) {
  ClearEditor();
}

void DictionaryPage::SaveEntry_Click(IInspectable const&,
                                     RoutedEventArgs const&) {
  wubipinyin::UserEntry entry;
  entry.id = m_selected_id;
  entry.text = HstringToUtf8(EntryTextBox().Text());
  entry.code = HstringToUtf8(CodeBox().Text());
  std::transform(entry.code.begin(), entry.code.end(), entry.code.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  entry.scheme = SelectedTag(SchemeBox()) == L"pinyin"
                     ? wubipinyin::HybridRoute::kPinyin
                     : wubipinyin::HybridRoute::kWubi;
  entry.enabled = EnabledToggle().IsOn();

  const double weight = WeightBox().Value();
  if (entry.text.empty() || entry.code.empty()) {
    SetStatus(false, L"请填写文字和编码");
    return;
  }
  if (!std::isfinite(weight) || weight < 1 || weight > 100000 ||
      std::floor(weight) != weight) {
    SetStatus(false, L"权重必须是 1 到 100000 的整数");
    return;
  }
  entry.weight = static_cast<int>(weight);

  std::wstring error;
  if (!wubipinyin::settings::SettingsSession::Current().UpsertEntry(&entry,
                                                                      &error)) {
    SetStatus(false, error);
    return;
  }
  ClearEditor();
  if (ReloadEntries()) {
    SetStatus(true, L"词条已保存");
  }
}

void DictionaryPage::DeleteEntry_Click(IInspectable const&,
                                       RoutedEventArgs const&) {
  if (m_selected_id <= 0) {
    return;
  }
  std::wstring error;
  if (!wubipinyin::settings::SettingsSession::Current().DeleteEntry(
          m_selected_id, &error)) {
    SetStatus(false, error);
    return;
  }
  ClearEditor();
  if (ReloadEntries()) {
    SetStatus(true, L"词条已删除");
  }
}

bool DictionaryPage::ReloadEntries() {
  std::vector<wubipinyin::UserEntry> entries;
  std::wstring error;
  if (!wubipinyin::settings::SettingsSession::Current().ListEntries(&entries,
                                                                      &error)) {
    SetStatus(false, error);
    return false;
  }
  m_entries = std::move(entries);
  RenderEntries();
  return true;
}

void DictionaryPage::RenderEntries() {
  const std::wstring query = Lower(AsWide(SearchBox().Text()));
  EntriesList().Items().Clear();
  m_selected_id = 0;
  DeleteButton().IsEnabled(false);
  for (const auto& entry : m_entries) {
    const std::wstring text = AsWide(Utf8ToHstring(entry.text));
    const std::wstring code = AsWide(Utf8ToHstring(entry.code));
    if (!query.empty() && Lower(text).find(query) == std::wstring::npos &&
        Lower(code).find(query) == std::wstring::npos) {
      continue;
    }

    ListViewItem item;
    item.Padding(Thickness{12, 8, 12, 8});
    item.Tag(box_value(entry.id));
    StackPanel row;
    row.Orientation(Orientation::Horizontal);
    row.Spacing(12);

    TextBlock text_block;
    text_block.Width(104);
    text_block.Text(Utf8ToHstring(entry.text));
    TextBlock code_block;
    code_block.Width(94);
    code_block.Text(Utf8ToHstring(entry.code));
    TextBlock scheme_block;
    scheme_block.Width(40);
    scheme_block.Text(entry.scheme == wubipinyin::HybridRoute::kPinyin ? L"拼音"
                                                                         : L"五笔");
    TextBlock enabled_block;
    enabled_block.Text(entry.enabled ? L"启用" : L"停用");

    row.Children().Append(text_block);
    row.Children().Append(code_block);
    row.Children().Append(scheme_block);
    row.Children().Append(enabled_block);
    item.Content(row);
    EntriesList().Items().Append(item);
  }
}

void DictionaryPage::ClearEditor() {
  m_selected_id = 0;
  EntriesList().SelectedItem(nullptr);
  EntryTextBox().Text(L"");
  SchemeBox().SelectedIndex(0);
  CodeBox().Text(L"");
  WeightBox().Value(1000);
  EnabledToggle().IsOn(true);
  DeleteButton().IsEnabled(false);
}

void DictionaryPage::EditEntry(wubipinyin::UserEntry const& entry) {
  m_selected_id = entry.id;
  EntryTextBox().Text(Utf8ToHstring(entry.text));
  SelectTag(SchemeBox(), entry.scheme == wubipinyin::HybridRoute::kPinyin
                             ? L"pinyin"
                             : L"wubi");
  CodeBox().Text(Utf8ToHstring(entry.code));
  WeightBox().Value(entry.weight);
  EnabledToggle().IsOn(entry.enabled);
  DeleteButton().IsEnabled(true);
}

std::optional<wubipinyin::UserEntry> DictionaryPage::SelectedEntry() const {
  for (const auto& entry : m_entries) {
    if (entry.id == m_selected_id) {
      return entry;
    }
  }
  return std::nullopt;
}

void DictionaryPage::SetStatus(bool success, std::wstring const& message) {
  StatusBar().Severity(success ? InfoBarSeverity::Success : InfoBarSeverity::Error);
  StatusBar().Title(success ? L"已更新" : L"无法完成操作");
  StatusBar().Message(message);
  StatusBar().IsOpen(true);
}

}  // namespace winrt::WubiPinyinSettings::implementation
