import FramecraftCore
import SwiftUI

/// First launch: what the app does → connect KIE → three tips.
struct OnboardingView: View {
    @Environment(AppModel.self) private var model
    @State private var step = 0
    @State private var key = ""
    @State private var checking = false
    @State private var error: String?

    var body: some View {
        VStack(spacing: 0) {
            ZStack {
                switch step {
                case 0: welcome
                case 1: connect
                default: tips
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .padding(32)
            .transition(.asymmetric(insertion: .move(edge: .trailing).combined(with: .opacity), removal: .opacity))
            .id(step)

            Divider()
            HStack {
                HStack(spacing: 6) {
                    ForEach(0..<3) { index in
                        Capsule()
                            .fill(index == step ? AnyShapeStyle(Theme.gradient) : AnyShapeStyle(Color.primary.opacity(0.15)))
                            .frame(width: index == step ? 22 : 8, height: 8)
                    }
                }
                .accessibilityElement()
                .accessibilityLabel("Paso \(step + 1) de 3")
                Spacer()
                if step == 1 && !model.hasKieKey {
                    Button("Lo hago después") { next() }
                }
                if step > 0 {
                    Button("Atrás") { withAnimation(.snappy) { step -= 1 } }
                }
                Button(step == 2 ? "Empezar a crear" : "Continuar") {
                    if step == 2 { model.finishOnboarding() } else { next() }
                }
                .buttonStyle(GradientButtonStyle(height: 34))
                .keyboardShortcut(.defaultAction)
                .disabled(checking)
            }
            .padding(16)
        }
        .frame(width: 640, height: 520)
        .interactiveDismissDisabled(false)
    }

    private func next() {
        withAnimation(.snappy) { step = min(step + 1, 2) }
    }

    private var welcome: some View {
        VStack(spacing: 18) {
            AppMark(size: 84)
            Text("Bienvenido a Framecraft")
                .font(.system(size: 30, weight: .bold, design: .rounded))
                .brandGradientText()
            Text("Tu estudio para crear fotos y videos UGC con IA: que parezcan grabados con un celular de verdad, no un anuncio.")
                .font(.title3)
                .multilineTextAlignment(.center)
                .foregroundStyle(.secondary)
            VStack(alignment: .leading, spacing: 12) {
                feature("photo.fill", "Imágenes", "GPT Image 2 y Nano Banana Pro, con tus fotos de referencia.")
                feature("video.fill", "Videos", "Seedance 2.5 con voz, lip-sync y sonido real.")
                feature("wand.and.stars", "Asistente con método", "Convierte tu idea en un prompt profesional y lo revisa por vos.")
            }
            .padding(.top, 6)
        }
    }

    private func feature(_ symbol: String, _ title: String, _ detail: String) -> some View {
        HStack(spacing: 14) {
            Image(systemName: symbol)
                .font(.title3)
                .foregroundStyle(.white)
                .frame(width: 38, height: 38)
                .background(Theme.gradient, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.headline)
                Text(detail).font(.callout).foregroundStyle(.secondary)
            }
        }
        .accessibilityElement(children: .combine)
    }

    private var connect: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Conectá tu cuenta de KIE").font(.title.weight(.bold))
            Text("KIE es el servicio que genera las imágenes y los videos. Pagás solo lo que usás, con tus propios créditos.")
                .foregroundStyle(.secondary)
            VStack(alignment: .leading, spacing: 10) {
                Label("Entrá a kie.ai y creá tu cuenta (o iniciá sesión).", systemImage: "1.circle.fill")
                Label("Copiá tu clave de API.", systemImage: "2.circle.fill")
                Label("Pegala acá abajo.", systemImage: "3.circle.fill")
            }
            .font(.callout)
            Link(destination: KieClient.keyPageURL) {
                Label("Abrir kie.ai para conseguir mi clave", systemImage: "arrow.up.right.square")
            }
            if model.hasKieKey {
                Label("¡Listo! KIE está conectado.", systemImage: "checkmark.seal.fill")
                    .font(.title3.weight(.semibold))
                    .foregroundStyle(Theme.success)
            } else {
                HStack {
                    SecureField("Pegá tu clave de KIE", text: $key)
                        .textFieldStyle(.roundedBorder)
                        .onSubmit(verify)
                    Button(checking ? "Verificando…" : "Conectar", action: verify)
                        .disabled(key.trimmingCharacters(in: .whitespaces).isEmpty || checking)
                }
                if let error {
                    Label(error, systemImage: "exclamationmark.triangle.fill").font(.callout).foregroundStyle(.orange)
                }
            }
            Text("Se guarda en el Llavero de tu Mac. Nunca sale de tu equipo salvo para hablar con KIE.")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private func verify() {
        guard !checking else { return }
        checking = true
        Task {
            defer { checking = false }
            do {
                try await model.saveKieKey(key)
                key = ""
                error = nil
            } catch {
                self.error = error.localizedDescription
            }
        }
    }

    private var tips: some View {
        VStack(alignment: .leading, spacing: 18) {
            Text("Tres trucos para arrancar").font(.title.weight(.bold))
            tip("wand.and.stars", "Empezá por el asistente", "Contale tu idea como se la contarías a un amigo. Con la skill “UGC de celular” escribe el prompt con el método del kit.")
            tip("checkmark.seal", "Mirá la revisión del prompt", "En video, la app chequea bloques de tiempo, cantidad de diálogo, referencias y lenguaje de anuncio mientras escribís.")
            tip("forward.frame", "Encadená clips", "En la Biblioteca, “Continuar desde el último fotograma” prepara el próximo clip para que arranque exactamente donde terminó el anterior.")
            Text("Atajos: ⌘↩ generar · ⇧⌘↩ crear prompt · ⌥⌘I asistente · ⌘O agregar referencias")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private func tip(_ symbol: String, _ title: String, _ detail: String) -> some View {
        HStack(alignment: .top, spacing: 14) {
            Image(systemName: symbol)
                .font(.title2)
                .foregroundStyle(Theme.gradient)
                .frame(width: 34)
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.headline)
                Text(detail).font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
        }
        .accessibilityElement(children: .combine)
    }
}
