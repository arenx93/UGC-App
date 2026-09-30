# Paso 4 — Generar en kie.ai (Seedance 2.5)

> Datos reunidos el 30/09/2026 de la doc de kie (docs.kie.ai), de la integración que ya usa la app
> Framecraft (`lib/server.ts`) y de guías públicas. La doc oficial no se pudo abrir desde el entorno
> donde se escribió esto: **la primera vez, abrí https://docs.kie.ai/market/bytedance/seedance-2-5
> y confirmá nombres de campos, duraciones y resoluciones.** Si algo cambió, corregí este archivo y
> `scripts/kie.py`.

## 1. Endpoints

| Qué | Método y URL |
|---|---|
| Subir archivo (referencias) | `POST https://kieai.redpandaai.co/api/file-stream-upload` (multipart: `file`, `uploadPath`, `fileName`) → `data.downloadUrl`. **Se borra a los 3 días.** |
| Crear tarea | `POST https://api.kie.ai/api/v1/jobs/createTask` |
| Estado | `GET https://api.kie.ai/api/v1/jobs/recordInfo?taskId=…` → `data.state` = waiting · queuing · generating · success · fail |
| Resultado | `data.resultJson` (string JSON) → `resultUrls[]`. **La URL expira (~24 h): descargar siempre.** |
| Error | `data.failCode`, `data.failMsg` |

Autenticación: `Authorization: Bearer $KIE_API_KEY`. `callBackUrl` es opcional (no se usa en local).

## 2. Cuerpo de createTask

```json
{
  "model": "bytedance/seedance-2-5",
  "input": {
    "prompt": "…",
    "reference_image_urls": ["…P1", "…LUGAR", "…ULTIMO_FRAME"],
    "reference_video_urls": ["…PANTALLA"],
    "reference_audio_urls": ["…VOZ_P1", "…VOZ_P2"],
    "aspect_ratio": "9:16",
    "resolution": "1080p",
    "duration": 25,
    "generate_audio": true,
    "return_last_frame": true
  }
}
```

| Campo | Valores | Nuestro preset |
|---|---|---|
| `duration` | 4–30 s (enteros) | 15–30 según el beat; 10 para reacciones |
| `resolution` | 480p · 720p · 1080p | **480p para probar** el guion/voces, **1080p** para el final |
| `aspect_ratio` | 21:9 · 16:9 · 4:3 · 1:1 · 3:4 · 9:16 | `9:16` |
| `generate_audio` | bool | `true` (diálogo con lip-sync) |
| `return_last_frame` | bool (no con draft) | `true` salvo la última escena |
| `first_frame_url` / `last_frame_url` | modo excluyente con referencias | **no se usan** (el primer frame va como `@Image3`) |

Omitir los arrays vacíos (no mandar `[]`).

## 3. Costos (orientativos, confirmar en la cuenta)

Kie cobra por segundo: ~US$0,14/s en 480p y ~US$0,32/s en 720p; 1080p ≈ 2,5× 720p. Un video
de 6 clips (~140 s generados) cuesta ≈ US$20 en 480p y bastante más en 1080p.
**Estrategia**: generar todo en 480p, revisar guion, voces y ritmo; regenerar en 1080p solo los
clips aprobados (mismo prompt). `kie.py crear` muestra el costo estimado y pide confirmación.

## 4. Con el script

```bash
export KIE_API_KEY=…                                      # nunca en archivos del proyecto
python3 scripts/kie.py revisar videos/grandpa/plan_escenas.json          # lint de todas las escenas
python3 scripts/kie.py crear   videos/grandpa/plan_escenas.json S2       # sube refs + crea tarea (pide confirmación)
python3 scripts/kie.py esperar videos/grandpa/plan_escenas.json S2       # espera, descarga clips/S2.mp4 y S2_last.jpg
python3 scripts/kie.py cadena  videos/grandpa/plan_escenas.json          # todas en orden, encadenando último frame
python3 scripts/kie.py subir   refs/voz_P1.mp3                           # solo subir, imprime URL
python3 scripts/kie.py frame   clips/S2.mp4 --t -0.4                     # extraer un frame (seg negativos = desde el final)
```

`plan_escenas.json`:
```json
{
  "proyecto": "grandpa-diner",
  "resolution": "480p",
  "aspect_ratio": "9:16",
  "refs": {
    "P1": "refs/P1_triptico.png",
    "LUGAR": "refs/diner_sheet.png",
    "EXTERIOR": "refs/parking_sheet.png",
    "PANTALLA": "refs/pantalla_10s.mp4",
    "VOZ_P1": "refs/voz_P1.mp3",
    "VOZ_P2": "refs/voz_P2.mp3"
  },
  "escenas": [
    {"id": "S1", "prompt": "prompts/S1.txt", "duration": 15,
     "images": ["P1", "LUGAR"], "audios": ["VOZ_P1", "VOZ_P2"], "return_last_frame": true},
    {"id": "S2", "prompt": "prompts/S2.txt", "duration": 25, "continua": "S1",
     "images": ["P1", "LUGAR"], "audios": ["VOZ_P1", "VOZ_P2"]},
    {"id": "S5", "prompt": "prompts/S5.txt", "duration": 30, "continua": "S4",
     "images": ["P1", "EXTERIOR"], "videos": ["PANTALLA"], "audios": ["VOZ_P1", "VOZ_P2"],
     "return_last_frame": false}
  ]
}
```
- `continua: "S1"` agrega el último frame de S1 al final de `images` (→ `@Image3` si hay 2 antes).
- Las rutas son relativas al JSON. Las URLs subidas se cachean en `.kie_cache.json` (3 días).
- El estado de cada escena (taskId, archivos) queda en `.kie_estado.json`.

## 5. Revisión de cada clip descargado (antes de seguir)

1. `ffprobe` → 9:16 y la resolución pedida (480×854 / 720×1280 / 1080×1920).
2. Mirarlo entero con audio. ¿Habla quien tiene que hablar? ¿Dice exactamente las líneas? ¿Lee
   acotaciones en voz alta? ¿La cara y la ropa coinciden con @Image1? ¿Aparece texto? ¿Manos raras?
3. `python3 scripts/transcribir.py clips/S2.mp4` y comparar con el guion: palabras cambiadas → se
   regenera o se corta la línea en montaje.
4. Si falla: cambiar **una** cosa por vez (una línea de dirección, una causa de cámara) y regenerar.
   Anotar qué se cambió en `guion.md`. No reescribir el prompt entero.

Fallas típicas y arreglo:
| Falla | Arreglo |
|---|---|
| Silencios largos | faltan palabras: subir al presupuesto; reforzar PACING |
| Lee la acotación | falta VOICE DIRECTION o la acotación no está entre paréntesis |
| Repite frases del audio de referencia | falta el bloqueo de palabras en CHARACTERS |
| P2 apagado | falta la línea "never sound bored…" |
| Cambia la cara entre clips | la biblia no es idéntica; tríptico poco claro; agregar "P1 looks exactly like @Image1" |
| Recompone el primer frame | falta la última frase del bloque FIRST FRAME |
| Se ve "de publicidad" | palabras prohibidas fuera de RESTRICTIONS; lente 1x lejos; luz perfecta |
| Texto/garabatos en pantalla | RESTRICTIONS incompleto; objetos con texto en el lugar (carteles) → sacarlos del model sheet |
| "Face detected" / rechazo | referencia demasiado fotográfica de una cara real: regenerar tríptico |
