import SwiftUI

/// Brand colors and reusable styles. Works in light and dark appearance.
enum Theme {
    static let coral = Color(red: 1.0, green: 0.54, blue: 0.30)
    static let pink = Color(red: 1.0, green: 0.24, blue: 0.60)
    static let violet = Color(red: 0.55, green: 0.36, blue: 0.96)
    static let success = Color(red: 0.20, green: 0.74, blue: 0.47)

    static let gradient = LinearGradient(colors: [coral, pink, violet], startPoint: .topLeading, endPoint: .bottomTrailing)
    static let softGradient = LinearGradient(
        colors: [coral.opacity(0.16), pink.opacity(0.14), violet.opacity(0.16)],
        startPoint: .topLeading, endPoint: .bottomTrailing
    )

    static let cardRadius: CGFloat = 14
    static let cardFill = Color(nsColor: .controlBackgroundColor)
    static let hairline = Color.primary.opacity(0.09)
}

/// A rounded card surface.
struct Card<Content: View>: View {
    var padding: CGFloat = 16
    @ViewBuilder var content: Content

    var body: some View {
        content
            .padding(padding)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Theme.cardFill, in: RoundedRectangle(cornerRadius: Theme.cardRadius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: Theme.cardRadius, style: .continuous).strokeBorder(Theme.hairline))
            .shadow(color: .black.opacity(0.05), radius: 6, y: 2)
    }
}

/// Numbered step header: "1 · Describí tu idea".
struct StepHeader: View {
    let number: Int
    let title: String
    var subtitle: String?
    var trailing: AnyView?

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Text("\(number)")
                .font(.system(size: 12, weight: .heavy, design: .rounded))
                .foregroundStyle(.white)
                .frame(width: 22, height: 22)
                .background(Theme.gradient, in: Circle())
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.title3.weight(.semibold))
                if let subtitle {
                    Text(subtitle).font(.callout).foregroundStyle(.secondary)
                }
            }
            Spacer()
            trailing
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Paso \(number): \(title)")
    }
}

/// Big primary action with the brand gradient.
struct GradientButtonStyle: ButtonStyle {
    var height: CGFloat = 44

    func makeBody(configuration: Configuration) -> some View {
        GradientButtonBody(configuration: configuration, height: height)
    }

    private struct GradientButtonBody: View {
        let configuration: Configuration
        let height: CGFloat
        @Environment(\.isEnabled) private var isEnabled
        @State private var hovering = false

        var body: some View {
            configuration.label
                .font(.headline)
                .foregroundStyle(.white)
                .padding(.horizontal, 20)
                .frame(minHeight: height)
                .background(Theme.gradient, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(.white.opacity(0.25)))
                .shadow(color: Theme.pink.opacity(isEnabled ? (hovering ? 0.45 : 0.3) : 0), radius: hovering ? 14 : 9, y: 4)
                .opacity(isEnabled ? 1 : 0.45)
                .scaleEffect(configuration.isPressed ? 0.98 : 1)
                .animation(.snappy(duration: 0.2), value: configuration.isPressed)
                .animation(.easeOut(duration: 0.2), value: hovering)
                .onHover { hovering = $0 }
        }
    }
}

/// Selectable card used for models, presets and skills.
struct ChoiceCard: View {
    let title: String
    var subtitle: String?
    var symbol: String?
    let selected: Bool
    var badge: String?
    let action: () -> Void

    @State private var hovering = false

    var body: some View {
        Button(action: action) {
            HStack(alignment: .top, spacing: 10) {
                if let symbol {
                    Image(systemName: symbol)
                        .font(.system(size: 16, weight: .semibold))
                        .foregroundStyle(selected ? AnyShapeStyle(Color.white) : AnyShapeStyle(Theme.pink))
                        .frame(width: 30, height: 30)
                        .background(selected ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Theme.pink.opacity(0.12)),
                                    in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                }
                VStack(alignment: .leading, spacing: 3) {
                    HStack(spacing: 6) {
                        Text(title).font(.callout.weight(.semibold)).foregroundStyle(.primary)
                        if let badge {
                            Text(badge)
                                .font(.caption2.weight(.bold))
                                .padding(.horizontal, 6).padding(.vertical, 2)
                                .background(Theme.softGradient, in: Capsule())
                        }
                    }
                    if let subtitle {
                        Text(subtitle)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                            .multilineTextAlignment(.leading)
                    }
                }
                Spacer(minLength: 0)
                if selected {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundStyle(Theme.pink)
                        .accessibilityHidden(true)
                }
            }
            .padding(10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .fill(selected ? AnyShapeStyle(Theme.softGradient) : AnyShapeStyle(Color.primary.opacity(hovering ? 0.06 : 0.03)))
            )
            .overlay(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .strokeBorder(selected ? Theme.pink.opacity(0.7) : Theme.hairline, lineWidth: selected ? 1.5 : 1)
            )
            .contentShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        }
        .buttonStyle(.plain)
        .onHover { hovering = $0 }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(selected ? [.isSelected] : [])
    }
}

/// Segmented-style pill choice ("1K", "2K", "4K").
struct PillPicker<Value: Hashable>: View {
    let options: [Value]
    @Binding var selection: Value
    let label: (Value) -> String
    var accessibilityName: String

