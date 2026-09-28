import FramecraftCore
import SwiftUI
import UniformTypeIdentifiers

struct SkillsView: View {
    @Environment(AppModel.self) private var model
    @State private var selectedID: String?
    @State private var importing = false
    @State private var dropTargeted = false
    @State private var editing: Skill?

    var body: some View {
        HSplitView {
            List(selection: $selectedID) {
                Section("Incluidas") {
                    ForEach(model.builtinSkills) { skill in
                        row(skill).tag(skill.id as String?)
                    }
                }
                Section("Tuyas") {
                    if model.customSkills.isEmpty {
                        Text("Importá un SKILL.md, un .md o un .txt, o creá una nueva.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    ForEach(model.customSkills) { skill in
                        row(skill).tag(skill.id as String?)
                    }
                }
            }
            .frame(minWidth: 240, idealWidth: 280, maxWidth: 360)
            .overlay {
                if dropTargeted {
                    RoundedRectangle(cornerRadius: 12).strokeBorder(Theme.gradient, style: StrokeStyle(lineWidth: 3, dash: [8, 5])).padding(6)
                }
            }
            .dropDestination(for: URL.self) { urls, _ in
                importSkills(urls)
                return true
            } isTargeted: { dropTargeted = $0 }

            Group {
                if let skill = model.allSkills.first(where: { $0.id == selectedID }) {
                    SkillDetail(skill: skill, edit: { editing = $0 }, select: { selectedID = $0 })
                } else {
                    ContentUnavailableView {
                        Label("Elegí una skill", systemImage: "brain.head.profile")
                    } description: {
                        Text("Una skill es un método escrito que el asistente aplica siempre igual. Arrastrá acá un SKILL.md para importarlo.")
                    }
                }
            }
            .frame(minWidth: 420, maxWidth: .infinity, maxHeight: .infinity)
        }
        .navigationTitle("Skills")
        .toolbar {
            ToolbarItemGroup {
                Button { importing = true } label: { Label("Importar", systemImage: "square.and.arrow.down") }
                    .help("Importar un SKILL.md, .md o .txt (también una carpeta con SKILL.md)")
                Button {
                    editing = Skill(name: "Nueva skill", summary: "", content: "", media: .any)
                } label: { Label("Nueva", systemImage: "plus") }
                    .help("Escribir una skill desde cero")
            }
        }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.plainText, UTType(filenameExtension: "md") ?? .plainText, .folder], allowsMultipleSelection: true) { result in
            if case .success(let urls) = result { importSkills(urls) }
        }
        .sheet(item: $editing) { skill in
            SkillEditor(skill: skill) { saved in
                model.saveSkill(saved)
                selectedID = saved.id
            }
        }
        .onAppear { if selectedID == nil { selectedID = model.skillID.isEmpty ? SkillLibrary.ugcID : model.skillID } }
    }

    private func row(_ skill: Skill) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(skill.name).font(.callout.weight(.medium)).lineLimit(1)
            Text(mediaLabel(skill.media)).font(.caption2).foregroundStyle(.secondary)
        }
        .padding(.vertical, 2)
    }

    private func importSkills(_ urls: [URL]) {
        for url in urls {
            do {
                try model.importSkill(from: url)
                selectedID = model.customSkills.last?.id
            } catch {
                model.show("\(url.lastPathComponent): \(error.localizedDescription)", .error)
            }
        }
    }
}

func mediaLabel(_ media: SkillMedia) -> String {
    switch media {
    case .image: "Para imágenes"
    case .video: "Para videos"
    case .any: "Para imágenes y videos"
    }
}

