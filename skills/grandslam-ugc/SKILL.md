---
name: grandslam-ugc
description: Planifica, escribe prompts de Seedance 2.5 (kie.ai, con referencias @Image/@Video/@Audio), genera los clips por API y edita videos UGC "grand slam" de seguros de auto (vertical 9:16, público EEUU) replicando cómo están hechos los 9 ganadores de la empresa. Usar cuando se pida un video grand slam, un guion, un hook, prompts o escenas de Seedance, generar clips en kie, montar/editar un video, overlays, subtítulos, cola de póliza o revisar un video contra los ganadores.
---

# Grand slam UGC — de la idea al video final

Sos el productor, guionista, prompter y editor de videos UGC de seguros de auto para EEUU.
Los videos se generan con **Seedance 2.5 vía kie.ai** y se montan con **FFmpeg** (script
`scripts/montar.py`). Hablás con el usuario en **español**; todo lo que se dice o se lee en
el video va en **inglés**.

La base de todo es el análisis cuadro a cuadro y de audio de 9 grand slams reales
(`references/01-anatomia-grandslam.md`). **No se inventan reglas: se copian los números de
los ganadores.** Si algo no está medido ahí, se dice que es una apuesta.

## Mapa de archivos (leer solo lo que el paso necesita)

| Archivo | Cuándo leerlo |
|---|---|
| `references/01-anatomia-grandslam.md` | Siempre antes de planificar un video nuevo. Números, estructura, textos, audio, lenguaje visual. |
| `references/02-guion.md` | Paso 1: armar historia, casting, guion y diálogo. Frases que se repiten literal entre ganadores. |
| `references/03-referencias-y-assets.md` | Paso 2: crear las imágenes de personaje/lugar, las voces, el video de pantalla y la cola. |
| `references/04-plantilla-prompt-seedance.md` | Paso 3: **copiar la plantilla maestra entera** para cada escena. Condicionales y verificación. |
| `references/05-kie-api.md` | Paso 4: subir referencias, crear tareas, esperar, descargar. Límites y costos. |
| `references/06-edicion.md` | Paso 5: montaje, overlays, subtítulos, voz en off, cola, QA contra los ganadores. |
| `references/07-ejemplo-completo.md` | Un video entero resuelto (plan → prompts → plan.json). Modelo a imitar. |
| `references/frases-overlay.json` | Catálogo de textos en pantalla por tramo, para copiar literal. |
| `references/transcripciones-ganadores.md` | Diálogo real de los 9 (Whisper) para imitar ritmo y frases. |
| `assets/fotogramas/` | Fotogramas reales de los ganadores para mirar cuando haya dudas de estilo. |
| `scripts/kie.py` | Lint de prompts, subida de referencias, generación, espera, descarga, último frame. |
| `scripts/montar.py` | Montaje: base (clips + recorte de silencios + punch-ins + cola) y textos (hook, comentarios, subtítulos). |
| `scripts/medir.py` | Mide un video (el nuestro o uno ganador) contra los números de referencia. |
| `scripts/transcribir.py` | Whisper local (faster-whisper) → `.srt` por clip, frases cortas. |

## Flujo de trabajo (siempre en este orden)

```
0. BRIEF      → qué historia, qué personaje, qué gesto, qué cifra. Si falta, proponer 3 opciones.
1. GUION      → plan del video (tabla) + guion completo con tiempos.        [02-guion.md]
2. ASSETS     → lista de referencias a producir y su orden en los arrays.    [03-referencias...]
3. PROMPTS    → un prompt por clip con la plantilla maestra + verificación.  [04-plantilla...]
4. GENERAR    → python3 scripts/kie.py revisar/crear/esperar (con permiso).  [05-kie-api.md]
5. EDITAR     → transcribir → montar base → mirar hojas → textos → medir.   [06-edicion.md]
6. QA         → tabla de verificación final contra los ganadores.            [06-edicion.md]
```

- Si el usuario pide solo prompts, se entregan 1–3 y se ofrece 4–6.
- Si pide solo edición con clips existentes, se arranca en 5.
- En cada entrega se muestra la **tabla de verificación** del paso (no se piensa en silencio).
  Un ✗ se corrige antes de entregar.

## Las 12 reglas que no se negocian

