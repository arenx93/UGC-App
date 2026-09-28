---
name: "prompts-arthas-cachito"
description: "Genera prompts cinematográficos de 10s para Gemini Omni 1.1 Flash o Veo 3.1 de la saga Arthas y Cachito. Usar cuando se pida un prompt, escena, toma o capítulo de la saga o de sus personajes."
---

# Prompts cinemáticos para Omni Flash 1.1 — Arthas y Cachito

El método no sale de la documentación interna del proyecto: sale de la documentación oficial de Google, de guías de producción verificables y de renders propios medidos con ffmpeg y análisis espectral. Donde el proyecto y la evidencia se contradicen, **gana la evidencia**. Cada regla indica si está verificada en render o si todavía es extrapolación.

## 0. Carpeta y fuentes de verdad

Raíz: `/Users/raguscope/Downloads/arthas y cachito` (vía `mcp__remote-devices__device_*`: `device_list_dir`, `device_stage_files`, `device_commit_files`). Si no está conectada, pedir `personajes_db.json` y `trama_db.json` adjuntos.

- **Lore y escenas** → `trama_db.json` (claves "1" a "20"; del 8 al 18 traen `escenas`). Gana sobre `TRAMA_EPISODIOS_*.md`: el Cap 14 del JSON es *La Gran Purga Terapéutica* en Las Fauces.
- **Apariencia** → `personajes_db.json`, y sobre todo **`BLOQUES_PERSONAJE.md`**, con los párrafos congelados listos para pegar.
- **Qué funciona y qué no** → `BITACORA_OMNI.md`. Leerla antes de escribir, actualizarla con cada render.
- **Geometría y staging** → `PLAN_ESCENAS_DINAMICAS.md`. **Calidad de salida** → `PLAN_CALIDAD.md`.
- **Continuidad** → el `.md` de la escena anterior en `prompts/`.
- `GEMINI.md`, `AGENTS.md`, `MUSE_AI.md` son históricos: lore y acentos sí, reglas técnicas parcialmente refutadas (§10).

**Verificar siempre el texto contra el model sheet.** La base le atribuía al Carcelero barba, ojos incandescentes, cadenas y runas flotantes que no existen en la lámina, y por eso salía deforme: cuando texto e imagen se contradicen, el modelo promedia los dos. Si el usuario pide una escena que no existe en la base, inventarla es aceptable **sólo** para acción y diálogo.

## 1. Reglas verificadas en render

1. **Nunca corchetes de tiempo si querés plano único.** `[00:00 - 00:03]` es la sintaxis oficial de **multi-plano** y Omni intenta cortar por defecto. El prompt con timecodes cortó en 00:07.04, justo donde empezaba el tercer segmento; los dos sin timecodes no cortaron. La evolución va en prosa. Pedir: **"In a single continuous shot, no scene cuts, one unbroken camera move."**

2. **Nunca pongas la cámara sobre la línea que une a quien habla con lo que mira.** Si además pedís primer plano de la cara es una contradicción geométrica, y el modelo la resuelve girando al personaje hacia la lente.

3. **Diálogo o caminata: 250 a 400 palabras.** Un prompt de 3.373 palabras rindió peor que uno de 384 en todos los ejes medidos. **Excepción: acción** (§4).

4. **Para que no haya texto en pantalla, no pidas escritura.** "Runas grabadas" o "sigilos tallados" producen cirílico falso. Los negativos **no lo evitan**: el prompt que quitó el muro de negativos pero siguió pidiendo sigilos tuvo texto igual. Escribir **"smooth, bare, unmarked stone"**.

5. **El fin del diálogo se pide en prosa simple**: *"He finishes speaking at six seconds and his mouth stays firmly closed for the rest of the shot."* El bloque técnico anti-loop dejó que la voz se pasara a 7.2s.

6. **Separá los colores que compiten** dentro de un personaje: Illidan salió verde hasta que se escribió *"deep lavender-purple skin"* con los tatuajes verdes nombrados aparte.

