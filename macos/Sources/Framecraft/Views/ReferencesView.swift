import FramecraftCore
import SwiftUI

struct ReferencesView: View {
    @Environment(AppModel.self) private var model
    @State private var importing = false
    @State private var dropTargeted = false
    @State private var pendingDelete: ReferenceFile?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Tus referencias").font(.largeTitle.weight(.bold))
                    Text("Fotos de producto, caras, locaciones, clips y voces para guiar al modelo. Hacé clic para usarlas en la próxima generación: el orden define @Image1, @Image2…")
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                DropHint { importing = true }
                ForEach(ReferenceKind.allCases) { kind in
                    let items = model.references(kind)
                    if !items.isEmpty {
                        VStack(alignment: .leading, spacing: 10) {
                            Text(title(kind)).font(.title3.weight(.semibold))
                            LazyVGrid(columns: [GridItem(.adaptive(minimum: 150, maximum: 190), spacing: 14)], alignment: .leading, spacing: 14) {
                                ForEach(items) { reference in
                                    VStack(alignment: .leading, spacing: 4) {
                                        ReferenceTile(reference: reference)
                                        Text(ByteCountFormatter.string(fromByteCount: reference.bytes, countStyle: .file)
                                             + (reference.durationMs != nil ? " · " + MediaTools.formattedDuration(reference.durationSeconds) : ""))
                                            .font(.caption2)
                                            .foregroundStyle(.tertiary)
                                    }
                                    .contextMenu {
                                        Button("Eliminar", role: .destructive) { pendingDelete = reference }
                                    }
                                }
                            }
                        }
                    }
                }
                if model.references.isEmpty {
                    Text("Tip: también podés arrastrar archivos a cualquier parte de la pantalla Crear.")
                        .font(.callout)
                        .foregroundStyle(.secondary)
                }
            }
            .padding(28)
            .frame(maxWidth: 1100, alignment: .leading)
        }
        .overlay {
            if dropTargeted {
                RoundedRectangle(cornerRadius: 18).strokeBorder(Theme.gradient, style: StrokeStyle(lineWidth: 3, dash: [10, 6])).padding(12)
            }
        }
        .dropDestination(for: URL.self) { urls, _ in
            Task { await model.importFiles(urls, select: false) }
            return true
        } isTargeted: { dropTargeted = $0 }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.image, .movie, .audio], allowsMultipleSelection: true) { result in
            if case .success(let urls) = result { Task { await model.importFiles(urls, select: false) } }
        }
        .confirmationDialog("¿Eliminar esta referencia?", isPresented: Binding(get: { pendingDelete != nil }, set: { if !$0 { pendingDelete = nil } })) {
            Button("Eliminar", role: .destructive) {
                if let reference = pendingDelete { model.deleteReferences([reference.id]) }
            }
        } message: {
            Text("Las generaciones que ya hiciste no cambian.")
        }
        .navigationTitle("Referencias")
        .toolbar {
            ToolbarItem {
                Button { importing = true } label: { Label("Agregar", systemImage: "plus") }
                    .help("Agregar referencias")
            }
        }
    }

    private func title(_ kind: ReferenceKind) -> String {
        switch kind {
        case .image: "Imágenes"
        case .video: "Videos (hasta 30 s)"
        case .audio: "Audios (hasta 30 s) · se usan como timbre de voz"
        }
    }
}
