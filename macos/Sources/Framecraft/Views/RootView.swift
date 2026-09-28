import FramecraftCore
import QuickLook
import SwiftUI

struct RootView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var model = model
        NavigationSplitView {
            Sidebar()
                .navigationSplitViewColumnWidth(min: 200, ideal: 220, max: 280)
        } detail: {
            detail
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .overlay(alignment: .top) {
            if let banner = model.banner {
                BannerView(banner: banner) { withAnimation(.snappy) { model.banner = nil } }
                    .padding(.top, 10)
                    .padding(.horizontal, 20)
                    .transition(.move(edge: .top).combined(with: .opacity))
                    .task(id: banner.id) {
                        try? await Task.sleep(for: .seconds(banner.style == .error ? 9 : 4.5))
                        if model.banner?.id == banner.id { withAnimation(.snappy) { model.banner = nil } }
                    }
            }
        }
        .sheet(isPresented: $model.showOnboarding) {
            OnboardingView()
                .environment(model)
        }
        .sheet(isPresented: $model.showCommandPalette) {
            CommandPalette()
                .environment(model)
        }
        .sheet(isPresented: $model.showPromptPreview) {
            PromptPreviewSheet()
                .environment(model)
        }
        .sheet(item: detailBinding) { job in
            JobDetailView(jobID: job.id)
                .environment(model)
        }
        .quickLookPreview($model.quickLookURL)
        .tint(Theme.pink)
    }

    @ViewBuilder
    private var detail: some View {
        switch model.section ?? .create {
        case .create: CreateView()
        case .stories: StoriesView()
        case .library: LibraryView()
        case .references: ReferencesView()
        case .skills: SkillsView()
        case .guide: GuideView()
        }
    }

    private var detailBinding: Binding<Job?> {
        Binding(
            get: { model.detailJobID.flatMap { id in model.jobs.first { $0.id == id } } },
            set: { model.detailJobID = $0?.id }
        )
    }
}

struct Sidebar: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var model = model
        List(selection: $model.section) {
            Section("Estudio") {
                ForEach([SidebarItem.create, .stories, .library, .references, .skills]) { item in
                    Label(item.title, systemImage: item.symbol)
                        .badge(badge(for: item))
                        .tag(item)
                }
            }
            Section("Aprender") {
                Label(SidebarItem.guide.title, systemImage: SidebarItem.guide.symbol)
                    .tag(SidebarItem.guide)
            }
            if !model.activeJobs.isEmpty {
                Section("En curso") {
                    ForEach(model.activeJobs.prefix(5)) { job in
                        ActiveJobRow(job: job)
                            .onTapGesture {
                                model.filter = .active
                                model.section = .library
                            }
                    }
                }
            }
        }
        .listStyle(.sidebar)
        .safeAreaInset(edge: .bottom) { AccountFooter().padding(10) }
    }

    private func badge(for item: SidebarItem) -> Int {
        switch item {
        case .library: model.completedCount
        case .references: model.references.count
        case .stories: model.stories.count
        default: 0
        }
    }
}

struct ActiveJobRow: View {
    let job: Job

    var body: some View {
        HStack(spacing: 8) {
            ZStack {
                Circle().stroke(Color.primary.opacity(0.12), lineWidth: 3)
                Circle()
                    .trim(from: 0, to: max(0.04, CGFloat(job.progress) / 100))
                    .stroke(Theme.gradient, style: StrokeStyle(lineWidth: 3, lineCap: .round))
                    .rotationEffect(.degrees(-90))
            }
            .frame(width: 16, height: 16)
            VStack(alignment: .leading, spacing: 1) {
                Text(job.kind == .video ? "Video" : "Imagen").font(.caption.weight(.semibold))
                Text(job.prompt).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(job.kind == .video ? "Video" : "Imagen") en curso, \(job.progress) por ciento")
    }
}

/// KIE connection + credits, always visible at the bottom of the sidebar.
struct AccountFooter: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            if model.hasKieKey {
                HStack(spacing: 8) {
                    Image(systemName: "bolt.circle.fill").foregroundStyle(Theme.success)
                    VStack(alignment: .leading, spacing: 0) {
                        Text("KIE conectado").font(.caption.weight(.semibold))
                        Text(creditsText).font(.caption2).foregroundStyle(.secondary).contentTransition(.numericText())
                    }
                    Spacer()
                    Button {
                        Task { await model.refreshCredits() }
                    } label: {
                        Image(systemName: "arrow.clockwise")
                            .symbolEffect(.pulse, isActive: model.checkingCredits)
                    }
                    .buttonStyle(.borderless)
                    .help("Actualizar créditos")
                    .accessibilityLabel("Actualizar créditos")
                }
            } else {
                Button {
                    model.showOnboarding = true
                } label: {
                    Label("Conectar KIE", systemImage: "key.fill").frame(maxWidth: .infinity)
                }
                .buttonStyle(GradientButtonStyle(height: 34))
                .help("Necesitás una clave de KIE para generar")
            }
            SettingsLink {
                Label("Ajustes", systemImage: "gearshape").font(.caption)
            }
            .buttonStyle(.borderless)
        }
        .padding(10)
        .background(.background.opacity(0.5), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 10, style: .continuous).strokeBorder(Theme.hairline))
    }

    private var creditsText: String {
        guard let credits = model.credits else { return "Créditos: —" }
        return "Créditos: " + credits.formatted(.number.precision(.fractionLength(0...1)))
    }
}

/// Shows the exact text that will be sent to the model.
struct PromptPreviewSheet: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("Prompt final").font(.title2.weight(.bold))
            Text(model.mode == .image
                 ? "Este es el texto exacto que recibe el modelo, con tus presets de cámara y encuadre incluidos."
                 : "Este es el texto exacto que recibe Seedance 2.5.")
                .foregroundStyle(.secondary)
            ScrollView {
                Text(model.finalPrompt.isEmpty ? "Todavía no escribiste un prompt." : model.finalPrompt)
                    .font(.system(.body, design: .monospaced))
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(12)
            }
            .background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 10))
            HStack {
                Text("\(model.finalPrompt.count) caracteres").font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("Copiar") { model.copyToPasteboard(model.finalPrompt) }
                Button("Listo") { dismiss() }.keyboardShortcut(.defaultAction)
            }
        }
        .padding(24)
        .frame(width: 640, height: 520)
    }
}
