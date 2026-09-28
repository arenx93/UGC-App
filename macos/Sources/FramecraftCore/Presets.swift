import Foundation

/// An image model offered by KIE.
public struct ImageModel: Identifiable, Hashable, Sendable {
    public let id: String
    public let name: String
    public let note: String
    public let symbol: String
}

/// A camera or framing preset. `id` is the stable key (stored in the library and
/// matching the web app); `text` is the English instruction added to the prompt.
public struct StylePreset: Identifiable, Hashable, Sendable {
    public let id: String
    public let title: String
    public let symbol: String
    public let summary: String
    public let text: String
}

/// Port of `lib/presets.ts` from the web app. Keep both in sync.
public enum Presets {
    public static let none = "N/A"
    public static let videoModelID = "bytedance/seedance-2-5"
    public static let videoModelName = "Seedance 2.5"

    public static let imageModels: [ImageModel] = [
        ImageModel(id: "gpt-image-2", name: "GPT Image 2", note: "Versátil. Ideal para empezar.", symbol: "sparkles"),
        ImageModel(id: "gpt-image-2-5-flare", name: "GPT Image 2.5 · Flare", note: "Más creativo y expresivo.", symbol: "flame"),
        ImageModel(id: "gpt-image-2-5-sunburst", name: "GPT Image 2.5 · Sunburst", note: "Máximo detalle y nitidez.", symbol: "sun.max"),
        ImageModel(id: "nano-banana-pro", name: "Nano Banana Pro", note: "El que mejor respeta tus fotos de referencia.", symbol: "photo.on.rectangle.angled"),
    ]

    public static let cameras: [StylePreset] = [
        StylePreset(id: none, title: "Sin preset", symbol: "circle.slash", summary: "Solo tu prompt.", text: ""),
        StylePreset(
            id: "Android Camera", title: "Celular Android", symbol: "iphone.gen1",
            summary: "Foto casera: algo de ruido, foco suave, exposición imperfecta.",
            text: "Low-quality Android phone snapshot: slightly soft focus, subtle motion blur, visible digital noise, compressed detail and imperfect exposure. Natural, unpolished everyday photography."
        ),
        StylePreset(
            id: "iPhone Camera", title: "iPhone", symbol: "iphone",
            summary: "Foto espontánea de iPhone con piel real y luz disponible.",
            text: "Candid iPhone snapshot with natural smartphone processing, realistic skin texture, available light and spontaneous everyday framing. Avoid a polished studio look."
        ),
        StylePreset(
            id: "TV / News Camera", title: "Noticiero", symbol: "tv",
            summary: "Cuadro de cámara de noticias local, documental.",
            text: "Local television news camera footage captured as a still: documentary framing, on-location broadcast lighting, slightly compressed video detail and realistic local-news color. No station logo, lower third or text unless requested."
        ),
    ]

    public static let films: [StylePreset] = [
        StylePreset(id: none, title: "Sin encuadre", symbol: "circle.slash", summary: "El encuadre lo decide el prompt.", text: ""),
        StylePreset(
            id: "POV", title: "POV", symbol: "hand.raised",
            summary: "En primera persona: se ve una sola mano.",
            text: "First-person POV through a handheld phone at 1× magnification. Exactly one hand is visible; the other hand is holding the phone outside the frame. Slight handheld motion blur, eye-level perspective and natural framing. No extra hands or fingers."
        ),
        StylePreset(
            id: "Selfie", title: "Selfie", symbol: "person.crop.square",
            summary: "Cámara frontal a un brazo de distancia.",
            text: "Front-facing phone selfie, candid expression, imperfect slightly off-center framing, natural arm-length perspective and spontaneous composition. Unretouched everyday appearance."
        ),
    ]

    public static let imageResolutions = ["1K", "2K", "4K"]
    public static let videoResolutions = ["480p", "720p", "1080p"]
    public static let videoAspects = ["9:16", "16:9", "1:1", "4:3", "3:4", "21:9", "adaptive"]
    public static let videoDurationRange = 4...30
    public static let videoDurationShortcuts = [5, 10, 15, 20, 25, 30]

    public static func cameraText(_ id: String) -> String { cameras.first { $0.id == id }?.text ?? "" }
    public static func filmText(_ id: String) -> String { films.first { $0.id == id }?.text ?? "" }
    public static func isValidCamera(_ id: String) -> Bool { cameras.contains { $0.id == id } }
    public static func isValidFilm(_ id: String) -> Bool { films.contains { $0.id == id } }

    public static func imageModel(_ id: String) -> ImageModel? { imageModels.first { $0.id == id } }
    public static func modelName(_ id: String) -> String {
        id == videoModelID ? videoModelName : imageModel(id)?.name ?? id
    }

    /// Longest prompt the user may type for an image model.
    public static func maxPromptLength(imageModel: String) -> Int { imageModel == "nano-banana-pro" ? 9000 : 18000 }
    /// Longest final prompt (with presets) accepted by KIE for an image model.
    public static func maxComposedLength(imageModel: String) -> Int { imageModel == "nano-banana-pro" ? 10000 : 20000 }
    public static let maxVideoPromptLength = 30000

