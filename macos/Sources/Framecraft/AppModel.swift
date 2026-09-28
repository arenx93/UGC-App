import AppKit
import FramecraftCore
import SwiftUI
import UserNotifications

enum SidebarItem: String, Hashable, CaseIterable, Identifiable {
    case create, library, references, skills, guide
    var id: String { rawValue }

    var title: String {
        switch self {
        case .create: "Crear"
        case .library: "Biblioteca"
        case .references: "Referencias"
        case .skills: "Skills"
        case .guide: "Guía UGC"
        }
    }

    var symbol: String {
        switch self {
        case .create: "wand.and.stars"
        case .library: "photo.stack"
        case .references: "paperclip"
        case .skills: "brain.head.profile"
        case .guide: "book"
        }
    }
}

enum LibraryFilter: String, CaseIterable, Identifiable {
    case all, images, videos, active, favorites, failed
    var id: String { rawValue }

    var title: String {
        switch self {
        case .all: "Todo"
        case .images: "Imágenes"
        case .videos: "Videos"
        case .active: "En curso"
        case .favorites: "Favoritos"
        case .failed: "Con error"
        }
    }
}

struct Banner: Identifiable, Equatable {
    enum Style { case success, info, error }
    let id = UUID()
    var text: String
    var style: Style
}

/// App state and actions. Everything the UI does goes through here.
@MainActor
@Observable
final class AppModel {
    // MARK: Dependencies
    let store: LibraryStore
    let keys: KeyStore
    let resources: SkillResources?
    let isDemo: Bool

    // MARK: Navigation
    var section: SidebarItem? = .create
    var showAssistant = true
    var showOnboarding = false
    var showPromptPreview = false
    var banner: Banner?
    var detailJobID: UUID?
    var quickLookURL: URL?

    // MARK: Library
    var jobs: [Job] = []
    var references: [ReferenceFile] = []
    var customSkills: [Skill] = []
    let builtinSkills: [Skill]
    var notifyWhenDone: Bool
    private var onboardingDone: Bool

    // MARK: Account
    var hasKieKey = false
    var hasOpenAIKey = false
    var credits: Double?
    var checkingCredits = false

    // MARK: Create form
    var mode: MediaKind { didSet { if oldValue != mode { modeChanged() } } }
    var prompt = ""
    var imageModel: String { didSet { fixAspect() } }
    var imageResolution: String { didSet { fixAspect() } }
    var imageAspect: String
    var quantity: Int
    var camera: String
    var film: String
    var videoResolution: String
    var videoAspect: String
    var videoDuration: Int
    var videoAudio: Bool
    var selectedImages: [UUID] = []
    var selectedVideos: [UUID] = []
    var selectedAudios: [UUID] = []
    var isGenerating = false
    var generationStep: String?
    var isImporting = false

    // MARK: Assistant
    var idea = ""
    var skillID: String
    var provider: PromptProvider
    var promptModel: String
    var dialogueLanguage: String
    var includeWalterExample = false
    var draft = ""
    var notes: String?
    var sources: [PromptExample] = []
    var feedback = ""
    var isBuildingPrompt = false
    /// Text received so far while the assistant writes (live preview).
    var streamingText: String?
    var codexStatus: CodexCLI.Status = .unknown
    var assistantError: String?

    // MARK: Library UI
    var filter: LibraryFilter = .all
    var search = ""

    @ObservationIgnored private var pollTask: Task<Void, Never>?
    @ObservationIgnored private var askedNotifications = false

    // MARK: Init

    init(store: LibraryStore, keys: KeyStore, isDemo: Bool = false) {
        self.store = store
        self.keys = keys
        self.isDemo = isDemo
        let resources = SkillResources.locate()
        self.resources = resources
        builtinSkills = SkillLibrary.builtins(resources)

        let index = store.load()
        let p = index.preferences
        jobs = index.jobs.sorted { $0.created > $1.created }
        references = index.references.sorted { $0.created > $1.created }
        customSkills = index.skills
        mode = p.mode
        imageModel = Presets.imageModel(p.imageModel) == nil ? "gpt-image-2" : p.imageModel
        imageResolution = Presets.imageResolutions.contains(p.imageResolution) ? p.imageResolution : "1K"
        imageAspect = p.imageAspect
        quantity = min(max(p.quantity, 1), 4)
        camera = Presets.isValidCamera(p.camera) ? p.camera : Presets.none
        film = Presets.isValidFilm(p.film) ? p.film : Presets.none
        videoResolution = Presets.videoResolutions.contains(p.videoResolution) ? p.videoResolution : "720p"
        videoAspect = Presets.videoAspects.contains(p.videoAspect) ? p.videoAspect : "9:16"
        videoDuration = min(max(p.videoDuration, Presets.videoDurationRange.lowerBound), Presets.videoDurationRange.upperBound)
        videoAudio = p.videoAudio
        provider = p.provider
        promptModel = PromptRequests.promptModels.contains { $0.id == p.promptModel } ? p.promptModel : "gpt-5-6-terra"
        dialogueLanguage = p.dialogueLanguage
        notifyWhenDone = p.notifyWhenDone
        onboardingDone = p.onboardingDone
        skillID = p.mode == .video ? SkillLibrary.ugcID : SkillLibrary.jsonProfileID

        hasKieKey = keys.read(KeyAccount.kie) != nil
        hasOpenAIKey = keys.read(KeyAccount.openAI) != nil
        fixAspect()
        showOnboarding = !isDemo && !onboardingDone
        markInterruptedSubmissions()
        if !isDemo {
            startPolling()
            Task { await refreshCredits() }
        }
    }

