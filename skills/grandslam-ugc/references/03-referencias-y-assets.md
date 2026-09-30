# Paso 2 — Referencias y assets

Seedance 2.5 en modo **referencia multimodal** recibe tres arrays independientes. El número del
`@` sale **del orden en el array**, no del nombre del archivo. Reordenar liga la referencia a otro
archivo en silencio y sin error: por eso el orden es fijo y `kie.py revisar` lo verifica.

## 1. Orden fijo de referencias

```
reference_image_urls: [0] P1 (tríptico del personaje)         → @Image1
                      [1] LUGAR (model sheet de la locación)   → @Image2
                      [2] ÚLTIMO FRAME del clip anterior       → @Image3   (solo si continúa)
                      [3+] P3, objetos (billetes, auto, etc.)  → @Image4…  (si hacen falta)
reference_video_urls: [0] PANTALLA (screen recording de la app) → @Video1  (solo en el clip del teléfono)
reference_audio_urls: [0] voz P1                                → @Audio1
                      [1] voz P2 (el que filma)                  → @Audio2
```
Si un clip no continúa al anterior y tiene P3, P3 pasa a ser `@Image3`. No importa: el prompt
tiene que usar el número que corresponde al array **de ese clip**. `kie.py revisar` imprime el
mapa `@ → archivo` de cada escena para que se chequee a ojo.

Límites (Seedance 2.5, confirmar en docs.kie.ai si algo falla):
- Imágenes: hasta 30, JPG/PNG/WEBP, ≤ 30 MB c/u.
- Videos: hasta 10, MP4/MOV, ≤ 200 MB, **duración total ≤ 30 s**.
- Audios: hasta 10, MP3/WAV, **cada uno 2–30 s y el total ≤ 30 s** → dos voces de 12–15 s c/u.
- Los modos *first frame*, *first+last frame* y *referencia multimodal* son **excluyentes**. Como
  necesitamos voces y personaje, siempre usamos multimodal y el primer frame se clava con `@Image3`
  dentro del prompt (bloque FIRST FRAME).
- **Caras reales**: el filtro bloquea fotos de personas reales (y retratos IA demasiado
  fotográficos a veces). Usar personajes generados; si rebota, bajar el realismo del tríptico
  (luz de estudio plana, fondo gris) o probar otra variante.

## 2. @Image1 — tríptico del personaje

Una sola imagen 16:9 o 3:2 con el mismo personaje tres veces: frente cuerpo entero, 3/4 medio
cuerpo, primer plano de la cara. Fondo gris liso, luz plana. Generar con GPT Image 2 o Nano Banana
Pro (en la app Framecraft o por kie). Prompt base:

```
Character reference sheet, three views of the SAME person side by side on a plain light-gray
background, flat even studio light, no text, no labels: (1) full body front view, standing
relaxed, arms at sides; (2) three-quarter view from the waist up; (3) close-up of the face, neutral
expression. The person: [82]-year-old [white American] man, [thin build, slightly stooped], [thin
white hair combed back, clean-shaven, deep lines around the eyes, age spots]. Wearing [a short-sleeve
beige button-up server shirt with a black name tag, black trousers, black apron, black non-slip
shoes]. Real skin texture, ordinary unglamorous look, no makeup, not a model.
```
Reglas: la ropa se describe prenda por prenda con color y es **la misma** que va en el bloque
CHARACTERS de todos los prompts. Nada de accesorios que no se vayan a nombrar.

## 3. @Image2 — model sheet de la locación

Una imagen 2×2 del mismo lugar desde 4 ángulos a la altura del pecho, **a la hora del día del
clip**, vacío o con gente de fondo borrosa. Incluir los objetos que la acción usa (mesa, capó, carrito).

```
Location reference sheet, 2x2 grid of the SAME place from four eye-level angles, no people in
the foreground, no text: [a small American family diner, 11 a.m.: red vinyl booths, laminated
wood tables with ketchup bottles and napkin dispensers, ceiling fans with warm pendant lights, a
wide window wall to a parking lot with bright daylight, drop ceiling with fluorescent panels].
Shot on a smartphone, ordinary, slightly cluttered, realistic.
```
Para exteriores de estacionamiento: sol de mediodía, cielo claro, asfalto gris con líneas
blancas, autos estacionados comunes (Nissan, Toyota, Ford), un edificio comercial bajo.

## 4. @Image3 — último frame (continuidad)