7. **Escala en metros y comparada**, más un **punto de contacto corporal**: *"ten metres tall, five times Illidan's height"* + *"reaches only to the top of his shin"*. El punto de contacto es lo que más pesa, porque es una relación geométrica verificable en vez de una abstracción.

## 2. Gramática del prompt

Anatomía oficial: **[Cinematografía] + [Sujeto] + [Acción] + [Contexto] + [Estilo y ambiente]**, en prosa corrida.

```
1. CÁMARA: tipo de plano, movimiento único, ángulo, lente. Si hay ataque o
   varios personajes, la geometría completa va acá (§3).

2. AUDIO: ambiente y foley en 3-4 capas jerarquizadas. VA EN LA PRIMERA MITAD;
   enterrarlo al final es un error documentado.

3. UN párrafo por personaje, encabezado por <IMAGE_REF_N>, copiado TEXTUALMENTE
   de BLOQUES_PERSONAJE.md. Nunca reescribirlo de nuevo.

4. ACCIÓN y DIÁLOGO, con la geometría de miradas adentro y el cierre del habla.

5. ENTORNO: una o dos frases. Superficies lisas si no querés texto.

6. "In a single continuous shot, no scene cuts, one unbroken camera move."

7. LUZ: paleta → iluminación → dirección, con Kelvin.

8. Cláusula de ficción, una línea.
```

**Diálogo.** La sintaxis oficial usa comillas. Pero el modelo **no sabe quién es "Marcus"**: con dos personas en cuadro le da la línea a la equivocada. Abrir con un **identificador visual inconfundible** (tabla en `BLOQUES_PERSONAJE.md`): *"The stubbled man with the iron chains across his chest says, in Spanish with a Castilian accent, …"*. Modificador emocional antes de la línea. Entran 10 a 14 palabras antes de los 7s, no 16 a 20.

**Audio.** Máximo 3 o 4 capas, marcadas `(foreground)`, `(midground)`, `(background)`. Atar el sonido a la imagen con lenguaje causal: *"as the hammer lands, grit skitters across the stone"*. Modificadores espaciales útiles: *in the distance, cuts through, faintly, echoing*. Cerrar con **"No background music. No subtitles."**

**Negativos.** La guía oficial desaconseja listas largas. Cerrar con una frase corta de lo que **sí** tiene que seguir siendo verdad.

**Grano.** No pedírselo al modelo: va en post, después del upscale, donde además hace de dithering. Sí pedir luz, prótesis prácticas y óptica anamórfica.

**Formato.** 10s por generación, extensible de a 10s hasta 40s. 16:9 o 9:16. Omni no acepta `seed`. **360p sólo para probar; la toma aprobada se vuelve a tirar en 1080p o 4K.** 360p es un noveno de los píxeles de 1080p y es la causa más grande y más barata de que los videos se vean mal.

**Palabras basura prohibidas**: `photorealistic`, `hyperrealistic`, `8k`, `ultra-detailed`, `cinematic lighting`, `octane render`, `masterpiece`, `trending on artstation`, y parámetros de cámara ficticios.

## 3. Geometría: dónde se rompe todo

Los modelos **obedecen bien y anclan mal el espacio**: Veo3 puntuó 0,22 en consistencia espacial y "alucina el objeto objetivo en vez de anclarlo en la geometría real" (Video4Spatial, arXiv 2512.03040). Y las contradicciones se **promedian** en vez de resolverse (LTX).

### Ataques a distancia — causa verificada de fallo

1. **La cámara no puede contradecir hacia dónde miran.** De perfil → disparan a un costado. Hacia el fondo → se los ve de espaldas.
2. **El objetivo tiene que estar en el mismo cuadro que el atacante.**
3. **Ningún aliado en la línea de tiro.** Apilarlos en profundidad. *(Esto hizo que Svetlana le pegara al Orco.)*
4. **El vector en términos de pantalla**: *"diagonally across the frame from lower left to upper right"*.
5. **Declarar la línea despejada**: *"nothing and nobody stands between them and the giant"*.
6. **El impacto se ancla a una parte del cuerpo nombrada y visible.**

