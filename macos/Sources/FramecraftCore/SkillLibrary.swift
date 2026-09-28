import Foundation

/// A prompt example from the built-in GPT Image 2 library (CC BY 4.0, YouMind OpenLab).
public struct PromptExample: Codable, Hashable, Sendable {
    public let title: String
    public let prompt: String
    public let author: String
    public let source: String
}

private final class BundleToken {}

/// Files bundled with FramecraftCore (Resources/Skills).
public final class SkillResources: @unchecked Sendable {
    public let root: URL

    public init(root: URL) {
        self.root = root
    }

    /// Finds the resource bundle both inside Framecraft.app and in `swift run` / `swift test`.
    public static func locate() -> SkillResources? {
        let bundleName = "Framecraft_FramecraftCore.bundle"
        var candidates: [URL] = []
        if let resources = Bundle.main.resourceURL { candidates.append(resources.appendingPathComponent(bundleName)) }
        candidates.append(Bundle.main.bundleURL.appendingPathComponent(bundleName))
        if let executable = Bundle.main.executableURL?.deletingLastPathComponent() {
            candidates.append(executable.appendingPathComponent(bundleName))
        }
        candidates.append(Bundle(for: BundleToken.self).bundleURL.deletingLastPathComponent().appendingPathComponent(bundleName))

        let fileManager = FileManager.default
        for url in candidates where fileManager.fileExists(atPath: url.path) {
            if let bundle = Bundle(url: url), let skills = bundle.url(forResource: "Skills", withExtension: nil) {
                return SkillResources(root: skills)
            }
            for relative in ["Skills", "Contents/Resources/Skills"] {
                let direct = url.appendingPathComponent(relative)
                if fileManager.fileExists(atPath: direct.path) { return SkillResources(root: direct) }
            }
        }
        return nil
    }

    public func url(_ relative: String) -> URL { root.appendingPathComponent(relative) }

    public func text(_ relative: String) -> String {
        (try? String(contentsOf: url(relative), encoding: .utf8)) ?? ""
    }

    /// The JSON visual-profile instructions (stored as a JSON string).
    public lazy var jsonProfile: String = {
        guard let data = try? Data(contentsOf: self.url("json-image/json-profile.json")),
              let text = try? JSONDecoder().decode(String.self, from: data)
        else { return "" }
        return text
    }()

    public lazy var examples: [PromptExample] = {
        guard let data = try? Data(contentsOf: self.url("json-image/examples.json")),
              let list = try? JSONDecoder().decode([PromptExample].self, from: data)
        else { return [] }
        return list
    }()

    public var guidePDF: URL { url("guia/documento-base-ugc-seedance.pdf") }
}

/// Built-in and user skills for the prompt assistant.
public enum SkillLibrary {
    public static let jsonProfileID = "builtin-json-image"
    public static let ugcID = "builtin-ugc-celular"
    public static let arthasID = "builtin-arthas-cachito"
    public static let generalID = ""

    public static let exampleAttribution =
        "Examples adapted from YouMind OpenLab, Awesome GPT Image 2, CC BY 4.0. Original authors and sources accompany selected examples."

    /// Built-in skills. The Seedance kit skills bundle their SKILL.md plus the
    /// knowledge files that ChatGPT would get as "Knowledge".
    public static func builtins(_ resources: SkillResources?) -> [Skill] {
        let epoch = Date(timeIntervalSince1970: 0)
        var skills = [
            Skill(
                id: jsonProfileID,
                name: "Perfil JSON + biblioteca GPT Image 2",
                summary: "Arma un perfil visual JSON completo, inspirado en 123 prompts de ejemplo. Ideal para fotos de producto y estilos muy controlados.",
                content: "",
                media: .image, isBuiltin: true, created: epoch
            ),
        ]
        guard let resources else { return skills }
        let ugc = parse(markdown: resources.text("ugc-celular/SKILL.md"), fallbackName: "UGC de celular")
        skills.append(Skill(
            id: ugcID,
            name: "UGC de celular · Seedance 2.5",
            summary: "Videos que parecen grabados con un teléfono real: causas físicas en vez de adjetivos, bloques de tiempo, diálogo con densidad medida y restricciones anti-anuncio.",
            content: [
                ugc.body,
                "# Knowledge: parámetros, video largo y arco narrativo\n\n" + resources.text("ugc-celular/PARAMETROS-Y-ARCO.md"),
                "# Knowledge: lecciones del pack de Walter (lo que funcionó)\n\n" + resources.text("ugc-celular/LECCIONES-WALTER.md"),
                "# Regla de idioma\n\nEl prompt se escribe en el idioma en que se habla en el video (campo dialogueLanguage del brief). Mezclar idioma de instrucción con idioma de diálogo invita a que el modelo cruce los dos.",
            ].joined(separator: "\n\n---\n\n"),
            media: .video, isBuiltin: true, created: epoch
        ))
        let arthas = parse(markdown: resources.text("arthas-cachito/SKILL.md"), fallbackName: "Arthas y Cachito")
        skills.append(Skill(
            id: arthasID,
            name: "Arthas y Cachito · Omni / Veo 3.1",
            summary: "Prompts cinematográficos de 10 s de la saga, para Gemini Omni 1.1 Flash o Veo 3.1 (copiá el prompt a esa herramienta).",
            content: arthas.body + "\n\n---\n\n# Knowledge: elenco, lore y filtros\n\n" + resources.text("arthas-cachito/ELENCO-LORE-FILTROS.md"),
            media: .video, isBuiltin: true, created: epoch
        ))
        return skills
    }

