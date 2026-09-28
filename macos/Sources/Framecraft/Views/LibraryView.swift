import AVKit
import FramecraftCore
import SwiftUI

struct LibraryView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var model = model
        Group {
            if model.jobs.isEmpty {
                ContentUnavailableView {
                    Label("Tu biblioteca está vacía", systemImage: "photo.stack")
                } description: {
                    Text("Todo lo que generes aparece acá y se guarda en Imágenes ▸ Framecraft.")
                } actions: {
                    Button("Crear mi primera pieza") { model.section = .create }
                        .buttonStyle(GradientButtonStyle(height: 36))
                }
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: 16) {
                        Picker("Filtro", selection: $model.filter) {
                            ForEach(LibraryFilter.allCases) { Text($0.title).tag($0) }
                        }
                        .pickerStyle(.segmented)
                        .labelsHidden()
                        .frame(maxWidth: 560)

                        if model.filteredJobs.isEmpty {
                            ContentUnavailableView.search(text: model.search)
                                .frame(maxWidth: .infinity, minHeight: 300)
                        } else {
                            LazyVGrid(columns: [GridItem(.adaptive(minimum: 230, maximum: 340), spacing: 16)], alignment: .leading, spacing: 16) {
                                ForEach(model.filteredJobs) { job in
                                    MediaCard(job: job)
                                }
                            }
                        }
                    }
                    .padding(24)
                }
                .background(alignment: .top) { AuraBackground(intensity: 0.7).frame(height: 300).ignoresSafeArea() }
            }
        }
        .searchable(text: $model.search, placement: .toolbar, prompt: "Buscar por prompt o modelo")
        .navigationTitle("Biblioteca")
        .navigationSubtitle("\(model.completedCount) creaciones")
        .toolbar {
            ToolbarItem {
                Button {
                    NSWorkspace.shared.open(model.store.mediaRoot)
                } label: {
                    Label("Abrir carpeta", systemImage: "folder")
                }
                .help("Abrir Imágenes ▸ Framecraft en Finder")
            }
        }
    }
}

