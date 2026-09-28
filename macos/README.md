# Framecraft para macOS (Swift)

App nativa de macOS, escrita 100 % en Swift y SwiftUI, para crear fotos y videos
UGC con KIE (GPT Image 2, Nano Banana Pro y Seedance 2.5). Requiere macOS 14
Sonoma o posterior. Funciona en Macs con Apple Silicon y con Intel.

![Crear un video con la skill UGC y la revisión del prompt](../docs/macos/02-crear-video.png)

| Biblioteca | Bienvenida |
|---|---|
| ![Biblioteca](../docs/macos/05-biblioteca-oscuro.png) | ![Bienvenida](../docs/macos/09-bienvenida.png) |

Más capturas en [`docs/macos/`](../docs/macos/) (se regeneran desde Actions → *macOS app (Swift)* → *Run workflow* con “screenshots”).

## Descargar

1. En GitHub, abrí **Actions → macOS app (Swift)** y entrá en la ejecución más
   reciente con ✅.
2. En **Artifacts**, descargá **Framecraft-macOS** (trae el `.dmg`).
3. Abrí el `.dmg` y arrastrá **Framecraft** a **Aplicaciones**.
4. La primera vez: **clic derecho → Abrir → Abrir** (la app todavía no está
   firmada con un certificado de Apple). Si macOS dice que "está dañada":
   `xattr -cr /Applications/Framecraft.app` en Terminal.

## Qué hay adentro

| Pantalla | Para qué |
|---|---|
| **Crear** | Tres pasos: 1) describí tu idea, 2) referencias, 3) ajustes. Imagen o video, con vista previa del prompt final y ⌘↩ para generar. |
| **Asistente** (panel lateral, ⌥⌘I) | Convierte una idea suelta en un prompt listo, siguiendo una *skill*. Podés pedirle ajustes ("más corto", "cambiá la locación") sin empezar de cero. |
| **Revisión del prompt** | En video, chequea en vivo el checklist del kit: bloques de tiempo, densidad de diálogo (2,47 palabras/s), etiquetas @Image/@Video/@Audio que no existen, lenguaje de anuncio, restricciones y audios de referencia. |
| **Biblioteca** | Todo lo generado, con filtros, búsqueda, favoritos, Vista rápida (espacio), arrastrar a Finder, reusar ajustes y **continuar desde el último fotograma** para encadenar clips. |
| **Referencias** | Fotos, videos (≤30 s) y audios (≤30 s). El orden de selección define @Image1, @Image2… Se pueden arrastrar desde Finder a cualquier parte de *Crear*. |
| **Skills** | Las incluidas y las tuyas. Importá cualquier `SKILL.md` (con encabezado YAML `name`/`description`), `.md` o `.txt`, o escribí una nueva. |
| **Guía UGC** | El método del kit de Seedance para leer dentro de la app, más el PDF base y el pack de Walter. |

### Skills incluidas

- **UGC de celular · Seedance 2.5** — la skill `prompts-ugc-celular` del kit,
  con su *knowledge* (parámetros de kie.ai, video largo y arco narrativo), las
  lecciones del pack de Walter y la regla de idioma. Opcional: sumar el pack
  completo de Walter como ejemplo (más preciso, usa más tokens).
- **Arthas y Cachito · Omni / Veo 3.1** — la skill de la saga con su elenco,
  lore y filtros.
- **Perfil JSON + biblioteca GPT Image 2** — la que ya tenía la versión web.

## Tus datos

- Creaciones y referencias: **Imágenes ▸ Framecraft** (`~/Pictures/Framecraft`).
- Índice de la biblioteca: `~/Library/Application Support/Framecraft Studio/library.json`.
- Claves de KIE y OpenAI: en el **Llavero** de macOS.

## Desarrollo

Abrí `macos/Package.swift` con Xcode (Archivo ▸ Abrir…) y ejecutá el esquema
**Framecraft**. O desde Terminal:

```bash
cd macos
swift test                 # tests del núcleo
swift run Framecraft       # abre la app
scripts/build-app.sh       # genera build/Framecraft.app y el .dmg
```

| Carpeta | Qué es |
|---|---|
| `Sources/FramecraftCore` | Lógica sin interfaz: presets, clientes de KIE/OpenAI, asistente, skills, revisión del prompt. |
| `Sources/FramecraftCore/Resources/Skills` | Las skills y el material del kit. Para actualizar una skill, reemplazá su `SKILL.md`. |
| `Sources/Framecraft` | La app SwiftUI (`AppModel.swift` coordina todo; `Views/` las pantallas). |
| `Tests/FramecraftCoreTests` | Tests; `Fixtures/web-parity.json` sale del código TypeScript de la versión web para verificar que el port da lo mismo. |

`Framecraft --snapshot <carpeta>` genera capturas de todas las pantallas con
datos de ejemplo (lo usa CI).

### Firmar y notarizar (opcional)

Con una cuenta de Apple Developer:
`SIGN_IDENTITY="Developer ID Application: …" scripts/build-app.sh` y después
`xcrun notarytool submit build/Framecraft-*.dmg --wait` + `xcrun stapler staple`.
