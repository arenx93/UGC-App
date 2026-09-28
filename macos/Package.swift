// swift-tools-version:5.10
// Framecraft — native macOS app (SwiftUI).
// Open this folder in Xcode (File ▸ Open… ▸ Package.swift) to edit and run,
// or build the .app with scripts/build-app.sh.
import PackageDescription

let package = Package(
    name: "Framecraft",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "Framecraft", targets: ["Framecraft"]),
    ],
    targets: [
        // UI-free logic: presets, KIE/OpenAI clients, prompt builder, skills, prompt checker.
        .target(
            name: "FramecraftCore",
            resources: [.copy("Resources/Skills")]
        ),
        // SwiftUI app.
        .executableTarget(
            name: "Framecraft",
            dependencies: ["FramecraftCore"]
        ),
        .testTarget(
            name: "FramecraftCoreTests",
            dependencies: ["FramecraftCore"],
            resources: [.copy("Fixtures")]
        ),
    ]
)