struct MediaCard: View {
    @Environment(AppModel.self) private var model
    let job: Job
    @State private var hovering = false
    @State private var confirmDelete = false

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            preview
                .frame(height: 230)
                .frame(maxWidth: .infinity)
                .clipped()
                .overlay(alignment: .topTrailing) { hoverActions }
                .overlay(alignment: .topLeading) { kindBadge }
                .contentShape(Rectangle())
                .onTapGesture { model.detailJobID = job.id }
                .onDrag { dragProvider() }
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 6) {
                    Text(Presets.modelName(job.model)).font(.caption.weight(.semibold))
                    Text(details).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                    Spacer(minLength: 0)
                    Text(job.created, format: .relative(presentation: .named))
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                }
                Text(job.prompt)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(2)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(12)
        }
        .background(Theme.cardFill, in: RoundedRectangle(cornerRadius: Theme.cardRadius, style: .continuous))
        .clipShape(RoundedRectangle(cornerRadius: Theme.cardRadius, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: Theme.cardRadius, style: .continuous)
                .strokeBorder(hovering ? Theme.pink.opacity(0.5) : Theme.hairline, lineWidth: hovering ? 1.5 : 1)
        )
        .shadow(color: .black.opacity(hovering ? 0.14 : 0.06), radius: hovering ? 12 : 6, y: hovering ? 6 : 2)
        .scaleEffect(hovering ? 1.01 : 1)
        .animation(.snappy(duration: 0.2), value: hovering)
        .onHover { hovering = $0 }
        .contextMenu { JobMenu(job: job, confirmDelete: $confirmDelete) }
        .confirmationDialog("¿Eliminar esta generación?", isPresented: $confirmDelete) {
            Button("Eliminar", role: .destructive) { model.deleteJob(job) }
        } message: {
            Text("Se borran los archivos de tu biblioteca. No se reintegran créditos de KIE.")
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("\(job.kind == .video ? "Video" : "Imagen"), \(statusText). \(job.prompt)")
    }

    private var details: String {
        var parts = [job.settings.resolution, job.settings.aspect]
        if let duration = job.settings.duration { parts.append("\(duration) s") }
        return parts.joined(separator: " · ")
    }

    private var statusText: String {
        switch job.status {
        case .success: "lista"
        case .fail: "falló"
        case .unknown: "hay que revisar el envío"
        default: "en curso \(job.progress)%"
        }
    }

    @ViewBuilder
    private var preview: some View {
        switch job.status {
        case .success:
            if let url = model.outputURLs(job).first {
                ZStack {
                    Color.black.opacity(0.9)
                    Thumbnail(url: url, video: job.kind == .video, maxPixel: 700, contentMode: .fill)
                    if job.kind == .video {
                        if hovering {
                            HoverVideoPlayer(url: url)
                                .transition(.opacity)
                        } else {
                            Image(systemName: "play.fill")
                                .font(.system(size: 20, weight: .bold))
                                .foregroundStyle(.white)
                                .frame(width: 50, height: 50)
                                .glassSurface(Circle())
                                .transition(.opacity)
                        }
                    }
                    if job.outputs.count > 1 {
                        VStack {
                            Spacer()
                            HStack {
                                Spacer()
                                Label("\(job.outputs.count)", systemImage: "square.stack")
                                    .font(.caption.weight(.bold))
                                    .padding(6)
                                    .background(.ultraThinMaterial, in: Capsule())
                                    .padding(8)
                            }
                        }
                    }
                }
            } else {
                placeholder(icon: "questionmark.square.dashed", title: "Archivo no encontrado", message: "Puede que se haya movido de la carpeta.")
            }
        case .fail:
            placeholder(icon: "exclamationmark.triangle", title: "La generación falló", message: job.error ?? "", tint: .orange)
        case .unknown:
            placeholder(icon: "questionmark.circle", title: "Revisá el envío", message: job.error ?? "", tint: .orange)
        default:
            ProgressCard(job: job)
        }
    }

    private func placeholder(icon: String, title: String, message: String, tint: Color = .secondary) -> some View {
        VStack(spacing: 8) {
            Image(systemName: icon).font(.system(size: 30)).foregroundStyle(tint)
            Text(title).font(.callout.weight(.semibold))
            Text(message).font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.center).lineLimit(4)
        }
        .padding(16)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(tint.opacity(0.08))
    }

    private var kindBadge: some View {
        Image(systemName: job.kind == .video ? "video.fill" : "photo.fill")
            .font(.caption.weight(.semibold))
            .foregroundStyle(.white)
            .padding(7)
            .background(.black.opacity(0.25), in: Circle())
            .glassSurface(Circle())
            .padding(8)
            .accessibilityHidden(true)
    }

    private var hoverActions: some View {
        HStack(spacing: 6) {
            if job.status == .success {
                circleButton(job.isFavorite ? "heart.fill" : "heart", help: job.isFavorite ? "Quitar de favoritos" : "Marcar como favorito") {
                    model.toggleFavorite(job)
                }
                circleButton("eye", help: "Vista rápida") {
                    model.quickLookURL = model.outputURLs(job).first
                }
            }
            Menu {
                JobMenu(job: job, confirmDelete: $confirmDelete)
            } label: {
                Image(systemName: "ellipsis")
                    .font(.callout.weight(.bold))
                    .foregroundStyle(.white)
                    .frame(width: 30, height: 30)
                    .background(.black.opacity(0.25), in: Circle())
                    .glassSurface(Circle(), interactive: true)
            }
            .menuStyle(.button)
            .buttonStyle(.plain)
            .menuIndicator(.hidden)
            .fixedSize()
            .accessibilityLabel("Más acciones")
        }
        .glassGroup(spacing: 6)
        .padding(8)
        .opacity(hovering || job.isFavorite ? 1 : 0)
    }

    private func circleButton(_ symbol: String, help: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: symbol)
                .font(.callout.weight(.semibold))
                .foregroundStyle(symbol == "heart.fill" ? Theme.pink : .white)
                .frame(width: 30, height: 30)
                .background(.black.opacity(0.25), in: Circle())
                .glassSurface(Circle(), interactive: true)
        }
        .buttonStyle(.plain)
        .help(help)
        .accessibilityLabel(help)
    }

    private func dragProvider() -> NSItemProvider {
        guard let url = model.outputURLs(job).first else { return NSItemProvider() }
        return NSItemProvider(contentsOf: url) ?? NSItemProvider()
    }
}

/// Animated card for a generation in progress.
struct ProgressCard: View {
    let job: Job
    @State private var animate = false

