#include "pch.h"

#include "App.xaml.h"
#include "src/MainWindow.h"
#include "framecraft/library.h"

namespace winrt::Framecraft::implementation {

App::App() {
    // The theme is fixed at launch (every screen resolves its colors when it is built):
    // --dark / --light (screenshots), else the choice saved in Ajustes, else Windows' setting.
    std::wstring theme;
    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        for (int i = 1; i < argc; ++i) {
            if (std::wstring_view(argv[i]) == L"--dark") theme = L"dark";
            if (std::wstring_view(argv[i]) == L"--light") theme = L"light";
        }
        LocalFree(argv);
    }
    if (theme.empty()) {
        PWSTR folder = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder))) {
            auto index = fc::library::load(std::filesystem::path(folder) / L"Framecraft Studio" / L"library.json");
            if (index.preferences.theme == "dark") theme = L"dark";
            if (index.preferences.theme == "light") theme = L"light";
        }
        CoTaskMemFree(folder);
    }
    if (theme == L"dark") RequestedTheme(Microsoft::UI::Xaml::ApplicationTheme::Dark);
    if (theme == L"light") RequestedTheme(Microsoft::UI::Xaml::ApplicationTheme::Light);

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
    UnhandledException([](IInspectable const&, Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& e) {
        if (IsDebuggerPresent()) {
            auto message = e.Message();
            __debugbreak();
        }
    });
#endif
}

void App::OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
    // Framecraft.exe --snapshot <folder>: renders every screen with demo data and exits (used by CI).
    std::optional<std::filesystem::path> snapshot;
    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        for (int i = 1; i + 1 < argc; ++i) {
            if (std::wstring_view(argv[i]) == L"--snapshot") snapshot = std::filesystem::path(argv[i + 1]);
        }
        LocalFree(argv);
    }
    fcapp::MainWindow::Launch(snapshot);
}

}  // namespace winrt::Framecraft::implementation
