import AppKit
import FramecraftCore
import SwiftUI

/// `Framecraft --snapshot <dir>`: renders the main screens with demo data to PNG files.
/// Used by CI to review the interface; never touches the real library or the Keychain.
@MainActor
enum SnapshotRunner {
    static func run(outputDirectory: URL) {
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)
        try? FileManager.default.createDirectory(at: outputDirectory, withIntermediateDirectories: true)

        let model = DemoData.makeModel()
        let full = CGSize(width: 1440, height: 900)
        var savedStories: [Story] = []

        func configureImage() {
            model.section = .create
            model.mode = .image
            model.showAssistant = true
            model.prompt = "Selfie espontánea de una chica de 25 años tomando café junto a una ventana con lluvia, luz fría de tarde, piel real con poros, pelo un poco despeinado, taza de cerámica con vapor."
            model.selectedImages = Array(model.references(.image).prefix(2).map(\.id))
            model.idea = "Selfie tomando café en un día de lluvia"
            model.draft = model.prompt
            model.camera = "iPhone Camera"
            model.film = "Selfie"
        }

        func configureVideo() {
            model.section = .create
            model.mode = .video
            model.showAssistant = true
            model.skillID = SkillLibrary.ugcID
            model.videoDuration = 15
            model.selectedImages = Array(model.references(.image).prefix(2).map(\.id))
            model.selectedAudios = model.references(.audio).map(\.id)
            model.prompt = DemoData.videoPrompt
            model.idea = "Chica recomienda su sérum frente al espejo del baño"
            model.draft = DemoData.videoPrompt
            model.notes = "Cargá @Image1 (el frasco) y @Image2 (el baño) en ese orden. Diálogo: 34 de ~37 palabras para 15 s."
        }

        let scenes: [(String, CGSize, NSAppearance.Name, () -> Void, () -> AnyView)] = [
            ("01-crear-imagen", full, .aqua, configureImage, { AnyView(RootView()) }),
            ("02-crear-video", full, .aqua, configureVideo, { AnyView(RootView()) }),
            ("03-crear-video-oscuro", full, .darkAqua, configureVideo, { AnyView(RootView()) }),
            ("04-biblioteca", full, .aqua, {
                model.section = .library
                model.filter = .all
            }, { AnyView(RootView()) }),
            ("05-biblioteca-oscuro", full, .darkAqua, { model.section = .library }, { AnyView(RootView()) }),
            ("06-referencias", full, .aqua, { model.section = .references }, { AnyView(RootView()) }),
            ("07-skills", full, .aqua, { model.section = .skills }, { AnyView(RootView()) }),
            ("08-guia", full, .aqua, { model.section = .guide }, { AnyView(RootView()) }),
            ("08b-historias", full, .aqua, {
                model.section = .stories
                model.selectedStoryID = model.stories.first?.id
            }, { AnyView(RootView()) }),
            ("09-bienvenida", CGSize(width: 640, height: 520), .aqua, {}, { AnyView(OnboardingView()) }),
            ("10-bienvenida-oscuro", CGSize(width: 640, height: 520), .darkAqua, {}, { AnyView(OnboardingView()) }),
            ("11-ajustes", CGSize(width: 560, height: 470), .aqua, {}, { AnyView(SettingsView()) }),
            ("12-detalle", CGSize(width: 1100, height: 760), .aqua, {}, {
                AnyView(JobDetailView(jobID: model.jobs.first { $0.status == .success }?.id ?? UUID()))
            }),
            ("13-paleta-de-comandos", CGSize(width: 640, height: 480), .aqua, {}, { AnyView(CommandPalette()) }),
            ("14-barra-de-menus", CGSize(width: 320, height: 330), .aqua, {}, { AnyView(MenuBarPanel()) }),
            ("15-historias-oscuro", full, .darkAqua, {
                model.section = .stories
                model.selectedStoryID = model.stories.first?.id
            }, { AnyView(RootView()) }),
            ("16-historias-plantillas", full, .aqua, {
                savedStories = model.stories
                model.stories = []
                model.selectedStoryID = nil
                model.section = .stories
            }, { AnyView(RootView()) }),
        ]