### Varios personajes — extrapolación, a verificar

Cuando dos le hablan a un tercero, los dos salen frontales y el tercero queda atrás: el plano frontal es la composición más común del entrenamiento y ese prior le gana a la instrucción.

1. **La cámara va DETRÁS de los dos, mirando hacia el tercero**: *"camera behind X's shoulder, looking toward Y"*. Si la lente está detrás, el tercero no puede quedar atrás.
2. **Nombrar quién ancla el primer plano.**
3. **Nunca describir dos personajes como pareja.** Cada uno en su frase.
4. **Escalonar en profundidad, no en fila.** Una fila lado a lado *es* la pose frontal.
5. **Nombrar al tercero primero.**
6. **Prohibir la pose frontal en positivo**: *"no face is squared to the lens; every face is turned toward another character in the scene"*.

**Ventaja de esta saga:** Zovaal mide 10 m. Su pierna, su puño o su antebrazo cerca de la lente lo fijan adelante por geometría pura.

**Chequeo obligatorio:** ¿la acción es **físicamente alcanzable** para la altura de cada personaje? A 10 m el antebrazo queda a 4-5 m: nadie a pie lo alcanza. Golpear canilla, trepar o volar.

## 4. Escenas de acción

Acá se invierte la regla de brevedad. La guía de Veo dice textual: *"For ultimate control over fast-paced scenes, leave nothing to chance"*. Los prompts de pelea pesan 500 a 750 palabras y está bien, **siempre que el presupuesto extra vaya a coreografía y no a descripción estática**.

- **Más detalle por beat, no más beats.** En 10s entran 3 o 4 momentos físicos.
- **Los modelos tiran a cámara lenta por defecto.** Forzar velocidad con verbos que la lleven adentro (*explodes, whips, rips, rockets*), modificadores (*at full speed*) y **"no slow motion at any point"**.
- **Mover el entorno**: niebla que se rasga, chispas, polvo, tela que restalla.
- **El impacto viaja en las dos direcciones**: que vuelva por el mango y sacuda al que pegó, y que recorra el cuerpo del que lo recibe (pecho, hombros, cabeza, pies). Es lo que da sensación de masa.
- **Un solo ángulo por toma.** Los cambios de ángulo pierden identidad.
- **Cámara documental de zona de guerra**: la inestabilidad intencional lee como real.
- **Diálogo corto o ninguno.** El largo hace que el personaje atropelle los gestos físicos.
- **Nada de etiquetas de segmentación** tipo `BEAT ONE`: misma familia que los timecodes.
- **Presupuestar ~3 generaciones por plano útil.**

## 5. Drama: que la escena tenga una idea

Una escena técnicamente correcta puede ser dramáticamente vacía. Es una comedia: cada toma necesita **una idea que actuar**, no sólo una maniobra que ejecutar.

- **Poner el chiste en escena, no narrarlo.** "De tres maneras descoordinadas" es contarle el chiste al modelo; escalonar las reacciones y dejar que se vea.
- **Usar el contraste entre personajes.** Svetlana trabaja como cirujana, sin un gesto de más; el Orco hace lo contrario en todo y la mira con devoción. Ella no le devuelve la mirada.
- **Un gesto vale más que una línea.** Illidan quieto, gujas al piso, girando despacio la cabeza hacia Marcus, es el personaje entero y además le ahorra al modelo una acción compleja.
- **Dirección de actuación explícita**: *"her face does not change"*, *"open helpless devotion"*.

## 6. Identidad entre tomas

- **Los bloques de personaje van idénticos en todas las escenas.** Las descripciones inconsistentes son causa documentada de deriva. Por eso existe `BLOQUES_PERSONAJE.md`: copiar y pegar, no reescribir.
- **2 a 4 referencias desde ángulos distintos** (frontal, tres cuartos, perfil): los model sheets del proyecto ya son exactamente eso.
- **Encadenado de fotogramas**: exportar un fotograma limpio de cada clip aprobado y usarlo como imagen inicial del siguiente. Reduce la deriva de forma significativa.
- **Máximo 3 personajes por toma.** Si son cuatro, sacar a uno de cuadro. Todos hablan o reaccionan: prohibido el maniquí mudo.
- Otras causas de deriva: falta de referencias, **movimiento de cámara demasiado agresivo** y cambios drásticos de iluminación. La handheld violenta de las peleas tiene un costo en identidad.
- El objetivo realista es **continuidad perceptual**, no identidad pixel a pixel.

