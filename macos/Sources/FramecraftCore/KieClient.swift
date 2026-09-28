import Foundation

/// Error from KIE. `definite` means KIE clearly rejected the request, so no task
/// was created; otherwise the request may have reached KIE (possible charge).
public struct KieError: LocalizedError, Sendable {
    public let message: String
    public let definite: Bool

    public init(_ message: String, definite: Bool) {
        self.message = message
        self.definite = definite
    }

    public var errorDescription: String? { message }
}

public struct KieTaskRecord: Sendable {
    public let state: String
    public let progress: Int
    public let failMessage: String?
    public let resultURLs: [URL]
}

/// Minimal KIE API client (https://kie.ai). Port of `lib/server.ts`.
public final class KieClient: @unchecked Sendable {
    public static let apiBase = "https://api.kie.ai"
    public static let uploadURL = URL(string: "https://kieai.redpandaai.co/api/file-stream-upload")!
    public static let keyPageURL = URL(string: "https://kie.ai/api-key")!

    private let key: String
    private let session: URLSession

    public init(key: String, session: URLSession = .shared) {
        self.key = key.trimmingCharacters(in: .whitespacesAndNewlines)
        self.session = session
    }

    // MARK: Account

    /// Remaining credits. Also validates the key (KIE rejects invalid keys here).
    public func credits() async throws -> Double? {
        let json = try await call("/api/v1/chat/credit", body: nil)
        if let number = json["data"] as? NSNumber { return number.doubleValue }
        if let text = json["data"] as? String { return Double(text) }
        return nil
    }

    // MARK: Generation

    public func createTask(model: String, input: [String: Any]) async throws -> String {
        let json = try await call("/api/v1/jobs/createTask", body: ["model": model, "input": input])
        guard let taskId = (json["data"] as? [String: Any])?["taskId"] as? String, !taskId.isEmpty else {
            throw KieError("KIE no devolvió un ID de tarea.", definite: false)
        }
        return taskId
    }