    /// Aspect ratios KIE accepts for a model and resolution.
    public static func ratios(model: String, resolution: String) -> [String] {
        if model.hasPrefix("gpt-image-2-5") {
            return ["auto", "1:1", "3:2", "2:3", "4:3", "3:4", "16:9", "9:16", "21:9"]
                + (resolution == "1K" ? ["27:16", "16:27", "9:8", "8:9"] : [])
        }
        let all = model == "nano-banana-pro"
            ? ["1:1", "2:3", "3:2", "3:4", "4:3", "4:5", "5:4", "9:16", "16:9", "21:9"]
            : ["auto", "1:1", "3:2", "2:3", "4:3", "3:4", "5:4", "4:5", "16:9", "9:16", "2:1", "1:2", "3:1", "1:3", "21:9", "9:21"]
        let blocked: Set<String> = resolution == "2K"
            ? ["5:4", "4:5", "3:1", "1:3", "9:21"]
            : resolution == "4K" ? ["3:1", "1:3", "9:21"] : []
        return all
            .filter { r in !(model == "gpt-image-2" && ((resolution != "1K" && r == "auto") || (resolution == "4K" && r == "1:1"))) }
            .filter { r in model == "nano-banana-pro" || !blocked.contains(r) }
    }

    /// Final prompt sent to an image model: the user's prompt plus the selected
    /// camera/film presets. JSON visual profiles get the presets merged in.
    public static func composePrompt(_ prompt: String, camera: String, film: String, aspect: String? = nil) -> String {
        let text = prompt.trimmingCharacters(in: .whitespacesAndNewlines)
        let cameraText = cameraText(camera)
        let filmText = filmText(film)
        guard text.hasPrefix("{") else {
            return [text, cameraText, filmText].filter { !$0.isEmpty }.joined(separator: "\n\n")
        }
        guard let data = text.data(using: .utf8),
              let parsed = try? JSONSerialization.jsonObject(with: data),
              var profile = parsed as? [String: Any]
        else { return text }

        func object(_ value: Any?) -> [String: Any] { value as? [String: Any] ?? [:] }

        if let aspect, !aspect.isEmpty, aspect != "auto" {
            var composition = object(profile["composition"])
            composition["aspect_ratio"] = aspect
            profile["composition"] = composition
        }
        if cameraText.isEmpty && filmText.isEmpty { return serialize(profile) ?? text }

        var specs = object(profile["technical_specs"])
        if !cameraText.isEmpty {
            specs["camera_style"] = cameraText
            switch camera {
            case "Android Camera":
                specs["sharpness"] = "Slightly soft focus and subtle motion blur"
                specs["grain"] = "Visible digital noise"
                specs["texture"] = "Compressed detail, imperfect exposure"
            case "iPhone Camera":
                specs["sharpness"] = "Natural smartphone detail, realistic skin texture"
                specs["grain"] = "Subtle natural phone noise"
                specs["texture"] = "Unretouched candid detail"
            case "TV / News Camera":
                specs["sharpness"] = "Slightly compressed broadcast video detail"
                specs["texture"] = "Local-news documentary footage"
            default:
                break
            }
        }
        if !filmText.isEmpty {
            var composition = object(profile["composition"])
            composition["framing"] = filmText
            profile["composition"] = composition
            specs["perspective"] = film == "POV"
                ? "First-person eye-level handheld phone, 1x magnification"
                : "Front-facing phone selfie at arm’s length, imperfect off-center framing"
            if film == "POV" {
                specs["motion_blur"] = "Slight handheld motion blur"
                var subject = object(profile["subject_analysis"])
                subject["hands_and_gestures"] = [
                    "left_hand": "Outside the frame, holding the phone",
                    "right_hand": "The only visible hand; natural gesture consistent with the scene",
                    "finger_positions": "Natural anatomy, no extra fingers",
                    "interaction": "Exactly one visible hand; phone-holding hand stays out of frame",
                    "visible_hand_count": 1,
                ] as [String: Any]
                profile["subject_analysis"] = subject
            }
        }
        profile["technical_specs"] = specs
        var overrides: [String: Any] = [
            "priority": "These selected camera and framing constraints override conflicting descriptions elsewhere in this profile, including generation_parameters.prompts.",
        ]
        if !cameraText.isEmpty { overrides["camera"] = cameraText }
        if !filmText.isEmpty { overrides["film"] = filmText }
        profile["framecraft_overrides"] = overrides
        return serialize(profile) ?? text
    }

    static func serialize(_ object: [String: Any]) -> String? {
        guard let data = try? JSONSerialization.data(withJSONObject: object, options: [.withoutEscapingSlashes]) else { return nil }
        return String(data: data, encoding: .utf8)
    }
}

/// The dialogue-density rule from the Seedance kit: the model speaks about
/// 2.47 words per second and fills any spare time with silence.
public enum DialogueMath {
    public static let wordsPerSecond = 2.47

    public static func targetWords(seconds: Int) -> Int {
        Int((Double(seconds) * wordsPerSecond).rounded())
    }

    public static func seconds(forWords words: Int) -> Double {
        Double(words) / wordsPerSecond
    }
}
