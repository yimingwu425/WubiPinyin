#pragma once

#include "DictionaryPage.g.h"
#include "WubiPinyinCore.h"
#include <optional>
#include <vector>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace winrt::WubiPinyinSettings::implementation {

struct DictionaryPage : DictionaryPageT<DictionaryPage> {
  DictionaryPage();

  void Page_Loaded(IInspectable const& sender,
                   Microsoft::UI::Xaml::RoutedEventArgs const& args);
  void SearchBox_TextChanged(
      Microsoft::UI::Xaml::Controls::TextBox const& sender,
      Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);
  void EntriesList_SelectionChanged(
      IInspectable const& sender,
      Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
  void NewEntry_Click(IInspectable const& sender,
                      Microsoft::UI::Xaml::RoutedEventArgs const& args);
  void SaveEntry_Click(IInspectable const& sender,
                       Microsoft::UI::Xaml::RoutedEventArgs const& args);
  void DeleteEntry_Click(IInspectable const& sender,
                         Microsoft::UI::Xaml::RoutedEventArgs const& args);

 private:
  bool ReloadEntries();
  void RenderEntries();
  void ClearEditor();
  void EditEntry(wubipinyin::UserEntry const& entry);
  std::optional<wubipinyin::UserEntry> SelectedEntry() const;
  void SetStatus(bool success, std::wstring const& message);

  std::vector<wubipinyin::UserEntry> m_entries;
  std::int64_t m_selected_id = 0;
};

}  // namespace winrt::WubiPinyinSettings::implementation

namespace winrt::WubiPinyinSettings::factory_implementation {

struct DictionaryPage : DictionaryPageT<DictionaryPage,
                                        implementation::DictionaryPage> {};

}  // namespace winrt::WubiPinyinSettings::factory_implementation