    var body: some View {
        ZStack {
            LinearGradient(colors: [Theme.coral.opacity(0.25), Theme.pink.opacity(0.25), Theme.violet.opacity(0.3), Theme.pink.opacity(0.25)],
                           startPoint: animate ? .topLeading : .bottomLeading, endPoint: animate ? .bottomTrailing : .topTrailing)
            VStack(spacing: 12) {
                ZStack {
                    Circle().stroke(Color.primary.opacity(0.1), lineWidth: 6)
                    Circle()
                        .trim(from: 0, to: max(0.03, CGFloat(job.progress) / 100))
                        .stroke(Theme.gradient, style: StrokeStyle(lineWidth: 6, lineCap: .round))
                        .rotationEffect(.degrees(-90))
                        .animation(.easeInOut, value: job.progress)
                    Text("\(job.progress)%").font(.callout.weight(.bold).monospacedDigit())
                }
                .frame(width: 64, height: 64)
                Text(title).font(.callout.weight(.semibold))
                Text(job.error ?? "Podés seguir trabajando mientras tanto.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                    .lineLimit(3)
                    .padding(.horizontal, 12)
            }
        }
        .onAppear {
            withAnimation(.easeInOut(duration: 3).repeatForever(autoreverses: true)) { animate = true }
        }
    }

    private var title: String {
        switch job.status {
        case .submitting: "Enviando…"
        case .queued: "En cola…"
        case .saving: "Guardando…"
        default: job.kind == .video ? "Creando tu video…" : "Creando tu imagen…"
        }
    }
}

/// Actions shared by the context menu, the "…" menu and the detail view.
struct JobMenu: View {
    @Environment(AppModel.self) private var model
    let job: Job
    @Binding var confirmDelete: Bool

    var body: some View {
        let urls = model.outputURLs(job)
        if job.status == .success {
            Button("Ver en grande") { model.detailJobID = job.id }
            Button("Vista rápida") { model.quickLookURL = urls.first }
            Button("Mostrar en Finder") { model.revealInFinder(urls) }
            ShareLink(items: urls) { Label("Compartir…", systemImage: "square.and.arrow.up") }
            if job.kind == .video {
                Button("Continuar desde el último fotograma") { Task { await model.continueFromLastFrame(job) } }
            } else if let first = job.outputs.first {
                Button("Usar como referencia") { Task { await model.useAsReference(job, output: first) } }
            }
            Divider()
        }
        if job.model != Presets.storyCutModelID {
            Button("Reusar ajustes y prompt") { model.reuse(job) }
        }
        Button("Copiar prompt") { model.copyToPasteboard(job.prompt) }
        if job.status == .success {
            Button(job.isFavorite ? "Quitar de favoritos" : "Marcar como favorito") { model.toggleFavorite(job) }
        }
        Divider()
        Button("Eliminar…", role: .destructive) { confirmDelete = true }
            .disabled(!job.status.isFinished)
    }
}

/// Large preview of a generation with all its actions.
struct JobDetailView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    let jobID: UUID
    @State private var index = 0
    @State private var confirmDelete = false

