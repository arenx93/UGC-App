import FramecraftCore
import SwiftUI

/// "Empezá con una plantilla": ready-made UGC formats, one click to load them in the assistant.
struct TemplateGallery: View {
    @Environment(AppModel.self) private var model
    @AppStorage("templatesExpanded") private var expanded = true

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Button {
                withAnimation(.snappy) { expanded.toggle() }
            } label: {
                HStack(spacing: 6) {
                    Image(systemName: "square.grid.2x2.fill").foregroundStyle(Theme.gradient)
                    Text("Empezá con una plantilla").font(.headline)
                    Text("\(Templates.creative.count)").font(.caption.weight(.bold)).foregroundStyle(.secondary)
                    Image(systemName: "chevron.right")
                        .font(.caption.weight(.bold))
                        .foregroundStyle(.secondary)
                        .rotationEffect(.degrees(expanded ? 90 : 0))
                    Spacer()
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel(expanded ? "Ocultar plantillas" : "Mostrar plantillas")

            if expanded {
                ScrollView(.horizontal, showsIndicators: false) {
                    LazyHStack(spacing: 10) {
                        ForEach(sorted) { template in
                            TemplateCard(template: template) { model.applyTemplate(template) }
                        }
                    }
                    .padding(.vertical, 4)
                    .padding(.horizontal, 2)
                }
                .scrollClipDisabled()
                .transition(.opacity.combined(with: .move(edge: .top)))
            }
        }
    }

    /// Current mode first.
    private var sorted: [CreativeTemplate] {
        Templates.creative.filter { $0.media == model.mode } + Templates.creative.filter { $0.media != model.mode }
    }
}

struct TemplateCard: View {
    let template: CreativeTemplate
    let action: () -> Void
    @State private var hovering = false

    var body: some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Image(systemName: template.symbol)
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(.white)
                        .frame(width: 30, height: 30)
                        .background(Theme.gradient, in: RoundedRectangle(cornerRadius: 9, style: .continuous))
                        .symbolEffect(.bounce, value: hovering)
                    Spacer()
                    Label(template.media == .video ? "\(template.duration ?? 10) s" : template.aspect,
                          systemImage: template.media == .video ? "video.fill" : "photo.fill")
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(.secondary)
                }
                Text(template.title).font(.callout.weight(.semibold)).foregroundStyle(.primary)
                Text(template.subtitle)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(2)
                    .multilineTextAlignment(.leading)
            }
            .padding(12)
            .frame(width: 176, height: 118, alignment: .topLeading)
            .background(Theme.cardFill.opacity(hovering ? 1 : 0.8), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .strokeBorder(hovering ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Theme.hairline), lineWidth: hovering ? 1.5 : 1)
            )
            .shadow(color: Theme.pink.opacity(hovering ? 0.18 : 0), radius: 12, y: 6)
            .scaleEffect(hovering ? 1.02 : 1)
            .animation(.snappy(duration: 0.2), value: hovering)
            .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
        }
        .buttonStyle(.plain)
        .onHover { hovering = $0 }
        .help(template.idea)
        .accessibilityLabel("Plantilla \(template.title): \(template.subtitle)")
    }
}

/// Story starting points shown when there are no stories yet.
struct StoryTemplateGrid: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 220), spacing: 12)], spacing: 12) {
            ForEach(Templates.stories) { template in
                Button {
                    model.newStory(from: template)
                } label: {
                    HStack(alignment: .top, spacing: 12) {
                        Image(systemName: template.symbol)
                            .font(.system(size: 17, weight: .semibold))
                            .foregroundStyle(.white)
                            .frame(width: 36, height: 36)
                            .background(Theme.gradient, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
                        VStack(alignment: .leading, spacing: 3) {
                            Text(template.title).font(.callout.weight(.semibold))
                            Text(template.subtitle).font(.caption).foregroundStyle(.secondary)
                            Text(template.sceneCount.map { "\($0) escenas" } ?? "Escenas automáticas")
                                .font(.caption2.weight(.semibold)).foregroundStyle(Theme.pink)
                        }
                        Spacer(minLength: 0)
                    }
                    .padding(12)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(Theme.cardFill.opacity(0.86), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).strokeBorder(Theme.hairline))
                    .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
                }
                .buttonStyle(.plain)
                .help(template.brief)
            }
        }
    }
}
