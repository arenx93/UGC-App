#pragma once
// Small builders for WinUI controls, so every screen can be written in plain C++.

#include <functional>
#include <string>
#include <vector>

namespace fcapp::ui {

namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxm = winrt::Microsoft::UI::Xaml::Media;

enum class Text { caption, body, bodyStrong, subtitle, title, titleLarge, display };

/// Brand colors (coral → pink → violet) and the pink accent applied to every Fluent control.
void applyTheme();
winrt::Windows::UI::Color rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
muxm::Brush brandGradient();
muxm::Brush softGradient();
muxm::Brush pinkBrush();
muxm::Brush resource(wchar_t const* key);
mux::Style style(wchar_t const* key);

muxc::TextBlock text(std::string const& value, Text kind = Text::body);
muxc::TextBlock secondary(std::string const& value, Text kind = Text::caption);
/// Title with the brand gradient (drawn as a gradient fill through a text mask is not available in XAML,
/// so the gradient goes on the glyphs' foreground via a LinearGradientBrush).
muxc::TextBlock gradientTitle(std::string const& value, double size = 30);
muxc::FontIcon icon(std::wstring const& glyph, double size = 16);
muxc::StackPanel vstack(double spacing = 8);
muxc::StackPanel hstack(double spacing = 8);
muxc::Grid columns(std::vector<mux::GridLength> const& widths, double spacing = 12);
void place(muxc::Grid const& grid, mux::UIElement const& child, int column, int row = 0);
mux::GridLength star(double value = 1);
mux::GridLength pixels(double value);
mux::GridLength autoLength();
mux::Thickness margin(double left, double top, double right, double bottom);
mux::Thickness uniform(double value);

/// Fluent card: layer fill, subtle stroke, rounded corners.
muxc::Border card(mux::UIElement const& content, double padding = 18);
/// Numbered step header ("1 · Describí tu idea").
mux::UIElement stepHeader(int number, std::string const& title, std::string const& subtitle);
/// "@Image1"-style pill with the brand gradient.
muxc::Border pill(std::string const& value, bool gradient = true);
/// Rounded square with the brand gradient and a white glyph.
muxc::Border glyphBadge(std::wstring const& glyph, double size = 36, bool gradient = true);

/// Big primary action with the brand gradient.
muxc::Button primaryButton(std::string const& label, std::wstring const& glyph, std::function<void()> onClick);
muxc::Button button(std::string const& label, std::wstring const& glyph, std::function<void()> onClick);
muxc::Button subtleButton(std::string const& label, std::wstring const& glyph, std::function<void()> onClick);
muxc::HyperlinkButton link(std::string const& label, std::function<void()> onClick);
/// Button content with an icon and a label.
mux::UIElement labeled(std::wstring const& glyph, std::string const& label);
void setLabel(muxc::Button const& button, std::wstring const& glyph, std::string const& label);
void tooltip(mux::DependencyObject const& element, std::string const& value);
void accessible(mux::DependencyObject const& element, std::string const& name);

/// Multi-line text box with a placeholder; `onChange` fires on every edit.
muxc::TextBox editor(std::string const& placeholder, double minHeight, std::function<void(std::string const&)> onChange,
                     bool monospace = false);
muxc::TextBox field(std::string const& placeholder, std::function<void(std::string const&)> onChange);
muxc::ComboBox combo(std::vector<std::string> const& items, int selected, std::function<void(int)> onChange);
muxc::ToggleSwitch toggle(std::string const& header, bool on, std::function<void(bool)> onChange);
/// Segmented choice (SelectorBar-like) built from toggle buttons.
mux::UIElement segmented(std::vector<std::string> const& items, int selected, std::function<void(int)> onChange);

/// Selectable card (models, presets, skills, templates).
muxc::Button choiceCard(std::string const& title, std::string const& subtitle, std::wstring const& glyph, bool selected,
                        std::function<void()> onClick, std::string const& badge = "");

/// Removes all children and adds new ones.
void setChildren(muxc::Panel const& panel, std::vector<mux::UIElement> const& children);
void show(mux::UIElement const& element, bool visible);

/// Content dialog with a primary button; `onPrimary` runs when accepted.
muxc::ContentDialog dialog(mux::XamlRoot const& root, std::string const& title, mux::UIElement const& content,
                           std::string const& primary, std::string const& close);
winrt::fire_and_forget confirm(mux::XamlRoot root, std::string title, std::string message, std::string action,
                               std::function<void()> onConfirm);

}  // namespace fcapp::ui
