import FramecraftCore
import SwiftUI

struct AssistantView: View {
    @Environment(AppModel.self) private var model

    static let languages = ["Español", "Español rioplatense", "Inglés", "Portugués", "Sin diálogo"]
    @State private var showEngine = true

    var body: some View {
        @Bindable var model = model
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                header
                skillPicker
                ideaField
                if model.mode == .video { videoOptions }
                providerOptions
                buildButton
                if let error = model.assistantError {
                    Label(error, systemImage: "exclamationmark.triangle.fill")
                        .font(.callout)
                        .foregroundStyle(.orange)
                        .fixedSize(horizontal: false, vertical: true)
                }
                if let live = model.streamingText, model.isBuildingPrompt {
                    VStack(alignment: .leading, spacing: 6) {
                        Label(live.isEmpty ? "Pensando…" : "Escribiendo…", systemImage: "ellipsis.bubble")
                            .font(.caption.weight(.semibold))
                            .symbolEffect(.pulse)
                        if !live.isEmpty {
                            Text(live.suffix(1500))
                                .font(.system(.caption, design: .monospaced))
                                .foregroundStyle(.secondary)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(8)
                                .background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 8))
                        }
                    }
                } else if !model.draft.isEmpty { result }
            }
            .padding(18)
        }
        .background(.background)
    }

    private var header: some View {
        HStack(alignment: .top) {
            VStack(alignment: .leading, spacing: 4) {
                HStack(spacing: 8) {
                    Text("Asistente de prompts").font(.title3.weight(.bold))
                    Text("IA")
                        .font(.caption2.weight(.heavy))
                        .foregroundStyle(.white)
                        .padding(.horizontal, 6).padding(.vertical, 2)
                        .background(Theme.gradient, in: Capsule())
                }
                Text("Contale tu idea con tus palabras y la convierte en un prompt listo, siguiendo la skill que elijas.")
                    .font(.callout)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }

    // MARK: Skill

    private var skillPicker: some View {
        VStack(alignment: .leading, spacing: 8) {
            FieldTitle("Skill (método)", help: "Una skill es un método escrito que el asistente aplica siempre igual.")
            ChoiceCard(
                title: "General",
                subtitle: model.mode == .image ? "Prompt claro en lenguaje natural." : "Prompt de video claro, sin método específico.",
                symbol: "text.bubble",
                selected: model.skillID == SkillLibrary.generalID
            ) { model.skillID = SkillLibrary.generalID }
            ForEach(model.skillsForMode) { skill in
                ChoiceCard(
                    title: skill.name,
                    subtitle: skill.summary.isEmpty ? nil : skill.summary,
                    symbol: symbol(for: skill),
                    selected: model.skillID == skill.id,
                    badge: skill.id == SkillLibrary.ugcID ? "Recomendada" : (skill.isBuiltin ? nil : "Tuya")
                ) { model.skillID = skill.id }
            }
            if model.isUGCSkill {
                @Bindable var model = model
                Toggle(isOn: $model.includeWalterExample) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Usar el pack de Walter como ejemplo").font(.callout)
                        Text("8 escenas reales que funcionaron. Más precisión en tono y estructura, pero consume más tokens.")
                            .font(.caption).foregroundStyle(.secondary)
                    }
                }
                .toggleStyle(.switch)
                .controlSize(.small)
            }
            if model.skillID == SkillLibrary.arthasID {
                Label("Pensada para Gemini Omni / Veo 3.1: copiá el prompt a esa herramienta.", systemImage: "info.circle")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            Button {
                model.section = .skills
            } label: {
                Label("Ver, importar o crear skills", systemImage: "square.and.arrow.down")
                    .font(.caption)
            }
            .buttonStyle(.link)
        }
    }

    private func symbol(for skill: Skill) -> String {
        switch skill.id {
        case SkillLibrary.jsonProfileID: "curlybraces.square"
        case SkillLibrary.ugcID: "iphone.gen3.radiowaves.left.and.right"
        case SkillLibrary.arthasID: "wand.and.rays"
        default: "doc.text"
        }
    }

    // MARK: Idea

    private var ideaField: some View {
        @Bindable var model = model
        return VStack(alignment: .leading, spacing: 8) {
            FieldTitle("¿Qué tenés en mente?")
            PromptEditor(
                text: $model.idea,
                placeholder: model.mode == .image
                    ? "Una foto espontánea de alguien tomando café una mañana de lluvia…"
                    : "Una chica muestra su sérum nuevo en el baño y cuenta, medio dormida, por qué le cambió la piel…",
                minHeight: 110,
                accessibilityName: "Tu idea"
            )
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 6) {
                    ForEach(examples, id: \.self) { example in
                        Button(example) { model.idea = example }
                            .buttonStyle(.bordered)
                            .controlSize(.small)
                            .help("Usar esta idea de ejemplo")
                    }
                }
            }
            if model.mode == .video, !model.referenceTags.isEmpty {
                VStack(alignment: .leading, spacing: 3) {
                    Text("Referencias que va a usar:").font(.caption.weight(.semibold))
                    ForEach(model.referenceTags, id: \.self) { Text($0).font(.caption.monospaced()).foregroundStyle(.secondary) }
                }
                .padding(8)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 8))
            } else if model.mode == .image, !model.selectedImages.isEmpty {
                Label("Va a analizar tus \(min(model.selectedImages.count, 4)) imagen(es) de referencia.", systemImage: "eye")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var examples: [String] {
        model.mode == .image
            ? ["Selfie tomando café en un día de lluvia", "Sérum sobre mármol con luz de ventana", "POV abriendo un paquete en el sillón", "Unboxing de zapatillas en la cama"]
            : ["Chica recomienda su sérum frente al espejo", "Mozo de 82 años cuenta su historia en un diner", "Pareja prueba una hamburguesa nueva en el auto", "Review honesta de auriculares caminando por la calle"]
    }

    // MARK: Options

    private var videoOptions: some View {
        @Bindable var model = model
        return VStack(alignment: .leading, spacing: 8) {
            FieldTitle("Idioma del diálogo", help: "El prompt se escribe en el idioma en que se habla en el video (regla del kit).")
            Picker("Idioma del diálogo", selection: $model.dialogueLanguage) {
                ForEach(Self.languages, id: \.self) { Text($0).tag($0) }
            }
            .labelsHidden()
            Label("\(model.videoDuration) s de clip → ~\(DialogueMath.targetWords(seconds: model.videoDuration)) palabras de diálogo", systemImage: "timer")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private var providerOptions: some View {
        @Bindable var model = model
        return DisclosureGroup(isExpanded: $showEngine) {
            VStack(alignment: .leading, spacing: 10) {
                Picker("Motor", selection: $model.provider) {
                    ForEach(PromptProvider.allCases) { Text($0.title).tag($0) }
                }
                .pickerStyle(.segmented)
                .labelsHidden()
                switch model.provider {
                case .kie:
                    Picker("Modelo", selection: $model.promptModel) {
                        ForEach(PromptRequests.promptModels) { Text("\($0.name) · \($0.note)").tag($0.id) }
                    }
                    Text("Usa tus créditos de KIE. La respuesta aparece en vivo mientras se escribe.")
                        .font(.caption).foregroundStyle(.secondary)
                    HStack {
                        Button(model.isTestingConnection ? "Probando…" : "Probar conexión") {
                            Task { await model.testPromptModel() }
                        }
                        .controlSize(.small)
                        .disabled(model.isTestingConnection)
                        if let result = model.connectionTest {
                            Text(result).font(.caption).foregroundStyle(result.hasPrefix("✓") ? Theme.success : .orange)
                                .lineLimit(3)
                        }
                    }
                case .codex:
                    CodexAccountRow()
                case .openai:
                    if !model.hasOpenAIKey {
                        SettingsLink { Text("Agregar clave de OpenAI en Ajustes") }
                    }
                    Text("GPT-4.1 mini con tu clave de API de OpenAI.").font(.caption).foregroundStyle(.secondary)
                }
            }
            .padding(.top, 6)
        } label: {
            Text("Motor: \(engineName)").font(.callout.weight(.semibold))
        }
        .task(id: model.provider) {
            if model.provider == .codex, model.codexStatus == .unknown { await model.refreshCodexStatus() }
        }
    }

    private var engineName: String {
        switch model.provider {
        case .kie: PromptRequests.promptModels.first { $0.id == model.promptModel }?.name ?? "KIE"
        case .codex: "ChatGPT (Codex)"
        case .openai: "OpenAI"
        }
    }

    private var buildButton: some View {
        Button {
            Task { await model.buildPrompt() }
        } label: {
            HStack {
                if model.isBuildingPrompt {
                    ProgressView().controlSize(.small).tint(.white)
                    Text("Escribiendo tu prompt…")
                } else {
                    Image(systemName: "wand.and.stars")
                    Text(model.draft.isEmpty ? "Crear prompt" : "Crear otro")
                }
            }
            .frame(maxWidth: .infinity)
        }
        .buttonStyle(GradientButtonStyle(height: 42))
        .disabled(model.isBuildingPrompt || model.idea.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
        .help("Crear prompt (⇧⌘↩)")
    }

    // MARK: Result

    private var result: some View {
        @Bindable var model = model
        return VStack(alignment: .leading, spacing: 10) {
            Divider()
            HStack {
                Text("Tu prompt").font(.headline)
                Spacer()
                Text("\(model.draft.count.formatted()) caracteres").font(.caption).foregroundStyle(.secondary)
            }
            if let text = model.draftProfilePrompt {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Prompt principal", systemImage: "text.quote").font(.caption.weight(.semibold))
                    Text(text).font(.callout).textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
                    HStack {
                        Button {
                            model.useDraftText()
                        } label: {
                            Label("Usar texto simple", systemImage: "arrow.left.circle.fill").frame(maxWidth: .infinity)
                        }
                        .buttonStyle(GradientButtonStyle(height: 34))
                        .help("Usa solo este prompt en texto")
                    }
                }
                .padding(10)
                .background(Theme.softGradient, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
                DisclosureGroup("Perfil JSON completo (avanzado)") {
                    PromptEditor(text: $model.draft, placeholder: "", minHeight: 220, monospaced: true, accessibilityName: "Perfil JSON generado")
                }
                .font(.caption)
            } else {
                PromptEditor(text: $model.draft, placeholder: "", minHeight: 220, monospaced: model.draft.hasPrefix("{"), accessibilityName: "Prompt generado")
            }
            if let notes = model.notes {
                VStack(alignment: .leading, spacing: 4) {
                    Label("Notas del asistente", systemImage: "lightbulb").font(.caption.weight(.semibold))
                    Text(notes).font(.caption).fixedSize(horizontal: false, vertical: true)
                }
                .padding(10)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(Theme.softGradient, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
            }
            if !model.sources.isEmpty {
                DisclosureGroup("Ejemplos que lo inspiraron") {
                    VStack(alignment: .leading, spacing: 4) {
                        ForEach(model.sources, id: \.title) { example in
                            Text("• \(example.title) — \(cleanAuthor(example.author))").font(.caption).foregroundStyle(.secondary)
                        }
                        Text("Ejemplos de YouMind OpenLab · CC BY 4.0").font(.caption2).foregroundStyle(.tertiary)
                    }
                }
                .font(.caption)
            }
            HStack {
                Button {
                    model.useDraft()
                } label: {
                    Label(model.draftProfilePrompt == nil ? "Usar este prompt" : "Usar perfil JSON completo", systemImage: "arrow.left.circle.fill").frame(maxWidth: .infinity)
                }
                .buttonStyle(GradientButtonStyle(height: 36))
                Button {
                    model.copyToPasteboard(model.draft)
                } label: {
                    Image(systemName: "doc.on.doc")
                }
                .buttonStyle(.bordered)
                .controlSize(.large)
                .help("Copiar el prompt")
                .accessibilityLabel("Copiar el prompt")
            }
            VStack(alignment: .leading, spacing: 6) {
                FieldTitle("¿Querés ajustar algo?")
                HStack {
                    TextField("Ej.: más corto, que hable más rápido, cambiá la locación a una cocina…", text: $model.feedback, axis: .vertical)
                        .textFieldStyle(.roundedBorder)
                        .lineLimit(1...3)
                        .onSubmit { Task { await model.buildPrompt(refine: true) } }
                    Button("Ajustar") { Task { await model.buildPrompt(refine: true) } }
                        .disabled(model.isBuildingPrompt || model.feedback.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
    }

    private func cleanAuthor(_ author: String) -> String {
        author.replacingOccurrences(of: #"\[([^\]]+)\]\([^)]*\)"#, with: "$1", options: .regularExpression)
    }
}

/// Codex CLI status and ChatGPT sign-in.
struct CodexAccountRow: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            switch model.codexStatus {
            case .unknown, .checking:
                HStack { ProgressView().controlSize(.small); Text("Comprobando tu sesión de ChatGPT…").font(.caption) }
            case .notInstalled:
                Label("No se encontró Codex en esta copia de la app", systemImage: "exclamationmark.triangle.fill")
                    .font(.callout).foregroundStyle(.orange)
                Text("Descargá la versión más reciente de Framecraft (ya lo incluye) o instalalo desde Terminal:")
                    .font(.caption).foregroundStyle(.secondary)
                ForEach([CodexCLI.brewCommand, CodexCLI.installCommand], id: \.self) { command in
                    HStack {
                        Text(command).font(.caption.monospaced()).textSelection(.enabled)
                        Spacer()
                        Button("Copiar") { model.copyToPasteboard(command) }.controlSize(.small)
                    }
                    .padding(6)
                    .background(Color.primary.opacity(0.05), in: RoundedRectangle(cornerRadius: 6))
                }
                Button("Comprobar de nuevo") { Task { await model.refreshCodexStatus() } }
            case .loggedOut:
                Text("Usá tu propia cuenta de ChatGPT (Plus, Pro, Team…). No gasta créditos de KIE.")
                    .font(.caption).foregroundStyle(.secondary)
                Button {
                    Task { await model.codexLogin() }
                } label: {
                    Label("Iniciar sesión con ChatGPT", systemImage: "person.crop.circle.badge.checkmark").frame(maxWidth: .infinity)
                }
                .buttonStyle(GradientButtonStyle(height: 34))
            case .loggingIn:
                HStack {
                    ProgressView().controlSize(.small)
                    Text("Terminá de iniciar sesión en el navegador que se abrió…").font(.caption)
                }
            case .loggedIn(let detail):
                Label(detail, systemImage: "checkmark.seal.fill").font(.caption).foregroundStyle(Theme.success)
                HStack {
                    Text("Usa tu plan de ChatGPT. Puede tardar un poco más.").font(.caption).foregroundStyle(.secondary)
                    Spacer()
                    Button("Cerrar sesión") { Task { await model.codexLogout() } }.controlSize(.small)
                }
            }
        }
    }
}
