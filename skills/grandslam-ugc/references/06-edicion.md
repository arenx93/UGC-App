# Paso 5–6 — Edición y QA

El montaje hace la mitad del grand slam: Seedance entrega tomas continuas a ~2,5 palabras/seg; los
ganadores tienen 3,5–4 pal/seg, 10–21 cortes y texto en pantalla todo el tiempo. Todo eso se hace acá.

Herramienta: `scripts/montar.py` (FFmpeg + Pillow). No se usa CapCut.

## 1. Orden de trabajo

1. **Juntar**: `clips/S1.mp4 … Sn.mp4`, la cola (video o imagen), voces en off si hay, fuentes.
2. **Transcribir**: `python3 scripts/transcribir.py clips/` → `transcripciones/Sn.srt`. Sin `.srt`
   no hay subtítulos (nunca se inventan tiempos). Revisar nombres y cifras en los `.srt`.
3. **Elegir tramos** leyendo cada `.srt` (+ mirar el clip): qué frases quedan, dónde cortar.
   Reglas en §2. Escribir el `plan.json` (§5).
4. `python3 scripts/montar.py base plan.json` → `base.mp4` + `base.srt` (subtítulos ya movidos a la
   línea de tiempo final) + marcas de placa y cola.
5. `python3 scripts/montar.py hoja base.mp4` y **mirar las hojas**: caras, llanto en primer plano,
   abrazo que tapa el lente, tramo del teléfono, fin de la escena. Recién con eso se fijan los tiempos
   de hook y comentarios (línea de tiempo de `base.mp4`).
6. `python3 scripts/montar.py textos plan.json` → `final.mp4`.
7. `python3 scripts/medir.py final.mp4 --srt base.srt --cola <placa+cola> --gesto <s> --telefono <s>`.
8. `python3 scripts/montar.py hoja final.mp4`, mirar, completar la tabla QA (§7). Un ✗ se corrige y se
   vuelve a renderizar.

## 2. Cómo cortar un clip de Seedance

- **Arranque**: sacar el primer 0,3–1 s (el modelo "arranca" el movimiento) salvo en S1.
- **Final**: cortar apenas termina la última palabra útil; el clip siguiente arranca en acción.
- **Jump cuts** en cambios de hablante o de frase donde la cara cambia poco. Cada 3–6 s.
- **Punch-in**: alternar tramos con `zoom` 1.0 y 1.12–1.2 del mismo clip; simula el "corte más cerca"
  de los ganadores y esconde el salto. `ancla_y` 0.3–0.4 para mantener la cara.
- **Líneas malas**: si el modelo cambió una palabra, dijo una acotación o se trabó, se saca el tramo.
  Si era una cifra clave, se regenera el clip (no se tapa con texto).
- **Ráfaga de acción** (comprar algo, cargar bolsas, abrir el capó): 3–5 tramos de ~2 s.
- **Saltos de lugar/tiempo**: corte seco, sin placa.
- **Tramo del teléfono**: primeros planos de pantalla intercalados con la cara, 15–30 s en total.
- `recortar_silencios: true` (−30 dB, 0,3 s) saca los huecos que quedan; revisar que no coma
  respiraciones importantes (subir `silencio_min` a 0.4 si pasa).

Objetivo de la escena: **≥ 3,3 pal/seg**, **10–21 cortes** en todo el video, **toma mediana 3–6 s**.

## 3. Textos en pantalla

Dos capas a la vez, sin taparse: **comentario** (y ≈ 0,40–0,60) y **subtítulos** (y ≈ 0,78).

**Hook** (`hook`): corto 3–5 s o largo 11–13 s (el plano observacional con narración).
Estilo por defecto `contorno` (condensada MAYÚSCULAS blanca, contorno negro), 3 líneas, la línea del
gancho `amarilla`, emojis ❤️🥺 en línea aparte. Alternativa `caja_negra`. `y` 0,50–0,60 si hay aire,
0,20–0,30 si abajo hay tablero/manos/mesa. Nunca sobre la cara.

**Comentarios** (`comentarios`): siempre hay uno, se reemplaza cada 5–12 s sin hueco, 4–8 por video.
Se sacan en el primer plano de llanto y en el tramo del teléfono; van encima del abrazo que tapa el
lente. Estilos: `contorno` (por defecto), `caja_blanca` (emoji adentro), `caja_negra` (peso y CTA).
Frases literales en `frases-overlay.json`. Errores de tipeo y la "i" minúscula no se corrigen.

**Subtítulos**: salen del `.srt`; `montar.py` los mueve a la línea final, los deja hasta la frase
siguiente y los pasa a minúscula sin puntuación. Blancos por defecto; `"color_subs": "amarillo"` si
hay mucho texto. Sin subtítulos sobre la narración del hook largo si va por `voz_en_off`, ni sobre la cola.

**Cierre**: CTA 5–9 s sobre la última reacción real (`caja_negra` o `contorno`). Placa opcional
`placa_cta` ~1,5 s. Después, la cola sin textos.

Fuentes: `fuentes/Anton-Regular.ttf` (o Bebas Neue) para hook, `fuentes/Montserrat-SemiBold.ttf`
para comentarios/subtítulos. Sin ellas el script usa DejaVu/Arial y hay que avisar (se ve distinto).

## 4. Audio

