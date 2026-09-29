# Framecraft para Windows — guía para agentes (Codex)

App nativa en **C++20 + WinUI 3** (Windows App SDK 1.7, C++/WinRT), sin empaquetar
(`WindowsPackageType=None`, self-contained). Toda la UI se construye en C++ (no hay XAML salvo
`App.xaml`, que solo carga los estilos de Fluent).

## Compilar

Desde la raíz del repo, en PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File windows\scripts\build.ps1          # tests del núcleo + app
powershell -ExecutionPolicy Bypass -File windows\scripts\build.ps1 -Run     # compila y abre la app
powershell -ExecutionPolicy Bypass -File windows\scripts\build.ps1 -Package # + .zip e instalador en windows\dist
```

Resultado: `windows\build\Release\Framecraft.exe`.
Requisito: Visual Studio 2022 o posterior con el workload **Desarrollo de escritorio con C++**
(MSVC v143+, Windows 10/11 SDK y "C++ CMake tools"). NuGet y Codex CLI se descargan solos.

Comprobación rápida sin UI (renderiza cada pantalla con datos de ejemplo a PNG y sale):
`windows\build\Release\Framecraft.exe --snapshot <carpeta> --light` (o `--dark`).

## Estructura

- `core/` — lógica portable sin APIs de Windows (presets, prompts, revisor de prompts, skills,
  historias, plantillas, biblioteca). Tests en `core/tests/core_tests.cpp`; también compila en
  Linux/macOS con CMake. Si cambiás algo acá, corré los tests.
- `app/src/` — la app: `AppModel` (estado + acciones), `Services` (KIE, OpenAI, Codex,
  credenciales), `Http` (WinHTTP), `Media` (último fotograma, concatenar), `Ui` (helpers de
  controles), y una página por sección: `CreatePage`, `StoriesPage`, `LibraryPage`,
  `SkillsGuidePages`, `PageCommon`, `MainWindow`.
- Los skills y assets se copian desde `macos/Sources/FramecraftCore/Resources` al compilar.

## Si la compilación falla

- **Toolset**: el proyecto usa `$(DefaultPlatformToolset)`; no fijes `v143`.
- **Errores de C++/WinRT**: los tipos usan los alias `mux` (Microsoft.UI.Xaml), `muxc` (Controls),
  `muxm` (Media). Para texto usá `hs(std::string)` → `hstring` y `str(hstring)` → UTF-8.
- **Coroutines**: los argumentos de eventos se toman por valor; volver al hilo de UI con
  `co_await wil::resume_foreground(dispatcher)`.
- No cambies versiones de paquetes en `packages.config` salvo que el error lo pida.
- Mantené los textos de la interfaz en español rioplatense (vos), como el resto de la app.
