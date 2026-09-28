import Foundation

public enum MediaKind: String, Codable, Sendable, CaseIterable, Identifiable {
    case image, video
    public var id: String { rawValue }
}

public enum JobStatus: String, Codable, Sendable {
    case submitting, queued, generating, saving, success, fail, unknown

    public var isActive: Bool { [.submitting, .queued, .generating, .saving].contains(self) }
    /// A finished job can be deleted; an active one must be waited for.
    public var isFinished: Bool { !isActive }
}

/// Settings a generation was created with (used by "Reusar ajustes").
public struct JobSettings: Codable, Hashable, Sendable {
    public var resolution: String
    public var aspect: String
    public var camera: String?
    public var film: String?
    public var duration: Int?
    public var generateAudio: Bool?
    public var imageReferences: [UUID]
    public var videoReferences: [UUID]
    public var audioReferences: [UUID]

    public init(
        resolution: String, aspect: String, camera: String? = nil, film: String? = nil,
        duration: Int? = nil, generateAudio: Bool? = nil,
        imageReferences: [UUID] = [], videoReferences: [UUID] = [], audioReferences: [UUID] = []
    ) {
        self.resolution = resolution
        self.aspect = aspect
        self.camera = camera
        self.film = film
        self.duration = duration
        self.generateAudio = generateAudio
        self.imageReferences = imageReferences
        self.videoReferences = videoReferences
        self.audioReferences = audioReferences
    }
}

/// One generation. Image batches create one job per image (shared `batchID`).
public struct Job: Codable, Identifiable, Hashable, Sendable {
    public var id: UUID
    public var batchID: UUID
    public var kind: MediaKind
    public var model: String
    public var prompt: String
    public var finalPrompt: String
    public var settings: JobSettings
    public var status: JobStatus
    public var progress: Int
    public var taskId: String?
    /// File names inside the library's generations folder.
    public var outputs: [String]
    public var error: String?
    public var created: Date
    public var updated: Date
    public var favorite: Bool?

    public init(
        id: UUID = UUID(), batchID: UUID, kind: MediaKind, model: String, prompt: String, finalPrompt: String,
        settings: JobSettings, status: JobStatus = .submitting, progress: Int = 0, taskId: String? = nil,
        outputs: [String] = [], error: String? = nil, created: Date = Date(), updated: Date = Date(), favorite: Bool? = nil
    ) {
        self.id = id
        self.batchID = batchID
        self.kind = kind
        self.model = model
        self.prompt = prompt
        self.finalPrompt = finalPrompt
        self.settings = settings
        self.status = status
        self.progress = progress
        self.taskId = taskId
        self.outputs = outputs
        self.error = error
        self.created = created
        self.updated = updated
        self.favorite = favorite
    }

    public var isFavorite: Bool { favorite ?? false }
}

public enum ReferenceKind: String, Codable, Sendable, CaseIterable, Identifiable {
    case image, video, audio
    public var id: String { rawValue }

    /// Tag used inside Seedance prompts: @Image1, @Video1, @Audio1…
    public var tagPrefix: String {
        switch self {
        case .image: "@Image"
        case .video: "@Video"
        case .audio: "@Audio"
        }
    }

    public var maxBytes: Int64 {
        switch self {
        case .image: 30 * 1024 * 1024
        case .video: 200 * 1024 * 1024
        case .audio: 15 * 1024 * 1024
        }
    }

    public var maxMegabytes: Int { Int(maxBytes / 1024 / 1024) }

    public var allowedMimes: [String] {
        switch self {
        case .image: ["image/png", "image/jpeg", "image/webp"]
        case .video: ["video/mp4", "video/quicktime", "video/x-matroska"]
        case .audio: ["audio/mpeg", "audio/wav", "audio/x-wav", "audio/aac", "audio/mp4", "audio/ogg"]
        }
    }

    public var formatsDescription: String {
        switch self {
        case .image: "PNG, JPG o WebP"
        case .video: "MP4, MOV o MKV"
        case .audio: "MP3, WAV, AAC, M4A u OGG"
        }
    }
}

/// A reference file the user imported (stored in the library folder).
public struct ReferenceFile: Codable, Identifiable, Hashable, Sendable {
    public var id: UUID
    public var name: String
    public var kind: ReferenceKind
    public var mime: String
    public var fileName: String
    public var durationMs: Int?
    public var bytes: Int64
    public var created: Date

    public init(
        id: UUID = UUID(), name: String, kind: ReferenceKind, mime: String, fileName: String,
        durationMs: Int? = nil, bytes: Int64, created: Date = Date()
    ) {
        self.id = id
        self.name = name
        self.kind = kind
        self.mime = mime
        self.fileName = fileName
        self.durationMs = durationMs
        self.bytes = bytes
        self.created = created
    }

    public var durationSeconds: Double { Double(durationMs ?? 0) / 1000 }
}

public enum SkillMedia: String, Codable, Sendable, CaseIterable {
    case image, video, any

    public func supports(_ kind: MediaKind) -> Bool {
        self == .any || rawValue == kind.rawValue
    }
}

/// A creative skill: text instructions that guide the prompt assistant.
public struct Skill: Codable, Identifiable, Hashable, Sendable {
    public var id: String
    public var name: String
    public var summary: String
    public var content: String
    public var media: SkillMedia
    public var isBuiltin: Bool
    public var created: Date

    public init(
        id: String = UUID().uuidString, name: String, summary: String, content: String,
        media: SkillMedia = .any, isBuiltin: Bool = false, created: Date = Date()
    ) {
        self.id = id
        self.name = name
        self.summary = summary
        self.content = content
        self.media = media
        self.isBuiltin = isBuiltin
        self.created = created
    }
}