`kie.py esperar` descarga el mp4 y extrae el último frame limpio (`clips/S1_last.jpg`) con FFmpeg,
aunque kie también lo devuelva. Si el último frame está movido o tapado, usar uno 0,3–0,5 s antes
(`kie.py frame clips/S1.mp4 --t -0.4`). Ese frame va como `[2]` en el clip siguiente.

## 5. @Video1 — la pantalla del teléfono

Screen recording vertical (9:16) de **la página real del cotizador** o de una maqueta fiel:
grilla de marcas → año → "Agent Notes" con descuentos → "WHAT WE FOUND FOR YOU" (años) →
"Missed savings" → cuota final. 8–12 s, sin audio, sin notificaciones.
- Grabarlo en un teléfono de verdad (iPhone: Centro de control → Grabar pantalla) y recortar con
  `ffmpeg -ss 1 -t 10 -i pantalla.mov -an -vf scale=720:-2 pantalla_10s.mp4`.
- La cuota que muestra la pantalla debe ser **la misma** que P2 dice en el diálogo.
- En el prompt va el bloque que la aísla (no contamina el resto del plano y no suena).

## 6. @Audio1 / @Audio2 — voces

- 12–15 s cada una (el total de audios ≤ 30 s), voz sola, sin música ni ambiente, lectura
  **neutra** (no actuada: si viene quebrada, todo el clip sale quebrado).
- Mismo nivel en las dos (una diferencia de 10 dB se lee como voz lejana o débil).
- Fuente: actor grabado con el teléfono cerca, o TTS de calidad con una voz fija por personaje.
- Limpieza y nivelado:
  ```
  ffmpeg -i crudo.wav -ss 0.3 -t 14 -af "highpass=f=80,lowpass=f=12000,afftdn=nf=-25,loudnorm=I=-20:TP=-2" -ar 44100 -ac 1 voz_P1.mp3
  ```
- Un personaje sin audio de referencia (P3) se describe en palabras en CHARACTERS.

## 7. La cola de póliza (19–21 s)

Orden de preferencia:
1. **Reusar el asset que ya existe** ($52.90, papel) — los ganadores lo reusan en 3 videos.
2. **Filmarla de verdad**: imprimir la hoja, filmarla con el teléfono en mano, luz de interior, el
   dedo recorre descuentos → missed savings → cuota; foco que entra y sale. 20–22 s sin cortes.
3. Generar la hoja como imagen (texto exacto) e imprimirla para filmarla (mejor que animarla).
4. Último recurso: `montar.py` con `"cola": {"imagen": "poliza.png"}` simula cámara en mano sobre
   la imagen fija (zoom y deriva lenta). Se ve menos real: avisar.

**No generar la cola con Seedance**: los números impresos salen deformados y la cifra final tiene
que ser legible y exacta.

Prompt de la hoja (GPT Image 2, que escribe texto bien):
```
A photo-realistic printed US letter page lying on a table, shot from above with a smartphone,
slight perspective, soft indoor light. The page is an "Auto Insurance Savings Calculator" printout:
header "Policy" in blue, big blue price "$52.90" with small "1 Month Policy Payment"; section
"Policy Information" with rows Policy Number FC-2026-10482, Effective 07/27/2026, Expiration
07/27/2027, Named Insured Hayden Blair, Address 2300 Camelback Rd, City Phoenix, AZ; section "Agent
Notes" with checkmarks: Homeowner Discount -$163.33, Safe Driver Discount -$32.67, Multi-Vehicle
Discount -$32.67, Low Mileage Discount -$16.33; section "WHAT WE FOUND FOR YOU" with three boxes
2024 $768.37, 2025 $743.91, 2026 $743.96 and a bar "Missed savings $2256.24 since you bought your
car" and a green outlined button "Connect Bank". Clean sans-serif, realistic print texture.
```
(Cambiar cifra/nombre/ciudad en cada cola nueva. La cifra nunca redonda.)

## 8. Voz en off (hook y cola)

- **Narración del hook**: preferible generarla **dentro de S1** (P2 habla bajo mientras filma de
  lejos: sale sincronizada con el aire del lugar). Si no, grabarla y usar `voz_en_off` con `duck 0.25`.
- **Testimonio de la cola**: grabado por el creador/actor (misma voz que P2 idealmente) o TTS con la
  voz de P2. Se monta con `voz_en_off` `start: "cola"`, `duck: 0`.
- No se clonan voces de personas reales sin permiso.