    // MARK: Derived state

    var allSkills: [Skill] { builtinSkills + customSkills }
    var skillsForMode: [Skill] { allSkills.filter { $0.media.supports(mode) } }
    var currentSkill: Skill? { allSkills.first { $0.id == skillID } }
    var isUGCSkill: Bool { skillID == SkillLibrary.ugcID }

    var aspectOptions: [String] {
        mode == .image ? Presets.ratios(model: imageModel, resolution: imageResolution) : Presets.videoAspects
    }

    var activeJobs: [Job] { jobs.filter { $0.status.isActive } }

    func references(_ kind: ReferenceKind) -> [ReferenceFile] { references.filter { $0.kind == kind } }

    func selection(_ kind: ReferenceKind) -> [UUID] {
        switch kind {
        case .image: selectedImages
        case .video: selectedVideos
        case .audio: selectedAudios
        }
    }

    func selectedReferences(_ kind: ReferenceKind) -> [ReferenceFile] {
        selection(kind).compactMap { id in references.first { $0.id == id } }
    }

    func selectionLimit(_ kind: ReferenceKind) -> Int {
        switch kind {
        case .image: mode == .image ? 4 : 30
        case .video, .audio: 10
        }
    }

    func selectedSeconds(_ kind: ReferenceKind) -> Double {
        selectedReferences(kind).reduce(0) { $0 + $1.durationSeconds }
    }

    /// Final text sent to the model (image presets included).
    var finalPrompt: String {
        mode == .image
            ? Presets.composePrompt(prompt, camera: camera, film: film, aspect: imageAspect)
            : prompt.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    var lintReport: LintReport? {
        guard mode == .video, !prompt.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return nil }
        return PromptLinter.lintVideo(
            prompt, duration: videoDuration,
            references: .init(images: selectedImages.count, videos: selectedVideos.count, audios: selectedAudios.count),
            ugcStyle: skillID != SkillLibrary.arthasID
        )
    }

    /// Why "Generar" is disabled (nil when it can run). Shown as help text.
    var generateBlocker: String? {
        if isGenerating { return "Ya se está enviando una generación." }
        if prompt.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty { return "Escribí un prompt (o pedile uno al asistente)." }
        if mode == .image {
            if prompt.count > Presets.maxPromptLength(imageModel: imageModel) {
                return "El prompt supera los \(Presets.maxPromptLength(imageModel: imageModel)) caracteres de este modelo."
            }
        } else if prompt.count > Presets.maxVideoPromptLength {
            return "El prompt de video supera los \(Presets.maxVideoPromptLength) caracteres."
        }
        return nil
    }

    var settingsSummary: String {
        if mode == .image {
            let model = Presets.imageModel(imageModel)?.name ?? imageModel
            return "\(model) · \(imageResolution) · \(imageAspect) · \(quantity == 1 ? "1 imagen" : "\(quantity) imágenes")"
        }
        return "\(Presets.videoModelName) · \(videoResolution) · \(videoAspect) · \(videoDuration) s\(videoAudio ? " · con audio" : "")"
    }

    var filteredJobs: [Job] {
        let query = search.trimmingCharacters(in: .whitespaces).lowercased()
        return jobs.filter { job in
            let passes: Bool = switch filter {
            case .all: true
            case .images: job.kind == .image
            case .videos: job.kind == .video
            case .active: job.status.isActive
            case .favorites: job.isFavorite
            case .failed: job.status == .fail || job.status == .unknown
            }
            return passes && (query.isEmpty || job.prompt.lowercased().contains(query) || Presets.modelName(job.model).lowercased().contains(query))
        }
    }

    var completedCount: Int { jobs.filter { $0.status == .success }.reduce(0) { $0 + $1.outputs.count } }

    // MARK: Messages

    func show(_ text: String, _ style: Banner.Style = .info) {
        withAnimation(.snappy) { banner = Banner(text: text, style: style) }
    }

    // MARK: Persistence

    func persist() {
        var index = LibraryIndex()
        index.jobs = jobs
        index.references = references
        index.skills = customSkills
        index.preferences = Preferences(
            mode: mode, imageModel: imageModel, imageResolution: imageResolution, imageAspect: imageAspect,
            quantity: quantity, camera: camera, film: film, videoResolution: videoResolution, videoAspect: videoAspect,
            videoDuration: videoDuration, videoAudio: videoAudio, provider: provider, promptModel: promptModel,
            dialogueLanguage: dialogueLanguage, notifyWhenDone: notifyWhenDone, onboardingDone: onboardingDone
        )
        store.save(index)
        updateDockBadge()
    }

    func finishOnboarding() {
        onboardingDone = true
        showOnboarding = false
        persist()
    }

    // MARK: Form behaviour

    private func modeChanged() {
        if !(currentSkill?.media.supports(mode) ?? true) || skillID == SkillLibrary.generalID {
            skillID = mode == .video ? SkillLibrary.ugcID : SkillLibrary.jsonProfileID
        }
        if mode == .image && selectedImages.count > 4 {
            selectedImages = Array(selectedImages.prefix(4))
            show("En imágenes se usan hasta 4 referencias: quedaron las primeras 4.")
        }
        draft = ""
        notes = nil
        sources = []
    }

    private func fixAspect() {
        let options = Presets.ratios(model: imageModel, resolution: imageResolution)
        if !options.contains(imageAspect) { imageAspect = options.first ?? "1:1" }
    }

