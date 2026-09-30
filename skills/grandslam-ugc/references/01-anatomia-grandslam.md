# Anatomía de un grand slam — lo medido en los 9 ganadores

Fuente: 9 videos de la carpeta GRANDSLAM (vertical, seguros de auto EEUU). Medido con FFmpeg
(cortes, silencios, loudness), Whisper small.en (diálogo con tiempos) y lectura de fotogramas
cada 2 s. Conteos de cortes ±2, tiempos de texto ±1 s. **No hay métricas de rendimiento por
video: las reglas pesan igual para los 9.**

## 1. Los 9 videos de un vistazo

| Video | Tipo | Dur. | Cortes | Toma mediana | Hook | Gesto | Teléfono | Cola | Pal/seg |
|---|---|---|---|---|---|---|---|---|---|
| MillionaireGrandpa (Rafael) | real | 2:52 | 14 | largas (hasta 43 s) | 3 s caja negra | $100 42% · $500 58% | 74% | 21 s papel $52.90 | 2,48 |
| Widowed (Daniel) | real | 2:14 | 10 | — | 13 s observacional | $500 42% | 58% | 19 s pantalla $71/MONTH | 3,97 |
| IWillSurviveHeyHey (Facundo) | real | 1:57 | 21 | 3,6 s | 5 s | $500 36% | 48% | 19 s laptop $47.72 | 4,08 |
| WidowPresent (Facundo) | real | 1:48 | 13 | 6,3 s | 5 s | $1300 33% | 54% | 20 s papel $64.20 | 3,51 |
| GrandpaSaved (Rafael) | **IA (Seedance)** | 1:38 | 16 | 2,9 s | 11 s observacional + narración | batería 43% | 65% | 21 s papel $52.90 | 2,76 |
| DaddyVsSon (Juan) | real | 1:30 | 15 | 3,9 s | 5 s | plata 46% | 73% | no | 3,47 |
| Yourself (Facundo) | real | 1:20 | ~3 | largas | 5 s caja negra | — | 65% | no | 4,24 |
| ViralTeacher (Agustina) | noticiero | 1:44 | — | — | todo el video | compras 5% | 50% | 20 s papel $52.90 | 3,59 |
| MillionaireXNews (Daniel) | noticiero | 0:59 | — | — | todo el video | — | 37% | no | 3,96 |

Resumen duro:
- **Duración** 0:59–2:52, mediana 1:48 (con cola). Objetivo: 1:20–2:00.
- **Silencio** 0–2% bajo −30 dB; ningún hueco > 0,6 s. Loudness −13 a −19 LUFS (objetivo −16).
- **Cortes** 10–21 en 6 de 7 nativos; toma mediana 2,9–6,3 s; toma larga permitida (20–43 s) si el diálogo sostiene.
- **Densidad de habla** (palabras/seg sobre la duración hablada): reales 3,5–4,2, mediana 3,6; el
  ganador IA 2,76; cobertura de habla 69–91% del tiempo. **Conclusión: los clips de Seedance
  (~2,5 pal/seg) hay que apretarlos en montaje para llegar a ≥ 3,3.**
- **Cola de prueba** en 6 de 9 (19–21 s). La misma cola ($52.90) aparece en 3 videos: es un asset reutilizable.

## 2. El arco (en % de la duración total)

```
 0–5%     HOOK         anomalía + promesa (texto grande). Acción ya empezada.
 5–20%    ENCUENTRO    lo aborda, pregunta simple, primer intercambio.
15–30%    DOLOR        el dato que duele: viudo/a, hija muerta, auto por embargar, seguro de $250–300.
25–45%    CONTEXTO     por qué está así: trabaja para los nietos, cuida a alguien, junta latas.
33–46%    GESTO 1      la plata / la ayuda física + reacción ("Are you serious?").
45–55%    PUENTE       "There's one more thing I want to do for you" → ir al auto.
48–74%    TELÉFONO     "Pull out your phone… select your car and the year" (mediana 65%).
 ~70–85%  CIFRA        "$1,700 back" + "$49 a month, full coverage" + reacción grande.
 últimos 5–9 s de escena  CTA en texto "If your bill is over $80 you can do this too".
 +19–21 s COLA         papel/pantalla de póliza + testimonio en voz en off.
```

La escena real ocupa el 79–88% del video y la cola el resto.

## 3. Lenguaje visual (mirado en los fotogramas)

Esto es lo que hace que "parezca real" y es lo que más se pierde en los prompts:

