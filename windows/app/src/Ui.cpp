#include "pch.h"

#include "Ui.h"

#include "WinUtil.h"

using namespace winrt;
namespace wu = winrt::Windows::UI;

namespace fcapp::ui {

wu::Color rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return wu::Color{a, r, g, b}; }

void applyTheme() {
    auto resources = mux::Application::Current().Resources();
    // Pink accent for every Fluent control (buttons, toggles, focus, selection).
    std::pair<wchar_t const*, wu::Color> accents[] = {
        {L"SystemAccentColor", rgb(0xFF, 0x3D, 0x99)},       {L"SystemAccentColorLight1", rgb(0xFF, 0x62, 0xAD)},
        {L"SystemAccentColorLight2", rgb(0xFF, 0x8C, 0xC4)}, {L"SystemAccentColorLight3", rgb(0xFF, 0xB8, 0xDB)},
        {L"SystemAccentColorDark1", rgb(0xE0, 0x24, 0x80)},  {L"SystemAccentColorDark2", rgb(0xB8, 0x1A, 0x68)},
        {L"SystemAccentColorDark3", rgb(0x8C, 0x12, 0x4F)},
    };
    for (auto const& [key, color] : accents) resources.Insert(box_value(key), box_value(color));
}

muxm::Brush brandGradient() {
    muxm::LinearGradientBrush brush;
    brush.StartPoint({0, 0});
    brush.EndPoint({1, 1});
    auto stop = [&](wu::Color color, double offset) {
        muxm::GradientStop s;
        s.Color(color);
        s.Offset(offset);
        brush.GradientStops().Append(s);
    };
    stop(rgb(0xFF, 0x8A, 0x4D), 0.0);
    stop(rgb(0xFF, 0x3D, 0x99), 0.5);
    stop(rgb(0x8C, 0x5C, 0xF5), 1.0);
    return brush;
}

muxm::Brush softGradient() {
    muxm::LinearGradientBrush brush;
    brush.StartPoint({0, 0});
    brush.EndPoint({1, 1});
    auto stop = [&](wu::Color color, double offset) {
        muxm::GradientStop s;
        s.Color(color);
        s.Offset(offset);
        brush.GradientStops().Append(s);
    };
    stop(rgb(0xFF, 0x8A, 0x4D, 0x2A), 0.0);
    stop(rgb(0xFF, 0x3D, 0x99, 0x24), 0.5);
    stop(rgb(0x8C, 0x5C, 0xF5, 0x2A), 1.0);
    return brush;
}

muxm::Brush pinkBrush() { return muxm::SolidColorBrush(rgb(0xFF, 0x3D, 0x99)); }

muxm::Brush resource(wchar_t const* key) {
    auto value = mux::Application::Current().Resources().TryLookup(box_value(key));
    return value ? value.as<muxm::Brush>() : muxm::Brush(muxm::SolidColorBrush(rgb(128, 128, 128)));
}

mux::Style style(wchar_t const* key) {
    auto value = mux::Application::Current().Resources().TryLookup(box_value(key));
    return value ? value.as<mux::Style>() : mux::Style(nullptr);
}

muxc::TextBlock text(std::string const& value, Text kind) {
    muxc::TextBlock block;
    block.Text(hs(value));
    block.TextWrapping(mux::TextWrapping::Wrap);
    wchar_t const* key = L"BodyTextBlockStyle";
    switch (kind) {
    case Text::caption: key = L"CaptionTextBlockStyle"; break;
    case Text::body: key = L"BodyTextBlockStyle"; break;
    case Text::bodyStrong: key = L"BodyStrongTextBlockStyle"; break;
    case Text::subtitle: key = L"SubtitleTextBlockStyle"; break;
    case Text::title: key = L"TitleTextBlockStyle"; break;
    case Text::titleLarge: key = L"TitleLargeTextBlockStyle"; break;
    case Text::display: key = L"DisplayTextBlockStyle"; break;
    }
    if (auto s = style(key)) block.Style(s);
    return block;
}

muxc::TextBlock secondary(std::string const& value, Text kind) {
    auto block = text(value, kind);
    block.Foreground(resource(L"TextFillColorSecondaryBrush"));
    return block;
}

muxc::TextBlock gradientTitle(std::string const& value, double size) {
    muxc::TextBlock block;
    block.Text(hs(value));
    block.FontSize(size);
    block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
    block.Foreground(brandGradient());
    block.TextWrapping(mux::TextWrapping::Wrap);
    block.Margin(margin(0, 0, 0, 2));
    return block;
}

muxc::FontIcon icon(std::wstring const& glyph, double size) {
    muxc::FontIcon value;
    value.Glyph(glyph);
    value.FontSize(size);
    return value;
}

muxc::StackPanel vstack(double spacing) {
    muxc::StackPanel panel;
    panel.Spacing(spacing);
    return panel;
}

