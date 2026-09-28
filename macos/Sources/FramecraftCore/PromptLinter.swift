import Foundation

/// One item of the prompt checklist.
public struct LintItem: Identifiable, Hashable, Sendable {
    public enum State: String, Sendable { case ok, warning, problem, info }

    public let id: String
    public let state: State
    public let title: String
    public let detail: String
}

public struct LintReport: Sendable {
    public let items: [LintItem]
    public let dialogueWords: Int
    public let targetWords: Int
    public let promptWords: Int

    public var problems: Int { items.filter { $0.state == .problem }.count }
    public var warnings: Int { items.filter { $0.state == .warning }.count }
    public var passed: Int { items.filter { $0.state == .ok }.count }
}

/// Checks a video prompt against the Seedance kit checklist
/// (see Resources/Skills/ugc-celular/SKILL.md, "Checklist antes de entregar un prompt").
public enum PromptLinter {
    /// Words that push Seedance into "ad" mode (kit: "Prohibido siempre").
    public static let adWords = [
        "cinematic", "shallow depth of field", "bokeh", "gimbal", "steadicam", "dolly", "slider", "crane shot",
        "drone", "orbit", "rack focus", "teal and orange", "film grain", "lut", "halation", "anamorphic",
        "beauty filter", "color grade", "studio lighting", "rim light", "slow motion",
        "cinematográfico", "cinematográfica", "cámara lenta",
    ]

    static let negations = ["no ", "not ", "never ", "without ", "avoid", "sin ", "nunca ", "ni ", "evitar", "prohibid", "zero ", "none"]

    public struct Selection: Sendable {
        public var images: Int
        public var videos: Int
        public var audios: Int
        public init(images: Int = 0, videos: Int = 0, audios: Int = 0) {
            self.images = images
            self.videos = videos
            self.audios = audios
        }
    }

