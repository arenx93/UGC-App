import FramecraftCore
import SwiftUI

struct SettingsView: View {
    var body: some View {
        TabView {
            AccountSettings()
                .tabItem { Label("Cuenta", systemImage: "key.fill") }
            GeneralSettings()
                .tabItem { Label("General", systemImage: "gearshape") }
            AboutView()
                .tabItem { Label("Acerca de", systemImage: "info.circle") }
        }
        .frame(width: 560, height: 470)
        .tint(Theme.pink)
    }
}

struct AccountSettings: View {
    @Environment(AppModel.self) private var model
    @State private var kieKey = ""
    @State private var openAIKey = ""
    @State private var saving = false
    @State private var error: String?

    var body: some View {
        Form {
            Section {
                if model.hasKieKey {
                    LabeledContent("Estado") {
                        Label("Conectado", systemImage: "checkmark.circle.fill").foregroundStyle(Theme.success)
                    }
                    if let credits = model.credits {
                        LabeledContent("Créditos disponibles", value: credits.formatted(.number.precision(.fractionLength(0...1))))
                    }
                }
                SecureField(model.hasKieKey ? "Reemplazar clave de KIE" : "Pegá tu clave de KIE", text: $kieKey)
                HStack {
                    Button(saving ? "Verificando…" : "Guardar y verificar") {
                        Task {
                            saving = true
                            defer { saving = false }
                            do {
                                try await model.saveKieKey(kieKey)
                                kieKey = ""
                                error = nil
                            } catch {
                                self.error = error.localizedDescription
                            }
                        }
                    }
                    .disabled(kieKey.trimmingCharacters(in: .whitespaces).isEmpty || saving)
                    Link("Conseguir mi clave", destination: KieClient.keyPageURL)
                    Spacer()
                    if model.hasKieKey {
                        Button("Desconectar", role: .destructive) { model.removeKieKey() }
                    }
                }
                if let error {
                    Text(error).font(.caption).foregroundStyle(.orange)
                }
            } header: {
                Text("KIE — imágenes, videos y prompts")
            } footer: {
                Text("Tu clave se guarda en el Llavero de macOS y solo se usa para hablar con kie.ai.")
            }

            Section {
                if model.hasOpenAIKey {
                    LabeledContent("Estado") {
                        Label("Guardada", systemImage: "checkmark.circle.fill").foregroundStyle(Theme.success)
                    }
                }
                SecureField("sk-…", text: $openAIKey)
                HStack {
                    Button("Guardar") {
                        do {
                            try model.saveOpenAIKey(openAIKey)
                            openAIKey = ""
                            error = nil
                        } catch {
                            self.error = error.localizedDescription
                        }
                    }
                    .disabled(openAIKey.trimmingCharacters(in: .whitespaces).isEmpty)
                    Spacer()
                    if model.hasOpenAIKey {
                        Button("Quitar", role: .destructive) { model.removeOpenAIKey() }
                    }
                }
            } header: {
                Text("OpenAI — opcional, solo para el asistente")
            }
        }
        .formStyle(.grouped)
    }
}

struct GeneralSettings: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var model = model
        Form {
            Section("Biblioteca") {
                LabeledContent("Tus creaciones") {
                    Text(model.store.mediaRoot.path(percentEncoded: false)).font(.caption).textSelection(.enabled)
                }
                Button("Abrir en Finder") { NSWorkspace.shared.open(model.store.mediaRoot) }
            }
            Section("Avisos") {
                Toggle("Avisarme cuando termine una generación", isOn: $model.notifyWhenDone)
                    .onChange(of: model.notifyWhenDone) { _, _ in model.persist() }
                Text("Llega una notificación si estás en otra app. El ícono del Dock muestra cuántas hay en curso.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Section("Ayuda") {
                Button("Ver la bienvenida de nuevo") { model.showOnboarding = true }
            }
        }
        .formStyle(.grouped)
    }
}

struct AboutView: View {
    var body: some View {
        VStack(spacing: 12) {
            AppMark(size: 72)
            Text("Framecraft").font(.title.weight(.bold))
            Text("Estudio de contenido UGC para macOS")
                .foregroundStyle(.secondary)
            Text("Versión \(Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "dev")")
                .font(.caption)
                .foregroundStyle(.tertiary)
            Text("Genera con KIE: GPT Image 2, Nano Banana Pro y Seedance 2.5. Ejemplos de prompts de YouMind OpenLab (CC BY 4.0).")
                .font(.caption)
                .multilineTextAlignment(.center)
                .foregroundStyle(.secondary)
                .padding(.horizontal, 40)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

/// The app mark: viewfinder + play on the brand gradient.
struct AppMark: View {
    var size: CGFloat = 48

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: size * 0.26, style: .continuous).fill(Theme.gradient)
            Image(systemName: "viewfinder")
                .font(.system(size: size * 0.62, weight: .semibold))
                .foregroundStyle(.white)
            Image(systemName: "play.fill")
                .font(.system(size: size * 0.24, weight: .bold))
                .foregroundStyle(.white)
        }
        .frame(width: size, height: size)
        .shadow(color: Theme.pink.opacity(0.35), radius: size * 0.2, y: size * 0.08)
        .accessibilityHidden(true)
    }
}