| Rasgo | Qué se ve | Dónde |
|---|---|---|
| **Ultra gran angular 0.5x** | Distorsión de barril, techo curvo, cara grande y fondo lejos. Cámara a 0,6–1,5 m. | Todos los POV (MillionaireGrandpa, IWS, Widowed, GrandpaSaved, DaddyVsSon) |
| **Manos del que filma en cuadro** | Dan agua, la lata, los billetes; señalan la pantalla; tocan el hombro en el abrazo. | 7 de 7 nativos |
| **Altura baja** | A la altura del pecho o de la mesa (DaddyVsSon: el borde de la mesa y la bandeja ocupan el tercio inferior). | Escenas sentadas |
| **Hook escondido** | Plano de lejos, sujeto chico en cuadro: a través del parabrisas con el capó reflejando (Widowed), desde la mesa con el ticket en primer plano (DaddyVsSon), desde el otro lado del estacionamiento (GrandpaSaved). Zoom digital 2x (se nota el ruido). | Hooks largos y cortos |
| **Luz dura** | Sol de mediodía, cielo quemado, sombras duras, flare. Interiores con fluorescente/LED mixto y ventana. | Todos |
| **Primer plano de pantalla** | El teléfono del sujeto en su mano, la cámara a 20–30 cm, el dedo del que filma toca/scrollea, **reflejo de la cara en la pantalla**, foco que va y viene, texto medio legible. | IWS 0:56–1:18, Widowed, WidowPresent |
| **Abrazo que tapa el lente** | 2–4 s de hombro/tela llenando el cuadro. Se usa para texto largo. | MillionaireGrandpa 1:50, Widowed 1:50 |
| **Primer plano de llanto** | Cara a < 60 cm, ojos húmedos, sin texto encima. | MillionaireGrandpa 1:56, GrandpaSaved 0:32 |
| **Salto de lugar/tiempo** | Corte seco (restaurante → puerta al atardecer "coming back at four"). Sin placa. | MillionaireGrandpa 1:34, DaddyVsSon 0:55 |

Flujo de la app que se ve en pantalla (IWS, Widowed): grilla de marcas de auto → "Vehicle Year" →
"Agent Notes" con descuentos (homeowner, safe driver, multi-vehicle, low mileage) → "WHAT WE FOUND
FOR YOU" 2024/2025/2026 → "Missed savings since you bought your car" → "Auto Insurance Policy $93".

## 4. El hook

Dos tipos:
- **Corto (3–5 s), in medias res**: la acción ya pasa (entrega de DoorDash, mozo acercándose, mujer
  caminando). 2–3 planos: general del lugar → corte más cerca a los 2–4 s.
