# Paso 3 — Plantilla maestra de prompt (Seedance 2.5)

## Cómo se usa (leer primero)

1. **Cada escena se arma copiando la plantilla entera** y rellenando los `[CORCHETES]`. Nunca de
   memoria ni copiando la escena anterior (arrastra errores).
2. **Ningún bloque se borra**, salvo los `[CONDICIONAL]` según la tabla de la sección 3.
3. Los bloques de la **biblia de continuidad** (CHARACTERS, LOCATION, IMAGE, CAMERA base) se
   escriben una vez en S1 y se copian **palabra por palabra** en todas las escenas. Cambiar un
   descriptor entre escenas es la causa documentada de deriva de personaje.
4. Toda entrega termina con la **VERIFICACIÓN** (sección 5) respondida y con `kie.py revisar`.
5. En conversaciones largas, releer este archivo antes de cada escena nueva.

Por qué funciona cada regla: sección 6.

## 1. Presupuesto de palabras

Seedance habla a **~2,47 palabras/seg** (medido en salidas reales). Solo cuentan las palabras
**dentro de comillas**. Tolerancia ±10%.

| Duración | 10 s | 15 s | 20 s | 25 s | 30 s |
|---|---|---|---|---|---|
| Palabras | ~25 | ~37 | ~49 | ~62 | ~74 |

Con menos, el modelo rellena con silencio (42 palabras en 30 s devolvieron 13 s de aire). Con
más, atropella. Líneas de **≤ 12 palabras**; cifras **en palabras** ("forty-nine a month").

## 2. PLANTILLA MAESTRA