muxc::StackPanel hstack(double spacing) {
    muxc::StackPanel panel;
    panel.Orientation(muxc::Orientation::Horizontal);
    panel.Spacing(spacing);
    return panel;
}

mux::GridLength star(double value) { return mux::GridLengthHelper::FromValueAndType(value, mux::GridUnitType::Star); }
mux::GridLength pixels(double value) { return mux::GridLengthHelper::FromPixels(value); }
mux::GridLength autoLength() { return mux::GridLengthHelper::Auto(); }
mux::Thickness margin(double left, double top, double right, double bottom) {
    return mux::ThicknessHelper::FromLengths(left, top, right, bottom);
}
mux::Thickness uniform(double value) { return mux::ThicknessHelper::FromUniformLength(value); }

muxc::Grid columns(std::vector<mux::GridLength> const& widths, double spacing) {
    muxc::Grid grid;
    grid.ColumnSpacing(spacing);
    for (auto const& width : widths) {
        muxc::ColumnDefinition column;
        column.Width(width);
        grid.ColumnDefinitions().Append(column);
    }
    return grid;
}

void place(muxc::Grid const& grid, mux::UIElement const& child, int column, int row) {
    muxc::Grid::SetColumn(child.as<mux::FrameworkElement>(), column);
    muxc::Grid::SetRow(child.as<mux::FrameworkElement>(), row);
    grid.Children().Append(child);
}

muxc::Border card(mux::UIElement const& content, double padding) {
    muxc::Border border;
    border.Child(content);
    border.Padding(uniform(padding));
    border.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(12));
    border.Background(resource(L"CardBackgroundFillColorDefaultBrush"));
    border.BorderBrush(resource(L"CardStrokeColorDefaultBrush"));
    border.BorderThickness(uniform(1));
    return border;
}

winrt::Windows::UI::Color successColor() { return rgb(51, 189, 120); }

mux::UIElement stepHeader(int number, std::string const& title, std::string const& subtitle, bool done) {
    auto row = hstack(12);
    muxc::Border badge;
    badge.Width(26);
    badge.Height(26);
    badge.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(13));
    if (done) badge.Background(muxm::SolidColorBrush(successColor()));
    else badge.Background(brandGradient());
    muxc::TextBlock digit;
    digit.Text(done ? winrt::hstring(L"\u2713") : to_hstring(number));
    digit.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
    digit.Foreground(muxm::SolidColorBrush(rgb(255, 255, 255)));
    digit.HorizontalAlignment(mux::HorizontalAlignment::Center);
    digit.VerticalAlignment(mux::VerticalAlignment::Center);
    badge.Child(digit);
    badge.VerticalAlignment(mux::VerticalAlignment::Top);
    badge.Margin(margin(0, 2, 0, 0));
    auto labels = vstack(2);
    labels.Children().Append(text(title, Text::subtitle));
    if (!subtitle.empty()) labels.Children().Append(secondary(subtitle, Text::body));
    row.Children().Append(badge);
    row.Children().Append(labels);
    accessible(row, "Paso " + std::to_string(number) + ": " + title + (done ? ", listo" : ""));
    return row;
}

muxc::Border pill(std::string const& value, bool gradient) {
    muxc::Border border;
    border.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
    border.Padding(margin(8, 2, 8, 3));
    border.Background(gradient ? brandGradient() : softGradient());
    muxc::TextBlock label;
    label.Text(hs(value));
    label.FontSize(12);
    label.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
    if (gradient) label.Foreground(muxm::SolidColorBrush(rgb(255, 255, 255)));
    border.Child(label);
    border.VerticalAlignment(mux::VerticalAlignment::Center);
    return border;
}

muxc::Border glyphBadge(std::wstring const& glyph, double size, bool gradient) {
    muxc::Border border;
    border.Width(size);
    border.Height(size);
    border.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(size * 0.28));
    border.Background(gradient ? brandGradient() : softGradient());
    auto symbol = icon(glyph, size * 0.46);
    symbol.Foreground(gradient ? muxm::Brush(muxm::SolidColorBrush(rgb(255, 255, 255))) : pinkBrush());
    symbol.HorizontalAlignment(mux::HorizontalAlignment::Center);
    symbol.VerticalAlignment(mux::VerticalAlignment::Center);
    border.Child(symbol);
    return border;
}

mux::UIElement labeled(std::wstring const& glyph, std::string const& label) {
    auto row = hstack(8);
    if (!glyph.empty()) {
        auto symbol = icon(glyph, 14);
        symbol.VerticalAlignment(mux::VerticalAlignment::Center);
        row.Children().Append(symbol);
    }
    muxc::TextBlock caption;
    caption.Text(hs(label));
    caption.VerticalAlignment(mux::VerticalAlignment::Center);
    row.Children().Append(caption);
    return row;
}

