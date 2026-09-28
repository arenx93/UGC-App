import FramecraftCore
import SwiftUI
import UniformTypeIdentifiers

struct CreateView: View {
    @Environment(AppModel.self) private var model
    @State private var dropTargeted = false

    var body: some View {
        @Bindable var model = model
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                header
                TemplateGallery()
                ModePicker()
                PromptSection()
                if model.mode == .video, let report = model.lintReport {
                    CheckerCard(report: report)
                }
                ReferencesSection()
                SettingsSection()
            }
            .padding(.horizontal, 28)
            .padding(.top, 24)
            .padding(.bottom, 110)
            .frame(maxWidth: 880)
            .frame(maxWidth: .infinity)
        }
        .background(alignment: .top) { AuraBackground().frame(height: 420).ignoresSafeArea() }
        .overlay(alignment: .bottom) { GenerateBar() }
        .overlay {
            if dropTargeted {
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .strokeBorder(Theme.gradient, style: StrokeStyle(lineWidth: 3, dash: [10, 6]))
                    .background(Theme.softGradient.opacity(0.5), in: RoundedRectangle(cornerRadius: 18, style: .continuous))
                    .overlay {
                        Label("Soltá para agregar como referencia", systemImage: "square.and.arrow.down")
                            .font(.title2.weight(.semibold))
                            .padding(18)
                            .background(.regularMaterial, in: Capsule())
                    }
                    .padding(12)
                    .allowsHitTesting(false)
            }
        }
        .dropDestination(for: URL.self) { urls, _ in
            Task { await model.importFiles(urls) }
            return true
        } isTargeted: { dropTargeted = $0 }
        .inspector(isPresented: $model.showAssistant) {
            AssistantView()
                .inspectorColumnWidth(min: 330, ideal: 390, max: 560)
        }
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button {
                    withAnimation { model.showAssistant.toggle() }
                } label: {
                    Label("Asistente de prompts", systemImage: "wand.and.stars.inverse")
                }
                .help("Mostrar u ocultar el asistente (⌥⌘I)")
            }
        }
        .navigationTitle("Crear")
    }

    private var header: some View {
        HStack(alignment: .top) {
            ScreenHeader(title: "¿Qué querés crear hoy?",
                         subtitle: "Elegí imagen o video, describí la escena y generá. Todo se guarda en Imágenes ▸ Framecraft.")
            Spacer()
            Button {
                model.showCommandPalette = true
            } label: {
                Label("Buscar o hacer…", systemImage: "command")
                    .font(.callout)
            }
            .glassButtonStyle()
            .help("Paleta de comandos (⌘K): acciones, plantillas, historias y creaciones")
        }
    }
}

// MARK: - Mode

struct ModePicker: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        HStack(spacing: 12) {
            card(.image, title: "Imagen", subtitle: "Fotos de producto, selfies y posts", symbol: "photo.fill")
            card(.video, title: "Video", subtitle: "Clips UGC con Seedance 2.5, con voz y sonido", symbol: "video.fill")
        }
    }

    private func card(_ kind: MediaKind, title: String, subtitle: String, symbol: String) -> some View {
        let selected = model.mode == kind
        return Button {
            withAnimation(.snappy) { model.mode = kind }
        } label: {
            HStack(spacing: 14) {
                Image(systemName: symbol)
                    .font(.system(size: 22, weight: .semibold))
                    .foregroundStyle(selected ? Color.white : Theme.pink)
                    .frame(width: 48, height: 48)
                    .background(selected ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Theme.pink.opacity(0.12)),
                                in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                VStack(alignment: .leading, spacing: 3) {
                    Text(title).font(.title3.weight(.bold))
                    Text(subtitle).font(.callout).foregroundStyle(.secondary).multilineTextAlignment(.leading)
                }
                Spacer(minLength: 0)
            }
            .padding(14)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .fill(selected ? AnyShapeStyle(Theme.softGradient) : AnyShapeStyle(Theme.cardFill))
            )
            .overlay(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .strokeBorder(selected ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Theme.hairline), lineWidth: selected ? 2 : 1)
            )
            .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Crear \(title.lowercased()): \(subtitle)")
        .accessibilityAddTraits(selected ? [.isSelected] : [])
        .keyboardShortcut(KeyEquivalent(kind == .image ? "1" : "2"), modifiers: [.command, .option])
    }
}

// MARK: - Step 1: prompt