    func insertTag(_ tag: String) {
        let needsSpace = !(prompt.last?.isWhitespace ?? true)
        prompt += (needsSpace ? " " : "") + tag + " "
    }

    // MARK: API keys

    private var kieKey: String? { keys.read(KeyAccount.kie) }

    func saveKieKey(_ raw: String) async throws {
        let key = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard key.count >= 10, key.count <= 512 else { throw KieError("Esa no parece una clave de KIE válida.", definite: true) }
        let balance = try await KieClient(key: key).credits()
        try keys.write(key, account: KeyAccount.kie)
        hasKieKey = true
        credits = balance
        show("KIE conectado. Ya podés generar imágenes, videos y prompts.", .success)
    }

    func removeKieKey() {
        keys.delete(KeyAccount.kie)
        hasKieKey = false
        credits = nil
        show("Se quitó la clave de KIE.")
    }

    func saveOpenAIKey(_ raw: String) throws {
        let key = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard key.hasPrefix("sk-"), key.count <= 1024 else { throw KieError("Las claves de OpenAI empiezan con \"sk-\".", definite: true) }
        try keys.write(key, account: KeyAccount.openAI)
        hasOpenAIKey = true
        show("Clave de OpenAI guardada. Se verifica al crear un prompt.", .success)
    }

    func removeOpenAIKey() {
        keys.delete(KeyAccount.openAI)
        hasOpenAIKey = false
        if provider == .openai { provider = .kie }
    }

    func refreshCredits() async {
        guard let key = kieKey else { return }
        checkingCredits = true
        defer { checkingCredits = false }
        if let value = try? await KieClient(key: key).credits() { credits = value }
    }

    // MARK: References

    /// Imports files (from the file picker or drag & drop) into the reference library.
    func importFiles(_ urls: [URL], select: Bool = true) async {
        guard !urls.isEmpty else { return }
        isImporting = true
        defer { isImporting = false }
        var imported = 0
        var errors: [String] = []
        for url in urls {
            do {
                let reference = try await importFile(url)
                references.insert(reference, at: 0)
                imported += 1
                if select { autoSelect(reference) }
            } catch {
                errors.append("\(url.lastPathComponent): \(error.localizedDescription)")
            }
        }
        persist()
        if let first = errors.first {
            show(errors.count == 1 ? first : "\(first) (y \(errors.count - 1) más)", .error)
        } else if imported > 0 {
            show(imported == 1 ? "Referencia agregada." : "\(imported) referencias agregadas.", .success)
        }
    }

    private func importFile(_ url: URL) async throws -> ReferenceFile {
        let access = url.startAccessingSecurityScopedResource()
        defer { if access { url.stopAccessingSecurityScopedResource() } }
        guard let match = MediaSniffer.classify(url) else {
            throw KieError("Formato no soportado. Usá imágenes PNG/JPG/WebP, videos MP4/MOV o audio MP3/WAV/M4A.", definite: true)
        }
        let kind = match.kind, mime = match.mime
        let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
        let bytes = (attributes[.size] as? NSNumber)?.int64Value ?? 0
        guard bytes <= kind.maxBytes else { throw KieError("Supera los \(kind.maxMegabytes) MB permitidos.", definite: true) }
        let handle = try FileHandle(forReadingFrom: url)
        let prefix = try handle.read(upToCount: 16) ?? Data()
        try handle.close()
        guard MediaSniffer.matches(prefix: prefix, mime: mime, kind: kind) else {
            throw KieError("El archivo no coincide con su formato (\(kind.formatsDescription)).", definite: true)
        }
        var durationMs: Int?
        if kind != .image {
            guard let seconds = await MediaTools.duration(of: url) else {
                throw KieError("No pude leer la duración. Convertilo a \(kind == .video ? "MP4" : "MP3 o M4A") y probá de nuevo.", definite: true)
            }
            guard seconds <= 30.05 else { throw KieError("Dura \(Int(seconds)) s: cada audio o video de referencia puede durar hasta 30 s.", definite: true) }
            durationMs = Int((seconds * 1000).rounded())
        }
        let id = UUID()
        let fileName = "\(id.uuidString.lowercased()).\(MediaSniffer.fileExtension(forMime: mime))"
        try FileManager.default.copyItem(at: url, to: store.referencesDir.appendingPathComponent(fileName))
        return ReferenceFile(id: id, name: String(url.lastPathComponent.prefix(150)), kind: kind, mime: mime,
                             fileName: fileName, durationMs: durationMs, bytes: bytes)
    }

    private func autoSelect(_ reference: ReferenceFile) {
        let limit = selectionLimit(reference.kind)
        switch reference.kind {
        case .image:
            if selectedImages.count < limit { selectedImages.append(reference.id) }
        case .video:
            if selectedVideos.count < limit, selectedSeconds(.video) + reference.durationSeconds <= 30 { selectedVideos.append(reference.id) }
        case .audio:
            if selectedAudios.count < limit, selectedSeconds(.audio) + reference.durationSeconds <= 30 { selectedAudios.append(reference.id) }
        }
    }

    func isSelected(_ reference: ReferenceFile) -> Bool { selection(reference.kind).contains(reference.id) }

    /// "@Image2" for the second selected image, etc.
    func tag(for reference: ReferenceFile) -> String? {
        guard let index = selection(reference.kind).firstIndex(of: reference.id) else { return nil }
        return reference.kind.tagPrefix + String(index + 1)
    }