void setLabel(muxc::Button const& button, std::wstring const& glyph, std::string const& label) {
    button.Content(labeled(glyph, label));
    accessible(button, label);
}

muxc::Button primaryButton(std::string const& label, std::wstring const& glyph, std::function<void()> onClick) {
    muxc::Button b;
    setLabel(b, glyph, label);
    auto gradient = brandGradient();
    auto white = muxm::SolidColorBrush(rgb(255, 255, 255));
    b.Background(gradient);
    b.Foreground(white);
    b.BorderThickness(uniform(0));
    b.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
    b.Padding(margin(20, 10, 20, 11));
    b.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
    // Keep the gradient in every visual state (hover, pressed); disabled fades.
    auto res = b.Resources();
    res.Insert(box_value(L"ButtonBackgroundPointerOver"), gradient);
    res.Insert(box_value(L"ButtonBackgroundPressed"), gradient);
    res.Insert(box_value(L"ButtonForegroundPointerOver"), white);
    res.Insert(box_value(L"ButtonForegroundPressed"), white);
    res.Insert(box_value(L"ButtonBackgroundDisabled"), resource(L"ControlFillColorDisabledBrush"));
    res.Insert(box_value(L"ButtonForegroundDisabled"), resource(L"TextFillColorDisabledBrush"));
    b.Click([onClick](auto&&, auto&&) {
        if (onClick) onClick();
    });
    return b;
}

muxc::Button button(std::string const& label, std::wstring const& glyph, std::function<void()> onClick) {
    muxc::Button b;
    setLabel(b, glyph, label);
    b.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(8));
    b.Click([onClick](auto&&, auto&&) {
        if (onClick) onClick();
    });
    return b;
}

muxc::Button subtleButton(std::string const& label, std::wstring const& glyph, std::function<void()> onClick) {
    auto b = button(label, glyph, onClick);
    if (auto s = style(L"SubtleButtonStyle")) b.Style(s);
    b.Background(muxm::SolidColorBrush(rgb(0, 0, 0, 0)));
    b.BorderThickness(uniform(0));
    return b;
}

muxc::HyperlinkButton link(std::string const& label, std::function<void()> onClick) {
    muxc::HyperlinkButton b;
    b.Content(box_value(hs(label)));
    b.Padding(uniform(0));
    b.Click([onClick](auto&&, auto&&) {
        if (onClick) onClick();
    });
    return b;
}

void tooltip(mux::DependencyObject const& element, std::string const& value) {
    muxc::ToolTipService::SetToolTip(element, box_value(hs(value)));
}

void accessible(mux::DependencyObject const& element, std::string const& name) {
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(element, hs(name));
}

muxc::TextBox editor(std::string const& placeholder, double minHeight, std::function<void(std::string const&)> onChange, bool monospace) {
    muxc::TextBox box;
    box.PlaceholderText(hs(placeholder));
    box.AcceptsReturn(true);
    box.TextWrapping(mux::TextWrapping::Wrap);
    box.MinHeight(minHeight);
    box.MaxHeight(minHeight * 3.2);
    box.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(8));
    muxc::ScrollViewer::SetVerticalScrollBarVisibility(box, muxc::ScrollBarVisibility::Auto);
    if (monospace) box.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
    box.TextChanged([onChange](winrt::Windows::Foundation::IInspectable const& sender, auto&&) {
        if (onChange) onChange(str(sender.as<muxc::TextBox>().Text()));
    });
    return box;
}

muxc::TextBox field(std::string const& placeholder, std::function<void(std::string const&)> onChange) {
    muxc::TextBox box;
    box.PlaceholderText(hs(placeholder));
    box.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(8));
    box.TextChanged([onChange](winrt::Windows::Foundation::IInspectable const& sender, auto&&) {
        if (onChange) onChange(str(sender.as<muxc::TextBox>().Text()));
    });
    return box;
}

muxc::ComboBox combo(std::vector<std::string> const& items, int selected, std::function<void(int)> onChange) {
    muxc::ComboBox box;
    for (auto const& item : items) box.Items().Append(box_value(hs(item)));
    box.SelectedIndex(selected);
    box.MinWidth(120);
    box.SelectionChanged([onChange](winrt::Windows::Foundation::IInspectable const& sender, auto&&) {
        int index = sender.as<muxc::ComboBox>().SelectedIndex();
        if (onChange && index >= 0) onChange(index);
    });
    return box;
}

muxc::ToggleSwitch toggle(std::string const& header, bool on, std::function<void(bool)> onChange) {
    muxc::ToggleSwitch value;
    if (!header.empty()) value.Header(box_value(hs(header)));
    value.IsOn(on);
    value.Toggled([onChange](winrt::Windows::Foundation::IInspectable const& sender, auto&&) {
        if (onChange) onChange(sender.as<muxc::ToggleSwitch>().IsOn());
    });
    return value;
}