struct PromptSection: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var model = model
        let limit = model.mode == .image ? Presets.maxPromptLength(imageModel: model.imageModel) : Presets.maxVideoPromptLength
        Card {
            VStack(alignment: .leading, spacing: 12) {
                StepHeader(
                    number: 1, title: "Describí tu idea",
                    subtitle: model.mode == .image
                        ? "Quién aparece, dónde, con qué luz y en qué estilo."
                        : "Qué pasa segundo a segundo, qué se dice y cómo se graba."
                )
                PromptEditor(
                    text: $model.prompt,
                    placeholder: model.mode == .image
                        ? "Ej.: Selfie espontánea de una chica tomando un café en una ventana con lluvia, luz de tarde, piel real…"
                        : "Ej.: 0–5 s: una chica levanta el frasco frente al espejo del baño y dice \"lo uso hace un mes\"…",
                    minHeight: 170,
                    accessibilityName: "Prompt"
                )
                if model.mode == .video, !model.selectedImages.isEmpty || !model.selectedVideos.isEmpty || !model.selectedAudios.isEmpty {
                    TagInsertRow()
                }
                if model.mode == .image, model.prompt.trimmingCharacters(in: .whitespaces).hasPrefix("{") {
                    Label("Prompt JSON detectado: los presets de cámara y encuadre se integran dentro del perfil.", systemImage: "curlybraces")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                HStack(spacing: 10) {
                    Button {
                        if model.idea.isEmpty { model.idea = model.prompt }
                        withAnimation { model.showAssistant = true }
                    } label: {
                        Label("Pedir ayuda al asistente", systemImage: "wand.and.stars")
                    }
                    .help("El asistente convierte una idea suelta en un prompt profesional")
                    Button {
                        model.showPromptPreview = true
                    } label: {
                        Label("Vista previa", systemImage: "doc.text.magnifyingglass")
                    }
                    .help("Ver el texto exacto que se envía al modelo")
                    if !model.prompt.isEmpty {
                        Button {
                            model.prompt = ""
                        } label: {
                            Label("Borrar", systemImage: "xmark.circle")
                        }
                        .help("Vaciar el prompt")
                    }
                    Spacer()
                    Text("\(model.prompt.count.formatted()) / \(limit.formatted())")
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(model.prompt.count > limit ? Color.red : Color.secondary)
                        .accessibilityLabel("\(model.prompt.count) de \(limit) caracteres")
                }
                .glassButtonStyle()
                .controlSize(.regular)
            }
        }
    }
}

/// Buttons that insert @Image1 / @Video1 / @Audio1 at the end of the prompt.
struct TagInsertRow: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 6) {
                Text("Insertar:").font(.caption).foregroundStyle(.secondary)
                ForEach(ReferenceKind.allCases) { kind in
                    ForEach(model.selectedReferences(kind)) { reference in
                        if let tag = model.tag(for: reference) {
                            Button {
                                model.insertTag(tag)
                            } label: {
                                Text(tag).font(.caption.weight(.semibold).monospaced())
                                    .padding(.horizontal, 8).padding(.vertical, 4)
                                    .background(Theme.softGradient, in: Capsule())
                                    .overlay(Capsule().strokeBorder(Theme.pink.opacity(0.35)))
                            }
                            .buttonStyle(.plain)
                            .help("\(tag) = \(reference.name)")
                            .accessibilityLabel("Insertar \(tag), \(reference.name)")
                        }
                    }
                }
            }
        }
    }
}

// MARK: - Checker