        for (name, size, appearance, configure, content) in scenes {
            configure()
            let root = content()
                .environment(model)
                .frame(width: size.width, height: size.height)
            let hosting = NSHostingView(rootView: root)
            hosting.sceneBridgingOptions = [.toolbars, .title]
            let window = NSWindow(
                contentRect: NSRect(origin: NSPoint(x: 40, y: 40), size: size),
                styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView],
                backing: .buffered, defer: false
            )
            window.appearance = NSAppearance(named: appearance)
            window.title = "Framecraft"
            window.contentView = hosting
            window.orderFrontRegardless()
            RunLoop.main.run(until: Date().addingTimeInterval(2.5))

            let target = hosting.superview ?? hosting
            if let rep = target.bitmapImageRepForCachingDisplay(in: target.bounds) {
                target.cacheDisplay(in: target.bounds, to: rep)
                if let png = rep.representation(using: .png, properties: [:]) {
                    try? png.write(to: outputDirectory.appendingPathComponent(name + ".png"))
                    print("snapshot: \(name).png")
                }
            }
            // Replace it with a real on-screen capture when allowed: cacheDisplay cannot
            // draw the sidebar/inspector materials. Needs screen-recording access.
            NSApp.activate(ignoringOtherApps: true)
            window.makeKeyAndOrderFront(nil)
            RunLoop.main.run(until: Date().addingTimeInterval(0.8))
            let capture = Process()
            capture.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
            let real = outputDirectory.appendingPathComponent(name + "-real.png")
            capture.arguments = ["-x", "-o", "-l\(window.windowNumber)", real.path]
            if (try? capture.run()) != nil {
                let deadline = Date().addingTimeInterval(10)
                while capture.isRunning && Date() < deadline { RunLoop.main.run(until: Date().addingTimeInterval(0.1)) }
                // Prefer the real capture when it worked.
                if !capture.isRunning, capture.terminationStatus == 0, FileManager.default.fileExists(atPath: real.path) {
                    let target = outputDirectory.appendingPathComponent(name + ".png")
                    try? FileManager.default.removeItem(at: target)
                    try? FileManager.default.moveItem(at: real, to: target)
                    print("snapshot: \(name).png (on-screen capture)")
                }
            }
            window.orderOut(nil)
            if !savedStories.isEmpty {
                model.stories = savedStories
                savedStories = []
            }
        }
        exit(0)
    }
}

/// Demo library for snapshots.
@MainActor
enum DemoData {
    static let videoPrompt = """
    FORMAT: Vertical 9:16, 15 seconds, one continuous take, no cuts. Rear 1x phone camera, wide lens ~26mm, held in one hand at chest height, constant breathing micro-shake, reframes half a second late. Deep depth of field: the bathroom stays fully readable.
    VOICE DIRECTION: text in parentheses is acting direction only and is NEVER spoken aloud.
    0–5 s: P1 lifts @Image1 next to her face in front of the mirror. P1 (half asleep, voice still low): "Okay, I've been using this every morning for a month."
    5–10 s: She tilts the jar, the autoexposure hunts when she passes the window. P1 (faster, a little laugh): "And my skin finally stopped fighting me, look."
    10–15 s: She leans closer to the phone, the frame drops slightly as her arm gets tired. P1 (quieter): "Not sponsored. I just really like it."
    AUDIO: diegetic phone microphone, small bathroom echo, tap dripping. Match @Audio1 for timbre, age, accent and pitch only. Do not reproduce any words from @Audio1 and do not copy its emotion.
    RESTRICTIONS: no music, no on-screen text, no gimbal, no bokeh, no beauty filter.
    """

