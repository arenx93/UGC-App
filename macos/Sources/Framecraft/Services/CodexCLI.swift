import Foundation
import FramecraftCore

/// Uses the OpenAI Codex CLI bundled with the app (or installed on the Mac), signed in
/// with the user's own ChatGPT account (`codex login`).
enum CodexCLI {
    static let installCommand = "npm install -g @openai/codex"
    static let brewCommand = "brew install --cask codex"

    enum Status: Equatable {
        case unknown, checking, notInstalled, loggedOut, loggingIn
        case loggedIn(String)
    }

    struct Output {
        let status: Int32
        let stdout: String
        let stderr: String
    }

    /// Codex CLI bundled inside Framecraft.app (Contents/MacOS/codex).
    static var bundledBinary: URL? {
        guard let folder = Bundle.main.executableURL?.deletingLastPathComponent() else { return nil }
        let binary = folder.appendingPathComponent("codex")
        return FileManager.default.isExecutableFile(atPath: binary.path) ? binary : nil
    }

    /// Runs `codex <arguments>` and waits (up to `timeout` seconds).
    static func run(_ arguments: [String], timeout: TimeInterval) async throws -> Output {
        let process = Process()
        if let bundled = bundledBinary {
            process.executableURL = bundled
            process.arguments = arguments
        } else {
            // Development builds: use a Codex installed on the Mac, via a login shell for the user's PATH.
            // $0 = "codex", "$@" = arguments: no shell quoting issues.
            process.executableURL = URL(fileURLWithPath: "/bin/zsh")
            process.arguments = ["-lc", "exec codex \"$@\"", "codex"] + arguments
        }
        process.currentDirectoryURL = FileManager.default.temporaryDirectory
        var environment = ProcessInfo.processInfo.environment
        environment["NO_COLOR"] = "1"
        process.environment = environment
        let out = Pipe(), err = Pipe()
        process.standardOutput = out
        process.standardError = err
        process.standardInput = FileHandle.nullDevice

        let collector = OutputCollector()
        out.fileHandleForReading.readabilityHandler = { collector.append(out: $0.availableData) }
        err.fileHandleForReading.readabilityHandler = { collector.append(err: $0.availableData) }

        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            process.terminationHandler = { _ in continuation.resume() }
            do {
                try process.run()
            } catch {
                process.terminationHandler = nil
                continuation.resume(throwing: error)
            }
            DispatchQueue.global().asyncAfter(deadline: .now() + timeout) {
                if process.isRunning { process.terminate() }
            }
        }
        out.fileHandleForReading.readabilityHandler = nil
        err.fileHandleForReading.readabilityHandler = nil
        collector.append(out: out.fileHandleForReading.readDataToEndOfFile())
        collector.append(err: err.fileHandleForReading.readDataToEndOfFile())
        return Output(status: process.terminationStatus, stdout: collector.stdout, stderr: collector.stderr)
    }

    static func status() async -> Status {
        guard let version = try? await run(["--version"], timeout: 20), version.status == 0 else { return .notInstalled }
        guard let login = try? await run(["login", "status"], timeout: 20) else { return .loggedOut }
        let text = (login.stdout + "\n" + login.stderr).trimmingCharacters(in: .whitespacesAndNewlines)
        if login.status == 0, !text.lowercased().contains("not logged in") {
            return .loggedIn(text.components(separatedBy: "\n").first ?? "Sesión iniciada")
        }
        return .loggedOut
    }

    /// Opens the browser to sign in with ChatGPT; returns when the login finishes.
    static func login() async throws {
        let result = try await run(["login"], timeout: 600)
        guard result.status == 0 else {
            throw KieError("No se completó el inicio de sesión de Codex. " + lastLines(result.stderr), definite: true)
        }
    }

    static func logout() async {
        _ = try? await run(["logout"], timeout: 30)
    }

    /// Runs one non-interactive request and returns the final answer.
    static func generate(prompt: String, images: [URL], model: String?) async throws -> String {
        let folder = FileManager.default.temporaryDirectory.appendingPathComponent("framecraft-codex-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        let answer = folder.appendingPathComponent("answer.txt")
        var arguments = ["exec", "--skip-git-repo-check", "--sandbox", "read-only", "--cd", folder.path, "--output-last-message", answer.path]
        if let model, !model.isEmpty { arguments += ["--model", model] }
        arguments.append(prompt)
        for image in images { arguments += ["--image", image.path] }

        let result = try await run(arguments, timeout: 600)
        let text = ((try? String(contentsOf: answer, encoding: .utf8)) ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        if !text.isEmpty { return text }
        let log = (result.stderr + "\n" + result.stdout).lowercased()
        if log.contains("login") || log.contains("unauthorized") || log.contains("401") {
            throw KieError("Codex no tiene una sesión activa. Iniciá sesión con tu cuenta de ChatGPT.", definite: true)
        }
        if log.contains("usage limit") || log.contains("rate limit") {
            throw KieError("Llegaste al límite de uso de tu plan de ChatGPT en Codex. Probá más tarde o usá KIE.", definite: true)
        }
        throw KieError("Codex no devolvió un prompt. " + lastLines(result.stderr), definite: true)
    }

    static func lastLines(_ text: String) -> String {
        text.split(separator: "\n").suffix(3).joined(separator: " ").trimmingCharacters(in: .whitespaces)
    }
}

private final class OutputCollector: @unchecked Sendable {
    private let lock = NSLock()
    private var out = Data()
    private var err = Data()

    func append(out data: Data) {
        lock.lock(); out.append(data); lock.unlock()
    }

    func append(err data: Data) {
        lock.lock(); err.append(data); lock.unlock()
    }

    var stdout: String { lock.lock(); defer { lock.unlock() }; return String(decoding: out, as: UTF8.self) }
    var stderr: String { lock.lock(); defer { lock.unlock() }; return String(decoding: err, as: UTF8.self) }
}