struct CheckerCard: View {
    @Environment(AppModel.self) private var model
    let report: LintReport
    @State private var expanded = true

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 12) {
                Button {
                    withAnimation(.snappy) { expanded.toggle() }
                } label: {
                    HStack(spacing: 10) {
                        Image(systemName: report.problems > 0 ? "exclamationmark.octagon.fill" : report.warnings > 0 ? "exclamationmark.triangle.fill" : "checkmark.seal.fill")
                            .font(.title2)
                            .foregroundStyle(report.problems > 0 ? Color.red : report.warnings > 0 ? Color.orange : Theme.success)
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Revisión del prompt").font(.headline)
                            Text(summary).font(.caption).foregroundStyle(.secondary)
                        }
                        Spacer()
                        Image(systemName: "chevron.down")
                            .rotationEffect(.degrees(expanded ? 0 : -90))
                            .foregroundStyle(.secondary)
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Revisión del prompt. \(summary)")
                .accessibilityHint(expanded ? "Contraer" : "Expandir")

                if expanded {
                    DialogueMeter(words: report.dialogueWords, target: report.targetWords, seconds: model.videoDuration)
                    VStack(alignment: .leading, spacing: 10) {
                        ForEach(report.items) { item in
                            HStack(alignment: .top, spacing: 10) {
                                Image(systemName: icon(item.state))
                                    .foregroundStyle(color(item.state))
                                    .frame(width: 18)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(item.title).font(.callout.weight(.semibold))
                                    Text(item.detail).font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                                }
                            }
                            .accessibilityElement(children: .combine)
                        }
                    }
                    Text("Basado en el método del kit de prompts de Seedance. Es una guía: vos decidís.")
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                }
            }
        }
    }

    private var summary: String {
        var parts = ["\(report.passed) en orden"]
        if report.warnings > 0 { parts.append("\(report.warnings) sugerencia\(report.warnings == 1 ? "" : "s")") }
        if report.problems > 0 { parts.append("\(report.problems) problema\(report.problems == 1 ? "" : "s")") }
        return parts.joined(separator: " · ")
    }

    private func icon(_ state: LintItem.State) -> String {
        switch state {
        case .ok: "checkmark.circle.fill"
        case .warning: "exclamationmark.triangle.fill"
        case .problem: "xmark.octagon.fill"
        case .info: "info.circle.fill"
        }
    }

    private func color(_ state: LintItem.State) -> Color {
        switch state {
        case .ok: Theme.success
        case .warning: .orange
        case .problem: .red
        case .info: .blue
        }
    }
}

/// Dialogue words vs. what fits in the clip (2.47 words per second).
struct DialogueMeter: View {
    let words: Int
    let target: Int
    let seconds: Int

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Label("Diálogo", systemImage: "quote.bubble")
                    .font(.caption.weight(.semibold))
                Spacer()
                Text("\(words) de ~\(target) palabras para \(seconds) s")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.secondary)
            }
            GeometryReader { proxy in
                let fraction = min(Double(words) / Double(max(target, 1)), 1.4) / 1.4
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.primary.opacity(0.08))
                    // Ideal band: 75%–125% of the target.
                    Capsule()
                        .fill(Theme.success.opacity(0.22))
                        .frame(width: proxy.size.width * (0.5 / 1.4))
                        .offset(x: proxy.size.width * (0.75 / 1.4))
                    Capsule()
                        .fill(Theme.gradient)
                        .frame(width: max(6, proxy.size.width * fraction))
                }
            }
            .frame(height: 8)
            .accessibilityElement()
            .accessibilityLabel("Diálogo: \(words) palabras de unas \(target) recomendadas")
        }
    }
}

// MARK: - Step 2: references

struct ReferencesSection: View {
    @Environment(AppModel.self) private var model
    @State private var importing = false

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 14) {
                StepHeader(
                    number: 2, title: "Referencias",
                    subtitle: model.mode == .image
                        ? "Opcional. Hasta 4 fotos de tu producto, persona o estilo."
                        : "Opcional. Se nombran @Image1, @Video1, @Audio1… en este orden dentro del prompt.",
                    trailing: AnyView(
                        Button {
                            importing = true
                        } label: {
                            Label("Agregar archivos", systemImage: "plus")
                        }
                        .glassButtonStyle()
                        .keyboardShortcut("o")
                        .help("Elegir imágenes, videos o audios (⌘O). También podés arrastrarlos a la ventana.")
                    )
                )
                if model.references.isEmpty {
                    DropHint { importing = true }
                } else {
                    ForEach(model.mode == .image ? [ReferenceKind.image] : ReferenceKind.allCases) { kind in
                        ReferenceStrip(kind: kind)
                    }
                }
                if model.isImporting {
                    ProgressView("Importando…").controlSize(.small)
                }
            }
        }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.image, .movie, .audio], allowsMultipleSelection: true) { result in
            if case .success(let urls) = result {
                Task { await model.importFiles(urls) }
            }
        }
    }
}

