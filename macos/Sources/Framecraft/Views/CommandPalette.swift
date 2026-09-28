import FramecraftCore
import SwiftUI

/// ⌘K: type to jump anywhere or run any action (screens, templates, stories, past creations, skills).
struct CommandPalette: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @State private var query = ""
    @State private var selection = 0
    @FocusState private var focused: Bool

    struct Item: Identifiable {
        let id: String
        let group: String
        let title: String
        var subtitle: String?
        let symbol: String
        var shortcut: String?
        let run: @MainActor () -> Void
    }

    var body: some View {
        let results = filtered
        VStack(spacing: 0) {
            HStack(spacing: 10) {
                Image(systemName: "magnifyingglass").font(.title3).foregroundStyle(.secondary)
                TextField("Buscá una acción, plantilla, historia o creación…", text: $query)
                    .textFieldStyle(.plain)
                    .font(.title3)
                    .focused($focused)
                    .onSubmit { run(results) }
                    .onChange(of: query) { selection = 0 }
                    .accessibilityLabel("Buscar")
                    .onKeyPress(.downArrow) {
                        selection = min(selection + 1, max(results.count - 1, 0))
                        return .handled
                    }
                    .onKeyPress(.upArrow) {
                        selection = max(selection - 1, 0)
                        return .handled
                    }
                    .onKeyPress(.escape) {
                        dismiss()
                        return .handled
                    }
                Text("esc").font(.caption.monospaced()).foregroundStyle(.tertiary)
            }
            .padding(16)
            Divider()
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 2) {
                        if results.isEmpty {
                            Text("Nada coincide con “\(query)”.")
                                .foregroundStyle(.secondary)
                                .padding(20)
                        }
                        ForEach(Array(results.enumerated()), id: \.element.id) { index, item in
                            if index == 0 || results[index - 1].group != item.group {
                                Text(item.group.uppercased())
                                    .font(.caption2.weight(.bold))
                                    .foregroundStyle(.tertiary)
                                    .padding(.horizontal, 12)
                                    .padding(.top, index == 0 ? 6 : 12)
                                    .padding(.bottom, 2)
                            }
                            row(item, selected: index == selection)
                                .id(item.id)
                                .onTapGesture {
                                    selection = index
                                    run(results)
                                }
                        }
                    }
                    .padding(8)
                }
                .onChange(of: selection) {
                    if results.indices.contains(selection) { proxy.scrollTo(results[selection].id, anchor: .center) }
                }
            }
            Divider()
            HStack(spacing: 14) {
                hint("↑↓", "moverse")
                hint("↩", "abrir")
                hint("esc", "cerrar")
                Spacer()
                Text("\(results.count) resultado\(results.count == 1 ? "" : "s")").font(.caption).foregroundStyle(.secondary)
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 8)
        }
        .frame(width: 640, height: 480)
        .onAppear { focused = true }
    }

    private func row(_ item: Item, selected: Bool) -> some View {
        HStack(spacing: 12) {
            Image(systemName: item.symbol)
                .font(.system(size: 14, weight: .semibold))
                .foregroundStyle(selected ? AnyShapeStyle(Color.white) : AnyShapeStyle(Theme.pink))
                .frame(width: 28, height: 28)
                .background(selected ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Theme.pink.opacity(0.12)),
                            in: RoundedRectangle(cornerRadius: 8, style: .continuous))
            VStack(alignment: .leading, spacing: 1) {
                Text(item.title).font(.callout.weight(.medium)).lineLimit(1)
                if let subtitle = item.subtitle {
                    Text(subtitle).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                }
            }
            Spacer()
            if let shortcut = item.shortcut {
                Text(shortcut).font(.caption.monospaced()).foregroundStyle(.secondary)
            }
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .background(selected ? Theme.pink.opacity(0.12) : .clear, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
        .contentShape(Rectangle())
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(selected ? [.isSelected, .isButton] : [.isButton])
    }

    private func hint(_ key: String, _ text: String) -> some View {
        HStack(spacing: 4) {
            Text(key).font(.caption.monospaced().weight(.semibold))
                .padding(.horizontal, 5).padding(.vertical, 1)
                .background(Color.primary.opacity(0.08), in: RoundedRectangle(cornerRadius: 4))
            Text(text).font(.caption).foregroundStyle(.secondary)
        }
    }

    private func run(_ results: [Item]) {
        guard results.indices.contains(selection) else { return }
        let item = results[selection]
        dismiss()
        item.run()
    }

    // MARK: Items

    private var filtered: [Item] {
        let words = Self.normalize(query).split(separator: " ").map(String.init)
        let all = items
        guard !words.isEmpty else { return all.filter { $0.group != "Creaciones" }.prefix(40).map { $0 } }
        return all.filter { item in
            let haystack = Self.normalize(item.title + " " + (item.subtitle ?? "") + " " + item.group)
            return words.allSatisfy { haystack.contains($0) }
        }
        .prefix(60).map { $0 }
    }

    static func normalize(_ text: String) -> String {
        text.folding(options: [.caseInsensitive, .diacriticInsensitive], locale: .current)
    }

    private var items: [Item] {
        let model = self.model
        var list: [Item] = [
            Item(id: "new-image", group: "Acciones", title: "Nueva imagen", symbol: "photo.badge.plus", shortcut: "⌘N") {
                model.mode = .image
                model.section = .create
            },
            Item(id: "new-video", group: "Acciones", title: "Nuevo video", symbol: "video.badge.plus", shortcut: "⇧⌘N") {
                model.mode = .video
                model.section = .create
            },
            Item(id: "new-story", group: "Acciones", title: "Nueva historia", subtitle: "Brief → escenas con sus prompts",
                 symbol: "film.stack") { model.newStory() },
            Item(id: "assistant", group: "Acciones", title: "Crear prompt con el asistente", symbol: "wand.and.stars", shortcut: "⇧⌘↩") {
                model.section = .create
                model.showAssistant = true
            },
            Item(id: "generate", group: "Acciones", title: "Generar ahora", subtitle: model.settingsSummary,
                 symbol: "sparkles", shortcut: "⌘↩") { Task { await model.generate() } },
            Item(id: "import", group: "Acciones", title: "Agregar referencias", subtitle: "Fotos, videos o audios",
                 symbol: "paperclip.badge.ellipsis") { model.section = .references },
            Item(id: "folder", group: "Acciones", title: "Abrir la carpeta de mis creaciones", symbol: "folder") {
                NSWorkspace.shared.open(model.store.mediaRoot)
            },
            Item(id: "credits", group: "Acciones", title: "Actualizar créditos de KIE", symbol: "bolt.circle") {
                Task { await model.refreshCredits() }
            },
        ]
        for (index, item) in SidebarItem.allCases.enumerated() {
            list.append(Item(id: "go-\(item.rawValue)", group: "Ir a", title: item.title, symbol: item.symbol, shortcut: "⌘\(index + 1)") {
                model.section = item
            })
        }
        list += Templates.creative.map { template in
            Item(id: "tpl-\(template.id)", group: "Plantillas", title: template.title,
                 subtitle: "\(template.media == .video ? "Video" : "Imagen") · \(template.subtitle)", symbol: template.symbol) {
                model.applyTemplate(template)
            }
        }
        list += Templates.stories.map { template in
            Item(id: "stpl-\(template.id)", group: "Plantillas", title: "Historia: \(template.title)", subtitle: template.subtitle,
                 symbol: template.symbol) { model.newStory(from: template) }
        }
        list += model.stories.map { story in
            Item(id: "story-\(story.id)", group: "Historias", title: story.title,
                 subtitle: story.scenes.isEmpty ? "Sin escenas" : "\(story.scenes.count) escenas · \(story.totalDuration) s",
                 symbol: "film.stack") {
                model.selectedStoryID = story.id
                model.section = .stories
            }
        }
        list += model.allSkills.map { skill in
            Item(id: "skill-\(skill.id)", group: "Skills", title: "Usar skill: \(skill.name)", subtitle: skill.summary,
                 symbol: "brain.head.profile") { model.useSkill(skill) }
        }
        list += model.jobs.prefix(200).map { job in
            Item(id: "job-\(job.id)", group: "Creaciones", title: job.prompt.isEmpty ? "Sin prompt" : String(job.prompt.prefix(90)),
                 subtitle: "\(Presets.modelName(job.model)) · \(job.created.formatted(.relative(presentation: .named)))",
                 symbol: job.kind == .video ? "play.rectangle" : "photo") {
                model.detailJobID = job.id
            }
        }
        return list
    }
}