    static func makeModel() -> AppModel {
        let store = LibraryStore.temporary()
        let keys = MemoryKeyStore()
        try? keys.write("demo-key-0000000000", account: KeyAccount.kie)
        var index = LibraryIndex()
        index.preferences.onboardingDone = true

        let palettes: [[NSColor]] = [
            [.systemOrange, .systemPink], [.systemTeal, .systemPurple], [.systemPink, .systemIndigo],
            [.systemYellow, .systemRed], [.systemMint, .systemBlue], [.systemPurple, .systemOrange],
        ]
        let names = ["Selfie con café", "Sérum en mármol", "Unboxing zapatillas", "POV en el sillón", "Look de oficina", "Receta en la cocina"]
        func image(_ index: Int, size: CGSize) -> Data {
            let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(size.width), pixelsHigh: Int(size.height), bitsPerSample: 8,
                                       samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
            NSGraphicsContext.saveGraphicsState()
            NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
            let colors = palettes[index % palettes.count]
            NSGradient(starting: colors[0], ending: colors[1])?.draw(in: NSRect(origin: .zero, size: size), angle: 60)
            NSColor.white.withAlphaComponent(0.28).setFill()
            NSBezierPath(ovalIn: NSRect(x: size.width * 0.25, y: size.height * 0.38, width: size.width * 0.5, height: size.width * 0.5)).fill()
            NSColor.white.withAlphaComponent(0.18).setFill()
            NSBezierPath(roundedRect: NSRect(x: size.width * 0.12, y: size.height * 0.08, width: size.width * 0.76, height: size.height * 0.16), xRadius: 18, yRadius: 18).fill()
            let text = NSAttributedString(string: names[index % names.count], attributes: [
                .font: NSFont.systemFont(ofSize: size.width * 0.07, weight: .bold),
                .foregroundColor: NSColor.white,
            ])
            text.draw(at: NSPoint(x: size.width * 0.16, y: size.height * 0.12))
            NSGraphicsContext.restoreGraphicsState()
            return rep.representation(using: .png, properties: [:])!
        }

        let now = Date()
        for i in 0..<3 {
            let id = UUID()
            let fileName = "\(id.uuidString.lowercased()).png"
            let data = image(i + 3, size: CGSize(width: 600, height: 600))
            try? data.write(to: store.referencesDir.appendingPathComponent(fileName))
            index.references.append(ReferenceFile(id: id, name: ["frasco-serum.png", "baño-luz-natural.png", "modelo-perfil.png"][i],
                                                  kind: .image, mime: "image/png", fileName: fileName, bytes: Int64(data.count),
                                                  created: now.addingTimeInterval(Double(-i * 60))))
        }
        let audioID = UUID()
        let audioName = "\(audioID.uuidString.lowercased()).wav"
        try? Data(count: 64).write(to: store.referencesDir.appendingPathComponent(audioName))
        index.references.append(ReferenceFile(id: audioID, name: "voz-referencia.wav", kind: .audio, mime: "audio/wav",
                                              fileName: audioName, durationMs: 12_400, bytes: 64))

        let prompts = [
            "Selfie espontánea tomando café junto a una ventana con lluvia, luz de tarde, piel real.",
            "Frasco de sérum sobre mármol blanco con luz de ventana y sombras de plantas.",
            "Unboxing de zapatillas sobre la cama, POV con una sola mano visible.",
            "Chica en el sillón mostrando su celular con una sonrisa, luz cálida de lámpara.",
            "Look de oficina frente al espejo del ascensor, cámara de iPhone.",
            "Manos preparando una receta en una cocina chica, vista cenital.",
        ]
        for i in 0..<6 {
            let id = UUID()
            let tall = i % 2 == 0
            let fileName = "framecraft-demo-\(i + 1).png"
            try? image(i, size: tall ? CGSize(width: 540, height: 960) : CGSize(width: 800, height: 800))
                .write(to: store.outputURL(fileName))
            index.jobs.append(Job(
                id: id, batchID: UUID(), kind: .image, model: Presets.imageModels[i % 4].id, prompt: prompts[i], finalPrompt: prompts[i],
                settings: JobSettings(resolution: ["1K", "2K"][i % 2], aspect: tall ? "9:16" : "1:1", camera: "iPhone Camera", film: Presets.none),
                status: .success, progress: 100, taskId: "demo", outputs: [fileName],
                created: now.addingTimeInterval(Double(-3600 * (i + 1))), favorite: i == 1
            ))
        }
        let generatingVideoID = UUID()
        index.jobs.insert(Job(
            id: generatingVideoID, batchID: UUID(), kind: .video, model: Presets.videoModelID, prompt: "Chica recomienda su sérum frente al espejo del baño, 15 s, diálogo en inglés.",
            finalPrompt: videoPrompt, settings: JobSettings(resolution: "1080p", aspect: "9:16", duration: 15, generateAudio: true),
            status: .generating, progress: 46, taskId: "demo", created: now.addingTimeInterval(-120)
        ), at: 0)
        index.jobs.insert(Job(
            batchID: UUID(), kind: .image, model: "nano-banana-pro", prompt: "Retrato testimonial sosteniendo una barra de proteína.",
            finalPrompt: "", settings: JobSettings(resolution: "1K", aspect: "4:5"), status: .fail, progress: 12, taskId: "demo",
            error: "Content policy: reformulá el prompt.", created: now.addingTimeInterval(-600)
        ), at: 1)
        store.save(index)

        let firstImage = index.references.first?.id
        let audio = index.references.first { $0.kind == .audio }?.id
        let bible = "FORMAT: Vertical 9:16, one continuous take, no cuts. Rear 1x phone camera held by P2 (the customer). Deep depth of field.\nP1 WALTER: 82-year-old African American waiter, beige polo, red name tag, black half apron."
        index.stories = [Story(
            title: "Walter, 82 años", brief: "Un mozo de 82 años en un diner cuenta su historia al cliente que lo filma.",
            sceneDuration: 30, slots: [
                StoryReferenceSlot(kind: .image, referenceID: firstImage, note: "Identidad de Walter (tres vistas)"),
                StoryReferenceSlot(kind: .image, isLastFrame: true, note: "Primer fotograma clavado"),
                StoryReferenceSlot(kind: .audio, referenceID: audio, note: "Timbre de voz de Walter"),
            ],
            summary: "Un cliente filma a Walter, un mozo de 82 años, mientras le cuenta por qué sigue trabajando. La propina, el rechazo y el abrazo.",
            continuity: bible,
            referenceOrder: "Cargá @Image1 (Walter) y @Image2 (último fotograma de la escena anterior) en ese orden; @Audio1 solo como timbre.",
            scenes: [
                StoryScene(number: 1, title: "Hook: “Eighty-two”", summary: "El cliente le pregunta la edad a Walter mientras sirve café; la respuesta abre la historia.", duration: 30,
                           prompt: bible + "\n0–10 s: P1 pours coffee. P2 (curious, casual): \"How old are you, if you don't mind me asking?\"\n10–30 s: P1 (proud, a small laugh): \"Eighty-two.\" No music, no text.", notes: "Diálogo corto: sumar líneas para llegar a ~74 palabras.", jobIDs: [generatingVideoID]),
                StoryScene(number: 2, title: "La hija", summary: "Walter cuenta, sin dramatismo, que su hija falleció y por eso trabaja.", duration: 30,
                           prompt: bible + "\n@Image2 IS the first frame of this video.\n0–15 s: P1 (quiet, flat): \"My daughter passed away.\" Match @Audio1 for timbre only. No music, no cuts, no text."),
                StoryScene(number: 3, title: "El peso", summary: "El cliente cambia de tema con humor; Walter se ríe y toma el pedido.", duration: 25,
                           prompt: bible + "\n@Image2 IS the first frame of this video.\n0–25 s: P2 (lighter): \"So what are you having for breakfast?\" No music, no text."),
            ],
            messages: [StoryMessage(role: .assistant, text: "Historia creada: 3 escenas, 85 s en total.")]
        )]
        store.save(index)
        let model = AppModel(store: store, keys: keys, isDemo: true)
        model.credits = 1250
        return model
    }
}