## 7. Elenco canónico (claves de `personajes_db.json`)

| Clave | Personaje | Acento |
|---|---|---|
| `arthas_purge` | Arthas Purge Edition, El Purificador | Peninsular inquisidor fanático |
| `arthas_protolich` | Arthas Proto-Lich, ojeroso, saronita, cuernos de carnero | Peninsular sombrío, ronco |
| `arthas_humano` | Lord Arthas paladín restaurado, plata y oro, capa cobalto | Peninsular noble y sereno |
| `marcus_sin_armadura` | Marcus el Chupaladín en cueros, Naaru ascendido | Gallego canalla mujeriego |
| `marcus_acolito` | Marcus Acólito, túnica rasgada, cadenas en X, una sola hombrera izquierda | Gallego canalla |
| `marcus_broker` | Marcus Especulador de Oribos | Gallego + jerga de boliche |
| `elfo_rubio` | Johnny Awesome, cazador vanidoso | Porteño zarpado estilo El Bananero |
| `elfo_rubio_tunica` | Johnny Túnica, infiltrado cheto | Porteño zarpado ("¡SAPEE!") |
| `orco_colombiano` | El Orco Chamán vallenatero, relámpagos por los dedos | Colombiano paisa romántico |
| `svetlana` | Doctora Svetlana, draenei cirujana militar | Rusa marcial implacable |
| `enano_zombie` | Enano Zombie Peruano, DK sindicalista | Peruano criollo ("¡Asu mare!") |
| `kelthuzad` | Kel'Thuzad, lich académico | Peninsular snob sarcástico |
| `profesor_x` | Profesor X, goblin del peaje en silla a vapor | Porteño chanta de Warnes |
| `dredger_revendreth` | Dredger de fango | Gárgola carrasposa servil |
| `cachito_normal` | Cachito semielfo, pechera "I ♥ NZOTH" | Cordobés conurbano pícaro |
| `cachito_poseido` | Cachito Poseído, Corona de Dominación | Cordobés + eco cósmico |
| `sire_denathrius` | Sire Denathrius | Peninsular aristócrata teatral |
| `carcelero` | Zovaal, coloso gris de **10 m**, calvo, lampiño, espiral en el pecho, garras negras | Voz cósmica lenta y retumbante |
| `usnavy_troll` | Usnavy Bárbaro, chamán troll | Cubano sabroso ("¡Oye asere!") |
| `elfo_colorado` | El Colorado, místico psicodélico | Porteño hippie colgado |
| `reina_invierno` | La Reina del Invierno | Deidad etérea y gélida |
| `jurafauces` | Soldado Jurafauces (sin model sheet) | Gutural metálico |
| `illidan` | Illidan en bata y pantuflas / en combate con las Gujas | Tico dramático ("¡Diay maes!") |

**Alias que cambian desde el Cap 12**: `arthas`→`arthas_purge`, luego `arthas_protolich`; `marcus`/`chupaladin`→`marcus_sin_armadura`, luego `marcus_acolito`; `johnny`/`rubio`/`elfo`→`elfo_rubio`, luego `elfo_rubio_tunica`. Fijos: `orco`→`orco_colombiano`, `zovaal`→`carcelero`, `reina`→`reina_invierno`, `colorado`→`elfo_colorado`, `troll`→`usnavy_troll`, `enano`/`peruano`→`enano_zombie`, `goblin`→`profesor_x`, `draenei`→`svetlana`, `cachito`→`cachito_normal`.

## 8. Lore y capítulos

