import Foundation
import Security

/// Where API keys live. The app uses the macOS Keychain; demo/snapshot mode uses memory.
protocol KeyStore {
    func read(_ account: String) -> String?
    func write(_ value: String, account: String) throws
    func delete(_ account: String)
}

enum KeyAccount {
    static let kie = "kie-api-key"
    static let openAI = "openai-api-key"
}

struct KeychainStore: KeyStore {
    let service = "app.framecraft.mac"

    private func query(_ account: String) -> [String: Any] {
        [kSecClass as String: kSecClassGenericPassword,
         kSecAttrService as String: service,
         kSecAttrAccount as String: account]
    }

    func read(_ account: String) -> String? {
        var q = query(account)
        q[kSecReturnData as String] = true
        q[kSecMatchLimit as String] = kSecMatchLimitOne
        var item: CFTypeRef?
        guard SecItemCopyMatching(q as CFDictionary, &item) == errSecSuccess, let data = item as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    func write(_ value: String, account: String) throws {
        let data = Data(value.utf8)
        let status = SecItemUpdate(query(account) as CFDictionary, [kSecValueData as String: data] as CFDictionary)
        if status == errSecItemNotFound {
            var q = query(account)
            q[kSecValueData as String] = data
            q[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlock
            q[kSecAttrLabel as String] = "Framecraft · \(account)"
            let added = SecItemAdd(q as CFDictionary, nil)
            guard added == errSecSuccess else { throw KeyStoreError(status: added) }
        } else if status != errSecSuccess {
            throw KeyStoreError(status: status)
        }
    }

    func delete(_ account: String) {
        SecItemDelete(query(account) as CFDictionary)
    }
}

struct KeyStoreError: LocalizedError {
    let status: OSStatus
    var errorDescription: String? {
        let reason = SecCopyErrorMessageString(status, nil) as String? ?? "código \(status)"
        return "No se pudo guardar la clave en el Llavero: \(reason)."
    }
}

final class MemoryKeyStore: KeyStore {
    private var values: [String: String] = [:]
    func read(_ account: String) -> String? { values[account] }
    func write(_ value: String, account: String) throws { values[account] = value }
    func delete(_ account: String) { values[account] = nil }
}
