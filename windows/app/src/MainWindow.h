#pragma once
// Main window: Mica backdrop, custom title bar, side navigation, search/command bar and notices.

#include <filesystem>
#include <map>
#include <memory>
#include <optional>

#include "Pages.h"

namespace fcapp {

class MainWindow {
public:
    /// Normal launch, or `--snapshot <folder>`: renders every screen with demo data to PNG files and exits.
    static void Launch(std::optional<std::filesystem::path> snapshotFolder);

private:
    explicit MainWindow(std::optional<std::filesystem::path> snapshotFolder);
    void buildTitleBar();
    void buildNavigation();
    void buildBanner();
    void buildAccelerators();
    void select(Section section);
    void appWindowResize();
    void refresh(Change change);
    void refreshAccount();
    void refreshSuggestions(std::string const& query);
    void runSuggestion(std::string const& id);
    winrt::fire_and_forget runSnapshots();

    winrt::Microsoft::UI::Xaml::Window window_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Grid titleBar_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::NavigationView nav_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Grid content_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::InfoBar banner_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox search_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock credits_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem activeItem_{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer bannerTimer_{nullptr};
    std::map<Section, winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem> items_;
    std::map<Section, std::unique_ptr<Page>> pages_;
    std::vector<std::pair<std::string, std::string>> suggestionIDs_;  // text -> action id
    std::optional<std::filesystem::path> snapshotFolder_;
    bool selecting_ = false;
};

}  // namespace fcapp