    var body: some View {
        HStack(spacing: 4) {
            ForEach(options, id: \.self) { option in
                let selected = option == selection
                Button {
                    withAnimation(.snappy(duration: 0.2)) { selection = option }
                } label: {
                    Text(label(option))
                        .font(.callout.weight(selected ? .semibold : .regular))
                        .foregroundStyle(selected ? .white : .primary)
                        .padding(.horizontal, 12)
                        .frame(minWidth: 44, minHeight: 30)
                        .background {
                            if selected {
                                RoundedRectangle(cornerRadius: 8, style: .continuous).fill(Theme.gradient)
                            }
                        }
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("\(accessibilityName): \(label(option))")
                .accessibilityAddTraits(selected ? [.isSelected] : [])
            }
        }
        .padding(3)
        .background(Color.primary.opacity(0.05), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
    }
}

/// Visual aspect-ratio picker: each option is drawn with its real proportions.
struct AspectPicker: View {
    let options: [String]
    @Binding var selection: String

    var body: some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 62), spacing: 8)], spacing: 8) {
            ForEach(options, id: \.self) { option in
                let selected = option == selection
                Button {
                    withAnimation(.snappy(duration: 0.2)) { selection = option }
                } label: {
                    VStack(spacing: 6) {
                        shape(for: option)
                            .frame(width: 34, height: 34)
                        Text(title(option))
                            .font(.caption.weight(selected ? .semibold : .regular))
                            .lineLimit(1)
                            .minimumScaleFactor(0.8)
                    }
                    .padding(.vertical, 8)
                    .frame(maxWidth: .infinity)
                    .background(
                        RoundedRectangle(cornerRadius: 10, style: .continuous)
                            .fill(selected ? AnyShapeStyle(Theme.softGradient) : AnyShapeStyle(Color.primary.opacity(0.03)))
                    )
                    .overlay(
                        RoundedRectangle(cornerRadius: 10, style: .continuous)
                            .strokeBorder(selected ? Theme.pink.opacity(0.7) : Theme.hairline, lineWidth: selected ? 1.5 : 1)
                    )
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .help(hint(for: option))
                .accessibilityLabel("Formato \(title(option))")
                .accessibilityAddTraits(selected ? [.isSelected] : [])
            }
        }
    }

    private func title(_ option: String) -> String {
        switch option {
        case "auto": "Auto"
        case "adaptive": "Adaptativo"
        default: option
        }
    }

    private func hint(for option: String) -> String {
        switch option {
        case "9:16": "Vertical: Reels, TikTok, Shorts, historias"
        case "16:9": "Horizontal: YouTube, pantallas"
        case "1:1": "Cuadrado: feed"
        case "4:5": "Vertical de feed de Instagram"
        case "auto", "adaptive": "El modelo elige el formato"
        default: "Formato \(option)"
        }
    }

    @ViewBuilder
    private func shape(for option: String) -> some View {
        let parts = option.split(separator: ":").compactMap { Double($0) }
        if parts.count == 2, parts[0] > 0, parts[1] > 0 {
            let ratio = parts[0] / parts[1]
            let width = ratio >= 1 ? 30.0 : 30.0 * ratio
            let height = ratio >= 1 ? 30.0 / ratio : 30.0
            RoundedRectangle(cornerRadius: 4, style: .continuous)
                .strokeBorder(selection == option ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Color.secondary), lineWidth: 2)
                .frame(width: max(width, 6), height: max(height, 6))
        } else {
            Image(systemName: "sparkles.rectangle.stack")
                .font(.system(size: 18))
                .foregroundStyle(selection == option ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Color.secondary))
        }
    }
}

/// Text editor with a placeholder, used for prompts and ideas.
struct PromptEditor: View {
    @Binding var text: String
    let placeholder: String
    var minHeight: CGFloat = 140
    var monospaced = false
    var accessibilityName: String

    var body: some View {
        ZStack(alignment: .topLeading) {
            TextEditor(text: $text)
                .font(monospaced ? .system(.body, design: .monospaced) : .body)
                .scrollContentBackground(.hidden)
                .padding(8)
                .accessibilityLabel(accessibilityName)
            if text.isEmpty {
                Text(placeholder)
                    .foregroundStyle(.tertiary)
                    .padding(.horizontal, 13)
                    .padding(.vertical, 8)
                    .allowsHitTesting(false)
                    .accessibilityHidden(true)
            }
        }
        .frame(minHeight: minHeight)
        .background(Color(nsColor: .textBackgroundColor).opacity(0.6), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 10, style: .continuous).strokeBorder(Theme.hairline))
    }
}

/// Transient message shown at the top of the window.
struct BannerView: View {
    let banner: Banner
    let dismiss: () -> Void

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: icon).foregroundStyle(color).font(.title3)
            Text(banner.text)
                .font(.callout)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 8)
            Button(action: dismiss) {
                Image(systemName: "xmark").font(.caption.weight(.bold))
            }
            .buttonStyle(.borderless)
            .accessibilityLabel("Cerrar mensaje")
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 10)
        .frame(maxWidth: 620)
        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(color.opacity(0.4)))
        .shadow(color: .black.opacity(0.15), radius: 12, y: 4)
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(.isStaticText)
    }

    private var icon: String {
        switch banner.style {
        case .success: "checkmark.circle.fill"
        case .info: "info.circle.fill"
        case .error: "exclamationmark.triangle.fill"
        }
    }

    private var color: Color {
        switch banner.style {
        case .success: Theme.success
        case .info: .accentColor
        case .error: .orange
        }
    }
}

extension View {
    /// Gradient text.
    func brandGradientText() -> some View {
        overlay(Theme.gradient).mask(self)
    }
}
