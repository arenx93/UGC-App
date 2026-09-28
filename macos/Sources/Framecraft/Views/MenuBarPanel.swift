import AppKit
import FramecraftCore
import SwiftUI

/// Menu bar panel: what is generating, the latest results and quick actions.
struct MenuBarPanel: View {
    @Environment(AppModel.self) private var model
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Framecraft").font(.headline).brandGradientText()
                Spacer()
                if let credits = model.credits {
                    Label(credits.formatted(.number.precision(.fractionLength(0))), systemImage: "bolt.fill")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                        .help("Créditos de KIE")
                }
            }
            if model.activeJobs.isEmpty {
                Label("No hay nada generándose.", systemImage: "checkmark.circle")
                    .font(.callout).foregroundStyle(.secondary)
            } else {
                VStack(alignment: .leading, spacing: 8) {
                    ForEach(model.activeJobs.prefix(6)) { job in
                        VStack(alignment: .leading, spacing: 4) {
                            HStack {
                                Image(systemName: job.kind == .video ? "video.fill" : "photo.fill").foregroundStyle(Theme.pink)
                                Text(job.prompt).lineLimit(1).font(.callout)
                                Spacer()
                                Text("\(job.progress)%").font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                            }
                            ProgressView(value: Double(max(job.progress, 2)), total: 100).tint(Theme.pink)
                        }
                    }
                }
            }
            let recent = model.jobs.filter { $0.status == .success }.prefix(4)
            if !recent.isEmpty {
                Divider()
                Text("Últimas creaciones").font(.caption.weight(.semibold)).foregroundStyle(.secondary)
                HStack(spacing: 8) {
                    ForEach(Array(recent)) { job in
                        Button {
                            open { model.detailJobID = job.id }
                        } label: {
                            Thumbnail(url: model.outputURLs(job).first, video: job.kind == .video, maxPixel: 200)
                                .frame(width: 64, height: 64)
                                .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
                        }
                        .buttonStyle(.plain)
                        .help(job.prompt)
                    }
                }
            }
            Divider()
            HStack {
                Button("Nuevo video") { open { model.mode = .video; model.section = .create } }
                Button("Nueva imagen") { open { model.mode = .image; model.section = .create } }
                Spacer()
                Button("Abrir") { open {} }.keyboardShortcut(.defaultAction)
            }
            .controlSize(.small)
        }
        .padding(14)
        .frame(width: 320)
    }

    private func open(_ then: () -> Void) {
        then()
        openWindow(id: "main")
        NSApp.activate(ignoringOtherApps: true)
    }
}