struct SkillDetail: View {
    @Environment(AppModel.self) private var model
    let skill: Skill
    let edit: (Skill) -> Void
    let select: (String?) -> Void
    @State private var confirmDelete = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                HStack(alignment: .top) {
                    VStack(alignment: .leading, spacing: 6) {
                        Text(skill.name).font(.title.weight(.bold))
                        HStack(spacing: 6) {
                            Text(mediaLabel(skill.media))
                            if skill.isBuiltin { Text("· Incluida") }
                            if model.skillID == skill.id { Text("· En uso").foregroundStyle(Theme.pink) }
                        }
                        .font(.callout)
                        .foregroundStyle(.secondary)
                    }
                    Spacer()
                }
                if !skill.summary.isEmpty {
                    Text(skill.summary).font(.body).fixedSize(horizontal: false, vertical: true)
                }
                HStack {
                    Button {
                        model.useSkill(skill)
                    } label: {
                        Label("Usar en el asistente", systemImage: "wand.and.stars")
                    }
                    .buttonStyle(GradientButtonStyle(height: 34))
                    if skill.isBuiltin {
                        Button("Duplicar para editar") {
                            let copy = model.duplicate(skill)
                            select(copy.id)
                            edit(copy)
                        }
                        .buttonStyle(.bordered)
                        .disabled(skill.id == SkillLibrary.jsonProfileID)
                    } else {
                        Button("Editar") { edit(skill) }.buttonStyle(.bordered)
                        Button("Eliminar", role: .destructive) { confirmDelete = true }.buttonStyle(.bordered)
                    }
                }
                if skill.id == SkillLibrary.ugcID {
                    UGCRulesCard()
                }
                Divider()
                if skill.id == SkillLibrary.jsonProfileID {
                    Text("Genera un perfil visual JSON con 10 secciones (composición, color, luz, especificaciones técnicas…) y se inspira en los 3 ejemplos más parecidos a tu idea de una biblioteca de 123 prompts de GPT Image 2 (YouMind OpenLab, CC BY 4.0).")
                        .foregroundStyle(.secondary)
                } else {
                    Text("Instrucciones").font(.headline)
                    Text(skill.content)
                        .font(.system(.callout, design: .monospaced))
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(14)
                        .background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 10))
                }
            }
            .padding(24)
        }
        .confirmationDialog("¿Eliminar “\(skill.name)”?", isPresented: $confirmDelete) {
            Button("Eliminar", role: .destructive) {
                model.deleteSkill(skill)
                select(nil)
            }
        }
    }
}

/// The five rules of the Seedance kit, at a glance.
struct UGCRulesCard: View {
    static let rules: [(String, String, String)] = [
        ("atom", "Causas físicas, no adjetivos", "\"El operador reencuadra medio segundo tarde\" en vez de \"realista\"."),
        ("timer", "Acción por bloques de tiempo", "0–6 s, 6–12 s… Sin eso el modelo adivina el timeline."),
        ("quote.bubble", "Diálogo con densidad medida", "~2,47 palabras por segundo: 30 s ≈ 74 palabras. Los silencios se hacen en la edición."),
        ("theatermasks", "Emoción como mecánica física", "Entre paréntesis y aclarando que no se pronuncia: dónde respira, dónde baja la voz."),
        ("doc.on.doc", "La continuidad se copia y pega", "La biblia de personajes y locación va idéntica en cada escena."),
    ]

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 12) {
                Text("Las cinco reglas del método").font(.headline)
                ForEach(Self.rules, id: \.1) { symbol, title, detail in
                    HStack(alignment: .top, spacing: 12) {
                        Image(systemName: symbol)
                            .foregroundStyle(Theme.gradient)
                            .font(.title3)
                            .frame(width: 26)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(title).font(.callout.weight(.semibold))
                            Text(detail).font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    .accessibilityElement(children: .combine)
                }
            }
        }
    }
}

struct SkillEditor: View {
    @Environment(\.dismiss) private var dismiss
    @State var skill: Skill
    let save: (Skill) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text(skill.content.isEmpty ? "Nueva skill" : "Editar skill").font(.title2.weight(.bold))
            Form {
                TextField("Nombre", text: $skill.name)
                TextField("Descripción corta", text: $skill.summary, axis: .vertical)
                    .lineLimit(1...3)
                Picker("Sirve para", selection: $skill.media) {
                    Text("Imágenes y videos").tag(SkillMedia.any)
                    Text("Imágenes").tag(SkillMedia.image)
                    Text("Videos").tag(SkillMedia.video)
                }
            }
            .formStyle(.grouped)
            .frame(height: 170)
            Text("Instrucciones (markdown)").font(.headline)
            PromptEditor(
                text: $skill.content,
                placeholder: "Explicá tu estructura de prompt, estilo visual, vocabulario, restricciones y checklist…",
                minHeight: 260, monospaced: true, accessibilityName: "Instrucciones de la skill"
            )
            HStack {
                Text("\(skill.content.count.formatted()) / 100.000 caracteres").font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("Cancelar") { dismiss() }.keyboardShortcut(.cancelAction)
                Button("Guardar") {
                    save(skill)
                    dismiss()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(skill.name.trimmingCharacters(in: .whitespaces).isEmpty
                          || skill.content.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                          || skill.content.count > 100_000)
            }
        }
        .padding(24)
        .frame(width: 720, height: 680)
    }
}
