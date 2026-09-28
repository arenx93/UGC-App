import FramecraftCore
import SwiftUI

struct GuideView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Cómo escribir UGC que no parezca un anuncio")
                        .font(.system(size: 28, weight: .bold, design: .rounded))
                        .brandGradientText()
                    Text("El método del kit de prompts de Seedance 2.5, dentro de la app. El asistente lo aplica cuando elegís la skill “UGC de celular”, y la revisión del prompt lo chequea mientras escribís.")
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                HStack(spacing: 10) {
                    Button {
                        if let skill = model.builtinSkills.first(where: { $0.id == SkillLibrary.ugcID }) { model.useSkill(skill) }
                    } label: {
                        Label("Probar la skill UGC", systemImage: "wand.and.stars")
                    }
                    .buttonStyle(GradientButtonStyle(height: 36))
                    if let resources = model.resources {
                        Button {
                            NSWorkspace.shared.open(resources.guidePDF)
                        } label: {
                            Label("Documento base (PDF)", systemImage: "doc.richtext")
                        }
                        .buttonStyle(.bordered)
                        .controlSize(.large)
                        Button {
                            NSWorkspace.shared.open(resources.url("ugc-celular/EJEMPLO-WALTER.txt"))
                        } label: {
                            Label("Pack de Walter (8 escenas)", systemImage: "film.stack")
                        }
                        .buttonStyle(.bordered)
                        .controlSize(.large)
                    }
                }
                UGCRulesCard()
                if let resources = model.resources {
                    Card(padding: 22) {
                        MarkdownView(text: resources.text("guia/COMO-FUNCIONA.md"))
                    }
                }
            }
            .padding(28)
            .frame(maxWidth: 900, alignment: .leading)
            .frame(maxWidth: .infinity)
        }
        .navigationTitle("Guía UGC")
    }
}

/// Minimal block-level markdown renderer (headings, lists, quotes, code, tables).
struct MarkdownView: View {
    let text: String

    enum Block: Hashable {
        case heading(Int, String)
        case paragraph(String)
        case bullet(String)
        case numbered(String, String)
        case quote(String)
        case code(String)
        case table([[String]])
        case rule
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            ForEach(Array(Self.parse(text).enumerated()), id: \.offset) { _, block in
                view(for: block)
            }
        }
        .textSelection(.enabled)
    }

    @ViewBuilder
    private func view(for block: Block) -> some View {
        switch block {
        case .heading(let level, let text):
            inline(text)
                .font(level == 1 ? .title.weight(.bold) : level == 2 ? .title2.weight(.bold) : .title3.weight(.semibold))
                .padding(.top, level <= 2 ? 10 : 4)
                .accessibilityAddTraits(.isHeader)
        case .paragraph(let text):
            inline(text).fixedSize(horizontal: false, vertical: true)
        case .bullet(let text):
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                Circle().fill(Theme.pink).frame(width: 5, height: 5).offset(y: -3)
                inline(text).fixedSize(horizontal: false, vertical: true)
            }
            .padding(.leading, 6)
        case .numbered(let number, let text):
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                Text(number).font(.callout.weight(.bold)).foregroundStyle(Theme.pink)
                inline(text).fixedSize(horizontal: false, vertical: true)
            }
            .padding(.leading, 4)
        case .quote(let text):
            HStack(spacing: 10) {
                RoundedRectangle(cornerRadius: 2).fill(Theme.gradient).frame(width: 4)
                inline(text).italic().fixedSize(horizontal: false, vertical: true)
            }
        case .code(let text):
            Text(text)
                .font(.system(.callout, design: .monospaced))
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(12)
                .background(Color.primary.opacity(0.05), in: RoundedRectangle(cornerRadius: 8))
        case .table(let rows):
            Grid(alignment: .leading, horizontalSpacing: 14, verticalSpacing: 6) {
                ForEach(Array(rows.enumerated()), id: \.offset) { index, row in
                    GridRow {
                        ForEach(Array(row.enumerated()), id: \.offset) { _, cell in
                            inline(cell)
                                .font(index == 0 ? .callout.weight(.semibold) : .callout)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    if index == 0 { Divider() }
                }
            }
            .padding(12)
            .background(Color.primary.opacity(0.03), in: RoundedRectangle(cornerRadius: 8))
        case .rule:
            Divider().padding(.vertical, 6)
        }
    }

    private func inline(_ text: String) -> Text {
        let options = AttributedString.MarkdownParsingOptions(interpretedSyntax: .inlineOnlyPreservingWhitespace)
        if let attributed = try? AttributedString(markdown: text, options: options) { return Text(attributed) }
        return Text(text)
    }

    static func parse(_ markdown: String) -> [Block] {
        var blocks: [Block] = []
        var paragraph: [String] = []
        var code: [String]?
        var table: [[String]] = []
        var quote: [String] = []

        func flush() {
            if !paragraph.isEmpty { blocks.append(.paragraph(paragraph.joined(separator: " "))); paragraph = [] }
            if !table.isEmpty { blocks.append(.table(table)); table = [] }
            if !quote.isEmpty { blocks.append(.quote(quote.joined(separator: " "))); quote = [] }
        }

        for rawLine in markdown.replacingOccurrences(of: "\r\n", with: "\n").components(separatedBy: "\n") {
            let line = rawLine.trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("```") {
                if let lines = code {
                    blocks.append(.code(lines.joined(separator: "\n")))
                    code = nil
                } else {
                    flush()
                    code = []
                }
                continue
            }
            if code != nil {
                code?.append(rawLine)
                continue
            }
            if line.isEmpty { flush(); continue }
            if line.hasPrefix("|") {
                if !paragraph.isEmpty || !quote.isEmpty {
                    let pending = table
                    table = []
                    flush()
                    table = pending
                }
                let cells = line.trimmingCharacters(in: CharacterSet(charactersIn: "|"))
                    .components(separatedBy: "|")
                    .map { $0.trimmingCharacters(in: .whitespaces) }
                if cells.allSatisfy({ $0.allSatisfy { "-:".contains($0) } }) { continue }
                table.append(cells)
                continue
            }
            if line == "---" || line == "***" { flush(); blocks.append(.rule); continue }
            if line.hasPrefix("#") {
                flush()
                let level = line.prefix { $0 == "#" }.count
                blocks.append(.heading(level, line.dropFirst(level).trimmingCharacters(in: .whitespaces)))
                continue
            }
            if line.hasPrefix("> ") || line == ">" {
                if !paragraph.isEmpty { blocks.append(.paragraph(paragraph.joined(separator: " "))); paragraph = [] }
                quote.append(String(line.dropFirst(1)).trimmingCharacters(in: .whitespaces))
                continue
            }
            if line.hasPrefix("- ") || line.hasPrefix("* ") {
                flush()
                blocks.append(.bullet(String(line.dropFirst(2))))
                continue
            }
            if let dot = line.firstIndex(of: "."), line[..<dot].allSatisfy(\.isNumber), !line[..<dot].isEmpty,
               line[line.index(after: dot)...].hasPrefix(" ") {
                flush()
                blocks.append(.numbered(String(line[...dot]), line[line.index(after: dot)...].trimmingCharacters(in: .whitespaces)))
                continue
            }
            paragraph.append(line)
        }
        if let lines = code { blocks.append(.code(lines.joined(separator: "\n"))) }
        flush()
        return blocks
    }
}