    /// - Parameter ugcStyle: apply the phone-UGC rules (timestamps, ad words, wide depth of field).
    public static func lintVideo(_ prompt: String, duration: Int, references: Selection, ugcStyle: Bool) -> LintReport {
        let text = prompt
        let lower = text.lowercased()
        var items: [LintItem] = []
        let dialogue = dialogueWords(in: text)
        let target = DialogueMath.targetWords(seconds: duration)

        // 1. Referencias @ImageN / @VideoN / @AudioN vs. the files actually selected.
        let usage: [(ReferenceKind, Int)] = [(.image, references.images), (.video, references.videos), (.audio, references.audios)]
        var referenceProblems: [String] = []
        var unusedKinds: [String] = []
        for (kind, selected) in usage {
            let highest = highestTag(kind.tagPrefix, in: text)
            if highest > selected {
                let noun = kind == .image ? "imágenes" : kind == .video ? "videos" : "audios"
                referenceProblems.append("El prompt usa \(kind.tagPrefix)\(highest) pero seleccionaste \(selected) \(noun).")
            } else if selected > 0 && highest == 0 {
                unusedKinds.append(kind.tagPrefix)
            }
        }
        if !referenceProblems.isEmpty {
            items.append(LintItem(id: "refs", state: .problem, title: "Referencias que no existen",
                                  detail: referenceProblems.joined(separator: " ") + " El número sigue el orden de la lista de referencias."))
        } else if !unusedKinds.isEmpty {
            items.append(LintItem(id: "refs", state: .info, title: "Referencias sin nombrar",
                                  detail: "Seleccionaste archivos que el prompt no menciona (\(unusedKinds.joined(separator: ", "))…). Nombralos con su etiqueta para decirle al modelo para qué sirve cada uno."))
        } else if references.images + references.videos + references.audios > 0 {
            items.append(LintItem(id: "refs", state: .ok, title: "Referencias en orden", detail: "Cada etiqueta @ apunta a un archivo seleccionado."))
        }

        // 2. Timeline por bloques de tiempo (only for Seedance/UGC: in Omni/Veo they force cuts).
        if ugcStyle {
            if hasTimestamps(text) {
                items.append(LintItem(id: "timeline", state: .ok, title: "Bloques de tiempo", detail: "La acción está segmentada con timestamps."))
            } else {
                items.append(LintItem(id: "timeline", state: .warning, title: "Faltan bloques de tiempo",
                                      detail: "Segmentá la acción (por ejemplo 0–6 s, 6–12 s…). Sin eso el modelo adivina el timeline y aparecen saltos y manos rotas."))
            }
        }

        // 3. Densidad de diálogo (~2,47 palabras por segundo).
        if dialogue == 0 {
            items.append(LintItem(id: "dialogue", state: .info, title: "Sin diálogo entre comillas",
                                  detail: "Si alguien habla, poné sus líneas \"entre comillas\": eso activa el lip-sync. Para \(duration) s son unas \(target) palabras."))
        } else {
            let ratio = Double(dialogue) / Double(max(target, 1))
            if ratio < 0.75 {
                let silence = max(0, Int((Double(duration) - DialogueMath.seconds(forWords: dialogue)).rounded()))
                items.append(LintItem(id: "dialogue", state: .warning, title: "Poco diálogo: \(dialogue) de ~\(target) palabras",
                                      detail: "El modelo rellena con silencio el tiempo que sobra (≈\(silence) s de aire). Sumá diálogo o acortá el clip."))
            } else if ratio > 1.25 {
                items.append(LintItem(id: "dialogue", state: .warning, title: "Mucho diálogo: \(dialogue) de ~\(target) palabras",
                                      detail: "No entra en \(duration) s a 2,47 palabras por segundo: se va a atropellar. Recortá o alargá el clip."))
            } else {
                items.append(LintItem(id: "dialogue", state: .ok, title: "Densidad de diálogo: \(dialogue) palabras",
                                      detail: "Bien para \(duration) s (ideal ≈ \(target))."))
            }
        }

        // 4. Dirección entre paréntesis: must be declared as never spoken.
        if matches(text, #"\([^)]{3,}\)\s*:\s*["“]"#) {
            let declared = ["never spoken", "not spoken", "never said", "no se pronuncia", "no se dice", "not read aloud"].contains { lower.contains($0) }
            items.append(declared
                ? LintItem(id: "parentheses", state: .ok, title: "Acotaciones protegidas", detail: "Está aclarado que el texto entre paréntesis no se pronuncia.")
                : LintItem(id: "parentheses", state: .warning, title: "Aclarar que el paréntesis no se dice",
                           detail: "Agregá: \"Text in parentheses before a spoken line is acting direction only. It is NEVER spoken aloud.\" Si no, el modelo puede leer la acotación en voz alta."))
        }

        // 5. Audios de referencia = solo timbre.
        if highestTag("@Audio", in: text) > 0 {
            let timbre = lower.contains("timbre")
            let noWords = ["do not reproduce", "don't reproduce", "no repitas", "no reproduzcas", "not reproduce any words"].contains { lower.contains($0) }
            items.append(timbre && noWords
                ? LintItem(id: "audio", state: .ok, title: "Audio de referencia = solo timbre", detail: "Bloqueadas las palabras y la emoción del sample.")
                : LintItem(id: "audio", state: .warning, title: "Bloqueá el audio de referencia",
                           detail: "Declaralo como solo timbre: \"match @Audio1 for timbre, age, accent and pitch only. Do not reproduce any words from @Audio1 and do not copy its emotion\"."))
        }

        if ugcStyle {
            // 6. Palabras que empujan a modo anuncio.
            let found = adWords.filter { containsUnnegated(lower, $0) }
            items.append(found.isEmpty
                ? LintItem(id: "adwords", state: .ok, title: "Sin lenguaje de anuncio", detail: "No hay términos cinematográficos que arruinen el look de celular.")
                : LintItem(id: "adwords", state: .warning, title: "Lenguaje de anuncio: " + found.prefix(4).joined(separator: ", "),
                           detail: "Estos términos empujan al modelo a verse como publicidad. Pedí causas físicas (\"el operador reencuadra tarde\") en vez de estilos."))

            // 7. Restricciones.
            let restrictions: [(String, [String])] = [
                ("sin música", ["no music", "without music", "sin música", "sin musica", "no background music"]),
                ("sin cortes", ["no cuts", "no cut", "sin cortes", "continuous take", "continuous shot", "one take", "jump cut", "un solo plano"]),
                ("sin texto", ["no text", "no on-screen text", "no subtitles", "no captions", "sin texto", "sin subtítulos", "no overlays", "no graphics"]),
            ]
            let missing = restrictions.filter { _, needles in !needles.contains { lower.contains($0) } }.map(\.0)
            items.append(missing.isEmpty
                ? LintItem(id: "restrictions", state: .ok, title: "Restricciones completas", detail: "Música, cortes y texto están controlados.")
                : LintItem(id: "restrictions", state: .warning, title: "Faltan restricciones: " + missing.joined(separator: ", "),
                           detail: "Cerrá el prompt con restricciones en lenguaje natural. No existe un campo de negative prompt."))

            // 8. Profundidad de campo amplia.
            let deep = ["deep depth of field", "wide depth of field", "large depth of field", "everything in focus", "profundidad de campo amplia", "todo en foco", "background stays sharp", "background fully readable", "fondo se lee"]
            items.append(deep.contains { lower.contains($0) }
                ? LintItem(id: "dof", state: .ok, title: "Profundidad de campo amplia", detail: "El fondo se lee entero, como en un celular.")
                : LintItem(id: "dof", state: .info, title: "Declarar profundidad de campo amplia",
                           detail: "Un celular tiene todo en foco. Decilo explícito (\"deep depth of field, the background is fully readable\")."))
        }

        let length = text.count
        if length > Presets.maxVideoPromptLength {
            items.append(LintItem(id: "length", state: .problem, title: "Prompt demasiado largo",
                                  detail: "Tiene \(length) caracteres; el máximo es \(Presets.maxVideoPromptLength)."))
        }
        let words = text.split { $0.isWhitespace || $0.isNewline }.count
        return LintReport(items: items, dialogueWords: dialogue, targetWords: target, promptWords: words)
    }