    public func recordInfo(taskId: String) async throws -> KieTaskRecord {
        let encoded = taskId.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) ?? taskId
        let json = try await call("/api/v1/jobs/recordInfo?taskId=" + encoded, body: nil)
        let data = json["data"] as? [String: Any] ?? [:]
        let state = data["state"] as? String ?? ""
        let rawProgress = (data["progress"] as? NSNumber)?.doubleValue ?? Double(data["progress"] as? String ?? "")
        let progress = max(0, min(99, rawProgress.map { Int($0.rounded()) } ?? (state == "generating" ? 10 : 2)))
        var urls: [URL] = []
        var result: Any? = data["resultJson"]
        if let text = result as? String, let bytes = text.data(using: .utf8) {
            result = try? JSONSerialization.jsonObject(with: bytes)
        }
        if let list = (result as? [String: Any])?["resultUrls"] as? [Any] {
            urls = list.compactMap { ($0 as? String).flatMap(URL.init(string:)) }
        }
        let fail = (data["failMsg"] as? String).map { redact($0) }
        return KieTaskRecord(state: state, progress: progress, failMessage: fail, resultURLs: urls)
    }

    // MARK: Prompt assistant

    public func chatCompletion(model: String, body: [String: Any]) async throws -> [String: Any] {
        try await call("/\(model)/v1/chat/completions", body: body, timeout: 90)
    }

    public func codexResponses(body: [String: Any]) async throws -> [String: Any] {
        try await call("/codex/v1/responses", body: body, timeout: 120)
    }

    // MARK: Files

    /// Uploads a local reference file and returns KIE's temporary download URL.
    public func upload(file: URL, mime: String, displayName: String, fileName: String) async throws -> URL {
        let folder = mime.hasPrefix("image/") ? "images" : mime.hasPrefix("video/") ? "videos" : "audio"
        let boundary = "Framecraft-\(UUID().uuidString)"
        let body = FileManager.default.temporaryDirectory.appendingPathComponent("upload-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: body) }

        FileManager.default.createFile(atPath: body.path, contents: nil)
        let handle = try FileHandle(forWritingTo: body)
        func write(_ text: String) throws { try handle.write(contentsOf: Data(text.utf8)) }
        let safeName = displayName.replacingOccurrences(of: "\"", with: "'").replacingOccurrences(of: "\r", with: "").replacingOccurrences(of: "\n", with: "")
        try write("--\(boundary)\r\nContent-Disposition: form-data; name=\"file\"; filename=\"\(safeName)\"\r\nContent-Type: \(mime)\r\n\r\n")
        let input = try FileHandle(forReadingFrom: file)
        while let chunk = try input.read(upToCount: 1 << 20), !chunk.isEmpty {
            try handle.write(contentsOf: chunk)
        }
        try input.close()
        try write("\r\n--\(boundary)\r\nContent-Disposition: form-data; name=\"uploadPath\"\r\n\r\n\(folder)/framecraft")
        try write("\r\n--\(boundary)\r\nContent-Disposition: form-data; name=\"fileName\"\r\n\r\n\(fileName)")
        try write("\r\n--\(boundary)--\r\n")
        try handle.close()

        var request = URLRequest(url: Self.uploadURL)
        request.httpMethod = "POST"
        request.timeoutInterval = 180
        request.setValue("Bearer \(key)", forHTTPHeaderField: "Authorization")
        request.setValue("multipart/form-data; boundary=\(boundary)", forHTTPHeaderField: "Content-Type")
        let (data, response): (Data, URLResponse)
        do {
            (data, response) = try await session.upload(for: request, fromFile: body)
        } catch {
            throw KieError("No se pudo subir la referencia a KIE. Revisá tu conexión e intentá de nuevo.", definite: true)
        }
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        guard let json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else {
            throw KieError("KIE devolvió una respuesta inválida al subir la referencia.", definite: true)
        }
        guard (200..<300).contains(status), json["success"] as? Bool == true,
              let link = (json["data"] as? [String: Any])?["downloadUrl"] as? String,
              let url = URL(string: link)
        else {
            throw KieError("KIE no aceptó la referencia. Intentá de nuevo.", definite: true)
        }
        return url
    }

    // MARK: Transport

    func call(_ path: String, body: [String: Any]?, timeout: TimeInterval = 55) async throws -> [String: Any] {
        guard let url = URL(string: Self.apiBase + path) else { throw KieError("Ruta de KIE inválida.", definite: true) }
        var request = URLRequest(url: url)
        request.httpMethod = body == nil ? "GET" : "POST"
        request.timeoutInterval = timeout
        request.setValue("Bearer \(key)", forHTTPHeaderField: "Authorization")
        if let body {
            request.setValue("application/json", forHTTPHeaderField: "Content-Type")
            request.httpBody = try JSONSerialization.data(withJSONObject: body)
        }
        let (data, response): (Data, URLResponse)
        do {
            (data, response) = try await session.data(for: request)
        } catch let error as URLError where error.code == .timedOut {
            throw KieError("KIE tardó demasiado en responder.", definite: false)
        } catch {
            throw KieError("No se pudo conectar con KIE. Revisá tu conexión.", definite: false)
        }
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        guard let json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else {
            throw KieError("KIE devolvió una respuesta inválida (\(status)).", definite: (400..<500).contains(status))
        }
        var codeOK = true
        if let code = json["code"] as? NSNumber { codeOK = code.intValue == 200 }
        if let code = json["code"] as? String { codeOK = code == "200" }
        if !(200..<300).contains(status) || !codeOK {
            let message = (json["msg"] as? String)
                ?? (json["message"] as? String)
                ?? ((json["error"] as? [String: Any])?["message"] as? String)
                ?? "KIE no pudo completar la solicitud."
            throw KieError(Self.friendly(redact(message), status: status), definite: true)
        }
        return json
    }

    func redact(_ text: String) -> String {
        key.isEmpty ? text : text.replacingOccurrences(of: key, with: "[oculto]")
    }

    static func friendly(_ message: String, status: Int) -> String {
        let lower = message.lowercased()
        if status == 401 || lower.contains("unauthorized") || lower.contains("invalid api key") {
            return "KIE rechazó tu clave de API. Revisala en Ajustes."
        }
        if status == 402 || lower.contains("insufficient") || (lower.contains("credit") && lower.contains("not enough")) {
            return "No te alcanzan los créditos de KIE. Recargá en kie.ai y volvé a intentar."
        }
        return message
    }
}

/// OpenAI Responses API (optional prompt provider).
public final class OpenAIClient: @unchecked Sendable {
    public static let model = "gpt-4.1-mini"
    private let key: String
    private let session: URLSession

    public init(key: String, session: URLSession = .shared) {
        self.key = key.trimmingCharacters(in: .whitespacesAndNewlines)
        self.session = session
    }

    public func responses(body: [String: Any]) async throws -> [String: Any] {
        var request = URLRequest(url: URL(string: "https://api.openai.com/v1/responses")!)
        request.httpMethod = "POST"
        request.timeoutInterval = 90
        request.setValue("Bearer \(key)", forHTTPHeaderField: "Authorization")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONSerialization.data(withJSONObject: body)
        let (data, response): (Data, URLResponse)
        do {
            (data, response) = try await session.data(for: request)
        } catch {
            throw KieError("No se pudo conectar con OpenAI a tiempo. Tu idea sigue ahí: probá de nuevo.", definite: true)
        }
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        guard (200..<300).contains(status) else {
            switch status {
            case 401: throw KieError("OpenAI rechazó tu clave. Cambiala en Ajustes.", definite: true)
            case 429: throw KieError("Llegaste al límite o a la cuota de OpenAI. Revisá tu facturación o probá más tarde.", definite: true)
            case 403, 404: throw KieError("Tu proyecto de OpenAI no tiene acceso al modelo \(Self.model).", definite: true)
            default: throw KieError("OpenAI no pudo completar el prompt (\(status)). Probá de nuevo.", definite: true)
            }
        }
        guard let json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else {
            throw KieError("OpenAI devolvió una respuesta inválida.", definite: true)
        }
        return json
    }
}