    /// The full worked example (8 scenes) — optional extra context for the UGC skill.
    public static func walterExample(_ resources: SkillResources?) -> String {
        resources?.text("ugc-celular/EJEMPLO-WALTER.txt") ?? ""
    }

    /// Parses a SKILL.md-style file: optional YAML front matter with `name` and
    /// `description`, followed by the markdown body.
    public static func parse(markdown: String, fallbackName: String) -> (name: String, summary: String, body: String) {
        let text = markdown.replacingOccurrences(of: "\r\n", with: "\n")
        let lines = text.components(separatedBy: "\n")
        guard lines.first?.trimmingCharacters(in: .whitespaces) == "---",
              let end = lines.dropFirst().firstIndex(where: { $0.trimmingCharacters(in: .whitespaces) == "---" })
        else {
            return (fallbackName, "", text.trimmingCharacters(in: .whitespacesAndNewlines))
        }
        var fields: [String: String] = [:]
        var currentKey: String?
        for line in lines[1..<end] {
            if let colon = line.firstIndex(of: ":"), !line.hasPrefix(" "), !line.hasPrefix("\t") {
                let key = line[..<colon].trimmingCharacters(in: .whitespaces).lowercased()
                fields[key] = line[line.index(after: colon)...].trimmingCharacters(in: .whitespaces)
                currentKey = key
            } else if let key = currentKey {
                // Folded continuation line.
                fields[key, default: ""] += " " + line.trimmingCharacters(in: .whitespaces)
            }
        }
        func clean(_ value: String?) -> String {
            var value = (value ?? "").trimmingCharacters(in: .whitespaces)
            if value.count >= 2, let first = value.first, let last = value.last,
               (first == "\"" && last == "\"") || (first == "'" && last == "'") {
                value = String(value.dropFirst().dropLast())
            }
            return value.trimmingCharacters(in: .whitespaces)
        }
        let body = lines[(end + 1)...].joined(separator: "\n").trimmingCharacters(in: .whitespacesAndNewlines)
        let name = clean(fields["name"])
        return (name.isEmpty ? fallbackName : name, clean(fields["description"]), body)
    }

    /// Guesses whether an imported skill is meant for images or videos.
    public static func guessMedia(name: String, summary: String, body: String) -> SkillMedia {
        let text = (name + " " + summary + " " + body.prefix(3000)).lowercased()
        let video = ["video", "vídeo", "seedance", "clip", "veo", "omni", "escena", "timestamps"].filter { text.contains($0) }.count
        let image = ["imagen", "image", "foto", "photo", "gpt image", "nano banana"].filter { text.contains($0) }.count
        if video > image { return .video }
        if image > video { return .image }
        return .any
    }

    /// Port of `relevantExamples` in lib/builtin-skill.ts: the three examples whose
    /// title/prompt best match the words of the idea.
    public static func relevantExamples(idea: String, examples: [PromptExample]) -> [PromptExample] {
        let stop: Set<String> = ["the", "and", "with", "for", "image", "photo", "make", "create"]
        var words: [String] = []
        var seenWords = Set<String>()
        let lowered = idea.lowercased()
        if let regex = try? NSRegularExpression(pattern: "[\\p{L}\\p{N}]{3,}") {
            let range = NSRange(lowered.startIndex..., in: lowered)
            for match in regex.matches(in: lowered, range: range) {
                guard let r = Range(match.range, in: lowered) else { continue }
                let word = String(lowered[r])
                if !stop.contains(word), seenWords.insert(word).inserted { words.append(word) }
            }
        }
        var seenTitles = Set<String>()
        let unique = examples.filter { seenTitles.insert($0.title).inserted }
        let scored = unique.enumerated().compactMap { index, example -> (Int, Int, PromptExample)? in
            let title = example.title.lowercased()
            let prompt = example.prompt.lowercased()
            let score = words.reduce(0) { $0 + (title.contains($1) ? 5 : 0) + (prompt.contains($1) ? 1 : 0) }
            return score > 0 ? (score, index, example) : nil
        }
        return scored
            .sorted { $0.0 != $1.0 ? $0.0 > $1.0 : $0.1 < $1.1 }
            .prefix(3)
            .map { PromptExample(title: $0.2.title, prompt: String($0.2.prompt.prefix(6500)), author: $0.2.author, source: $0.2.source) }
    }
}