**El gran secreto (no revelar antes del Cap 19):** la banda no pelea entre sí, son aliados recorriendo las curias reclutando gente y artillería. Creen que **Zovaal secuestró a Cachito**: es falso. **El Primus es el titiritero**, controla a Cachito con el Sello del Primus para robar los 4 sellos y abrir **Zereth Mortis**. Zovaal y Denathrius sólo fingían ser los villanos para frenarlo.

01 Bastión · 02 Cachito roba el Sello · 03 Nace el Dreamteam · 04 Revendreth, el Orco · 05 Salones de Expiación · 06 Castillo Nathria · 07 Maldraxxus · 08 Tanques de hueso · 09 Trono vacío · 10 Ardenweald · 11 La Reina del Invierno · 12 El Colorado · 13 Profesor X · 14 Las Fauces · 15 Río de Almas · 16 Torghast · 17 Illidan en pantuflas · 18 Batalla contra el Carcelero · 19 EL PLOT TWIST · 20 Zereth Mortis.

Desde el Cap 12, estética 80s analógica (Eastman 5247, prótesis prácticas). Antes, Kodak Vision3 5219.

## 9. Filtros de seguridad

| Prohibido | Usar |
|---|---|
| joint, faso, porro, blunt | `smoldering rolled silverleaf roll` |
| weed, marihuana, drogas, narcotic | `sacred silverleaf herbs`, `botanical Ardenweald flora` |
| stoned, fumado, high, tripping | `dreamy meditative daze`, `glassy contemplative trance` |
| blood, sangre, gore, dying, mutilation | `anima residue`, `battle bruising`, `falling exhausted` |
| chiquito | `Cachito`, `el semielfo`, `nuestro compinche` |
| "Prince Arthas" | `Lord Arthas`, `Paladin Arthas` |
| LIVE ACTION ACTORS | `fictional fantasy characters` |
| WARCRAFT 2016 | `dark fantasy cinematic aesthetics` |
| celebrity, celebrities, real actors | `fictional fantasy persona(s)` |
| Mercado Pago, PayPal, Ko-fi, Patreon, CBU/CVU | en diálogo `el alias en pantalla`; el logo va en CapCut |

Ojo con **`joint`**: está baneado y se cuela en anatomía ("wing joint"). Usar `wing root`.

Una línea de ficción alcanza: *"All characters are fictional fantasy characters in an original creative production."*

## 10. Reglas del proyecto refutadas

No aplicarlas sin avisar que están en duda:

- **"Las comillas queman subtítulos"** — la sintaxis oficial las usa; el render con comillas no produjo subtítulos.
- **"Estructura de 6 o 15 bloques con timecodes"** — es la gramática de multi-plano; causa cortes.
- **"Muro de negativos"** — desaconsejado oficialmente, sin aporte medible.
- **"16 a 20 palabras de diálogo"** — sin respaldo y choca con los factores WPS del propio documento. Entran 10 a 14.

Lo que **sí** sobrevive: el lore, los acentos, la base de personajes, la tabla de seguridad, el tope de 3 personajes, la variedad de encuadres (nada de tomas dorsales repetidas) y el anclaje gravitatorio al describir pisadas y peso.

## 11. Entrega y medición

Prompt completo en bloque de código listo para copiar, y escrito a `prompts/capNN_escMM.md` con `device_commit_files`. Decir siempre **en qué orden cargar las referencias de imagen**. Cerrar con el conteo de palabras del prompt y del diálogo.

Después de cada render que mande el usuario, **medirlo**:
- cortes: `ffmpeg -i v.mp4 -vf "select='gt(scene,0.12)',metadata=print:file=-" -f null -`
- ventana de silencio: extraer wav, RMS y energía 300-3000 Hz por ventanas de 0,5s
- identidad y geometría: extraer fotogramas y revisarlos de a uno
- resolución real con `ffprobe`: si dice 640x360, es borrador

Anotar el resultado en `BITACORA_OMNI.md` separando lo verificado de lo supuesto. Nunca prometer consistencia garantizada: es una red estocástica y lo único que se garantiza es la validación formal.