#include "App.xaml.h"

#include <Windows.h>
#include <winrt/Microsoft.UI.Xaml.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  init_apartment(apartment_type::single_threaded);
  Application::Start([](auto&&) {
    make<winrt::WubiPinyinSettings::implementation::App>();
  });
  return 0;
}