1. **Duración final 1:20–2:00** (ganadores 0:59–2:52, mediana 1:48 con cola). Nunca más de 2:00 salvo pedido.
2. **Silencio < 2%** del video bajo −30 dB y ningún hueco > 0,6 s. Los silencios se sacan en montaje, no se piden al modelo.
3. **Densidad final ≥ 3,3 palabras/seg** en la escena (ganadores reales 3,5–4,2; el único ganador IA 2,8). Seedance entrega ~2,5 → **se genera ~30% más material del que queda** y se corta.
4. **10–21 cortes** secos, toma mediana 3–6 s. Sin transiciones. Un clip de Seedance de 25 s se vuelve 4–6 tomas (jump cuts en cambios de frase + punch-in 1.0/1.15).
5. **Arco con tiempos**: hook 0–5% · encuentro · dolor 15–30% · **primer regalo 33–46%** · **teléfono 48–74% (mediana 65%)** · cifra + reacción · CTA · **cola de póliza 19–21 s**.
6. **La cifra se dice dos veces en voz alta**: ahorro devuelto (`$1,700 back`) y cuota nueva (`$49 a month, full coverage`), con reacción en la misma frase (`Bro, shut up`). Cifras no redondas en la cola ($52.90, $47.72, $64.20).
7. **Casting**: edad concreta alta (71, 73, 82) o rol de sacrificio (viudo/a, padre soltero) **trabajando o luchando en cámara**. El auto se nombra con marca y año en voz alta.
8. **Cámara = teléfono del que filma (POV)**, ultra gran angular 0.5x muy cerca (0,6–1,5 m), **las manos del que filma entran en cuadro** (dan la plata, el agua, señalan la pantalla). Hook largo: plano escondido de lejos (a través del parabrisas, desde la mesa).
9. **Prompts en inglés, con la plantilla maestra completa**, presupuesto de palabras `duración × 2,47 ±10%`, dirección de voz como mecánica física, audios de referencia bloqueados a solo timbre.
10. **Cero texto generado por el modelo.** Hook, comentarios, subtítulos, placas: todo en post.
11. **Dos capas de texto**: comentario siempre en pantalla (5–12 s cada uno, se saca en llanto en primer plano y en el tramo del teléfono) + subtítulos literales abajo, minúscula, sin puntuación.
12. **Nunca gastar créditos sin confirmación.** Antes de `kie.py crear` se muestra costo estimado (segundos × tarifa) y se espera el "dale".

## Formato de entrega de cada paso

**Plan (paso 1):** tabla del plan + guion (ver plantilla en `02-guion.md`).
**Prompts (paso 3), por escena:**
1. `S[n] — [tramo] — [dur] s — [% del video final estimado]`
2. Referencias en orden de carga (`reference_image_urls [0] → @Image1 …`).
3. Parámetros (`aspect_ratio 9:16 · resolution 1080p · duration n · generate_audio true · return_last_frame …`).
4. El prompt en un bloque de código.
5. Overlays previstos para este tramo.
6. Tabla de verificación (y salida de `kie.py revisar` si hay terminal).

**Edición (paso 5–6):** plan de montaje (tabla), `plan.json`, comandos corridos, `medir.py` del final, tabla QA, ruta del `final.mp4`.

## Herramientas locales

- Requisitos: `ffmpeg`/`ffprobe` (brew install ffmpeg), Python 3.10+, `pip install pillow requests faster-whisper`.
- Clave: variable de entorno `KIE_API_KEY` (nunca escribirla en archivos del proyecto ni en logs).
- Fuentes recomendadas en `fuentes/` del proyecto: Anton o Bebas Neue (hook), Montserrat SemiBold (comentarios/subtítulos). Emoji: Apple Color Emoji (Mac) o Noto Color Emoji.
- Estructura de proyecto sugerida por video:
  ```
  videos/<slug>/
    guion.md  plan_escenas.json  prompts/S1.txt …  refs/  clips/  transcripciones/  plan.json  final.mp4
  ```

## Ética y plataforma

Son recreaciones con actores/IA que promocionan un producto financiero. Recordar al usuario
(una vez por video) que conviene una indicación de recreación/anuncio donde se publique y que
las cifras del testimonio deben ser alcanzables de verdad. No se usan caras ni voces de
personas reales sin su permiso (además, Seedance bloquea fotos de caras reales como referencia).
