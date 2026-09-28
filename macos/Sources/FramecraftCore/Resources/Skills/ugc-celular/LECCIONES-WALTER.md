# El pack de Walter — qué mirar y por qué funcionó

`WALTER-pack-final-8-escenas.txt` es el material real: 8 escenas, resultado de 2:45
después de cortar silencios. No es una plantilla teórica, es lo que se generó y salió.

Es también, hoy, **el mejor ejemplo trabajado del método**, y enseña cinco cosas que no
estaban en la skill cuando se escribieron esos prompts. Vale la pena leerlo entero, pero
esto es lo que hay que llevarse.

---

## 1. Densidad de diálogo: el hallazgo grande

> El modelo **rellena con silencio** el tiempo que le sobra.

Medido en salidas reales: **2,47 palabras por segundo**. Entonces un clip de 30 s pide
**~70 palabras de diálogo**. Con 42 palabras, el modelo devuelve 13 segundos de aire — y
esos 13 segundos no son pausas dramáticas, son un clip muerto.

De ahí sale el bloque `PACING — CRITICAL`, que es la instrucción más contraintuitiva de
todo el pack:

```
Characters answer immediately, often overlapping the last word of the previous speaker.
The gap between one line ending and the next beginning is about half a second — a breath,
never a hold. There is NO moment in this clip where nobody speaks for longer than one
second. Do not stretch the delivery to fill time and do not insert reflective silences.
```

**Los silencios se hacen en el montaje, no en la generación.** El video final dura 2:45
justamente porque se cortaron silencios de los clips de 30 s. Pedirle pausas emotivas al
modelo da aire vacío; pedirle densidad y después cortar da ritmo.

Cuando el beat sí necesita un respiro —el de S5, entre los 3 y los 8 segundos— se declara
como excepción explícita, con su ventana, y se aclara que **no está vacío**: "his face is
working through it".

## 2. Dirección emocional entre paréntesis

```
P1 (quiet, flat, stripped of everything — the sentence of a man who has said it many
times and has never made it easier, the last word almost inaudible): "My daughter
passed away."
```

Tres cosas que hacen que esto funcione:

- **Hay que declarar que el paréntesis no se pronuncia.** Va en el bloque base:
  *"Text in parentheses before a spoken line is acting direction for delivery only. It is
  NEVER spoken aloud. Only the words inside the quotation marks are ever said."* Sin esa
  línea el modelo lee la acotación en voz alta.
- **La dirección describe mecánica física, no emoción.** Dónde respira, dónde se le
  cierra la garganta, dónde le baja el volumen, dónde se queda sin aire. Nunca "emotional"
  ni "sad".
- **Es la aplicación exacta del principio de causas.** "Se emociona" es una etiqueta;
  "cuando se emociona la garganta se le cierra y el volumen baja en vez de subir" es una
  causa con consecuencia audible.

## 3. Los audios de referencia son solo timbre

Hay que bloquear dos cosas por separado, y las dos se declaran en el bloque de voz:

```
Match @Audio1 for timbre, age, accent and pitch only. Do not reproduce any words from
@Audio1 and do not copy its emotion — the emotion of every line is set by the direction
in parentheses.
```

Sin el primer bloqueo, el personaje repite palabras del audio de referencia. Sin el
segundo, hereda la emoción del sample y pisa la dirección escrita.

## 4. El first frame se clava, no se sugiere

`return_last_frame: true` te da el archivo; que el clip siguiente **empiece** ahí es otra
cosa, y hay que pedirla en lenguaje extremo:

```
@Image3 IS the first frame of this video. Frame 1 must be @Image3 itself, pixel for
pixel: identical framing, composition, camera position, lens, lighting, color, subject
positions, background. Do not reinterpret it, do not recompose it. The video begins ON
that image and all motion grows out of it. Treat @Image3 as the locked starting state of
the scene, not as a style reference.
```

La última frase es la que hace el trabajo: sin ella el modelo trata la imagen como
referencia de estilo y recompone la escena.

Y cuando una escena **no** continúa —S6, que salta a la playa de estacionamiento— el
bloque se quita entero y se reemplaza por `NEW LOCATION AND TIME — build this opening from
the description, do NOT continue any previous framing`. El cambio de locación hay que
declararlo como ruptura, no dejarlo a la deriva.

## 5. El corte se puede pedir como hecho diegético

S4 tiene un jump cut, y no es un error: es la persona que filma parando la grabación
mientras come.

```
This clip contains exactly ONE hard jump cut, at 10 seconds, and none anywhere else.
It is not an edit — it is the person who is filming stopping the recording while he eats
and starting it again afterwards. Render it as an abrupt frame-to-frame jump with no
transition, no fade, no dissolve and no motion match. The audio jumps with it.
```

Mismo principio que todo lo demás: se pide **la causa** ("dejó de grabar"), no el efecto
("cortá acá"). Y se cierra el agujero cambiando la restricción de "no cuts" por "exactly
ONE hard jump cut, at 10 seconds".

## Bonus: aislar un video dentro de la pantalla

S7 y S8 muestran la pantalla del teléfono de Walter con `@Video1` adentro. El riesgo es
que la referencia de video contamine el estilo del plano entero, y se bloquea así:

```
Use @Video1 ONLY as the content of that screen. Do NOT let @Video1's style, color
grading, framing, camera motion, pacing or subject matter influence the rest of the shot
in any way. Everything outside the phone screen is the parking lot scene described here.
```

Más el render del rectángulo: inset, deformado en perspectiva según el ángulo del
teléfono, con brillo de pantalla, reflejo del sol bajo y bisel visible. Y en audio: *"any
audio belonging to @Video1 is NOT heard — the phone screen plays silently."*

---

## Cosas que conviene anotar del pack

**Los prompts están en inglés.** La skill dice "escribir en español salvo que pida lo
contrario", y el pack que funcionó está íntegramente en inglés. Tiene sentido: el diálogo
va en inglés porque la escena es un diner americano, y mezclar idioma de instrucción con
idioma de diálogo invita a que el modelo cruce los dos. **Regla práctica: el prompt se
escribe en el idioma en que se habla en el video.**

**Tres personajes con etiqueta.** P1 Walter, P2 el que filma, P3 la moza. Etiquetar y
describir la voz de cada uno por separado es lo que evita que el modelo le dé la línea al
personaje equivocado.

**El POV está declarado como regla, no como encuadre.** *"The camera IS P2's phone"* y
*"his voice is noticeably louder than everyone else's, because the phone is in his hand"*.
El nivel de la voz es consecuencia de dónde está el teléfono: otra causa física.

**La estructura de las 8 escenas es modular.** Un `BLOQUE BASE` idéntico, y cada escena
declara qué **agrega**, qué **reemplaza** y qué **quita** de ese bloque. Es más robusto
que reescribir el bloque entero cada vez, y hace visible de un vistazo qué cambia entre
clips.

**Los overlays tienen una regla de oro.** Texto largo donde la imagen está vacía —el
abrazo, con el hombro tapando el lente—, texto corto o nada sobre caras y reacciones. El
primer plano de la lágrima va limpio.