struct DropHint: View {
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(spacing: 8) {
                Image(systemName: "square.and.arrow.down.on.square")
                    .font(.system(size: 26))
                    .foregroundStyle(Theme.gradient)
                Text("Arrastrá archivos acá o hacé clic para elegir").font(.callout.weight(.semibold))
                Text("Imágenes PNG/JPG/WebP (30 MB) · Videos MP4/MOV (200 MB, 30 s) · Audio MP3/WAV/M4A (15 MB, 30 s)")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 22)
            .background(Theme.softGradient.opacity(0.5), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(Theme.pink.opacity(0.4), style: StrokeStyle(lineWidth: 1.5, dash: [7, 5])))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Agregar archivos de referencia")
    }
}

/// One row per reference kind: selected files first (in tag order), then the rest.
struct ReferenceStrip: View {
    @Environment(AppModel.self) private var model
    let kind: ReferenceKind

    var body: some View {
        let selected = model.selectedReferences(kind)
        let others = model.references(kind).filter { !model.isSelected($0) }
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Label(title, systemImage: symbol).font(.callout.weight(.semibold))
                Spacer()
                Text(counter(selected.count))
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.secondary)
            }
            if selected.isEmpty && others.isEmpty {
                Text("Todavía no agregaste \(kind == .image ? "imágenes" : kind == .video ? "videos" : "audios").")
                    .font(.caption)
                    .foregroundStyle(.tertiary)
            } else {
                ScrollView(.horizontal, showsIndicators: true) {
                    HStack(spacing: 10) {
                        ForEach(selected + others) { reference in
                            ReferenceTile(reference: reference)
                        }
                    }
                    .padding(.vertical, 6)
                    .padding(.horizontal, 2)
                }
            }
        }
    }

    private var title: String {
        switch kind {
        case .image: "Imágenes"
        case .video: "Videos"
        case .audio: "Audios"
        }
    }

    private var symbol: String {
        switch kind {
        case .image: "photo"
        case .video: "film"
        case .audio: "waveform"
        }
    }

    private func counter(_ count: Int) -> String {
        var text = "\(count)/\(model.selectionLimit(kind)) en uso"
        if kind != .image { text += " · \(MediaTools.formattedDuration(model.selectedSeconds(kind))) de 30 s" }
        return text
    }
}

struct ReferenceTile: View {
    @Environment(AppModel.self) private var model
    let reference: ReferenceFile
    @State private var hovering = false

    var body: some View {
        let tag = model.tag(for: reference)
        Button {
            withAnimation(.snappy(duration: 0.2)) { model.toggleSelection(reference) }
        } label: {
            VStack(alignment: .leading, spacing: 5) {
                ZStack(alignment: .topLeading) {
                    preview
                        .frame(width: 104, height: 104)
                        .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
                        .overlay(
                            RoundedRectangle(cornerRadius: 10, style: .continuous)
                                .strokeBorder(tag != nil ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Theme.hairline), lineWidth: tag != nil ? 3 : 1)
                        )
                        .opacity(tag != nil || hovering ? 1 : 0.7)
                    if let tag {
                        Text(tag)
                            .font(.caption2.weight(.heavy).monospaced())
                            .foregroundStyle(.white)
                            .padding(.horizontal, 6).padding(.vertical, 3)
                            .background(Theme.gradient, in: Capsule())
                            .padding(6)
                    }
                }
                Text(reference.name)
                    .font(.caption2)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .frame(width: 104, alignment: .leading)
                    .foregroundStyle(.secondary)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .onHover { hovering = $0 }
        .help(tag == nil ? "Clic para usar \(reference.name)" : "\(tag!) · clic para dejar de usarla")
        .accessibilityLabel("\(reference.name)\(tag.map { ", en uso como \($0)" } ?? ", sin usar")")
        .accessibilityHint("Activa o desactiva esta referencia")
        .contextMenu {
            if tag != nil {
                Button("Mover antes") { model.moveSelection(reference.kind, id: reference.id, by: -1) }
                Button("Mover después") { model.moveSelection(reference.kind, id: reference.id, by: 1) }
                if let tag { Button("Insertar \(tag) en el prompt") { model.insertTag(tag) } }
                Divider()
            }
            Button("Vista rápida") { model.quickLookURL = model.url(for: reference) }
            Button("Mostrar en Finder") { model.revealInFinder([model.url(for: reference)]) }
            Divider()
            Button("Eliminar de la biblioteca", role: .destructive) { model.deleteReferences([reference.id]) }
        }
    }

    @ViewBuilder
    private var preview: some View {
        switch reference.kind {
        case .image:
            Thumbnail(url: model.url(for: reference), maxPixel: 300)
        case .video:
            ZStack {
                Thumbnail(url: model.url(for: reference), video: true, maxPixel: 300)
                Image(systemName: "play.circle.fill").font(.title).foregroundStyle(.white).shadow(radius: 4)
                durationBadge
            }
        case .audio:
            ZStack {
                Theme.softGradient
                Image(systemName: "waveform").font(.system(size: 30, weight: .semibold)).foregroundStyle(Theme.gradient)
                durationBadge
            }
        }
    }

    private var durationBadge: some View {
        VStack {
            Spacer()
            HStack {
                Spacer()
                Text(MediaTools.formattedDuration(reference.durationSeconds))
                    .font(.caption2.weight(.semibold).monospacedDigit())
                    .foregroundStyle(.white)
                    .padding(.horizontal, 5).padding(.vertical, 2)
                    .background(.black.opacity(0.6), in: Capsule())
                    .padding(5)
            }
        }
    }
}

// MARK: - Step 3: settings

struct SettingsSection: View {
    @Environment(AppModel.self) private var model
    @State private var showStyle = false

