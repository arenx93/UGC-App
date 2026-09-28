#include "pch.h"

#include "MainWindow.h"

using namespace winrt;
namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxm = winrt::Microsoft::UI::Xaml::Media;

namespace fcapp {

namespace {
std::unique_ptr<MainWindow> gMainWindow;
}

void MainWindow::Launch() {
    gMainWindow.reset(new MainWindow());
}

MainWindow::MainWindow() {
    window_ = mux::Window();
    window_.Title(L"Framecraft");
    window_.SystemBackdrop(muxm::MicaBackdrop());
    window_.ExtendsContentIntoTitleBar(true);

    muxc::Grid root;
    muxc::RowDefinition titleRow;
    titleRow.Height(mux::GridLengthHelper::FromPixels(48));
    root.RowDefinitions().Append(titleRow);
    root.RowDefinitions().Append(muxc::RowDefinition());

    muxc::StackPanel titleBar;
    titleBar.Orientation(muxc::Orientation::Horizontal);
    titleBar.Spacing(10);
    titleBar.Padding(mux::ThicknessHelper::FromLengths(16, 0, 0, 0));
    titleBar.VerticalAlignment(mux::VerticalAlignment::Center);
    muxc::TextBlock title;
    title.Text(L"Framecraft");
    title.Style(mux::Application::Current().Resources().Lookup(box_value(L"CaptionTextBlockStyle")).as<mux::Style>());
    titleBar.Children().Append(title);
    root.Children().Append(titleBar);

    muxc::NavigationView nav;
    nav.IsBackButtonVisible(muxc::NavigationViewBackButtonVisible::Collapsed);
    nav.PaneDisplayMode(muxc::NavigationViewPaneDisplayMode::Left);
    for (auto const& [label, glyph] : std::vector<std::pair<std::wstring, std::wstring>>{
             {L"Crear", L""}, {L"Historias", L""}, {L"Biblioteca", L""}, {L"Referencias", L""}}) {
        muxc::NavigationViewItem item;
        item.Content(box_value(label));
        muxc::FontIcon icon;
        icon.Glyph(glyph);
        item.Icon(icon);
        nav.MenuItems().Append(item);
    }
    muxc::TextBlock hello;
    hello.Text(L"Framecraft para Windows — C++ y WinUI 3");
    hello.Margin(mux::ThicknessHelper::FromUniformLength(32));
    hello.Style(mux::Application::Current().Resources().Lookup(box_value(L"TitleTextBlockStyle")).as<mux::Style>());
    nav.Content(hello);
    muxc::Grid::SetRow(nav, 1);
    root.Children().Append(nav);

    window_.Content(root);
    window_.SetTitleBar(titleBar);
    window_.AppWindow().Resize({1360, 880});
    window_.Activate();
}

}  // namespace fcapp
