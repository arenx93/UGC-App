import Foundation
import FramecraftCore

/// Choices remembered between launches.
struct Preferences: Codable, Equatable {
    var mode: MediaKind = .image
    var imageModel = "gpt-image-2"
    var imageResolution = "1K"
    var imageAspect = "1:1"
    var quantity = 1
    var camera = Presets.none
    var film = Presets.none
    var videoResolution = "720p"
    var videoAspect = "9:16"
    var videoDuration = 10
    var videoAudio = true
    var provider: PromptProvider = .kie
    var promptModel = PromptRequests.defaultPromptModel
    var dialogueLanguage = "Español"
    var notifyWhenDone = true
    var onboardingDone = false
}

struct LibraryIndex: Codable {
    var version = 1
    var jobs: [Job] = []
    var references: [ReferenceFile] = []
    var skills: [Skill] = []
    var preferences = Preferences()
}

/// Stores the library index (JSON) and the media files.
/// - Index: ~/Library/Application Support/Framecraft Studio/library.json
/// - Media: ~/Pictures/Framecraft/{Generaciones,Referencias}
final class LibraryStore {
    let indexURL: URL
    let mediaRoot: URL
    var generationsDir: URL { mediaRoot.appendingPathComponent("Generaciones", isDirectory: true) }
    var referencesDir: URL { mediaRoot.appendingPathComponent("Referencias", isDirectory: true) }

    init(indexURL: URL, mediaRoot: URL) {
        self.indexURL = indexURL
        self.mediaRoot = mediaRoot
        let fm = FileManager.default
        try? fm.createDirectory(at: indexURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? fm.createDirectory(at: generationsDir, withIntermediateDirectories: true)
        try? fm.createDirectory(at: referencesDir, withIntermediateDirectories: true)
    }

    static func standard() -> LibraryStore {
        let fm = FileManager.default
        let support = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Framecraft Studio", isDirectory: true)
        let pictures = fm.urls(for: .picturesDirectory, in: .userDomainMask).first
            ?? fm.homeDirectoryForCurrentUser.appendingPathComponent("Pictures")
        return LibraryStore(
            indexURL: support.appendingPathComponent("library.json"),
            mediaRoot: pictures.appendingPathComponent("Framecraft", isDirectory: true)
        )
    }

    static func temporary() -> LibraryStore {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("framecraft-demo-\(UUID().uuidString)", isDirectory: true)
        return LibraryStore(indexURL: root.appendingPathComponent("library.json"), mediaRoot: root.appendingPathComponent("Media"))
    }

    func load() -> LibraryIndex {
        guard let data = try? Data(contentsOf: indexURL) else { return LibraryIndex() }
        do {
            return try Self.decoder.decode(LibraryIndex.self, from: data)
        } catch {
            // Keep a copy of an unreadable index instead of overwriting it.
            let backup = indexURL.deletingPathExtension().appendingPathExtension("unreadable-\(Int(Date().timeIntervalSince1970)).json")
            try? FileManager.default.copyItem(at: indexURL, to: backup)
            return LibraryIndex()
        }
    }

    func save(_ index: LibraryIndex) {
        guard let data = try? Self.encoder.encode(index) else { return }
        try? data.write(to: indexURL, options: [.atomic])
    }

    func outputURL(_ fileName: String) -> URL { generationsDir.appendingPathComponent(fileName) }
    func referenceURL(_ reference: ReferenceFile) -> URL { referencesDir.appendingPathComponent(reference.fileName) }

    static let encoder: JSONEncoder = {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        return encoder
    }()

    static let decoder: JSONDecoder = {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }()
}
