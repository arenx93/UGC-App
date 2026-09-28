import Foundation
import FramecraftCore
#if canImport(FoundationModels)
import FoundationModels
#endif

/// Apple Intelligence's on-device model (macOS 26+): free, private, works offline.
/// The framework is weak-linked (see Package.swift), so the app still opens on macOS 14 and 15.
enum OnDeviceModel {
    enum Availability: Equatable {
        case available
        case unsupportedOS
        case notEligible
        case notEnabled
        case notReady
        case other(String)

        var message: String {
            switch self {
            case .available: "Listo: Apple Intelligence responde en este Mac, sin internet ni costo."
            case .unsupportedOS: "Necesita macOS 26 o posterior."
            case .notEligible: "Este Mac no es compatible con Apple Intelligence (requiere Apple Silicon)."
            case .notEnabled: "Activá Apple Intelligence en Ajustes del Sistema → Apple Intelligence y Siri."
            case .notReady: "El modelo se está descargando. Probá de nuevo en unos minutos."
            case .other(let reason): "Apple Intelligence no está disponible ahora (\(reason))."
            }
        }
    }

    static var availability: Availability {
        #if canImport(FoundationModels)
        if #available(macOS 26.0, *) {
            switch SystemLanguageModel.default.availability {
            case .available:
                return .available
            case .unavailable(.deviceNotEligible):
                return .notEligible
            case .unavailable(.appleIntelligenceNotEnabled):
                return .notEnabled
            case .unavailable(.modelNotReady):
                return .notReady
            case .unavailable(let other):
                return .other(String(describing: other))
            @unknown default:
                return .other("estado desconocido")
            }
        }
        #endif
        return .unsupportedOS
    }

    /// Generates a prompt on the Mac. Throws a friendly error when the model is not available.
    static func generate(instructions: String, prompt: String) async throws -> String {
        let state = availability
        guard state == .available else { throw KieError(state.message, definite: true) }
        #if canImport(FoundationModels)
        if #available(macOS 26.0, *) {
            let session = LanguageModelSession(instructions: instructions)
            do {
                let response = try await session.respond(to: prompt)
                return response.content
            } catch {
                throw KieError("Apple Intelligence no pudo responder: \(error.localizedDescription). Probá con una idea más corta o con otro motor.", definite: true)
            }
        }
        #endif
        throw KieError(Availability.unsupportedOS.message, definite: true)
    }
}
