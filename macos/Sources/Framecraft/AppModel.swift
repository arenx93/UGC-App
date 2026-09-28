import AppKit
import FramecraftCore
import SwiftUI
import UniformTypeIdentifiers
import UserNotifications

enum SidebarItem: String, Hashable, CaseIterable, Identifiable {
    case create, stories, library, references, skills, guide
    var id: String { rawValue }

    var title: String {
        switch self {
        case .create: "Crear"
        case .stories: "Historias"
        case .library: "Biblioteca"
        case .references: "Referencias"
        case .skills: "Skills"
        case .guide: "Guía UGC"
        }
    }

    var symbol: String {
        switch self {
        case .create: "wand.and.stars"
        case .stories: "film.stack"
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

/// Progress of "Generar todo" in a story.
struct StoryRun: Equatable {
    var storyID: UUID
    var current: Int
    var total: Int
    var text: String
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
    var showCommandPalette = false
    var banner: Banner?
    var detailJobID: UUID?
    var quickLookURL: URL?

    // MARK: Library
    var jobs: [Job] = []
    var stories: [Story] = []
    var selectedStoryID: UUID?
    /// What the story director is doing right now ("Escribiendo la historia…").
    var storyStatus: String?
    var storyStreaming: String?
    var storyError: String?
    /// Scene opened in Create; the next video generated is linked to it.
    @ObservationIgnored var pendingSceneLink: (story: UUID, scene: UUID)?
    /// "Generar todo": scenes sent one after another.
    var storyRun: StoryRun?
    @ObservationIgnored var storyRunTask: Task<Void, Never>?
    /// Story whose final video is being assembled.
    var assemblingStoryID: UUID?
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
    /// Every prompt the assistant wrote in this session (newest last), to go back to an earlier version.
    var draftHistory: [String] = []
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
        stories = (index.stories ?? []).sorted { $0.updated > $1.updated }
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
        promptModel = PromptRequests.promptModels.contains { $0.id == p.promptModel } ? p.promptModel : PromptRequests.defaultPromptModel
        dialogueLanguage = p.dialogueLanguage
        notifyWhenDone = p.notifyWhenDone
        onboardingDone = p.onboardingDone
        skillID = p.mode == .video ? SkillLibrary.ugcID : SkillLibrary.generalID

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
        index.stories = stories
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
        if !(currentSkill?.media.supports(mode) ?? true) || (mode == .video && skillID == SkillLibrary.generalID) {
            skillID = mode == .video ? SkillLibrary.ugcID : SkillLibrary.generalID
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

    /// Name of the assistant's current engine ("GPT-5.6 Terra", "ChatGPT (Codex)"…).
    var engineName: String {
        switch provider {
        case .kie: PromptRequests.promptModels.first { $0.id == promptModel }?.name ?? "KIE"
        case .codex: "ChatGPT (Codex)"
        case .apple: "Apple Intelligence"
        case .openai: "OpenAI"
        }
    }

    // MARK: Templates

    /// Loads a template: right mode, settings and skill, and its idea in the assistant.
    func applyTemplate(_ template: CreativeTemplate) {
        mode = template.media
        if template.media == .video {
            if let duration = template.duration {
                videoDuration = min(max(duration, Presets.videoDurationRange.lowerBound), Presets.videoDurationRange.upperBound)
            }
            if Presets.videoAspects.contains(template.aspect) { videoAspect = template.aspect }
            skillID = SkillLibrary.ugcID
        } else {
            if aspectOptions.contains(template.aspect) { imageAspect = template.aspect }
            skillID = SkillLibrary.generalID
        }
        idea = template.idea
        section = .create
        withAnimation(.snappy) { showAssistant = true }
        show(template.placeholders.isEmpty
             ? "Plantilla “\(template.title)” lista en el asistente."
             : "Plantilla “\(template.title)”: cambiá lo que está entre [corchetes] y tocá Crear prompt.", .success)
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
    /// Saves the last frame of a generated video as an image reference.
    func lastFrameReference(of job: Job, name: String? = nil) async throws -> ReferenceFile {
        guard let output = job.outputs.first else { throw KieError("Ese video todavía no tiene archivo.", definite: true) }
        let frame = try await MediaTools.lastFrame(of: store.outputURL(output))
        guard let png = MediaTools.pngData(frame) else { throw KieError("No se pudo guardar el fotograma.", definite: true) }
        let id = UUID()
        let fileName = "\(id.uuidString.lowercased()).png"
        try png.write(to: store.referencesDir.appendingPathComponent(fileName))
        let reference = ReferenceFile(id: id, name: name ?? "Último fotograma · \(job.prompt.prefix(40))", kind: .image, mime: "image/png",
                                      fileName: fileName, bytes: Int64(png.count))
        references.insert(reference, at: 0)
        return reference
    }

    /// Extracts the last frame of a generated video so the next clip can start exactly there.
    func continueFromLastFrame(_ job: Job) async {
        do {
            let reference = try await lastFrameReference(of: job)
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
        if let link = pendingSceneLink {
            updateScene(story: link.story, scene: link.scene) { $0.jobIDs.append(job.id) }
            pendingSceneLink = nil
        }
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

    static let fileDate: DateFormatter = {
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
                        ? "No se encontró Codex en esta copia de la app. Descargá la última versión de Framecraft."
                        : "Iniciá sesión en Codex con tu cuenta de ChatGPT (abajo, en Motor)."
                    return
                }
            }
            key = ""
        case .apple:
            if mode == .image && currentSkill?.id == SkillLibrary.jsonProfileID {
                assistantError = "El perfil JSON es demasiado largo para el modelo del Mac. Elegí la skill General o el motor KIE / ChatGPT."
                return
            }
            let state = OnDeviceModel.availability
            guard state == .available else {
                assistantError = state.message
                return
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
                raw = try await runKie(model, client: client, instructions: instructions, brief: briefText, images: images, onText: live)
                if raw.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                    throw KieError("KIE no devolvió texto. Tu idea sigue ahí: probá de nuevo.", definite: true)
                }
            case .codex:
                let images = analysisIDs.compactMap { id in references.first { $0.id == id }.map { url(for: $0) } }
                raw = try await CodexCLI.generate(
                    prompt: PromptRequests.codexCLIPrompt(instructions: instructions, brief: briefText),
                    images: images, model: nil)
            case .apple:
                raw = try await OnDeviceModel.generate(
                    instructions: PromptRequests.onDeviceInstructions(media: mode, ugc: skill?.id == SkillLibrary.ugcID),
                    prompt: PromptRequests.onDeviceBrief(brief))
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
            draftHistory.append(result.prompt)
            if draftHistory.count > 20 { draftHistory.removeFirst() }
            if refine { feedback = "" }
            persist()
        } catch {
            assistantError = error.localizedDescription
        }
    }

    /// Tries each documented route for the model until one answers.
    private func runKie(_ model: PromptModel, client: KieClient, instructions: String, brief: String, images: [String],
                        maxTokens: Int = 8000, onText: @escaping @Sendable (String) -> Void) async throws -> String {
        var lastError: Error?
        for request in PromptRequests.kieRequests(for: model, instructions: instructions, brief: brief, images: images, maxTokens: maxTokens) {
            do {
                return try await client.streamText(request.path, body: request.body, readJSON: PromptRequests.reader(for: model), onText: onText)
            } catch {
                lastError = error
            }
        }
        throw lastError ?? KieError("KIE no respondió.", definite: true)
    }

    /// Result of "Probar conexión" for the selected KIE prompt model.
    var connectionTest: String?
    var isTestingConnection = false

    func testPromptModel() async {
        guard let key = kieKey else {
            connectionTest = "Conectá tu clave de KIE primero."
            return
        }
        let model = PromptRequests.promptModels.first { $0.id == promptModel } ?? PromptRequests.promptModels[0]
        isTestingConnection = true
        connectionTest = nil
        defer { isTestingConnection = false }
        let start = Date()
        do {
            let answer = try await runKie(model, client: KieClient(key: key), instructions: "Reply with the single word OK.",
                                          brief: "ping", images: [], onText: { _ in })
            let seconds = Date().timeIntervalSince(start)
            connectionTest = "✓ \(model.name) responde (\(String(format: "%.1f", seconds)) s): \(answer.prefix(40))"
        } catch {
            connectionTest = "✗ \(model.name): \(error.localizedDescription)"
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

    /// Main text prompt inside a JSON visual profile (nil when the draft is plain text).
    var draftProfilePrompt: String? { PromptRequests.mainPrompt(fromProfile: draft) }

    /// Uses only the readable text prompt from a JSON profile.
    func useDraftText() {
        guard let text = draftProfilePrompt else { return useDraft() }
        prompt = text
        section = .create
        show("Prompt de texto listo en el editor.", .success)
    }

    func restoreDraft(at index: Int) {
        guard draftHistory.indices.contains(index) else { return }
        withAnimation(.snappy) { draft = draftHistory[index] }
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
        if skillID == skill.id { skillID = mode == .video ? SkillLibrary.ugcID : SkillLibrary.generalID }
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

    func requestNotificationPermission() {
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

// MARK: - Stories

extension AppModel {
    var selectedStory: Story? { stories.first { $0.id == selectedStoryID } }

    func newStory(from template: StoryTemplate? = nil) {
        var story = Story()
        if let template {
            story.title = template.title
            story.brief = template.brief
            story.sceneCount = template.sceneCount
            story.sceneDuration = template.sceneDuration
        }
        stories.insert(story, at: 0)
        selectedStoryID = story.id
        section = .stories
        persist()
    }

    func deleteStory(_ id: UUID) {
        stories.removeAll { $0.id == id }
        if selectedStoryID == id { selectedStoryID = stories.first?.id }
        persist()
    }

    func updateStory(_ id: UUID, _ change: (inout Story) -> Void) {
        guard let index = stories.firstIndex(where: { $0.id == id }) else { return }
        change(&stories[index])
        stories[index].updated = Date()
    }

    func updateScene(story: UUID, scene: UUID, _ change: (inout StoryScene) -> Void) {
        updateStory(story) { story in
            guard let index = story.scenes.firstIndex(where: { $0.id == scene }) else { return }
            change(&story.scenes[index])
        }
    }

    func referenceLines(_ story: Story) -> [String] {
        let names = Dictionary(uniqueKeysWithValues: references.map { ($0.id, $0.name) })
        return StoryRequests.referenceLines(story, names: names)
    }

    func displayLines(_ story: Story) -> [String] {
        let names = Dictionary(uniqueKeysWithValues: references.map { ($0.id, $0.name) })
        return StoryRequests.displayLines(story, names: names)
    }

    private func skillPayload(for id: String) -> [String: Any]? {
        guard let skill = allSkills.first(where: { $0.id == id }), skill.id != SkillLibrary.jsonProfileID else { return nil }
        var content = skill.content
        if skill.id == SkillLibrary.ugcID {
            // Stories always get the worked example: it is the model of a full pack.
            content += "\n\n---\n\n# Ejemplo trabajado completo (pack de Walter, 8 escenas que funcionaron)\n\n" + SkillLibrary.walterExample(resources)
        }
        return ["name": skill.name, "content": content]
    }

    /// Runs the assistant with the selected engine (KIE, ChatGPT/Codex or OpenAI) and returns its raw text.
    private func runEngine(instructions: String, brief: String, imageIDs: [UUID], maxTokens: Int,
                           onText: @escaping @Sendable (String) -> Void) async throws -> String {
        switch provider {
        case .kie:
            guard let key = keys.read(KeyAccount.kie) else { throw KieError("Conectá tu clave de KIE en Ajustes.", definite: true) }
            let client = KieClient(key: key)
            let images = try await upload(Array(imageIDs.prefix(4)), client: client).map(\.absoluteString)
            let model = PromptRequests.promptModels.first { $0.id == promptModel } ?? PromptRequests.promptModels[0]
            return try await runKie(model, client: client, instructions: instructions, brief: brief, images: images, maxTokens: maxTokens, onText: onText)
        case .codex:
            if case .loggedIn = codexStatus {} else { await refreshCodexStatus() }
            guard case .loggedIn = codexStatus else { throw KieError("Iniciá sesión con ChatGPT (Asistente → Motor) o elegí KIE.", definite: true) }
            let images = imageIDs.prefix(4).compactMap { id in references.first { $0.id == id }.map { url(for: $0) } }
            return try await CodexCLI.generate(prompt: PromptRequests.codexCLIPrompt(instructions: instructions, brief: brief), images: Array(images), model: nil)
        case .apple:
            throw KieError("Las historias necesitan un modelo grande: en el asistente de Crear elegí KIE o ChatGPT como motor.", definite: true)
        case .openai:
            guard let key = keys.read(KeyAccount.openAI) else { throw KieError("Agregá tu clave de OpenAI en Ajustes o elegí otro motor.", definite: true) }
            let images = try imageIDs.prefix(4).compactMap { id -> String? in
                guard let reference = references.first(where: { $0.id == id }) else { return nil }
                return "data:\(reference.mime);base64," + (try Data(contentsOf: url(for: reference))).base64EncodedString()
            }
            return try PromptRequests.readOpenAI(await OpenAIClient(key: key).responses(body: PromptRequests.openAIBody(
                instructions: instructions, brief: brief, images: images, media: .video, structured: false, maxOutputTokens: maxTokens)))
        }
    }

    private func storyImageIDs(_ story: Story) -> [UUID] {
        story.slots.filter { $0.kind == .image && !$0.isLastFrame }.compactMap(\.referenceID)
    }

    private func liveStoryText() -> @Sendable (String) -> Void {
        { [weak self] text in Task { @MainActor in if self?.storyStatus != nil { self?.storyStreaming = text } } }
    }

    /// Writes (or rewrites, with `feedback`) the whole story as a pack of scene prompts.
    func generateStory(_ id: UUID, feedback: String? = nil) async {
        guard let story = stories.first(where: { $0.id == id }), storyStatus == nil else { return }
        guard !story.brief.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            storyError = "Escribí el brief de la historia."
            return
        }
        let revising = feedback != nil && !story.scenes.isEmpty
        storyError = nil
        storyStatus = revising ? "Reescribiendo la historia…" : "Escribiendo la historia…"
        storyStreaming = ""
        defer {
            storyStatus = nil
            storyStreaming = nil
        }
        if let feedback { updateStory(id) { $0.messages.append(StoryMessage(role: .user, text: feedback)) } }
        let brief = StoryRequests.storyBrief(story, referenceLines: referenceLines(story), skill: skillPayload(for: story.skillID),
                                             previous: revising, feedback: feedback)
        do {
            let raw = try await runEngine(instructions: StoryRequests.storyInstructions(), brief: brief,
                                          imageIDs: storyImageIDs(story), maxTokens: 32000, onText: liveStoryText())
            let draft = try StoryRequests.parseStory(raw, defaultDuration: story.sceneDuration)
            updateStory(id) { story in
                let previous = story.scenes
                story.title = draft.title
                story.summary = draft.summary
                story.continuity = draft.continuity
                story.referenceOrder = draft.referenceOrder
                story.scenes = draft.scenes.enumerated().map { index, scene in
                    // Keep links to videos already generated for unchanged scene numbers.
                    let old = previous.indices.contains(index) ? previous[index] : nil
                    return StoryScene(id: old?.id ?? UUID(), number: index + 1, title: scene.title, summary: scene.summary,
                                      duration: scene.duration, prompt: scene.prompt, notes: scene.notes,
                                      jobIDs: old?.prompt == scene.prompt ? (old?.jobIDs ?? []) : [])
                }
                story.messages.append(StoryMessage(role: .assistant, text: revising
                    ? "Listo: actualicé la historia (\(draft.scenes.count) escenas)."
                    : "Historia creada: \(draft.scenes.count) escenas, \(story.totalDuration) s en total."))
            }
            persist()
        } catch {
            storyError = error.localizedDescription
            updateStory(id) { $0.messages.append(StoryMessage(role: .assistant, text: "No pude completar el pedido: \(error.localizedDescription)")) }
        }
    }

    /// Rewrites one scene following the user's note.
    func refineScene(story id: UUID, scene sceneID: UUID, feedback: String) async {
        guard let story = stories.first(where: { $0.id == id }), let scene = story.scenes.first(where: { $0.id == sceneID }),
              storyStatus == nil else { return }
        let note = feedback.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !note.isEmpty else { return }
        storyError = nil
        storyStatus = "Ajustando la escena \(scene.number)…"
        storyStreaming = ""
        defer {
            storyStatus = nil
            storyStreaming = nil
        }
        updateStory(id) { $0.messages.append(StoryMessage(role: .user, text: note, sceneNumber: scene.number)) }
        let brief = StoryRequests.sceneBrief(story, scene: scene, referenceLines: referenceLines(story),
                                             skill: skillPayload(for: story.skillID), feedback: note)
        do {
            let raw = try await runEngine(instructions: StoryRequests.sceneInstructions(), brief: brief,
                                          imageIDs: storyImageIDs(story), maxTokens: 12000, onText: liveStoryText())
            let revised = try StoryRequests.parseSingleScene(raw, defaultDuration: scene.duration)
            updateScene(story: id, scene: sceneID) {
                $0.title = revised.title
                $0.summary = revised.summary
                $0.duration = revised.duration
                $0.prompt = revised.prompt
                $0.notes = revised.notes
            }
            updateStory(id) { $0.messages.append(StoryMessage(role: .assistant, text: "Escena \(scene.number) actualizada.", sceneNumber: scene.number)) }
            persist()
        } catch {
            storyError = error.localizedDescription
        }
    }

    /// Loads a scene into Create with its references in the story's fixed order.
    /// With `generate`, sends it to Seedance right away.
    func openScene(story id: UUID, scene sceneID: UUID, generate: Bool) async {
        guard let story = stories.first(where: { $0.id == id }),
              let sceneIndex = story.scenes.firstIndex(where: { $0.id == sceneID }) else { return }
        let scene = story.scenes[sceneIndex]
        var selected: [ReferenceKind: [UUID]] = [:]
        for kind in ReferenceKind.allCases {
            let needed = PromptLinter.highestTag(kind.tagPrefix, in: scene.prompt)
            var ids: [UUID] = []
            for slot in story.slots(kind).prefix(needed) {
                if slot.isLastFrame {
                    guard sceneIndex > 0,
                          let previousJob = story.scenes[sceneIndex - 1].jobIDs.reversed()
                            .compactMap({ jobID in jobs.first { $0.id == jobID && $0.status == .success } }).first
                    else {
                        show("\(story.tag(for: slot)) es el último fotograma de la escena anterior: generá primero la escena \(scene.number - 1).", .error)
                        return
                    }
                    do {
                        let frame = try await lastFrameReference(of: previousJob, name: "\(story.title) · último fotograma escena \(scene.number - 1)")
                        ids.append(frame.id)
                    } catch {
                        show("No se pudo extraer el último fotograma: \(error.localizedDescription)", .error)
                        return
                    }
                } else if let referenceID = slot.referenceID, references.contains(where: { $0.id == referenceID }) {
                    ids.append(referenceID)
                } else {
                    show("Falta el archivo de \(story.tag(for: slot)). Elegilo en las referencias de la historia.", .error)
                    return
                }
            }
            selected[kind] = ids
        }
        mode = .video
        prompt = scene.prompt
        videoDuration = min(max(scene.duration, Presets.videoDurationRange.lowerBound), Presets.videoDurationRange.upperBound)
        videoAspect = story.aspect
        videoResolution = story.resolution
        videoAudio = story.generateAudio
        skillID = story.skillID
        selectedImages = selected[.image] ?? []
        selectedVideos = selected[.video] ?? []
        selectedAudios = selected[.audio] ?? []
        pendingSceneLink = (story.id, scene.id)
        persist()
        if generate {
            await self.generate()
            pendingSceneLink = nil
            show("Escena \(scene.number) enviada a Seedance.", .success)
        } else {
            section = .create
            show("Escena \(scene.number) cargada en Crear con sus referencias en orden.", .success)
        }
    }

    func exportStory(_ id: UUID) {
        guard let story = stories.first(where: { $0.id == id }) else { return }
        let panel = NSSavePanel()
        panel.nameFieldStringValue = "\(story.title) - pack de prompts.txt"
        panel.allowedContentTypes = [.plainText]
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do {
            try StoryRequests.exportText(story, referenceLines: displayLines(story)).write(to: url, atomically: true, encoding: .utf8)
            show("Pack exportado.", .success)
        } catch {
            show("No se pudo exportar: \(error.localizedDescription)", .error)
        }
    }

    func latestJob(for scene: StoryScene) -> Job? {
        scene.jobIDs.reversed().compactMap { id in jobs.first { $0.id == id } }.first
    }

    func finishedClip(for scene: StoryScene) -> Job? {
        scene.jobIDs.reversed().compactMap { id in jobs.first { $0.id == id && $0.status == .success && !$0.outputs.isEmpty } }.first
    }

    // MARK: Generate every scene

    /// Sends every scene that has no video yet. When the story uses "last frame of the previous
    /// scene", each scene waits for the previous one to finish so its frame can be extracted.
    func generateAllScenes(_ id: UUID) {
        guard storyRun == nil, let story = stories.first(where: { $0.id == id }), !story.scenes.isEmpty else { return }
        guard hasKieKey else {
            showOnboarding = true
            return
        }
        requestNotificationPermission()
        let chained = story.slots.contains { $0.isLastFrame }
        storyRun = StoryRun(storyID: id, current: 0, total: story.scenes.count, text: "Preparando…")
        storyRunTask = Task { [weak self] in
            await self?.runAllScenes(id, chained: chained)
        }
    }

    func cancelStoryRun() {
        storyRunTask?.cancel()
        storyRunTask = nil
        storyRun = nil
        show("Se detuvo “Generar todo”. Las escenas ya enviadas siguen generándose.")
    }

    private func runAllScenes(_ id: UUID, chained: Bool) async {
        defer {
            storyRun = nil
            storyRunTask = nil
        }
        guard let sceneIDs = stories.first(where: { $0.id == id })?.scenes.map(\.id) else { return }
        let total = sceneIDs.count
        var sent = 0
        for (index, sceneID) in sceneIDs.enumerated() {
            guard !Task.isCancelled, let scene = stories.first(where: { $0.id == id })?.scenes.first(where: { $0.id == sceneID }) else { return }
            storyRun = StoryRun(storyID: id, current: index + 1, total: total, text: "Escena \(index + 1) de \(total)")
            let existing = latestJob(for: scene)
            if existing == nil || existing?.status == .fail || existing?.status == .unknown {
                storyRun?.text = "Enviando la escena \(index + 1) de \(total)…"
                await openScene(story: id, scene: sceneID, generate: true)
                guard let updated = stories.first(where: { $0.id == id })?.scenes.first(where: { $0.id == sceneID }),
                      let job = latestJob(for: updated), job.id != existing?.id else {
                    show("No se pudo enviar la escena \(index + 1). Revisala y volvé a tocar “Generar todo”.", .error)
                    return
                }
                sent += 1
            }
            if chained, index < total - 1 {
                storyRun?.text = "Esperando que termine la escena \(index + 1) para encadenar la \(index + 2)…"
                while !Task.isCancelled,
                      let current = stories.first(where: { $0.id == id })?.scenes.first(where: { $0.id == sceneID }),
                      let job = latestJob(for: current), job.status.isActive {
                    try? await Task.sleep(for: .seconds(4))
                }
                guard !Task.isCancelled else { return }
                if let current = stories.first(where: { $0.id == id })?.scenes.first(where: { $0.id == sceneID }), finishedClip(for: current) == nil {
                    show("La escena \(index + 1) falló: ajustala y volvé a tocar “Generar todo” (sigue desde ahí).", .error)
                    return
                }
            }
        }
        show(sent == 0 ? "Todas las escenas ya tenían video." : "Listo: \(sent) escena\(sent == 1 ? "" : "s") en camino. Cuando terminen, armá el video final.", .success)
    }

    // MARK: Final cut

    /// Joins the latest finished video of every scene into one MP4 (made on the Mac, no credits).
    func assembleStory(_ id: UUID) async {
        guard assemblingStoryID == nil, let story = stories.first(where: { $0.id == id }) else { return }
        let clips = story.scenes.map { scene in finishedClip(for: scene) }
        let missing = zip(story.scenes, clips).filter { $0.1 == nil }.map { "\($0.0.number)" }
        guard missing.isEmpty else {
            show("Faltan videos de la\(missing.count == 1 ? "" : "s") escena\(missing.count == 1 ? "" : "s") \(missing.joined(separator: ", ")).", .error)
            return
        }
        assemblingStoryID = id
        defer { assemblingStoryID = nil }
        let urls = clips.compactMap { $0?.outputs.first.map(store.outputURL) }
        let fileName = "framecraft-historia-\(Self.fileDate.string(from: Date()))-\(id.uuidString.prefix(4).lowercased()).mp4"
        let destination = store.outputURL(fileName)
        do {
            try await MediaTools.concatenate(urls, to: destination)
            let job = Job(batchID: UUID(), kind: .video, model: Presets.storyCutModelID, prompt: "Historia completa: \(story.title)",
                          finalPrompt: story.summary,
                          settings: JobSettings(resolution: story.resolution, aspect: story.aspect, duration: story.totalDuration,
                                                generateAudio: story.generateAudio),
                          status: .success, progress: 100, outputs: [fileName])
            withAnimation(.snappy) { jobs.insert(job, at: 0) }
            updateStory(id) { $0.finalCutJobID = job.id }
            persist()
            show("Video final listo: \(story.scenes.count) escenas, \(story.totalDuration) s. Está en la Biblioteca.", .success)
            quickLookURL = destination
        } catch {
            show("No se pudo armar el video final: \(error.localizedDescription)", .error)
        }
    }
}
