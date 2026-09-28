import FramecraftCore
import SwiftUI

/// "Historias": brief → a pack of scene prompts (like the Walter pack), refined scene by scene.
struct StoriesView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var model = model
        HSplitView {
            List(selection: $model.selectedStoryID) {
                ForEach(model.stories) { story in
                    VStack(alignment: .leading, spacing: 2) {
                        Text(story.title).font(.callout.weight(.medium)).lineLimit(1)
                        Text(story.scenes.isEmpty ? "Sin escenas todavía" : "\(story.scenes.count) escenas · \(story.totalDuration) s")
                            .font(.caption2).foregroundStyle(.secondary)
                    }
                    .padding(.vertical, 2)
                    .tag(story.id as UUID?)
                    .contextMenu {
                        Button("Eliminar historia", role: .destructive) { model.deleteStory(story.id) }
                    }
                }
            }
            .frame(minWidth: 220, idealWidth: 250, maxWidth: 320)
            .overlay {
                if model.stories.isEmpty {
                    Text("Todavía no hay historias").font(.caption).foregroundStyle(.secondary)
                }
            }

            Group {
                if let id = model.selectedStoryID, model.stories.contains(where: { $0.id == id }) {
                    StoryDetail(storyID: id)
                        .id(id)
                } else {
                    ScrollView {
                        VStack(alignment: .leading, spacing: 22) {
                            ScreenHeader(title: "Contá una historia",
                                         subtitle: "Pegá el brief (personajes, lugar, qué pasa, tono, duración) y el asistente la divide en escenas, cada una con su prompt listo, de qué trata y en qué orden cargar las referencias. Después la ajustás escena por escena, generás todo en cadena y armás el video final.")
                            Button { model.newStory() } label: {
                                Label("Historia en blanco", systemImage: "plus").frame(minWidth: 180)
                            }
                            .buttonStyle(GradientButtonStyle(height: 40))
                            Text("O empezá con un formato:").font(.headline)
                            StoryTemplateGrid()
                        }
                        .padding(32)
                        .frame(maxWidth: 820, alignment: .leading)
                        .frame(maxWidth: .infinity)
                    }
                    .background(alignment: .top) { AuraBackground().frame(height: 380).ignoresSafeArea() }
                }
            }
            .frame(minWidth: 560, maxWidth: .infinity, maxHeight: .infinity)
        }
        .navigationTitle("Historias")
        .toolbar {
            ToolbarItem {
                Button { model.newStory() } label: { Label("Nueva historia", systemImage: "plus") }
                    .help("Crear una historia nueva")
            }
        }
        .onAppear { if model.selectedStoryID == nil { model.selectedStoryID = model.stories.first?.id } }
    }
}

struct StoryDetail: View {
    @Environment(AppModel.self) private var model
    let storyID: UUID
    @State private var showBrief = true
    @State private var feedback = ""

    private var story: Story? { model.stories.first { $0.id == storyID } }

    private func binding<T>(_ keyPath: WritableKeyPath<Story, T>, default value: T) -> Binding<T> {
        Binding(
            get: { story?[keyPath: keyPath] ?? value },
            set: { newValue in model.updateStory(storyID) { $0[keyPath: keyPath] = newValue } }
        )
    }

    var body: some View {
        if let story {
            ScrollViewReader { proxy in
                ScrollView {
                    VStack(alignment: .leading, spacing: 18) {
                        header(story)
                        if story.scenes.isEmpty {
                            briefCard(story)
                        } else {
                            DisclosureGroup(isExpanded: $showBrief) { briefCard(story).padding(.top, 8) } label: {
                                Text("Brief, ajustes y referencias").font(.headline)
                            }
                        }
                        progress
                        if !story.scenes.isEmpty {
                            StoryProduction(storyID: storyID) { sceneID in
                                withAnimation(.snappy) { proxy.scrollTo(sceneID, anchor: .top) }
                            }
                            overview(story)
                            ForEach(story.scenes) { scene in
                                SceneCard(storyID: storyID, scene: scene)
                                    .id(scene.id)
                            }
                            conversation(story)
                        }
                    }
                    .padding(24)
                    .frame(maxWidth: 980, alignment: .leading)
                    .frame(maxWidth: .infinity)
                }
            }
            .background(alignment: .top) { AuraBackground(intensity: 0.6).frame(height: 260).ignoresSafeArea() }
            .onAppear { showBrief = story.scenes.isEmpty }
            .onDisappear { model.persist() }
        }
    }

