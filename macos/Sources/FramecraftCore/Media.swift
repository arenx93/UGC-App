import Foundation

/// File-type detection for reference files. Port of the checks in app/api/[...path]/route.ts.
public enum MediaSniffer {
    static let byExtension: [String: (ReferenceKind, String)] = [
        "png": (.image, "image/png"), "jpg": (.image, "image/jpeg"), "jpeg": (.image, "image/jpeg"), "webp": (.image, "image/webp"),
        "mp4": (.video, "video/mp4"), "m4v": (.video, "video/mp4"), "mov": (.video, "video/quicktime"), "mkv": (.video, "video/x-matroska"),
        "mp3": (.audio, "audio/mpeg"), "wav": (.audio, "audio/wav"), "aac": (.audio, "audio/aac"), "m4a": (.audio, "audio/mp4"), "ogg": (.audio, "audio/ogg"),
    ]

    public static var supportedExtensions: [String] { Array(byExtension.keys).sorted() }

    /// Kind and MIME type from a file extension (nil when unsupported).
    public static func classify(_ url: URL) -> (kind: ReferenceKind, mime: String)? {
        guard let match = byExtension[url.pathExtension.lowercased()] else { return nil }
        return (match.0, match.1)
    }

    /// Checks the first bytes of the file against its declared format.
    public static func matches(prefix: Data, mime: String, kind: ReferenceKind) -> Bool {
        let b = [UInt8](prefix.prefix(16))
        guard b.count >= 4 else { return false }
        func ascii(_ range: Range<Int>) -> String {
            guard range.upperBound <= b.count else { return "" }
            return String(decoding: b[range], as: UTF8.self)
        }
        switch kind {
        case .image:
            switch mime {
            case "image/png": return b[0] == 137 && b[1] == 80 && b[2] == 78 && b[3] == 71
            case "image/jpeg": return b[0] == 255 && b[1] == 216 && b[2] == 255
            default: return ascii(0..<4) == "RIFF" && ascii(8..<12) == "WEBP"
            }
        case .video:
            if mime == "video/x-matroska" { return b[0] == 26 && b[1] == 69 && b[2] == 223 && b[3] == 163 }
            return ascii(4..<8) == "ftyp"
        case .audio:
            if mime.contains("wav") { return ascii(0..<4) == "RIFF" && ascii(8..<12) == "WAVE" }
            if mime == "audio/ogg" { return ascii(0..<4) == "OggS" }
            if mime == "audio/mp4" { return ascii(4..<8) == "ftyp" }
            if mime == "audio/aac" { return b[0] == 255 && (b[1] & 246) == 240 }
            return ascii(0..<3) == "ID3" || (b[0] == 255 && (b[1] & 224) == 224)
        }
    }

    public static func fileExtension(forMime mime: String) -> String {
        switch mime {
        case "image/png": "png"
        case "image/jpeg": "jpg"
        case "image/webp": "webp"
        case "video/mp4": "mp4"
        case "video/quicktime": "mov"
        case "video/x-matroska": "mkv"
        case "audio/mpeg": "mp3"
        case "audio/wav", "audio/x-wav": "wav"
        case "audio/aac": "aac"
        case "audio/mp4": "m4a"
        case "audio/ogg": "ogg"
        default: "bin"
        }
    }
}

/// Builds the `input` objects for KIE createTask. Port of the generate routes.
public enum GenerationInputs {
    /// Model id and input for one image task.
    public static func image(model: String, finalPrompt: String, aspect: String, resolution: String, referenceURLs: [URL]) -> (model: String, input: [String: Any]) {
        var input: [String: Any] = ["prompt": finalPrompt, "aspect_ratio": aspect, "resolution": resolution]
        let urls = referenceURLs.map(\.absoluteString)
        if model == "nano-banana-pro" {
            input["image_input"] = urls
            input["output_format"] = "png"
            return (model, input)
        }
        if !urls.isEmpty { input["input_urls"] = urls }
        return (model + (urls.isEmpty ? "-text-to-image" : "-image-to-image"), input)
    }

    public static func video(
        prompt: String, resolution: String, aspect: String, duration: Int, generateAudio: Bool,
        images: [URL], videos: [URL], audios: [URL]
    ) -> [String: Any] {
        [
            "prompt": prompt,
            "reference_image_urls": images.map(\.absoluteString),
            "reference_video_urls": videos.map(\.absoluteString),
            "reference_audio_urls": audios.map(\.absoluteString),
            "return_last_frame": false,
            "generate_audio": generateAudio,
            "resolution": resolution,
            "aspect_ratio": aspect,
            "duration": duration,
        ]
    }

    /// Message stored on a job whose submission may or may not have reached KIE.
    public static let uncertainSubmission =
        "Puede que el pedido haya llegado a KIE. Revisá tu historial en kie.ai antes de reintentar, para no pagar dos veces."
}