    func toggleSelection(_ reference: ReferenceFile) {
        var list = selection(reference.kind)
        if let index = list.firstIndex(of: reference.id) {
            list.remove(at: index)
        } else {
            let limit = selectionLimit(reference.kind)
            guard list.count < limit else {
                show("Podés usar hasta \(limit) \(reference.kind == .image ? "imágenes" : reference.kind == .video ? "videos" : "audios") de referencia.", .error)
                return
            }
            if reference.kind != .image, selectedSeconds(reference.kind) + reference.durationSeconds > 30.05 {
                show("Los \(reference.kind == .video ? "videos" : "audios") seleccionados pueden sumar hasta 30 segundos.", .error)
                return
            }
            list.append(reference.id)
        }
        setSelection(list, for: reference.kind)
    }

    func moveSelection(_ kind: ReferenceKind, id: UUID, by offset: Int) {
        var list = selection(kind)
        guard let index = list.firstIndex(of: id) else { return }
        let target = index + offset
        guard list.indices.contains(target) else { return }
        list.swapAt(index, target)
        setSelection(list, for: kind)
    }

    private func setSelection(_ list: [UUID], for kind: ReferenceKind) {
        switch kind {
        case .image: selectedImages = list
        case .video: selectedVideos = list
        case .audio: selectedAudios = list
        }
    }

    func deleteReferences(_ ids: [UUID]) {
        let removed = Set(ids)
        for reference in references where removed.contains(reference.id) {
            try? FileManager.default.removeItem(at: store.referenceURL(reference))
        }
        references.removeAll { removed.contains($0.id) }
        selectedImages.removeAll { removed.contains($0) }
        selectedVideos.removeAll { removed.contains($0) }
        selectedAudios.removeAll { removed.contains($0) }
        persist()
        show(ids.count == 1 ? "Referencia eliminada." : "\(ids.count) referencias eliminadas.")
    }

    func url(for reference: ReferenceFile) -> URL { store.referenceURL(reference) }

    /// Adds a generated image to the references and selects it.
    func useAsReference(_ job: Job, output: String) async {
        await importFiles([store.outputURL(output)])
        section = .create
    }

    /// Extracts the last frame of a generated video so the next clip can start exactly there.
    func continueFromLastFrame(_ job: Job) async {
        guard let output = job.outputs.first else { return }
        do {
            let frame = try await MediaTools.lastFrame(of: store.outputURL(output))
            guard let png = MediaTools.pngData(frame) else { throw KieError("No se pudo guardar el fotograma.", definite: true) }
            let id = UUID()
            let fileName = "\(id.uuidString.lowercased()).png"
            try png.write(to: store.referencesDir.appendingPathComponent(fileName))
            let reference = ReferenceFile(id: id, name: "Último fotograma · \(job.prompt.prefix(40))", kind: .image, mime: "image/png",
                                          fileName: fileName, bytes: Int64(png.count))
            references.insert(reference, at: 0)
            mode = .video
            if selectedImages.count < selectionLimit(.image) { selectedImages.append(reference.id) }
            persist()
            section = .create
            let tag = self.tag(for: reference) ?? "@Image"
            show("Último fotograma agregado como \(tag). Pedile al prompt que \(tag) sea el primer fotograma.", .success)
        } catch {
            show("No se pudo extraer el último fotograma: \(error.localizedDescription)", .error)
        }
    }

    // MARK: Generation

    func generate() async {
        guard generateBlocker == nil else {
            if let reason = generateBlocker { show(reason, .error) }
            return
        }
        guard let key = kieKey else {
            showOnboarding = true
            return
        }
        isGenerating = true
        defer {
            isGenerating = false
            generationStep = nil
        }
        requestNotificationPermission()
        let client = KieClient(key: key)
        do {
            if mode == .image {
                try await generateImages(client)
            } else {
                try await generateVideo(client)
            }
            persist()
            startPolling()
        } catch {
            show(error.localizedDescription, .error)
        }
    }

    private func checkActiveLimit(adding count: Int) throws {
        let recent = activeJobs.filter { $0.created > Date().addingTimeInterval(-3600) }.count
        guard recent + count <= 12 else {
            throw KieError("Esperá a que terminen algunas generaciones en curso antes de lanzar más.", definite: true)
        }
    }

    private func generateImages(_ client: KieClient) async throws {
        let text = prompt.trimmingCharacters(in: .whitespacesAndNewlines)
        if text.hasPrefix("{") {
            guard let data = text.data(using: .utf8), (try? JSONSerialization.jsonObject(with: data)) is [String: Any] else {
                throw KieError("Tu prompt JSON tiene un error de sintaxis. Corregilo antes de generar.", definite: true)
            }
        }
        let final = finalPrompt
        guard final.count <= Presets.maxComposedLength(imageModel: imageModel) else {
            throw KieError("El prompt final (con presets) es demasiado largo para este modelo. Acortalo.", definite: true)
        }
        guard aspectOptions.contains(imageAspect) else { throw KieError("Ese formato no está disponible para este modelo y resolución.", definite: true) }
        try checkActiveLimit(adding: quantity)

        let referenceIDs = Array(selectedImages.prefix(4))
        generationStep = referenceIDs.isEmpty ? "Enviando…" : "Subiendo referencias…"
        let urls = try await upload(referenceIDs, client: client)
        generationStep = "Enviando a KIE…"

        let batch = UUID()
        let settings = JobSettings(resolution: imageResolution, aspect: imageAspect, camera: camera, film: film, imageReferences: referenceIDs)
        let newJobs = (0..<quantity).map { _ in
            Job(batchID: batch, kind: .image, model: imageModel, prompt: text, finalPrompt: final, settings: settings)
        }
        withAnimation(.snappy) { jobs.insert(contentsOf: newJobs, at: 0) }
        persist()
        let request = GenerationInputs.image(model: imageModel, finalPrompt: final, aspect: imageAspect, resolution: imageResolution, referenceURLs: urls)
        await withTaskGroup(of: Void.self) { group in
            for job in newJobs {
                group.addTask { @MainActor in
                    await self.submit(job.id) { try await client.createTask(model: request.model, input: request.input) }
                }
            }
        }
        show(quantity == 1 ? "Imagen en camino. Te aviso cuando esté lista." : "\(quantity) imágenes en camino.", .success)
    }

