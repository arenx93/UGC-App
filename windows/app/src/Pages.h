#pragma once
// Screens of the app. Each one builds its controls in C++ and refreshes itself on model changes.

#include <memory>

#include "AppModel.h"
#include "Ui.h"

namespace fcapp {

class Page {
public:
    virtual ~Page() = default;
    virtual winrt::Microsoft::UI::Xaml::UIElement root() = 0;
    virtual void refresh(Change change) = 0;
    /// Called when the page becomes visible.
    virtual void activated() {}
};

std::unique_ptr<Page> makeCreatePage();
std::unique_ptr<Page> makeChatPage();
std::unique_ptr<Page> makeStoriesPage();
std::unique_ptr<Page> makeLibraryPage();
std::unique_ptr<Page> makeReferencesPage();
std::unique_ptr<Page> makeSkillsPage();
std::unique_ptr<Page> makeGuidePage();
std::unique_ptr<Page> makeSettingsPage();

/// Dialogs.
winrt::fire_and_forget showJobDetail(winrt::Microsoft::UI::Xaml::XamlRoot root, std::string jobID);
winrt::fire_and_forget showOnboarding(winrt::Microsoft::UI::Xaml::XamlRoot root);
winrt::fire_and_forget showPromptPreview(winrt::Microsoft::UI::Xaml::XamlRoot root);
winrt::fire_and_forget showMediaViewer(winrt::Microsoft::UI::Xaml::XamlRoot root, std::filesystem::path file, bool video);
/// Onboarding content, also used for screenshots.
winrt::Microsoft::UI::Xaml::UIElement onboardingContent(std::function<void()> const& onDone);

/// Thumbnail control for a file (loads asynchronously and caches).
winrt::Microsoft::UI::Xaml::Controls::Grid thumbnail(std::filesystem::path const& file, bool video, int pixels, double width,
                                                    double height);
/// Media element (image or looping muted video) filling its container.
winrt::Microsoft::UI::Xaml::UIElement mediaView(std::filesystem::path const& file, bool video, bool autoplay);

std::string jobStatusText(fc::Job const& job);
std::wstring kindGlyph(fc::MediaKind kind);
std::wstring referenceGlyph(fc::ReferenceKind kind);

}  // namespace fcapp
