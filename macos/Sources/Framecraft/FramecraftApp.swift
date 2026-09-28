import AppKit
import FramecraftCore
import SwiftUI

@main
@MainActor
enum Launcher {
    static func main() {
        let arguments = CommandLine.arguments
        if let index = arguments.firstIndex(of: "--snapshot"), arguments.indices.contains(index + 1) {
            // Renders screenshots of the main screens with demo data (used by CI).
            SnapshotRunner.run(outputDirectory: URL(fileURLWithPath: arguments[index + 1]))
            return
        }
        FramecraftApp.main()
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    var onTerminate: (() -> Void)?

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Needed when launched as a bare executable (swift run / Xcode).
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationWillTerminate(_ notification: Notification) {
        onTerminate?()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
}

struct FramecraftApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    @State private var model = AppModel(store: .standard(), keys: KeychainStore())

    var body: some Scene {
        Window("Framecraft", id: "main") {
            RootView()
                .environment(model)
                .frame(minWidth: 1020, minHeight: 680)
                .onAppear { delegate.onTerminate = { [model] in model.persist() } }
        }
        .defaultSize(width: 1360, height: 860)
        .commands { FramecraftCommands(model: model) }

        Settings {
            SettingsView()
                .environment(model)
        }
    }
}

struct FramecraftCommands: Commands {
    let model: AppModel

    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("Nueva imagen") {
                model.mode = .image
                model.section = .create
            }
            .keyboardShortcut("n")
            Button("Nuevo video") {
                model.mode = .video
                model.section = .create
            }
            .keyboardShortcut("n", modifiers: [.command, .shift])
            Divider()
            Button("Generar") { Task { await model.generate() } }
                .keyboardShortcut(.return, modifiers: .command)
                .disabled(model.generateBlocker != nil)
            Button("Crear prompt con el asistente") { Task { await model.buildPrompt() } }
                .keyboardShortcut(.return, modifiers: [.command, .shift])
                .disabled(model.isBuildingPrompt)
        }
        CommandGroup(after: .sidebar) {
            Divider()
            ForEach(Array(SidebarItem.allCases.enumerated()), id: \.element) { index, item in
                Button(item.title) { model.section = item }
                    .keyboardShortcut(KeyEquivalent(Character(String(index + 1))), modifiers: .command)
            }
            Divider()
            Button(model.showAssistant ? "Ocultar asistente" : "Mostrar asistente") {
                model.section = .create
                model.showAssistant.toggle()
            }
            .keyboardShortcut("i", modifiers: [.command, .option])
        }
        CommandGroup(replacing: .help) {
            Button("Guía para prompts UGC") { model.section = .guide }
            Button("Conseguir mi clave de KIE") { NSWorkspace.shared.open(KieClient.keyPageURL) }
            Button("Abrir la carpeta de mis creaciones") { NSWorkspace.shared.open(model.store.mediaRoot) }
            Button("Ver bienvenida de nuevo") { model.showOnboarding = true }
        }
    }
}