    private func generateVideo(_ client: KieClient) async throws {
        let text = prompt.trimmingCharacters(in: .whitespacesAndNewlines)
        guard selectedSeconds(.video) <= 30.05, selectedSeconds(.audio) <= 30.05 else {
            throw KieError("Los videos y audios de referencia pueden sumar hasta 30 s cada uno.", definite: true)
        }
        try checkActiveLimit(adding: 1)
        let images = selectedImages, videos = selectedVideos, audios = selectedAudios
        generationStep = (images + videos + audios).isEmpty ? "Enviando…" : "Subiendo referencias…"
        let imageURLs = try await upload(images, client: client)
        let videoURLs = try await upload(videos, client: client)
        let audioURLs = try await upload(audios, client: client)
        generationStep = "Enviando a KIE…"
        let settings = JobSettings(resolution: videoResolution, aspect: videoAspect, duration: videoDuration, generateAudio: videoAudio,
                                   imageReferences: images, videoReferences: videos, audioReferences: audios)
        let job = Job(batchID: UUID(), kind: .video, model: Presets.videoModelID, prompt: text, finalPrompt: text, settings: settings)
        withAnimation(.snappy) { jobs.insert(job, at: 0) }
        persist()
        let input = GenerationInputs.video(prompt: text, resolution: videoResolution, aspect: videoAspect, duration: videoDuration,
                                           generateAudio: videoAudio, images: imageURLs, videos: videoURLs, audios: audioURLs)
        await submit(job.id) { try await client.createTask(model: Presets.videoModelID, input: input) }
        show("Video en camino. Suele tardar unos minutos: podés seguir trabajando.", .success)
    }

    private func submit(_ id: UUID, _ create: () async throws -> String) async {
        do {
            let taskId = try await create()
            update(id) {
                $0.taskId = taskId
                $0.status = .queued
                $0.progress = 1
            }
        } catch {
            let definite = (error as? KieError)?.definite ?? false
            update(id) {
                $0.status = definite ? .fail : .unknown
                $0.error = definite ? error.localizedDescription : GenerationInputs.uncertainSubmission
            }
        }
    }

    private func upload(_ ids: [UUID], client: KieClient) async throws -> [URL] {
        var urls: [URL] = []
        for id in ids {
            guard let reference = references.first(where: { $0.id == id }) else {
                throw KieError("Una referencia seleccionada ya no existe. Volvé a elegirla.", definite: true)
            }
            let ext = MediaSniffer.fileExtension(forMime: reference.mime)
            urls.append(try await client.upload(file: url(for: reference), mime: reference.mime, displayName: reference.name,
                                                fileName: "\(reference.id.uuidString.lowercased()).\(ext)"))
        }
        return urls
    }

    func update(_ id: UUID, _ change: (inout Job) -> Void) {
        guard let index = jobs.firstIndex(where: { $0.id == id }) else { return }
        change(&jobs[index])
        jobs[index].updated = Date()
    }

    // MARK: Polling

    func startPolling() {
        guard pollTask == nil, !isDemo, activeJobs.contains(where: { $0.taskId != nil }) else {
            updateDockBadge()
            return
        }
        pollTask = Task { [weak self] in
            while let self, !Task.isCancelled {
                let pending = self.jobs.filter { [.queued, .generating, .saving].contains($0.status) && $0.taskId != nil }
                if pending.isEmpty { break }
                await self.pollOnce(pending)
                try? await Task.sleep(for: .seconds(5))
            }
            self?.pollTask = nil
        }
    }

    private func pollOnce(_ pending: [Job]) async {
        guard let key = kieKey else { return }
        let client = KieClient(key: key)
        let batch = Array(pending.prefix(12))
        for start in stride(from: 0, to: batch.count, by: 2) {
            await withTaskGroup(of: Void.self) { group in
                for job in batch[start..<min(start + 2, batch.count)] {
                    group.addTask { @MainActor in await self.refresh(job.id, client: client) }
                }
            }
        }
        markInterruptedSubmissions()
        persist()
    }

