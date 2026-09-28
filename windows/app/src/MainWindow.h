#pragma once
// Main window: Mica backdrop, custom title bar and the side navigation.

namespace fcapp {

class MainWindow {
public:
    static void Launch();

private:
    MainWindow();
    winrt::Microsoft::UI::Xaml::Window window_{nullptr};
};

}  // namespace fcapp