mux::UIElement segmented(std::vector<std::string> const& items, int selected, std::function<void(int)> onChange) {
    muxc::Border frame;
    frame.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
    frame.Background(resource(L"ControlAltFillColorSecondaryBrush"));
    frame.BorderBrush(resource(L"CardStrokeColorDefaultBrush"));
    frame.BorderThickness(uniform(1));
    frame.Padding(uniform(3));
    frame.HorizontalAlignment(mux::HorizontalAlignment::Left);
    auto row = hstack(2);
    auto buttons = std::make_shared<std::vector<winrt::weak_ref<muxc::Primitives::ToggleButton>>>();
    for (size_t i = 0; i < items.size(); ++i) {
        muxc::Primitives::ToggleButton option;
        option.Content(box_value(hs(items[i])));
        option.IsChecked(static_cast<int>(i) == selected);
        option.MinWidth(52);
        option.Padding(margin(12, 5, 12, 6));
        option.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(8));
        option.BorderThickness(uniform(0));
        if (static_cast<int>(i) != selected) option.Background(muxm::SolidColorBrush(rgb(0, 0, 0, 0)));
        option.Click([buttons, i, onChange](auto&&, auto&&) {
            for (size_t k = 0; k < buttons->size(); ++k) {
                auto other = (*buttons)[k].get();
                if (!other) continue;
                other.IsChecked(k == i);
                if (k == i) other.ClearValue(muxc::Control::BackgroundProperty());
                else other.Background(muxm::SolidColorBrush(rgb(0, 0, 0, 0)));
            }
            if (onChange) onChange(static_cast<int>(i));
        });
        buttons->push_back(winrt::make_weak(option));
        row.Children().Append(option);
    }
    frame.Child(row);
    return frame;
}

muxc::Button choiceCard(std::string const& title, std::string const& subtitle, std::wstring const& glyph, bool selected,
                        std::function<void()> onClick, std::string const& badge) {
    muxc::Button b;
    b.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
    b.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
    b.Padding(uniform(12));
    b.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(12));
    b.Background(selected ? softGradient() : resource(L"CardBackgroundFillColorDefaultBrush"));
    b.BorderBrush(selected ? pinkBrush() : resource(L"CardStrokeColorDefaultBrush"));
    b.BorderThickness(uniform(selected ? 1.5 : 1));
    auto grid = columns({autoLength(), star(), autoLength()}, 12);
    if (!glyph.empty()) place(grid, glyphBadge(glyph, 32, selected), 0);
    auto labels = vstack(2);
    auto header = hstack(6);
    auto name = text(title, Text::bodyStrong);
    header.Children().Append(name);
    if (!badge.empty()) header.Children().Append(pill(badge, false));
    labels.Children().Append(header);
    if (!subtitle.empty()) labels.Children().Append(secondary(subtitle));
    place(grid, labels, 1);
    if (selected) {
        auto check = icon(L"", 14);
        check.Foreground(pinkBrush());
        check.VerticalAlignment(mux::VerticalAlignment::Top);
        place(grid, check, 2);
    }
    b.Content(grid);
    accessible(b, title + (subtitle.empty() ? "" : ": " + subtitle) + (selected ? " (elegido)" : ""));
    b.Click([onClick](auto&&, auto&&) {
        if (onClick) onClick();
    });
    return b;
}

void setChildren(muxc::Panel const& panel, std::vector<mux::UIElement> const& children) {
    panel.Children().Clear();
    for (auto const& child : children)
        if (child) panel.Children().Append(child);
}

void show(mux::UIElement const& element, bool visible) {
    element.Visibility(visible ? mux::Visibility::Visible : mux::Visibility::Collapsed);
}

muxc::ContentDialog dialog(mux::XamlRoot const& root, std::string const& title, mux::UIElement const& content, std::string const& primary,
                           std::string const& close) {
    muxc::ContentDialog value;
    value.XamlRoot(root);
    value.Title(box_value(hs(title)));
    value.Content(content);
    if (!primary.empty()) {
        value.PrimaryButtonText(hs(primary));
        value.DefaultButton(muxc::ContentDialogButton::Primary);
    }
    value.CloseButtonText(hs(close));
    if (auto s = style(L"DefaultContentDialogStyle")) value.Style(s);
    return value;
}

winrt::fire_and_forget confirm(mux::XamlRoot root, std::string title, std::string message, std::string action, std::function<void()> onConfirm) {
    auto value = dialog(root, title, text(message), action, "Cancelar");
    auto result = co_await value.ShowAsync();
    if (result == muxc::ContentDialogResult::Primary && onConfirm) onConfirm();
}

}  // namespace fcapp::ui