    private func refresh(_ id: UUID, client: KieClient) async {
        guard let job = jobs.first(where: { $0.id == id }), let taskId = job.taskId else { return }
        do {
            let record = try await client.recordInfo(taskId: taskId)
            switch record.state {
            case "fail":
                update(id) {
                    $0.status = .fail
                    $0.progress = record.progress
                    $0.error = record.failMessage ?? "KIE no pudo completar la generación."
                }
                notifyFinished(id)
            case "success":
                guard !record.resultURLs.isEmpty else { throw KieError("KIE no devolvió archivos para esta generación.", definite: true) }
                update(id) {
                    $0.status = .saving
                    $0.progress = 99
                }
                var outputs: [String] = []
                for (index, remote) in record.resultURLs.enumerated() {
                    outputs.append(try await download(remote, kind: job.kind, jobID: id, index: index))
                }
                update(id) {
                    $0.outputs = outputs
                    $0.status = .success
                    $0.progress = 100
                    $0.error = nil
                }
                notifyFinished(id)
                Task { await refreshCredits() }
            default:
                update(id) {
                    $0.status = .generating
                    $0.progress = record.progress
                    $0.error = nil
                }
            }
        } catch {
            // Keep polling; show why on the card.
            update(id) { $0.error = error.localizedDescription }
        }
    }

    private func download(_ remote: URL, kind: MediaKind, jobID: UUID, index: Int) async throws -> String {
        guard remote.scheme == "https" else { throw KieError("KIE devolvió una URL no soportada.", definite: true) }
        var request = URLRequest(url: remote)
        request.timeoutInterval = kind == .video ? 180 : 60
        let (temp, response) = try await URLSession.shared.download(for: request)
        let status = (response as? HTTPURLResponse)?.statusCode ?? 200
        guard (200..<300).contains(status) else { throw KieError("Todavía no se pudo guardar el archivo. Se reintenta solo.", definite: false) }
        let size = ((try? FileManager.default.attributesOfItem(atPath: temp.path)[.size]) as? NSNumber)?.int64Value ?? 0
        let limit: Int64 = kind == .video ? 250 * 1024 * 1024 : 30 * 1024 * 1024
        guard size <= limit else { throw KieError("El archivo generado supera el límite de \(limit / 1024 / 1024) MB.", definite: true) }

        let mime = response.mimeType ?? ""
        let ext: String
        if kind == .video {
            ext = mime == "video/quicktime" ? "mov" : "mp4"
        } else {
            let handle = try FileHandle(forReadingFrom: temp)
            let prefix = try handle.read(upToCount: 16) ?? Data()
            try handle.close()
            if mime == "image/png" || MediaSniffer.matches(prefix: prefix, mime: "image/png", kind: .image) {
                ext = "png"
            } else if mime == "image/webp" || MediaSniffer.matches(prefix: prefix, mime: "image/webp", kind: .image) {
                ext = "webp"
            } else if mime == "image/jpeg" || MediaSniffer.matches(prefix: prefix, mime: "image/jpeg", kind: .image) {
                ext = "jpg"
            } else {
                throw KieError("KIE devolvió un formato de imagen no soportado.", definite: true)
            }
        }
        let fileName = "framecraft-\(Self.fileDate.string(from: Date()))-\(jobID.uuidString.prefix(4).lowercased())-\(index + 1).\(ext)"
        let destination = store.outputURL(fileName)
        try? FileManager.default.removeItem(at: destination)
        try FileManager.default.moveItem(at: temp, to: destination)
        return fileName
    }