```
FORMAT: Vertical 9:16 smartphone video, [DURATION] seconds. One continuous handheld phone
recording of a real moment, filmed by someone who saw something happening and raised their
phone. This is real footage, not an advertisement and not a production imitating UGC.

[CONDICIONAL — FIRST FRAME]
FIRST FRAME — MANDATORY: @Image3 IS the first frame of this video. Frame 1 must be @Image3
itself, pixel for pixel: identical framing, composition, camera position and lens, lighting and
color, subject positions, background. Do not reinterpret it, do not recompose it, do not change
the distance or the angle. The video begins ON that image and all motion grows out of it. Treat
@Image3 as the locked starting state of the scene, not as a style reference.

VOICE DIRECTION: Text in parentheses before a spoken line is acting direction for delivery only —
emotion, tone, pace, breath and volume. It is NEVER spoken aloud. Only the words inside the
quotation marks are ever said.

PACING — CRITICAL: This clip is DENSE with dialogue. Someone is speaking for almost the entire
clip. Characters answer immediately, often overlapping the last word of the previous speaker. The
gap between one line ending and the next beginning is about half a second — a breath, never a
hold. There is NO moment in this clip where nobody speaks for longer than one second. Do not
stretch the delivery to fill time and do not insert reflective silences.
[CONDICIONAL — ESCENA EMOCIONAL] The emotion lives in HOW the words are said, not in the space
between them. No long pause to compose himself — he talks through the emotion.

POV: The camera IS P2's phone. P2 is never seen except his hands and forearms, which enter the
frame naturally when he hands something over, points or gestures. P2's voice is louder and closer
than everyone else's because the phone and its microphone are in his hand.

CHARACTERS AND VOICES:
P1 — [look from @Image1: exact age, ethnicity, build, face, hair]. Wearing [wardrobe item by item,
with colors]. [One line of character, e.g. "a proud man who does not want pity and does not want to
be a burden"]. P1 looks exactly like @Image1 for the entire clip.
P1's voice: match @Audio1 for timbre, age, accent and pitch only. Do not reproduce any words from
@Audio1 and do not copy its emotion — the emotion of every line is set by the direction in
parentheses.
P2 — the person filming, [age, ethnicity, visible only as hands: e.g. "brown skin, a plain black
watch on the left wrist, grey hoodie sleeves"]. Voice: match @Audio2 for timbre, age, accent and
pitch only. Do not reproduce any words from @Audio2 and do not copy its emotion. P2 sounds warm,
friendly and fully engaged, with real life and affection in his voice and an audible smile behind
most of what he says. He must NEVER sound bored, tired, monotone, detached or like he is reading.
[CONDICIONAL — P3] P3 — [look]. P3 speaks with [voice described in words: age, pitch, accent,
energy]. No audio reference.

LOCATION: [from @Image2: place, layout, objects, what is visible in the background, time of day].
The location matches @Image2.

CAMERA: Rear phone camera on the 0.5x ultra-wide lens (about 13–16 mm equivalent) with visible
barrel distortion at the edges, held in one hand at chest height, 0.6 to 1.5 m from P1, so his face
and upper body are large in frame and the background falls away. The arm breathes and trembles
slightly. The operator reframes half a second LATE, always after the action, never anticipating it.
Small pans of a few degrees follow the bodies. The horizon drifts and is not corrected. Natural
motion blur on fast movements. Deep depth of field: the background is fully readable.
[Scene-specific physical causes: e.g. "when P1 stands up the operator is late and cuts the top of
his head for a moment"; "P2's hand holding the bills enters from the bottom right at 12 s and
partially covers the lower frame"].

IMAGE: Practical light from the location only: [sources, e.g. hard midday sun from above with a
bright, almost blown-out sky | ceiling fluorescent panels and window daylight]. Mixed color
temperatures are left uncorrected. Auto-exposure visibly adjusts when [cause, e.g. the phone turns
toward the bright window]. Medium contrast, moderate saturation, natural smartphone color science,
realistic skin texture with pores, wrinkles and sweat, no color grading.

AUDIO: Diegetic sound only, recorded by the phone microphone: dialogue with room tone and natural
distance, plus [location ambience: e.g. distant traffic, a car door, wind hitting the mic | cutlery,
low chatter, kitchen noise]. All dialogue is in English. No music, no score, no added sound effects.

ACTING: Nobody poses. Nobody looks into the camera seeking approval. Reactions have latency: P1
sees → does not fully understand → checks with the other person → only then reacts. Emotion lives
in micro-expressions — eyes, jaw, breathing — not in big gestures or pauses.

ACTION:
0–[X] s — [physical action]. P2 ([delivery mechanics]): "[line]"
[X]–[Y] s — [physical action]. P1 ([delivery mechanics]): "[line]"
... (each block's length = real speaking time of its words, ~2.5 words per second; a new physical
beat every 3–6 s so the clip can be cut)

RESTRICTIONS: No on-screen text, no captions, no subtitles, no titles, no logos, no watermarks. No
music. [No cuts, no transitions, one continuous take. | CONDICIONAL — recording break]. No cinematic
look, no shallow depth of field, no bokeh, no gimbal, no steadicam, no dolly, no slider, no crane,
no drone, no orbit, no dramatic push-in, no rack focus, no teal and orange, no film grain, no LUT,
no halation, no anamorphic flare, no beauty filter, no professional color grade, no studio lighting,
no rim light, no slow motion. Faces and wardrobe stay identical for the whole clip. Hands have five
fingers and hold objects naturally. Nobody says any word that is not inside quotation marks.
```

## 3. Condicionales y bloques opcionales

| Situación | Qué hacer |
|---|---|
| Primera escena (S1) | Sin FIRST FRAME. Sin último frame en el array. |
| Escena que continúa la anterior (mismo lugar y momento) | FIRST FRAME + último frame previo como `[2]` → `@Image3`. |
| Salto de locación o de tiempo | Quitar FIRST FRAME y poner **SALTO**. Sin último frame. |
| Hook largo observacional | Reemplazar CAMERA y PACING por **HOOK OBSERVACIONAL**. |
| Tiempo interno (comer, esperar) | Bloque **RECORDING BREAK** + ajustar RESTRICTIONS. |
| Escena emocional | Las dos líneas del CONDICIONAL de PACING. |
| Se entrega plata / un objeto | Bloque **ENTREGA POV** en ACTION. |
| Aparece la pantalla del teléfono | Bloque **PANTALLA** en ACTION + `reference_video_urls[0]`. |
| Tercer personaje | Bloque P3 con voz descrita en palabras. |
| Última escena | `return_last_frame: false`. Cualquier otra: `true`. |