- La escena queda con su audio (diálogo generado).
- **Narración del hook** generada dentro de S1 → no hace falta nada. Si es un archivo aparte:
  `voz_en_off` con `start` 0.3 y `duck` 0.25.
- **Testimonio de la cola**: `voz_en_off` `start: "cola"`, `duck: 0` (silencia el audio del clip de
  la cola). La cifra del testimonio = la de la cola.
- Loudness final −16 LUFS (lo hace `textos`). La cola queda 3–10 dB más baja si el VO es más suave:
  está bien, así son los ganadores.
- Sin música.

## 5. plan.json

```json
{
  "clips": [
    {"file": "clips/S1.mp4", "srt": "transcripciones/S1.srt", "in": 0, "out": 14.6},
    {"file": "clips/S2.mp4", "srt": "transcripciones/S2.srt",
     "tramos": [[0.4, 7.2], [7.2, 11.0, 1.15], [11.0, 19.1], [19.1, 24.8, 1.18]]},
    {"file": "clips/S3.mp4", "srt": "transcripciones/S3.srt", "zoom": 1.1, "in": 0.5, "out": 24.7}
  ],
  "recortar_silencios": true,
  "silencio_db": -30,
  "silencio_min": 0.3,
  "placa_cta": {"text": "If you pay more than $80mo check below ❤️🥺", "dur": 1.5},
  "cola": {"file": "cola/cola_52.90.mp4", "in": 0, "out": 20},
  "voz_en_off": [
    {"file": "vo/testimonio_52.90.mp3", "start": "cola", "duck": 0.0}
  ],
  "base": "base.mp4",

  "hook": {"text": "74YO STILL DELIVERING GROCERIES\nTO RAISE HER GRANDSON\nSO I DID THIS FOR HER",
           "amarilla": [2], "emojis": "❤️🥺", "start": 0, "end": 11, "y": 0.28, "estilo": "contorno"},
  "comentarios": [
    {"text": "Watch this...", "start": 11, "end": 19},
    {"text": "This broke my heart", "start": 19, "end": 28}
  ],
  "color_subs": "blanco",
  "fuentes": {"hook": "fuentes/Anton-Regular.ttf", "texto": "fuentes/Montserrat-SemiBold.ttf"},
  "salida": "final.mp4"
}
```

- `in`/`out`: segundos del clip de origen. `tramos`: `[in, out]` o `[in, out, zoom]` → un jump cut por
  tramo, en orden. `zoom` > 1 = punch-in; `ancla_y` (0–1) dónde queda el recorte vertical.
- El `.srt` es el del archivo completo: el script recorta y corre los tiempos solo. Una frase partida
  por un corte queda en el tramo donde cae la mayor parte.
- `cola`: `{"file", "in", "out"}` (video) o `{"imagen": "poliza.png", "dur": 20}` (simula mano sobre
  una foto; menos real).
- `voz_en_off.start`: segundos de `base.mp4` o `"cola"`, `"placa"`, `"escena_fin"`. `duck` = volumen de
  la escena debajo. `vol` = volumen de la voz.
- Tiempos de `hook` y `comentarios`: línea de tiempo de `base.mp4` (mirar la hoja).
- `subtitulos`: omitido = `base.srt`; ruta = ese `.srt`; `""` = sin subtítulos.
- Corregiste un `.srt` de clip → `python3 scripts/montar.py srt plan.json` rehace `base.srt` sin renderizar.
- `\n` fuerza salto de línea; `amarilla` cuenta esas líneas desde 0. `size` opcional (hook 78, textos 60).

## 6. Reciclar un ganador en formato noticiero (opcional)

No está automatizado. Con FFmpeg: fondo negro 1080×1920, titular sentence case arriba (y 12–26%), el
video recortado a ~50% del ancho en la banda central (y 27–73%), tag rojo "VIRAL STORIES" + barra
blanca con texto negro MAYÚS abajo, voz de locutor en off ("This video went viral on social media…").
Usarlo solo sobre un video que ya funcionó.

## 7. QA final (se entrega escrita)

| # | Punto | ✓/✗ | Dato |
|---|---|---|---|
| 1 | 1080×1920, 30 fps | | medir.py |
| 2 | Duración 1:20–2:00 (o la pedida) | | |
| 3 | Silencio < 2% y hueco máx ≤ 0,6 s (sin placa/cola) | | medir.py |
| 4 | Densidad ≥ 3,3 pal/seg | | medir.py --srt |
| 5 | 10–21 cortes, toma mediana 3–6 s (contar jump cuts del plan) | | |
| 6 | Hook visible desde el frame 0, sin tapar caras | | hoja |
| 7 | Siempre hay comentario, salvo llanto y teléfono | | hoja |
| 8 | Comentario y subtítulos no se pisan | | hoja |
| 9 | Subtítulos en sincronía (2 frames al azar vs `.srt`) | | |
| 10 | Emojis dibujados (no cuadrados) | | hoja |
| 11 | Gesto al 33–46% y teléfono al 48–74% | | medir.py --gesto --telefono |
| 12 | Cifra dicha dos veces (ahorro + cuota) con reacción | | `.srt` |
| 13 | CTA sobre la última reacción; cola sin texto, con testimonio audible | | |
| 14 | Cifra de la cola no redonda y legible | | |
| 15 | −16 LUFS ±1 | | medir.py |

Entregar `final.mp4` + la tabla + una línea de contexto. Si se publica como hecho real, sugerir una
indicación de recreación/anuncio.