    var body: some View {
        @Bindable var model = model
        Card {
            VStack(alignment: .leading, spacing: 18) {
                StepHeader(number: 3, title: "Ajustes", subtitle: model.mode == .image ? "Modelo, calidad y formato." : "Calidad, formato, duración y audio.")
                if model.mode == .image {
                    imageSettings
                } else {
                    videoSettings
                }
            }
        }
    }

    @ViewBuilder
    private var imageSettings: some View {
        @Bindable var model = model
        FieldTitle("Modelo")
        LazyVGrid(columns: [GridItem(.flexible(), spacing: 10), GridItem(.flexible(), spacing: 10)], spacing: 10) {
            ForEach(Presets.imageModels) { item in
                ChoiceCard(title: item.name, subtitle: item.note, symbol: item.symbol, selected: model.imageModel == item.id) {
                    withAnimation(.snappy) { model.imageModel = item.id }
                }
            }
        }
        HStack(alignment: .top, spacing: 28) {
            VStack(alignment: .leading, spacing: 8) {
                FieldTitle("Resolución", help: "Más resolución = más detalle y más créditos.")
                PillPicker(options: Presets.imageResolutions, selection: $model.imageResolution, label: { $0 }, accessibilityName: "Resolución")
            }
            VStack(alignment: .leading, spacing: 8) {
                FieldTitle("Cantidad", help: "Las imágenes de un lote se generan en paralelo.")
                PillPicker(options: [1, 2, 3, 4], selection: $model.quantity, label: { "\($0)" }, accessibilityName: "Cantidad de imágenes")
            }
        }
        FieldTitle("Formato")
        AspectPicker(options: model.aspectOptions, selection: $model.imageAspect)
        DisclosureGroup(isExpanded: $showStyle) {
            VStack(alignment: .leading, spacing: 12) {
                FieldTitle("Cámara", help: "Se agrega al prompt automáticamente.")
                presetGrid(Presets.cameras, selection: $model.camera)
                FieldTitle("Encuadre")
                presetGrid(Presets.films, selection: $model.film)
            }
            .padding(.top, 8)
        } label: {
            HStack {
                Text("Estilo de cámara y encuadre").font(.callout.weight(.semibold))
                if model.camera != Presets.none || model.film != Presets.none {
                    Text("Activo").font(.caption2.weight(.bold)).padding(.horizontal, 6).padding(.vertical, 2).background(Theme.softGradient, in: Capsule())
                }
            }
        }
    }