    var body: some View {
        if let job = model.jobs.first(where: { $0.id == jobID }) {
            let urls = model.outputURLs(job)
            HStack(spacing: 0) {
                ZStack {
                    Color.black
                    if job.status == .success, urls.indices.contains(index) {
                        if job.kind == .video {
                            PlayerView(url: urls[index])
                        } else {
                            Thumbnail(url: urls[index], maxPixel: 2400, contentMode: .fit)
                        }
                    } else {
                        ProgressCard(job: job)
                    }
                }
                .frame(minWidth: 520, maxWidth: .infinity, maxHeight: .infinity)
                .overlay(alignment: .bottom) {
                    if urls.count > 1 {
                        HStack {
                            ForEach(urls.indices, id: \.self) { i in
                                Button("\(i + 1)") { index = i }
                                    .glassButtonStyle()
                                    .tint(i == index ? Theme.pink : nil)
                            }
                        }
                        .padding(10)
                        .background(.ultraThinMaterial, in: Capsule())
                        .padding()
                    }
                }

                ScrollView {
                    VStack(alignment: .leading, spacing: 14) {
                        HStack {
                            Text(Presets.modelName(job.model)).font(.title3.weight(.bold))
                            Spacer()
                            Button { dismiss() } label: { Image(systemName: "xmark.circle.fill").font(.title2) }
                                .buttonStyle(.plain)
                                .foregroundStyle(.secondary)
                                .keyboardShortcut(.cancelAction)
                                .accessibilityLabel("Cerrar")
                        }
                        Text(job.created.formatted(date: .abbreviated, time: .shortened)).font(.caption).foregroundStyle(.secondary)
                        Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 6) {
                            row("Resolución", job.settings.resolution)
                            row("Formato", job.settings.aspect)
                            if let duration = job.settings.duration { row("Duración", "\(duration) s") }
                            if let audio = job.settings.generateAudio { row("Audio", audio ? "Sí" : "No") }
                            if let camera = job.settings.camera, camera != Presets.none {
                                row("Cámara", Presets.cameras.first { $0.id == camera }?.title ?? camera)
                            }
                            if let film = job.settings.film, film != Presets.none {
                                row("Encuadre", Presets.films.first { $0.id == film }?.title ?? film)
                            }
                            let refs = job.settings.imageReferences.count + job.settings.videoReferences.count + job.settings.audioReferences.count
                            if refs > 0 { row("Referencias", "\(refs)") }
                        }
                        .font(.callout)
                        Divider()
                        HStack {
                            Text("Prompt").font(.headline)
                            Spacer()
                            Button("Copiar") { model.copyToPasteboard(job.prompt) }.controlSize(.small)
                        }
                        Text(job.prompt)
                            .font(.callout)
                            .textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .leading)
                        if let error = job.error {
                            Label(error, systemImage: "exclamationmark.triangle").font(.caption).foregroundStyle(.orange)
                        }
                        Divider()
                        VStack(alignment: .leading, spacing: 8) {
                            if job.model != Presets.storyCutModelID {
                                Button {
                                    model.reuse(job)
                                } label: {
                                    Label("Reusar ajustes y prompt", systemImage: "arrow.uturn.backward").frame(maxWidth: .infinity)
                                }
                                .buttonStyle(GradientButtonStyle(height: 36))
                            }
                            if job.status == .success {
                                if job.kind == .video {
                                    Button {
                                        Task { await model.continueFromLastFrame(job) }
                                        dismiss()
                                    } label: {
                                        Label("Continuar desde el último fotograma", systemImage: "forward.frame").frame(maxWidth: .infinity)
                                    }
                                    .help("Guarda el último fotograma como referencia para que el próximo clip empiece exactamente ahí")
                                }
                                Button {
                                    model.revealInFinder(urls)
                                } label: {
                                    Label("Mostrar en Finder", systemImage: "folder").frame(maxWidth: .infinity)
                                }
                                if urls.indices.contains(index) {
                                    ShareLink(item: urls[index]) {
                                        Label("Compartir", systemImage: "square.and.arrow.up").frame(maxWidth: .infinity)
                                    }
                                }
                            }
                            Button(role: .destructive) {
                                confirmDelete = true
                            } label: {
                                Label("Eliminar", systemImage: "trash").frame(maxWidth: .infinity)
                            }
                            .disabled(!job.status.isFinished)
                        }
                        .glassButtonStyle()
                        .controlSize(.large)
                    }
                    .padding(20)
                }
                .frame(width: 330)
            }
            .frame(minWidth: 900, idealWidth: 1100, minHeight: 620, idealHeight: 760)
            .confirmationDialog("¿Eliminar esta generación?", isPresented: $confirmDelete) {
                Button("Eliminar", role: .destructive) {
                    model.deleteJob(job)
                    dismiss()
                }
            } message: {
                Text("Se borran los archivos de tu biblioteca. No se reintegran créditos de KIE.")
            }
        } else {
            ContentUnavailableView("Esta generación ya no existe", systemImage: "photo")
                .frame(width: 500, height: 300)
        }
    }

    private func row(_ title: String, _ value: String) -> some View {
        GridRow {
            Text(title).foregroundStyle(.secondary)
            Text(value)
        }
    }
}

/// Keeps one AVPlayer per file so re-renders don't restart playback.
struct PlayerView: View {
    let url: URL
    @State private var player: AVPlayer?

    var body: some View {
        VideoPlayer(player: player)
            .task(id: url) {
                player?.pause()
                player = AVPlayer(url: url)
            }
            .onDisappear { player?.pause() }
    }
}

/// Muted, looping preview that plays while the pointer is over a video card.
struct HoverVideoPlayer: NSViewRepresentable {
    let url: URL

    func makeNSView(context: Context) -> LoopingPlayerNSView {
        let view = LoopingPlayerNSView()
        view.play(url)
        return view
    }

    func updateNSView(_ view: LoopingPlayerNSView, context: Context) {}

    static func dismantleNSView(_ view: LoopingPlayerNSView, coordinator: ()) {
        view.stop()
    }

    final class LoopingPlayerNSView: NSView {
        private let player = AVQueuePlayer()
        private var looper: AVPlayerLooper?
        private let playerLayer = AVPlayerLayer()

        override init(frame: NSRect) {
            super.init(frame: frame)
            wantsLayer = true
            playerLayer.player = player
            playerLayer.videoGravity = .resizeAspectFill
            layer?.addSublayer(playerLayer)
        }

        required init?(coder: NSCoder) { nil }

        override func layout() {
            super.layout()
            playerLayer.frame = bounds
        }

        func play(_ url: URL) {
            player.isMuted = true
            looper = AVPlayerLooper(player: player, templateItem: AVPlayerItem(url: url))
            player.play()
        }

        func stop() {
            player.pause()
            looper = nil
            player.removeAllItems()
        }
    }
}