**SALTO** (reemplaza FIRST FRAME):
```
NEW MOMENT: This clip starts in a new moment: [where / when]. Build this opening from the
description. Do NOT continue any previous framing.
```

**HOOK OBSERVACIONAL** (S1 largo; reemplaza CAMERA y PACING):
```
CAMERA: P2 is filming secretly from [inside his parked car through the windshield, the dark hood
and the dashboard edge visible at the bottom of the frame | a restaurant table, the edge of the
table and a paper receipt in the foreground]. The phone is on 2x digital zoom, so the image is
slightly soft and noisy. P1 is small in frame, [15–25] m away, [doing the work: collecting cans into
a shopping cart]. The phone is almost still — resting hand, tiny drifts — and P1 is never centered
perfectly. At [X] s P2 opens the door / stands up and starts walking toward P1: the frame bounces
with each step and the zoom drops to 1x.
PACING: P2 narrates in a low, close, half-whispered voice the whole time, as someone who does not
want to be noticed. P1 is too far to be heard; only ambience reaches the mic from P1's side.
```

**RECORDING BREAK** (después de FORMAT; en RESTRICTIONS reemplazar "No cuts, no transitions, one
continuous take" por "Apart from the single recording break, no cuts and no transitions"):
```
STRUCTURE — ONE RECORDING BREAK: This clip contains exactly ONE hard jump cut, at [X] seconds. It
is not an edit — it is the person filming stopping the recording and starting it again. Render it
as an abrupt frame-to-frame jump with no transition, no fade, no dissolve and no motion match. The
audio jumps with it.
```

**ENTREGA POV** (en el bloque ACTION donde se da algo):
```
[X]–[Y] s — P2's own hand enters the frame from the bottom right holding [five crumpled $100 bills
folded once]. He pushes them toward P1's hands. P1 does not take them at first: his hands stay
half-raised, palms open, and he looks from the bills to P2 and back. The camera is late and tilts
down to follow the hands.
```

**PANTALLA** (en ACTION donde aparece el teléfono):
```
[X]–[Y] s — Extreme close-up: P1 holds his own phone at chest height; the camera is 20–30 cm from
the screen. P2's index finger enters and taps and scrolls the screen. The phone screen shows
@Video1. Use @Video1 ONLY as the content of that screen, inset in the phone, following the phone's
angle, with screen glare, a faint reflection of P1's face on the glass and focus that hunts for a
moment. Do NOT let @Video1's style, color grading, framing, camera motion, pacing or subject matter
influence the rest of the shot in any way. Any audio belonging to @Video1 is NOT heard — the phone
screen plays silently.
```

**CÁMARA FRONTAL** (reemplaza CAMERA; raro en este formato):
```
CAMERA: Front phone camera, arm extended, slight wide-angle distortion, framing just above the
eyes, tilted composition. The arm gets tired and the frame slowly drops. [Resto de causas igual.]
```

## 4. Dirección de voz: cómo se escribe

Mecánica física, nunca adjetivos sueltos ("emotional", "sad", "happy" prohibidos solos):
```
P1 (quiet, flat, stripped of everything — the sentence of a man who has said it many times and has
never made it easier, the last word almost inaudible): "My daughter passed away."
P1 (a short disbelieving laugh first, voice cracking upward on the last word): "Bro, shut up."
P2 (leaning in, lowering his voice like sharing a secret, smiling): "Just pull it out for me."
```

## 5. VERIFICACIÓN (se entrega escrita en cada prompt)

```
VERIFICACIÓN S[n]
| # | Punto                                                                  | ✓/✗ | Dato |
|---|------------------------------------------------------------------------|-----|------|
| 1 | Palabras entre comillas vs presupuesto (dur × 2,47 ±10%)                |     | n / objetivo |
| 2 | Cada bloque de ACTION ≈ sus palabras ÷ 2,5 s (±1 s)                     |     | lista |
| 3 | Bloques presentes: FORMAT, VOICE DIRECTION, PACING, POV, CHARACTERS,   |     |      |
|   | LOCATION, CAMERA, IMAGE, AUDIO, ACTING, ACTION, RESTRICTIONS            |     |      |
| 4 | Condicionales aplicados (cuáles)                                        |     |      |
| 5 | Biblia idéntica a S1 (P1, ropa, P2 manos, LOCATION, IMAGE, CAMERA base) |     |      |
| 6 | Dirección entre paréntesis = mecánica física                            |     |      |
| 7 | Bloqueo de palabras Y emoción en @Audio1 y @Audio2                      |     |      |
| 8 | Línea "never sound bored…" de P2                                        |     |      |
| 9 | Cada imperfección de cámara tiene causa, no adjetivo                    |     |      |
|10 | Ninguna palabra prohibida fuera de RESTRICTIONS (cinematic, bokeh,      |     |      |
|   | realistic UGC style, authentic, 4K, masterpiece…)                       |     |      |
|11 | Latencia en toda reacción                                               |     |      |
|12 | Líneas ≤ 12 palabras; cifras escritas en palabras                       |     |      |
|13 | Ultra gran angular 0.5x cerca + manos de P2 donde hay entrega/pantalla  |     |      |
|14 | Orden de referencias = @ usados; ningún @ sin archivo ni archivo sin @  |     |      |
|15 | Teléfono cae en 48–74% del final estimado (si es el clip del teléfono)  |     |      |
|16 | Un beat físico cada 3–6 s (clip cortable)                               |     |      |
```
Los puntos 1, 2, 10, 12 y 14 los chequea `python3 scripts/kie.py revisar escena.json`.

## 6. Por qué funcionan las reglas

- **Densidad**: el modelo rellena con silencio. Por eso PACING y el presupuesto. Los silencios se
  hacen en montaje.
- **VOICE DIRECTION**: sin esa línea el modelo lee la acotación en voz alta.
- **Audios = solo timbre**: sin bloqueo de palabras repite frases del sample; sin bloqueo de emoción
  hereda su tono. Sin la línea de P2 sale desganado.
- **Causas físicas, no adjetivos**: "realista, estilo UGC, auténtico" el modelo lo entiende como
  "publicidad que imita UGC". Causas que ejecuta: reencuadre tarde, autoexposición que ajusta,
  horizonte que deriva, brazo que respira, motion blur, alguien tapa el lente.
- **0.5x cerca + manos**: es lo que se ve en los 7 ganadores nativos; 1x a 2 m se lee como
  "producción".
- **Sin texto generado**: sale deformado. Todo texto va en post.
- **Una toma continua por clip, cortes en post**: la continuidad de cara/voz es mejor dentro de una
  toma; los cortes rápidos de los ganadores se hacen en montaje (jump cuts + punch-in). Si hace falta
  un corte dentro del clip, se pide como hecho diegético (RECORDING BREAK). Seedance también corta si
  se escribe "lens switch", pero en este formato no se usa: introduce ángulos "de producción".
- **Uniones entre clips**: esconderlas sobre acción física (hombro que tapa, teléfono que baja, swing
  con motion blur). Dejar 0,5–1 s de solapamiento narrativo.
- **Las primeras 20–30 palabras pesan más**: por eso FORMAT va primero y dice "real footage".