    private func presetGrid(_ presets: [StylePreset], selection: Binding<String>) -> some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 190), spacing: 10)], spacing: 10) {
            ForEach(presets) { preset in
                ChoiceCard(title: preset.title, subtitle: preset.summary, symbol: preset.symbol, selected: selection.wrappedValue == preset.id) {
                    withAnimation(.snappy) { selection.wrappedValue = preset.id }
                }
            }
        }
    }

    @ViewBuilder
    private var videoSettings: some View {
        @Bindable var model = model
        HStack(spacing: 12) {
            Image(systemName: "sparkles.tv")
                .font(.title2)
                .foregroundStyle(Theme.gradient)
            VStack(alignment: .leading, spacing: 2) {
                Text("Seedance 2.5 · ByteDance").font(.callout.weight(.semibold))
                Text("Video con referencias de imagen, video y voz. Diálogo con lip-sync cuando va \"entre comillas\".")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
        VStack(alignment: .leading, spacing: 8) {
            FieldTitle("Resolución")
            PillPicker(options: Presets.videoResolutions, selection: $model.videoResolution, label: { $0 }, accessibilityName: "Resolución")
            Text("Tip: 720p para probar, 1080p para la versión final. Seedance no tiene 4K real.")
                .font(.caption).foregroundStyle(.secondary)
        }
        FieldTitle("Formato")
        AspectPicker(options: Presets.videoAspects, selection: $model.videoAspect)
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                FieldTitle("Duración")
                Spacer()
                Text("\(model.videoDuration) s")
                    .font(.title3.weight(.bold).monospacedDigit())
                    .contentTransition(.numericText())
            }
            Slider(
                value: Binding(get: { Double(model.videoDuration) }, set: { model.videoDuration = Int($0.rounded()) }),
                in: Double(Presets.videoDurationRange.lowerBound)...Double(Presets.videoDurationRange.upperBound),
                step: 1
            )
            .accessibilityLabel("Duración en segundos")
            .accessibilityValue("\(model.videoDuration) segundos")
            HStack(spacing: 6) {
                ForEach(Presets.videoDurationShortcuts, id: \.self) { seconds in
                    Button("\(seconds) s") { withAnimation(.snappy) { model.videoDuration = seconds } }
                        .glassButtonStyle()
                        .controlSize(.small)
                        .tint(model.videoDuration == seconds ? Theme.pink : nil)
                }
                Spacer()
                Label("≈ \(DialogueMath.targetWords(seconds: model.videoDuration)) palabras de diálogo", systemImage: "quote.bubble")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .help("El modelo habla ~2,47 palabras por segundo y rellena con silencio lo que sobra.")
            }
        }
        Toggle(isOn: $model.videoAudio) {
            VStack(alignment: .leading, spacing: 2) {
                Text("Generar audio").font(.callout.weight(.semibold))
                Text("Voces con lip-sync y sonido ambiente captado por el micrófono del teléfono. La música se agrega después, en la edición.")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
        .toggleStyle(.switch)
    }
}

struct FieldTitle: View {
    let title: String
    var help: String?

    init(_ title: String, help: String? = nil) {
        self.title = title
        self.help = help
    }

    var body: some View {
        HStack(spacing: 5) {
            Text(title).font(.subheadline.weight(.semibold)).foregroundStyle(.secondary)
            if let help {
                Image(systemName: "questionmark.circle")
                    .font(.caption)
                    .foregroundStyle(.tertiary)
                    .help(help)
                    .accessibilityLabel(help)
            }
        }
    }
}

// MARK: - Generate bar

struct GenerateBar: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 3) {
                Text(model.settingsSummary).font(.callout.weight(.semibold)).lineLimit(1)
                Text(model.hasKieKey ? "Usa tus créditos de KIE · ⌘↩ para generar" : "Conectá tu clave de KIE para generar")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            Spacer()
            if !model.activeJobs.isEmpty {
                Button {
                    model.filter = .active
                    model.section = .library
                } label: {
                    Label("\(model.activeJobs.count) en curso", systemImage: "hourglass")
                }
                .glassButtonStyle()
                .help("Ver las generaciones en curso")
            }
            Button {
                Task { await model.generate() }
            } label: {
                HStack(spacing: 8) {
                    if model.isGenerating {
                        ProgressView().controlSize(.small).tint(.white)
                        Text(model.generationStep ?? "Enviando…")
                    } else {
                        Image(systemName: "sparkles")
                        Text(buttonTitle)
                    }
                }
                .frame(minWidth: 190)
            }
            .buttonStyle(GradientButtonStyle(height: 46))
            .disabled(model.generateBlocker != nil)
            .help(model.generateBlocker ?? "Generar (⌘↩)")
        }
        .padding(.leading, 22)
        .padding(.trailing, 8)
        .padding(.vertical, 8)
        .glassSurface(RoundedRectangle(cornerRadius: 28, style: .continuous))
        .padding(.horizontal, 20)
        .padding(.bottom, 14)
        .frame(maxWidth: 920)
    }

    private var buttonTitle: String {
        if model.mode == .video { return "Generar video" }
        return model.quantity == 1 ? "Generar imagen" : "Generar \(model.quantity) imágenes"
    }
}
