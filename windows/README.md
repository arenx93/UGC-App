# Framecraft para Windows (C++)

App nativa para Windows 10 (1809 o posterior) y Windows 11, escrita en **C++20** con
**WinUI 3** (Windows App SDK) y C++/WinRT. Todas las pantallas se arman en C++: el único
archivo de marcado es `App.xaml`, que solo carga los estilos de Fluent.

Crea fotos y videos UGC con KIE (**GPT Image 2**, **Nano Banana Pro** y **Seedance 2.5**),
con el mismo asistente de prompts, las mismas skills y la misma sección de **Historias**
que la app de Mac.

## Descargar e instalar

1. En GitHub entrá a **Releases** y bajá `Framecraft-Setup-<versión>.exe` (instalador)
   o `Framecraft-<versión>-Windows-x64.zip` (portable).
   Las compilaciones de cada cambio también quedan en **Actions → Windows app (C++) → Artifacts**.
2. Abrí el instalador. **No pide permisos de administrador**: se instala solo para tu usuario.
3. La primera vez **Windows SmartScreen** puede avisar porque el instalador todavía no está
   firmado: tocá **Más información → Ejecutar de todas formas**.
4. Al abrir, pegá tu clave de KIE (queda en el **Administrador de credenciales de Windows**).

No hace falta instalar nada más: el runtime de Windows App SDK, el runtime de C++ y
**Codex CLI** vienen dentro de la app.

## Qué hay adentro

| Pantalla | Para qué |
|---|---|
| **Crear** | Imagen o video en tres pasos (idea, referencias, ajustes), plantillas de un clic, etiquetas @Image/@Video/@Audio y Ctrl+Enter para generar. La barra de generar flota sobre el contenido con material acrílico. |
| **Asistente de prompts** | Tu idea → un prompt listo siguiendo una *skill* (UGC de celular · Seedance 2.5, Arthas y Cachito, perfil JSON o las tuyas). Texto en vivo, ajustes y versiones anteriores. |
| **Revisión del prompt** | Bloques de tiempo, densidad de diálogo (2,47 palabras/s), etiquetas que no existen, lenguaje de anuncio y restricciones, mientras escribís. |
| **Historias** | Un brief → la historia en escenas, cada una con su prompt, de qué trata y el orden de referencias. Storyboard, **Generar todas las escenas** (encadenadas con el último fotograma) y **Armar video final** en un MP4, en tu PC y sin créditos. |
| **Biblioteca** | Todo lo generado, con filtros, búsqueda, favoritos, menú contextual y arrastrar al Explorador. |
| **Referencias** | Fotos, videos (≤30 s) y audios (≤30 s). Arrastralos desde el Explorador. |
| **Skills** y **Guía UGC** | Las skills incluidas, las tuyas (importar `SKILL.md`, `.md` o `.txt`) y el método del kit de Seedance. |
| **Buscar o hacer…** (Ctrl+K) | Ir a cualquier pantalla, usar una plantilla, abrir una historia o una creación. |

Diseño Windows 11: material **Mica**, barra de título integrada, navegación lateral de Fluent,
acento rosa de la marca, tema claro/oscuro, avisos de Windows cuando termina una generación y
progreso en la barra de tareas.

### Atajos

| | |
|---|---|
| Ctrl+K | Buscar o hacer… |
| Ctrl+Enter | Generar |
| Ctrl+Shift+Enter | Crear prompt con el asistente |
| Ctrl+N / Ctrl+Shift+N | Nueva imagen / nuevo video |
| Ctrl+Alt+N | Nueva historia |
| Ctrl+1 … Ctrl+6 | Ir a cada sección |

### Motores del asistente

| Motor | Qué usa |
|---|---|
| **KIE** | GPT‑5.6 Terra / Luna / Sol, GPT 5.2, Gemini 3.8 Flash, Gemini 3 Flash, Claude Opus 4.6, con tu clave de KIE. |
| **ChatGPT (Codex)** | Tu cuenta de ChatGPT. Codex viene incluido: solo iniciás sesión. |
| **OpenAI API** | GPT‑4.1 mini con tu clave de OpenAI (opcional). |

## Dónde se guarda todo

| Qué | Dónde |
|---|---|
| Biblioteca (índice) | `%LOCALAPPDATA%\Framecraft Studio\library.json` |
| Tus creaciones | `Imágenes\Framecraft\Generaciones` |
| Tus referencias | `Imágenes\Framecraft\Referencias` |
| Claves de KIE y OpenAI | Administrador de credenciales de Windows (`Framecraft/kie`, `Framecraft/openai`) |

## Desarrollo

```
windows/
  core/        Núcleo portable en C++20 (sin APIs de Windows): presets, prompts, revisión,
               skills, historias, plantillas y biblioteca. Se prueba en Linux y en Windows.
  app/         App WinUI 3: servicios (WinHTTP, credenciales, Codex, medios) y pantallas.
  installer/   Instalador de Inno Setup.
  scripts/     Descarga de Codex CLI para Windows.
```

Requisitos: Visual Studio 2022 o posterior con el workload de C++ de escritorio.

```powershell
# Núcleo y tests (cualquier sistema con CMake)
cmake -S windows/core -B build-core
cmake --build build-core --config Release
ctest --test-dir build-core -C Release

# App
nuget restore windows\app\packages.config -PackagesDirectory windows\packages
msbuild windows\app\Framecraft.vcxproj /p:Configuration=Release /p:Platform=x64
windows\build\Release\Framecraft.exe
```

`Framecraft.exe --snapshot <carpeta>` renderiza todas las pantallas con datos de ejemplo
(lo usa GitHub Actions para las capturas de `docs/windows/`).

### Publicar una versión

En **Actions → Windows app (C++) → Run workflow** escribí la versión en *release*
(por ejemplo `1.0.0`): compila, prueba y publica la Release con el instalador y el `.zip`.