    // MARK: Header

    private func header(_ story: Story) -> some View {
        HStack(alignment: .top) {
            VStack(alignment: .leading, spacing: 6) {
                TextField("Título", text: binding(\.title, default: ""))
                    .font(.system(size: 26, weight: .bold, design: .rounded))
                    .textFieldStyle(.plain)
                    .accessibilityLabel("Título de la historia")
                if !story.scenes.isEmpty {
                    Text("\(story.scenes.count) escenas · \(story.totalDuration) s · \(Presets.videoModelName) · \(story.aspect)")
                        .font(.callout).foregroundStyle(.secondary)
                }
            }
            Spacer()
            if !story.scenes.isEmpty {
                Button { model.exportStory(storyID) } label: { Label("Exportar pack", systemImage: "square.and.arrow.up") }
                    .glassButtonStyle()
                    .help("Guardar todos los prompts en un .txt, como el pack de Walter")
                Button {
                    model.copyToPasteboard(StoryRequests.exportText(story, referenceLines: model.displayLines(story)))
                } label: { Label("Copiar todo", systemImage: "doc.on.doc") }
                    .glassButtonStyle()
            }
        }
    }

    // MARK: Brief

    private func briefCard(_ story: Story) -> some View {
        Card {
            VStack(alignment: .leading, spacing: 14) {
                FieldTitle("Brief de la historia", help: "Todo lo que el asistente necesita: personajes, lugar, qué pasa, tono, arco, diálogo, idioma, duración total.")
                PromptEditor(
                    text: binding(\.brief, default: ""),
                    placeholder: "Ej.: Walter, mozo afroamericano de 82 años en un diner americano. Un cliente lo filma con el celular mientras le cuenta que su hija falleció y que trabaja para pagar deudas. El cliente le deja $100 de propina, afuera lo rechaza, se abrazan… 8 escenas, POV del que filma, diálogo en inglés.",
                    minHeight: 180,
                    accessibilityName: "Brief de la historia"
                )
                HStack(alignment: .top, spacing: 24) {
                    VStack(alignment: .leading, spacing: 6) {
                        FieldTitle("Duración por escena")
                        PillPicker(options: [10, 15, 20, 25, 30], selection: binding(\.sceneDuration, default: 20), label: { "\($0) s" }, accessibilityName: "Duración por escena")
                    }
                    VStack(alignment: .leading, spacing: 6) {
                        FieldTitle("Escenas", help: "Automático: el asistente decide cuántas necesita la historia.")
                        Picker("Escenas", selection: binding(\.sceneCount, default: nil)) {
                            Text("Automático").tag(Int?.none)
                            ForEach(2...12, id: \.self) { Text("\($0)").tag(Int?.some($0)) }
                        }
                        .labelsHidden()
                        .frame(width: 130)
                    }
                }
                HStack(alignment: .top, spacing: 24) {
                    VStack(alignment: .leading, spacing: 6) {
                        FieldTitle("Idioma del diálogo")
                        Picker("Idioma", selection: binding(\.dialogueLanguage, default: "Español")) {
                            ForEach(AssistantView.languages, id: \.self) { Text($0).tag($0) }
                        }
                        .labelsHidden()
                        .frame(width: 190)
                    }
                    VStack(alignment: .leading, spacing: 6) {
                        FieldTitle("Formato y calidad")
                        HStack {
                            Picker("Formato", selection: binding(\.aspect, default: "9:16")) {
                                ForEach(Presets.videoAspects, id: \.self) { Text($0).tag($0) }
                            }
                            .labelsHidden()
                            .frame(width: 110)
                            Picker("Resolución", selection: binding(\.resolution, default: "720p")) {
                                ForEach(Presets.videoResolutions, id: \.self) { Text($0).tag($0) }
                            }
                            .labelsHidden()
                            .frame(width: 100)
                            Toggle("Audio", isOn: binding(\.generateAudio, default: true))
                        }
                    }
                    VStack(alignment: .leading, spacing: 6) {
                        FieldTitle("Skill")
                        Picker("Skill", selection: binding(\.skillID, default: SkillLibrary.ugcID)) {
                            Text("General").tag(SkillLibrary.generalID)
                            ForEach(model.allSkills.filter { $0.media.supports(.video) }) { Text($0.name).tag($0.id) }
                        }
                        .labelsHidden()
                        .frame(maxWidth: 260)
                    }
                }
                Divider()
                StorySlotsEditor(storyID: storyID)
                Divider()
                HStack {
                    Text("Motor: \(model.engineName) · se cambia en el asistente de Crear")
                        .font(.caption).foregroundStyle(.secondary)
                    Spacer()
                    Button {
                        Task {
                            await model.generateStory(storyID)
                            showBrief = false
                        }
                    } label: {
                        Label(story.scenes.isEmpty ? "Crear historia" : "Rehacer desde el brief", systemImage: "wand.and.stars")
                            .frame(minWidth: 180)
                    }
                    .buttonStyle(GradientButtonStyle(height: 40))
                    .disabled(model.storyStatus != nil || story.brief.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
        }
    }


    // MARK: Progress

    @ViewBuilder
    private var progress: some View {
        if let status = model.storyStatus {
            Card {
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        ProgressView().controlSize(.small)
                        Text(status).font(.callout.weight(.semibold))
                    }
                    if let live = model.storyStreaming, !live.isEmpty {
                        Text(live.suffix(1800))
                            .font(.system(.caption, design: .monospaced))
                            .foregroundStyle(.secondary)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    } else {
                        Text("Una historia completa puede tardar uno o dos minutos.").font(.caption).foregroundStyle(.secondary)
                    }
                }
            }
        }
        if let error = model.storyError {
            Label(error, systemImage: "exclamationmark.triangle.fill")
                .font(.callout).foregroundStyle(.orange)
        }
    }

    // MARK: Overview

    private func overview(_ story: Story) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            if !story.summary.isEmpty {
                Card {
                    VStack(alignment: .leading, spacing: 6) {
                        Label("De qué trata", systemImage: "text.book.closed").font(.headline)
                        Text(story.summary).fixedSize(horizontal: false, vertical: true)
                    }
                }
            }
            Card {
                VStack(alignment: .leading, spacing: 8) {
                    Label("Orden de referencias — nunca cambiarlo", systemImage: "list.number").font(.headline)
                    let lines = model.displayLines(story)
                    if lines.isEmpty {
                        Text("Esta historia no usa referencias.").font(.callout).foregroundStyle(.secondary)
                    } else {
                        ForEach(lines, id: \.self) { Text($0).font(.callout.monospaced()) }
                    }
                    if !story.referenceOrder.isEmpty {
                        Text(story.referenceOrder).font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                    Text("El número del @ sigue el orden de la lista, no el nombre del archivo.").font(.caption).foregroundStyle(.tertiary)
                }
            }
            if !story.continuity.isEmpty {
                DisclosureGroup {
                    Text(story.continuity)
                        .font(.system(.callout, design: .monospaced))
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(12)
                        .background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 8))
                } label: {
                    Label("Biblia de continuidad (va idéntica en cada escena)", systemImage: "doc.on.doc").font(.headline)
                }
            }
        }
    }

    // MARK: Conversation

    private func conversation(_ story: Story) -> some View {
        Card {
            VStack(alignment: .leading, spacing: 10) {
                Label("Cambios a toda la historia", systemImage: "bubble.left.and.bubble.right").font(.headline)
                ForEach(story.messages.suffix(8)) { message in
                    HStack(alignment: .top, spacing: 8) {
                        Image(systemName: message.role == .user ? "person.circle.fill" : "sparkles")
                            .foregroundStyle(message.role == .user ? AnyShapeStyle(Color.secondary) : AnyShapeStyle(Theme.gradient))
                        VStack(alignment: .leading, spacing: 2) {
                            if let number = message.sceneNumber {
                                Text("Escena \(number)").font(.caption2.weight(.semibold)).foregroundStyle(.secondary)
                            }
                            Text(message.text).font(.callout).fixedSize(horizontal: false, vertical: true)
                        }
                    }
                }
                HStack {
                    TextField("Ej.: que el arco termine en la escena 6, más humor en la 2, cambiá el diner por una pizzería…", text: $feedback, axis: .vertical)
                        .textFieldStyle(.roundedBorder)
                        .lineLimit(1...4)
                    Button("Enviar") {
                        let text = feedback
                        feedback = ""
                        Task { await model.generateStory(storyID, feedback: text) }
                    }
                    .buttonStyle(GradientButtonStyle(height: 30))
                    .disabled(model.storyStatus != nil || feedback.trimmingCharacters(in: .whitespaces).isEmpty)
                }
                Text("Para cambiar una sola escena usá “Ajustar” dentro de esa escena: es más rápido y no toca las demás.")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }
}

/// The story's fixed reference positions (@Image1, @Video1, @Audio1…).
struct StorySlotsEditor: View {
    @Environment(AppModel.self) private var model
    let storyID: UUID

    var body: some View {
        let story = model.stories.first { $0.id == storyID }
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                FieldTitle("Referencias fijas de la historia", help: "Se cargan en este orden en todas las escenas. Agregá primero los archivos en Referencias.")
                Spacer()
                Menu {
                    ForEach(ReferenceKind.allCases) { kind in
                        Section(kind == .image ? "Imágenes" : kind == .video ? "Videos" : "Audios") {
                            ForEach(model.references(kind)) { reference in
                                Button(reference.name) { add(StoryReferenceSlot(kind: kind, referenceID: reference.id)) }
                            }
                            if model.references(kind).isEmpty { Text("No hay archivos") }
                        }
                    }
                    Divider()
                    Button("Último fotograma de la escena anterior (imagen)") {
                        add(StoryReferenceSlot(kind: .image, isLastFrame: true, note: "Primer fotograma clavado: continúa la escena anterior"))
                    }
                } label: {
                    Label("Agregar", systemImage: "plus")
                }
                .fixedSize()
            }
            if let story, !story.slots.isEmpty {
                ForEach(story.slots) { slot in
                    SlotRow(storyID: storyID, slot: slot, tag: story.tag(for: slot))
                }
            } else {
                Text("Sin referencias. Podés sumar la hoja del personaje, la locación, la voz o el último fotograma de la escena anterior.")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }

    private func add(_ slot: StoryReferenceSlot) {
        model.updateStory(storyID) { $0.slots.append(slot) }
        model.persist()
    }
}

struct SlotRow: View {
    @Environment(AppModel.self) private var model
    let storyID: UUID
    let slot: StoryReferenceSlot
    let tag: String

    var body: some View {
        HStack(spacing: 10) {
            Text(tag)
                .font(.caption.weight(.heavy).monospaced())
                .foregroundStyle(.white)
                .padding(.horizontal, 7).padding(.vertical, 3)
                .background(Theme.gradient, in: Capsule())
                .frame(width: 78, alignment: .leading)
            Group {
                if slot.isLastFrame {
                    Label("Último fotograma de la escena anterior", systemImage: "forward.frame")
                } else if let reference = model.references.first(where: { $0.id == slot.referenceID }) {
                    HStack(spacing: 6) {
                        if reference.kind == .image {
                            Thumbnail(url: model.url(for: reference), maxPixel: 120)
                                .frame(width: 28, height: 28)
                                .clipShape(RoundedRectangle(cornerRadius: 5))
                        }
                        Text(reference.name).lineLimit(1)
                    }
                } else {
                    Label("Archivo no encontrado", systemImage: "exclamationmark.triangle").foregroundStyle(.orange)
                }
            }
            .font(.callout)
            .frame(width: 260, alignment: .leading)
            TextField("Para qué sirve (ej.: identidad de Walter, locación, voz)", text: Binding(
                get: { slot.note },
                set: { value in model.updateStory(storyID) { story in
                    if let index = story.slots.firstIndex(where: { $0.id == slot.id }) { story.slots[index].note = value }
                } }
            ))
            .textFieldStyle(.roundedBorder)
            Button { move(-1) } label: { Image(systemName: "chevron.up") }.buttonStyle(.borderless).help("Subir")
            Button { move(1) } label: { Image(systemName: "chevron.down") }.buttonStyle(.borderless).help("Bajar")
            Button {
                model.updateStory(storyID) { $0.slots.removeAll { $0.id == slot.id } }
                model.persist()
            } label: { Image(systemName: "trash") }
                .buttonStyle(.borderless)
                .help("Quitar")
        }
    }

    private func move(_ offset: Int) {
        model.updateStory(storyID) { story in
            let same = story.slots.enumerated().filter { $0.element.kind == slot.kind }.map(\.offset)
            guard let position = same.firstIndex(where: { story.slots[$0].id == slot.id }),
                  same.indices.contains(position + offset) else { return }
            story.slots.swapAt(same[position], same[position + offset])
        }
        model.persist()
    }
}

/// One scene of the pack: what it is about, its prompt, checks and actions.
struct SceneCard: View {
    @Environment(AppModel.self) private var model
    let storyID: UUID
    let scene: StoryScene
    @State private var showPrompt = false
    @State private var note = ""

    var body: some View {
        let story = model.stories.first { $0.id == storyID }
        let report = PromptLinter.lintVideo(
            scene.prompt, duration: scene.duration,
            references: .init(
                images: story?.slots(.image).count ?? 0,
                videos: story?.slots(.video).count ?? 0,
                audios: story?.slots(.audio).count ?? 0
            ),
            ugcStyle: story?.skillID != SkillLibrary.arthasID
        )
        let job = model.latestJob(for: scene)
        Card {
            VStack(alignment: .leading, spacing: 12) {
                HStack(alignment: .firstTextBaseline, spacing: 10) {
                    Text("\(scene.number)")
                        .font(.system(size: 13, weight: .heavy, design: .rounded))
                        .foregroundStyle(.white)
                        .frame(width: 26, height: 26)
                        .background(Theme.gradient, in: Circle())
                    VStack(alignment: .leading, spacing: 2) {
                        Text(scene.title).font(.title3.weight(.semibold))
                        Text("\(scene.duration) s · \(usedTags.isEmpty ? "sin referencias" : usedTags.joined(separator: " "))")
                            .font(.caption.monospaced()).foregroundStyle(.secondary)
                    }
                    Spacer()
                    if let job { JobChip(job: job) }
                }
                if !scene.summary.isEmpty {
                    Text(scene.summary).fixedSize(horizontal: false, vertical: true)
                }
                if let notes = scene.notes {
                    Label(notes, systemImage: "lightbulb").font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                }
                HStack(spacing: 12) {
                    checkBadge(report)
                    Label("\(report.dialogueWords)/\(report.targetWords) palabras", systemImage: "quote.bubble")
                        .font(.caption).foregroundStyle(.secondary)
                }
                DisclosureGroup(isExpanded: $showPrompt) {
                    PromptEditor(
                        text: Binding(
                            get: { scene.prompt },
                            set: { value in model.updateScene(story: storyID, scene: scene.id) { $0.prompt = value } }
                        ),
                        placeholder: "", minHeight: 260, monospaced: true, accessibilityName: "Prompt de la escena \(scene.number)"
                    )
                    .padding(.top, 6)
                } label: {
                    Text("Prompt (\(scene.prompt.count.formatted()) caracteres)").font(.callout.weight(.semibold))
                }
                HStack(spacing: 8) {
                    Button { model.copyToPasteboard(scene.prompt) } label: { Label("Copiar", systemImage: "doc.on.doc") }
                    Button {
                        Task { await model.openScene(story: storyID, scene: scene.id, generate: false) }
                    } label: { Label("Llevar a Crear", systemImage: "arrow.right.circle") }
                        .help("Carga el prompt, la duración y las referencias en orden en la pantalla Crear")
                    Button {
                        Task { await model.openScene(story: storyID, scene: scene.id, generate: true) }
                    } label: { Label("Generar ahora", systemImage: "sparkles") }
                        .help("Envía la escena a Seedance con sus referencias")
                        .disabled(!model.hasKieKey)
                    if let job, job.status == .success {
                        Button { model.detailJobID = job.id } label: { Label("Ver video", systemImage: "play.circle") }
                    }
                }
                .glassButtonStyle()
                HStack {
                    TextField("Ajustar esta escena: ej. “que Walter tarde más en responder”, “sacá la moza”…", text: $note, axis: .vertical)
                        .textFieldStyle(.roundedBorder)
                        .lineLimit(1...3)
                        .onSubmit(refine)
                    Button("Ajustar", action: refine)
                        .disabled(model.storyStatus != nil || note.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
    }

    private var usedTags: [String] {
        ReferenceKind.allCases.flatMap { kind in
            (0..<PromptLinter.highestTag(kind.tagPrefix, in: scene.prompt)).map { "\(kind.tagPrefix)\($0 + 1)" }
        }
    }

    private func refine() {
        let text = note
        guard !text.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        note = ""
        Task { await model.refineScene(story: storyID, scene: scene.id, feedback: text) }
    }

    private func checkBadge(_ report: LintReport) -> some View {
        let problems = report.problems, warnings = report.warnings
        let text = problems > 0 ? "\(problems) problema\(problems == 1 ? "" : "s")" : warnings > 0 ? "\(warnings) sugerencia\(warnings == 1 ? "" : "s")" : "Revisión OK"
        return Label(text, systemImage: problems > 0 ? "xmark.octagon.fill" : warnings > 0 ? "exclamationmark.triangle.fill" : "checkmark.seal.fill")
            .font(.caption.weight(.semibold))
            .foregroundStyle(problems > 0 ? Color.red : warnings > 0 ? Color.orange : Theme.success)
            .help(report.items.filter { $0.state == .warning || $0.state == .problem }.map { "• \($0.title)" }.joined(separator: "\n"))
    }
}

struct JobChip: View {
    let job: Job

    var body: some View {
        let (text, color): (String, Color) = switch job.status {
        case .success: ("Video listo", Theme.success)
        case .fail, .unknown: ("Falló", .orange)
        default: ("Generando \(job.progress)%", Theme.pink)
        }
        Text(text)
            .font(.caption.weight(.semibold))
            .padding(.horizontal, 8).padding(.vertical, 3)
            .background(color.opacity(0.15), in: Capsule())
            .foregroundStyle(color)
    }
}

/// Storyboard of the scenes plus the production actions: generate everything and assemble the final video.
struct StoryProduction: View {
    @Environment(AppModel.self) private var model
    let storyID: UUID
    let onSelect: (UUID) -> Void

    var body: some View {
        if let story = model.stories.first(where: { $0.id == storyID }) {
            let ready = story.scenes.filter { model.finishedClip(for: $0) != nil }.count
            let running = model.storyRun?.storyID == storyID ? model.storyRun : nil
            Card {
                VStack(alignment: .leading, spacing: 14) {
                    HStack(alignment: .firstTextBaseline) {
                        Label("Storyboard", systemImage: "rectangle.split.3x1").font(.headline)
                        Text("\(ready) de \(story.scenes.count) escenas con video")
                            .font(.caption).foregroundStyle(.secondary)
                            .contentTransition(.numericText())
                        Spacer()
                    }
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(alignment: .top, spacing: 8) {
                            ForEach(story.scenes) { scene in
                                StoryboardTile(scene: scene, clip: model.finishedClip(for: scene), latest: model.latestJob(for: scene),
                                               current: running?.current == scene.number)
                                    .frame(width: max(96, CGFloat(scene.duration) * 7))
                                    .onTapGesture { onSelect(scene.id) }
                            }
                        }
                        .padding(.vertical, 2)
                    }
                    .scrollClipDisabled()
                    ProgressView(value: Double(ready), total: Double(max(story.scenes.count, 1)))
                        .tint(Theme.pink)
                        .accessibilityLabel("\(ready) de \(story.scenes.count) escenas listas")
                    HStack(spacing: 10) {
                        if let running {
                            ProgressView().controlSize(.small)
                            Text(running.text).font(.callout).lineLimit(2)
                            Spacer()
                            Button("Detener") { model.cancelStoryRun() }
                                .glassButtonStyle()
                        } else {
                            Button {
                                model.generateAllScenes(storyID)
                            } label: {
                                Label(ready == 0 ? "Generar todas las escenas" : "Generar las que faltan", systemImage: "sparkles.rectangle.stack")
                            }
                            .buttonStyle(GradientButtonStyle(height: 38))
                            .disabled(ready == story.scenes.count || !model.hasKieKey || model.storyRun != nil)
                            .help(story.slots.contains(where: \.isLastFrame)
                                  ? "Envía las escenas en orden: cada una espera a la anterior para usar su último fotograma."
                                  : "Envía todas las escenas sin video a Seedance.")
                            Button {
                                Task { await model.assembleStory(storyID) }
                            } label: {
                                if model.assemblingStoryID == storyID {
                                    HStack(spacing: 6) { ProgressView().controlSize(.small); Text("Armando…") }
                                } else {
                                    Label("Armar video final", systemImage: "film")
                                }
                            }
                            .glassButtonStyle()
                            .controlSize(.large)
                            .disabled(ready < story.scenes.count || model.assemblingStoryID != nil)
                            .help(ready < story.scenes.count
                                  ? "Disponible cuando todas las escenas tengan video."
                                  : "Une todas las escenas en un solo MP4 (\(story.totalDuration) s), en tu Mac y sin gastar créditos.")
                            if let finalID = story.finalCutJobID, model.jobs.contains(where: { $0.id == finalID }) {
                                Button {
                                    model.detailJobID = finalID
                                } label: {
                                    Label("Ver video final", systemImage: "play.rectangle.fill")
                                }
                                .glassButtonStyle()
                                .controlSize(.large)
                            }
                            Spacer()
                        }
                    }
                }
            }
        }
    }
}

struct StoryboardTile: View {
    let scene: StoryScene
    let clip: Job?
    let latest: Job?
    let current: Bool
    @Environment(AppModel.self) private var model
    @State private var hovering = false

    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            ZStack {
                if let clip, let url = model.outputURLs(clip).first {
                    Thumbnail(url: url, video: true, maxPixel: 300)
                } else {
                    Theme.softGradient
                    Text("\(scene.number)")
                        .font(.system(size: 26, weight: .heavy, design: .rounded))
                        .brandGradientText()
                }
            }
            .frame(height: 96)
            .frame(maxWidth: .infinity)
            .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
            .overlay(alignment: .bottomLeading) { status.padding(5) }
            .overlay(alignment: .topLeading) {
                Text("\(scene.duration) s")
                    .font(.caption2.weight(.bold).monospacedDigit())
                    .foregroundStyle(.white)
                    .padding(.horizontal, 5).padding(.vertical, 2)
                    .background(.black.opacity(0.45), in: Capsule())
                    .padding(5)
            }
            .overlay(
                RoundedRectangle(cornerRadius: 10, style: .continuous)
                    .strokeBorder(current ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(hovering ? Theme.pink.opacity(0.6) : Theme.hairline),
                                  lineWidth: current ? 2.5 : 1)
            )
            Text("\(scene.number). \(scene.title)").font(.caption.weight(.medium)).lineLimit(1)
        }
        .contentShape(Rectangle())
        .onHover { hovering = $0 }
        .help(scene.summary)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Escena \(scene.number): \(scene.title)")
        .accessibilityAddTraits(.isButton)
    }

    @ViewBuilder
    private var status: some View {
        if clip != nil {
            Image(systemName: "checkmark.circle.fill").foregroundStyle(.white, Theme.success).font(.callout)
        } else if let latest, latest.status.isActive {
            Text("\(latest.progress)%")
                .font(.caption2.weight(.bold).monospacedDigit())
                .foregroundStyle(.white)
                .padding(.horizontal, 5).padding(.vertical, 2)
                .background(Theme.pink, in: Capsule())
        } else if let latest, latest.status == .fail || latest.status == .unknown {
            Image(systemName: "exclamationmark.triangle.fill").foregroundStyle(.white, .orange).font(.callout)
        }
    }
}