- **Largo (11–13 s), observacional**: plano sostenido de lejos mientras el sujeto trabaja/sufre,
  **con narración en voz baja del que filma** ("Man it is 95 degrees out here and look over there…
  Let me grab some cold waters and go help them"). Termina cuando el que filma se acerca.

Texto del hook (estilos medidos):

| Estilo | Videos | Detalle |
|---|---|---|
| Condensada MAYÚSCULAS blanca, contorno negro | GrandpaSaved, IWS, WidowPresent, Widowed, DaddyVsSon | 3 líneas; la línea del gancho en **amarillo** en 2 de 5 ("SO I DID THIS FOR HIM", "UNTIL I DID THIS…"); emojis ❤️🥺 en línea aparte |
| Caja negra redondeada, texto blanco | MillionaireGrandpa, Yourself | MAYÚSCULAS o sentence case |
| Titular de noticiero | ViralTeacher, MillionaireXNews | sentence case fijo arriba todo el video |

Posición: y ≈ 45–60% si hay aire; y ≈ 15–35% si abajo hay tablero/manos/mesa. Nunca sobre la cara.

Fórmulas de texto (todas reales):
```
73YO GOT STRANDED / WITH HIS 2 GRANDKIDS / SO I DID THIS FOR HIM ❤️🥺
THIS SINGLE DAD SKIPPED / MEALS FOR HIS SON / SO I DID THIS FOR HIM 🥺❤️
71 Y/O STROKE SURVIVOR STILL / DOORDASHING EVERYDAY / UNTIL I DID THIS... ❤️‍🩹🥺
WIDOWED DAD GOES VIRAL / COLLECTING CANS WITH HIS KIDS ❤️🥺
WIDOWED MOM RECEIVES THE / PRESENT SHE DESERVES 🥺❤️
MILLIONAIRE GAVE GRANDPA A FRESH START 🥺❤️
Millionaire teaches valuable lesson...
```
Patrón: **[edad o rol] + [situación dura, visual y concreta] + [SO I DID THIS FOR HIM / UNTIL I DID THIS…]** + ❤️🥺.

## 5. Textos en pantalla

**Capa 1 — comentario del narrador**: frase corta siempre en pantalla, 5–12 s cada una, 4–8 por
video, y ≈ 40–60% (se mueve para no tapar caras). Estilos: blanco con contorno fino sin caja (el más
usado) · caja blanca texto negro con emoji adentro (TikTok nativo) · caja negra texto blanco para
frases de peso y CTA.

**Capa 2 — subtítulos**: literales, frase por frase (~2 s), 1–3 líneas, minúscula sin puntuación,
y ≈ 72–80%. Blancos (IWS, MillionaireGrandpa, Yourself) o **amarillos** (WidowPresent, Widowed).
DaddyVsSon y GrandpaSaved no llevan.

**Se saca el comentario**: en el primer plano de llanto y en el tramo del teléfono (quedan los
subtítulos). **Se pone** encima del abrazo que tapa el lente. Los errores de tipeo ("Wait fot it…")
y la "i" minúscula no se corrigen: las frases se repiten **literal** entre videos (catálogo en
`frases-overlay.json`).

## 6. El audio

1. **Narración del que filma** en el hook largo: voz baja, pegada al micrófono.
2. **Diálogo de escena**: denso (cobertura de habla 69–91%), respuestas inmediatas, se pisan.
   El que filma suena más fuerte (tiene el micrófono en la mano).
3. **La cifra dos veces**: ahorro devuelto y cuota nueva, con reacción en la misma frase.
4. **Testimonio en la cola** (primera persona, voz del creador, mismo audio reutilizado en 3 videos):
   > "Guys I just tried it. It gave me every discount for being a homeowner, safe driver and here's
   > the best part: $2,200 in savings back in my pocket and look at my new rate, $52.90 a month. I
   > drive a Toyota Camry, so this is less than I've ever paid in a long long time, all because I use
   > this website guys. I'll leave it below."
5. **Sin música** perceptible en los nativos (no se analizó a fondo). Ambiente real del lugar.
6. Nivel: −13 a −19 LUFS; la cola 3–10 dB más baja que la escena.

## 7. El cierre

- **CTA 5–9 s** sobre la última reacción real: `If your bill is over $80 you can do this too` (4
  videos) · `Give it a try if your bill is over $80/mo` · `Link below try this yourself!!`.
- Placa opcional ~1,5 s negra con caja blanca: `IF YOU PAY MORE THAN $80MO CHECK BELOW ❤️🥺`.
- **Cola 19–21 s**: segundo rodaje pegado con corte seco, otra luz, cámara en mano, foco que entra y
  sale, el dedo recorre descuentos → "missed savings" → cuota final. Sin textos ni subtítulos.

| Cola | Cifra final | Aparece en |
|---|---|---|
| Papel "Auto Insurance Savings Calculator": Agent Notes, WHAT WE FOUND FOR YOU $768.37/$743.91/$743.96, $2256.24 since you bought your car | Policy **$52.90** · 1 Month Policy Payment | GrandpaSaved, MillionaireGrandpa, ViralTeacher |
| Pantalla de laptop: $6661.05 missed savings | **$47.72** · 1 Month Policy Payment | IWS |
| Papel sobre el capó de un auto negro al sol | Auto Insurance Policy **$64.20** | WidowPresent |
| Pantalla: FULL COVERAGE, 2016 Chevrolet Malibu, 1 YEAR SAVINGS | **$71/MONTH** · Up to $952 | Widowed |

La cifra de la cola **no coincide** con la de la escena: es del creador, no del sujeto.

## 8. El "gesto" (primer regalo) — taxonomía

| Mecánica | Video | Por qué funciona |
|---|---|---|
| Propina enorme a quien trabaja | IWS ($500 al DoorDash), MillionaireGrandpa ($100 y $500 al mozo) | Contraste edad/trabajo |
| "Buy me a coffee and I'll let you pick anything" | WidowPresent, ViralTeacher | Reciprocidad: ella da primero, "she kept her word… so I'm gonna keep mine" |
| Plata escondida en un objeto | Widowed (billetes dentro de la lata) | Sorpresa visual, reacción en cámara |
| Arreglar el auto | GrandpaSaved (batería nueva) | Puente natural al seguro |
| Pagar la comida / plata en la mesa | DaddyVsSon | El hijo delata al padre ("because he doesn't have enough money") |
| Millonario que "enseña" | Yourself, MillionaireXNews | El producto es el tema desde el principio (formato distinto) |

Puente al producto (literal en 5 de 9): **"There's one more thing I want to do for you."**

## 9. Formato noticiero (reciclaje de un ganador)

| Elemento | MillionaireXNews (vertical) | ViralTeacher (horizontal) |
|---|---|---|
| Fondo | Negro | Negro |
| Titular | Blanco sentence case, 3 líneas, y ≈ 12–26% | Blanco sentence case, 2 líneas arriba |
| Video | Recortado a ~50% del ancho, y ≈ 27–73%, sobre fondo azul con globo | 16:9 en la banda central |
| Zócalo | Tag rojo "VIRAL STORIES" + barra blanca texto negro bold MAYÚS | Tag rojo "… NEWS" + ticker azul |
| Voz | Locutor en off ("This video went viral…") y puentes narrados | Presentadora al inicio y final + placa azul CTA + cola $52.90 |

Sirve para **reciclar** un video que ya funcionó, no como formato de origen.

## 10. Lo que el único ganador IA (GrandpaSaved) enseña

- Se puede ganar con Seedance: hook observacional de 11 s + narración, POV con manos, ultra gran
  angular, sol duro, primer plano de llanto, ráfaga de planos de ~2 s para la ayuda física
  (autopartes, batería, capó), teléfono al 65%, cola real de papel.
- Es el más lento en palabras/seg (2,76) y el de toma más corta (2,9 s): el editor compensó la
  lentitud del modelo **con muchos cortes**. Esa es la receta: generar denso, cortar mucho.
