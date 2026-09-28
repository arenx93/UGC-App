#pragma once
#include "App.xaml.g.h"

namespace winrt::Framecraft::implementation {

struct App : AppT<App> {
    App();
    void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
};

}  // namespace winrt::Framecraft::implementation