    private static let fileDate: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyyMMdd-HHmmss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        return formatter
    }()

    private func markInterruptedSubmissions() {
        let cutoff = Date().addingTimeInterval(-120)
        for job in jobs where job.status == .submitting && job.created < cutoff {
            update(job.id) {
                $0.status = .unknown
                $0.error = "El envío se interrumpió. " + GenerationInputs.uncertainSubmission
            }
        }
    }

    // MARK: Library actions

    func outputURLs(_ job: Job) -> [URL] { job.outputs.map(store.outputURL) }

    func deleteJob(_ job: Job) {
        guard job.status.isFinished else {
            show("Esperá a que termine esta generación para borrarla.", .error)
            return
        }
        for url in outputURLs(job) { try? FileManager.default.removeItem(at: url) }
        withAnimation(.snappy) { jobs.removeAll { $0.id == job.id } }
        if detailJobID == job.id { detailJobID = nil }
        persist()
        show("Generación eliminada. No se reintegran créditos de KIE.")
    }

    func toggleFavorite(_ job: Job) {
        update(job.id) { $0.favorite = !$0.isFavorite }
        persist()
    }

    /// Loads a past generation back into the Create form.
    func reuse(_ job: Job) {
        prompt = job.prompt
        if job.kind == .video {
            mode = .video
            videoResolution = job.settings.resolution
            videoAspect = job.settings.aspect
            videoDuration = job.settings.duration ?? videoDuration
            videoAudio = job.settings.generateAudio ?? true
            selectedImages = job.settings.imageReferences.filter { id in references.contains { $0.id == id } }
            selectedVideos = job.settings.videoReferences.filter { id in references.contains { $0.id == id } }
            selectedAudios = job.settings.audioReferences.filter { id in references.contains { $0.id == id } }
        } else {
            mode = .image
            imageModel = job.model
            imageResolution = job.settings.resolution
            imageAspect = job.settings.aspect
            camera = job.settings.camera ?? Presets.none
            film = job.settings.film ?? Presets.none
            selectedImages = job.settings.imageReferences.filter { id in references.contains { $0.id == id } }
        }
        detailJobID = nil
        section = .create
        show("Ajustes cargados. Revisalos y generá de nuevo.", .success)
    }

    func revealInFinder(_ urls: [URL]) {
        NSWorkspace.shared.activateFileViewerSelecting(urls)
    }

    func copyToPasteboard(_ text: String) {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
        show("Copiado al portapapeles.", .success)
    }

    // MARK: Prompt assistant

    var referenceTags: [String] {
        guard mode == .video else { return [] }
        return ReferenceKind.allCases.flatMap { kind in
            selectedReferences(kind).enumerated().map { index, reference in
                var line = "\(kind.tagPrefix)\(index + 1) = \(reference.name)"
                if let ms = reference.durationMs { line += " (\(MediaTools.formattedDuration(Double(ms) / 1000)))" }
                return line
            }
        }
    }

    func buildPrompt(refine: Bool = false) async {
        let ideaText = idea.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !ideaText.isEmpty || (refine && !draft.isEmpty) else {
            assistantError = "Contame primero tu idea."
            return
        }
        guard ideaText.count <= 6000 else {
            assistantError = "La idea puede tener hasta 6000 caracteres."
            return
        }
        let key: String
        switch provider {
        case .kie:
            guard let value = kieKey else {
                assistantError = "Conectá tu clave de KIE en Ajustes para crear prompts."
                return
            }
            key = value
        case .openai:
            guard let value = keys.read(KeyAccount.openAI) else {
                assistantError = "Agregá tu clave de OpenAI en Ajustes o elegí otro motor."
                return
            }
            key = value
        case .codex:
            if case .loggedIn = codexStatus {} else {
                await refreshCodexStatus()
                guard case .loggedIn = codexStatus else {
                    assistantError = codexStatus == .notInstalled
                        ? "Codex CLI no está instalado en esta Mac. Mirá cómo instalarlo abajo, en Motor."
                        : "Iniciá sesión en Codex con tu cuenta de ChatGPT (abajo, en Motor)."
                    return
                }
            }
            key = ""
        }
        assistantError = nil
        isBuildingPrompt = true
        streamingText = ""
        defer {
            isBuildingPrompt = false
            streamingText = nil
        }

        let skill = currentSkill
        let structured = mode == .image && skill?.id == SkillLibrary.jsonProfileID
        var skillPayload: [String: Any]?
        var examples: [PromptExample] = []
        if structured, let resources {
            let context = PromptRequests.jsonProfileSkill(idea: ideaText, resources: resources)
            skillPayload = context.context
            examples = context.examples
        } else if let skill, skill.id != SkillLibrary.jsonProfileID {
            var content = skill.content
            if skill.id == SkillLibrary.ugcID, includeWalterExample {
                content += "\n\n---\n\n# Ejemplo trabajado completo (pack de Walter, 8 escenas que funcionaron)\n\n" + SkillLibrary.walterExample(resources)
            }
            skillPayload = ["name": skill.name, "content": content]
        }

        let brief = PromptBrief(
            idea: ideaText.isEmpty ? "(ver previousPrompt)" : ideaText,
            media: mode,
            targetModel: mode == .image ? imageModel : Presets.videoModelID,
            aspect: mode == .image ? imageAspect : videoAspect,
            resolution: mode == .image ? imageResolution : videoResolution,
            camera: mode == .image ? camera : Presets.none,
            film: mode == .image ? film : Presets.none,
            duration: mode == .video ? videoDuration : nil,
            generateAudio: mode == .video ? videoAudio : nil,
            dialogueLanguage: mode == .video ? dialogueLanguage : nil,
            referenceTags: referenceTags,
            previousPrompt: refine ? draft : nil,
            feedback: refine ? feedback.trimmingCharacters(in: .whitespacesAndNewlines) : nil
        )
        let briefText = PromptRequests.briefJSON(brief, skill: skillPayload)
        let instructions = PromptRequests.instructions(media: mode, structured: structured)

        do {
            let analysisIDs = Array(selectedImages.prefix(4))
            let raw: String
            switch provider {
            case .kie:
                let client = KieClient(key: key)
                try checkAnalysisSize(analysisIDs)
                let images = try await upload(analysisIDs, client: client).map(\.absoluteString)
                let model = PromptRequests.promptModels.first { $0.id == promptModel } ?? PromptRequests.promptModels[0]
                let live: @Sendable (String) -> Void = { [weak self] text in
                    Task { @MainActor in if self?.isBuildingPrompt == true { self?.streamingText = text } }
                }
                if model.usesCodexAPI {
                    raw = try await client.streamText("/codex/v1/responses", body: PromptRequests.kieCodexBody(
                        model: model.id, instructions: instructions, brief: briefText, images: images),
                        readJSON: PromptRequests.readCodex, onText: live)
                } else {
                    var answer: String?
                    var lastError: Error?
                    for slug in model.chatSlugs where answer == nil {
                        do {
                            answer = try await client.streamText("/\(slug)/v1/chat/completions", body: PromptRequests.kieChatBody(
                                model: slug, instructions: instructions, brief: briefText, images: images),
                                readJSON: PromptRequests.readChat, onText: live)
                        } catch {
                            lastError = error
                        }
                    }
                    guard let answer else { throw lastError ?? KieError("KIE no respondió.", definite: true) }
                    raw = answer
                }
                if raw.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                    throw KieError("KIE no devolvió texto. Tu idea sigue ahí: probá de nuevo.", definite: true)
                }
            case .codex:
                let images = analysisIDs.compactMap { id in references.first { $0.id == id }.map { url(for: $0) } }
                raw = try await CodexCLI.generate(
                    prompt: PromptRequests.codexCLIPrompt(instructions: instructions, brief: briefText),
                    images: images, model: nil)
            case .openai:
                try checkAnalysisSize(analysisIDs)
                let images = try analysisIDs.compactMap { id -> String? in
                    guard let reference = references.first(where: { $0.id == id }) else { return nil }
                    return "data:\(reference.mime);base64," + (try Data(contentsOf: url(for: reference))).base64EncodedString()
                }
                raw = try PromptRequests.readOpenAI(await OpenAIClient(key: key).responses(body: PromptRequests.openAIBody(
                    instructions: instructions, brief: briefText, images: images, media: mode, structured: structured)))
            }
            let result = try PromptRequests.finalize(raw, media: mode, structured: structured)
            if mode == .image, Presets.composePrompt(result.prompt, camera: camera, film: film, aspect: imageAspect).count > Presets.maxComposedLength(imageModel: imageModel) {
                throw KieError("El prompt quedó demasiado largo para este modelo. Pedí una descripción más concisa.", definite: true)
            }
            withAnimation(.snappy) {
                draft = result.prompt
                notes = result.notes
                sources = examples
            }
            if refine { feedback = "" }
            persist()
        } catch {
            assistantError = error.localizedDescription
        }
    }

    func refreshCodexStatus() async {
        codexStatus = .checking
        codexStatus = await CodexCLI.status()
    }

    func codexLogin() async {
        codexStatus = .loggingIn
        do {
            try await CodexCLI.login()
            show("Sesión de ChatGPT iniciada en Codex.", .success)
        } catch {
            show(error.localizedDescription, .error)
        }
        await refreshCodexStatus()
    }

    func codexLogout() async {
        await CodexCLI.logout()
        await refreshCodexStatus()
    }

    private func checkAnalysisSize(_ ids: [UUID]) throws {
        let total = ids.compactMap { id in references.first { $0.id == id }?.bytes }.reduce(0, +)
        guard total <= PromptRequests.maxAnalysisBytes else {
            throw KieError("Para analizar referencias, elegí imágenes que sumen 8 MB o menos (para generar se admiten más grandes).", definite: true)
        }
    }

    func useDraft() {
        prompt = draft
        section = .create
        show("Prompt listo en el editor. Revisalo y generá.", .success)
    }

    // MARK: Skills

    func importSkill(from url: URL) throws {
        let access = url.startAccessingSecurityScopedResource()
        defer { if access { url.stopAccessingSecurityScopedResource() } }
        var file = url
        var isDirectory: ObjCBool = false
        if FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory), isDirectory.boolValue {
            file = url.appendingPathComponent("SKILL.md")
        }
        let attributes = try FileManager.default.attributesOfItem(atPath: file.path)
        guard ((attributes[.size] as? NSNumber)?.intValue ?? 0) <= 200_000 else {
            throw KieError("La skill es demasiado grande (máximo 200 KB).", definite: true)
        }
        let text = try String(contentsOf: file, encoding: .utf8)
        let fallback = file.lastPathComponent == "SKILL.md" ? url.lastPathComponent : file.deletingPathExtension().lastPathComponent
        let parsed = SkillLibrary.parse(markdown: text, fallbackName: fallback)
        guard !parsed.body.isEmpty else { throw KieError("El archivo está vacío.", definite: true) }
        let skill = Skill(name: String(parsed.name.prefix(100)), summary: parsed.summary, content: parsed.body,
                          media: SkillLibrary.guessMedia(name: parsed.name, summary: parsed.summary, body: parsed.body))
        customSkills.append(skill)
        persist()
        show("Skill “\(skill.name)” importada.", .success)
    }

    func saveSkill(_ skill: Skill) {
        if let index = customSkills.firstIndex(where: { $0.id == skill.id }) {
            customSkills[index] = skill
        } else {
            customSkills.append(skill)
        }
        persist()
    }

    func duplicate(_ skill: Skill) -> Skill {
        let copy = Skill(name: skill.name + " (copia)", summary: skill.summary, content: skill.content, media: skill.media)
        customSkills.append(copy)
        persist()
        return copy
    }

    func deleteSkill(_ skill: Skill) {
        customSkills.removeAll { $0.id == skill.id }
        if skillID == skill.id { skillID = mode == .video ? SkillLibrary.ugcID : SkillLibrary.jsonProfileID }
        persist()
    }

    func useSkill(_ skill: Skill) {
        if !skill.media.supports(mode) { mode = skill.media == .video ? .video : .image }
        skillID = skill.id
        section = .create
        showAssistant = true
    }

    // MARK: System integration

    private func updateDockBadge() {
        guard !isDemo else { return }
        let active = activeJobs.count
        NSApp?.dockTile.badgeLabel = active > 0 ? "\(active)" : nil
    }

    private var notificationsAvailable: Bool { Bundle.main.bundleIdentifier != nil && !isDemo }

    private func requestNotificationPermission() {
        guard notifyWhenDone, notificationsAvailable, !askedNotifications else { return }
        askedNotifications = true
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound, .badge]) { _, _ in }
    }

    private func notifyFinished(_ id: UUID) {
        guard notifyWhenDone, notificationsAvailable, !(NSApp?.isActive ?? true),
              let job = jobs.first(where: { $0.id == id }) else { return }
        let content = UNMutableNotificationContent()
        content.title = job.status == .success
            ? (job.kind == .video ? "Tu video está listo" : "Tu imagen está lista")
            : "La generación falló"
        content.body = String(job.prompt.prefix(120))
        content.sound = .default
        UNUserNotificationCenter.current().add(UNNotificationRequest(identifier: id.uuidString, content: content, trigger: nil))
    }
}