    /// Words spoken inside "quotes" / “quotes”.
    public static func dialogueWords(in text: String) -> Int {
        guard let regex = try? NSRegularExpression(pattern: #"["“]([^"”]{1,2000})["”]"#) else { return 0 }
        let range = NSRange(text.startIndex..., in: text)
        return regex.matches(in: text, range: range).reduce(0) { total, match in
            guard let r = Range(match.range(at: 1), in: text) else { return total }
            return total + text[r].split { $0.isWhitespace || $0.isNewline }.count
        }
    }

    /// Highest N used in tags like @Image3 (0 when absent).
    public static func highestTag(_ prefix: String, in text: String) -> Int {
        guard let regex = try? NSRegularExpression(pattern: NSRegularExpression.escapedPattern(for: prefix) + "(\\d+)", options: [.caseInsensitive]) else { return 0 }
        let range = NSRange(text.startIndex..., in: text)
        return regex.matches(in: text, range: range).compactMap { match -> Int? in
            guard let r = Range(match.range(at: 1), in: text) else { return nil }
            return Int(text[r])
        }.max() ?? 0
    }

    public static func hasTimestamps(_ text: String) -> Bool {
        let unit = #"(?:s|sec|secs|seconds|seg|segundos)\b"#
        return matches(text, #"\b\d{1,2}(?:[.,]\d)?\s*(?:"# + unit + #")?\s*[–—-]\s*\d{1,2}(?:[.,]\d)?\s*"# + unit)
            || matches(text, #"\b\d{1,2}:\d{2}\s*[–—-]\s*\d{1,2}:\d{2}\b"#)
    }

    static func matches(_ text: String, _ pattern: String) -> Bool {
        guard let regex = try? NSRegularExpression(pattern: pattern, options: [.caseInsensitive]) else { return false }
        return regex.firstMatch(in: text, range: NSRange(text.startIndex..., in: text)) != nil
    }

    /// True when `term` appears as a whole word/phrase not preceded by a negation
    /// in the same clause (e.g. "no gimbal" is fine, "a gimbal shot" is not).
    static func containsUnnegated(_ lower: String, _ term: String) -> Bool {
        let pattern = "(?<![\\p{L}])" + NSRegularExpression.escapedPattern(for: term) + "(?![\\p{L}])"
        guard let regex = try? NSRegularExpression(pattern: pattern) else { return false }
        let range = NSRange(lower.startIndex..., in: lower)
        for match in regex.matches(in: lower, range: range) {
            guard let r = Range(match.range, in: lower) else { continue }
            // Look back to the start of the clause (up to 80 characters).
            let windowStart = lower.index(r.lowerBound, offsetBy: -80, limitedBy: lower.startIndex) ?? lower.startIndex
            var window = String(lower[windowStart..<r.lowerBound])
            if let cut = window.lastIndex(where: { ".;\n".contains($0) }) {
                window = String(window[window.index(after: cut)...])
            }
            let padded = " " + window
            if !negations.contains(where: { padded.contains(" " + $0) }) {
                return true
            }
        }
        return false
    }
}
