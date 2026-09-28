#include "pch.h"

#include "App.xaml.h"
#include "src/MainWindow.h"

namespace winrt::Framecraft::implementation {

App::App() {
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
