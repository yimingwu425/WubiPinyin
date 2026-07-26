#pragma once

#include <winrt/WubiPinyinSettings.h>
#include "Pages/AboutPage.xaml.g.h"

namespace winrt::WubiPinyinSettings::implementation {

struct AboutPage : AboutPageT<AboutPage> {
  AboutPage();
};

}  // namespace winrt::WubiPinyinSettings::implementation

namespace winrt::WubiPinyinSettings::factory_implementation {

struct AboutPage : AboutPageT<AboutPage, implementation::AboutPage> {};

}  // namespace winrt::WubiPinyinSettings::factory_implementation
