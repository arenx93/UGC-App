# Cómo funciona este proyecto

Resumen del sistema con el que se escriben los prompts de video, qué es cada pieza,
cómo se redacta un prompt paso a paso y qué hay que cambiar para llevarlo a ChatGPT.

---

## 1. La arquitectura en una frase

No hay software propio ni pipeline automatizado. Lo que hay es **una skill: un archivo
de texto con el método**, que el modelo carga sola cuando detecta que el pedido encaja,
y a partir de ahí escribe todos los prompts con las mismas reglas.

```
Vos pedís          →  "hacé el prompt 4, Walter trae la cuenta"
El modelo detecta  →  matchea con la descripción de la skill
Carga SKILL.md     →  el método entero entra al contexto
Escribe el prompt  →  con estructura, banco de lenguaje y restricciones fijas
```

Lo único que hace que esto funcione mejor que pedirle un prompt a pelo es que
**el método está escrito una vez y se aplica idéntico siempre**. La consistencia entre
clips no sale de la memoria del modelo (que se le va), sale del archivo.

### Qué es técnicamente una skill

Una carpeta con un archivo `SKILL.md` adentro. El archivo arranca con un encabezado
YAML de dos campos y después es markdown común:

```yaml
---
name: "prompts-ugc-celular"
description: "Escribe prompts de video para Seedance 2.5 en kie.ai con estética de
celular real. Usar cuando se pida un prompt, escena o clip de video realista
grabado con teléfono."
---
```

- **`name`**: el identificador.
- **`description`**: lo único que el modelo lee siempre. Es el disparador: si no dice
  con qué palabras del usuario tiene que activarse, la skill no se carga nunca. Por eso
  está escrita con los términos que vos realmente usás ("prompt", "escena", "clip",
  "video realista grabado con teléfono"), no con una definición abstracta.
- **El cuerpo**: se carga entero, solo cuando la skill se activa. Ahí va el método.

Ese diseño —descripción corta siempre visible, cuerpo pesado bajo demanda— es lo que
permite tener muchas skills sin saturar el contexto.

### Las dos skills de este proyecto

| Skill | Para qué | Modelo destino |
|---|---|---|
| `prompts-ugc-celular` | UGC realista de celular (Walter, restaurante, propina) | Seedance 2.5 vía kie.ai |
| `prompts-arthas-cachito` | Saga de fantasía, prompts cinematográficos de 10s | Gemini Omni 1.1 Flash / Veo 3.1 |

Son opuestas a propósito. La de UGC prohíbe todo lo cinematográfico; la de Arthas lo
exige. Comparten la misma lógica de construcción, no el mismo vocabulario.

---

## 2. El principio del que sale todo

> **La estética no se pide con adjetivos, se pide con causas físicas.**

Es la regla de la que se derivan las demás. "Realista", "estilo UGC", "auténtico",
"crudo" son etiquetas, y el modelo las resuelve buscando en su entrenamiento qué se
parece a eso: publicidad que imita UGC. Iluminada, encuadrada, actuada.

Lo que sí ejecuta son **causas concretas con consecuencia visible**:

| En vez de pedir | Pedir la causa |
|---|---|
| "cámara handheld realista" | "el operador reencuadra medio segundo DESPUÉS de que pasa la acción" |
| "iluminación natural" | "el autoexposure se ajusta cuando pasa frente a la ventana" |
| "imperfecto, casero" | "el horizonte se desvía y nadie lo corrige" |
| "toma con nervio" | "el brazo respira, tiembla, y se cansa: el encuadre baja de a poco" |
| "reacción emocional real" | "mira el objeto, mira al cliente, vuelve al objeto, y recién ahí entiende" |

La regla mental para chequear un prompt: **"alguien vio algo y levantó el teléfono"**,
nunca "una producción que intenta parecer casera".

### El corolario de la actuación

El error más caro no es de cámara, es de actuación: **la emoción instantánea**. Un
modelo de video, si le pedís "se emociona", te entrega la cara de emoción en el
fotograma 1. Eso lee como actuación mala y hunde el clip entero.

La solución es programar **latencia en etapas**:

```
ve → no entiende del todo → verifica (mira a la otra persona) → recién ahí reacciona
```

Y declararlo por bloques de tiempo, no como adjetivo.

La forma madura de esto, la que salió del pack de Walter, es la **dirección entre
paréntesis antes de cada línea**, escrita como mecánica física y no como emoción:

```
P1 (quiet, flat, stripped of everything — the sentence of a man who has said it many
times and has never made it easier, the last word almost inaudible): "My daughter
passed away."
```

Dónde respira, dónde se le cierra la garganta, dónde baja el volumen, dónde se queda sin
aire. Nunca "emotional" ni "sad". Y hay que declarar explícitamente, en el bloque base,
que **el paréntesis no se pronuncia**: sin esa línea el modelo lee la acotación en voz
alta.

---

## 3. La estructura del prompt

Siempre en este orden:

```
FORMATO → ESCENA/LOCACIÓN → PERSONAJE(S) → ACCIÓN por bloques de tiempo →
CÁMARA → IMAGEN (color y textura) → AUDIO → RESTRICCIONES
```

El orden importa por dos motivos verificados:

1. **Los prompts oficiales de ByteDance declaran formato y estilo al inicio.** Lo que
   va primero pesa más.
2. **La acción va segmentada por timestamps** (`0–6 s`, `6–12 s`, `12–20 s`,
   `20–25 s`). Sin esa segmentación el modelo adivina el timeline y produce saltos de
   cámara, drift de personaje y manos rotas.

### La regla de densidad

Los timestamps solos no alcanzan: **el modelo rellena con silencio el tiempo que le
sobra.** Medido en salidas reales, habla a **2,47 palabras por segundo**, así que un clip
de 30 s necesita **~70 palabras de diálogo**. Con 42 palabras devuelve 13 segundos de
aire muerto.

De ahí que los bloques de tiempo tengan que coincidir con el tiempo real de habla, y que
el prompt declare la densidad como instrucción propia: respuestas inmediatas, solapando
la última palabra del otro, medio segundo de aire entre líneas, y ningún hueco de más de
un segundo en todo el clip.

**Los silencios se hacen en el montaje, no en la generación.** Pedirle pausas emotivas al
modelo da aire vacío; pedirle densidad y después cortar da ritmo.

> **Ojo con esto**, porque acá las dos skills se contradicen y está bien que se
> contradigan: en Seedance los bloques de tiempo son obligatorios; en Omni/Veo los
> corchetes de tiempo (`[00:00 - 00:03]`) son la sintaxis oficial de **multi-plano** y
> hacen que el modelo **corte**. En un render medido, el prompt con timecodes cortó en
> 00:07.04 y los dos sin timecodes no cortaron. Regla por modelo, no universal.

### Las cuatro secciones que nunca se saltean

**Cámara.** Hay un banco de lenguaje cerrado. Para trasera: `1x, equivalente a 24–28
mm, sostenida con una mano, altura de pecho, a 1,5–2,5 m, microtemblor de respiración,
reencuadres tardíos, paneos de pocos grados, profundidad de campo amplia, motion blur
natural`. Y una lista de prohibidos que empujan al modo anuncio: `cinematic, shallow
depth of field, bokeh, gimbal, steadicam, dolly, slider, crane, drone, orbit, push-in,
rack focus, teal and orange, film grain, LUT, halation, anamorphic flare, beauty
filter, studio lighting, rim light, slow motion`.

**Imagen.** Luz práctica del lugar y mezclas feas de temperatura: un LED verdoso contra
luz de ventana es más creíble que un cálido perfecto. Contraste medio, saturación
moderada, sin grading.

**Audio.** El diálogo va **entre comillas** dentro del prompt — eso es lo que dispara
el lip-sync. Audio diegético: voces captadas por el mismo micrófono del teléfono, con
sala y distancia, más ambiente del lugar. **Sin música** (la música va en post).

Si se pasan audios de referencia, hay que declarar que son **solo timbre**: *"match
@Audio1 for timbre, age, accent and pitch only. Do not reproduce any words from @Audio1
and do not copy its emotion"*. Son dos bloqueos distintos y hacen falta los dos: sin el
primero el personaje repite palabras del sample, sin el segundo hereda su emoción y pisa
la dirección escrita.

**Restricciones.** En lenguaje natural, dentro del prompt. No existe un campo
"negative prompt" universal, así que se escriben como frases.

### Checklist antes de entregar

- ¿Hay bloques de tiempo con timestamps?
- ¿Cada imperfección tiene una causa declarada, no solo un adjetivo?
- ¿El diálogo está entre comillas?
- ¿Está la lista de restricciones (sin gimbal, sin cortes, sin música, sin texto)?
- ¿La reacción emocional tiene latencia?
- ¿Se declaró profundidad de campo amplia?
- ¿Nadie posa ni mira a cámara buscando aprobación?

---

## 4. Cómo se encadena un video largo

Un archivo de 5 minutos excede lo que genera el modelo. Se arma por **beats de 20–30 s**
con tres mecanismos de continuidad:

**1. Biblia de continuidad.** Un bloque fijo —personajes, ropa, local, luz, cámara,
sonido— que se repite **casi palabra por palabra** en cada prompt. Cambiar descriptores
entre escenas es la causa documentada de drift. No se reescribe: se copia y pega.

**2. Encadenado por último fotograma.** Se exporta el último frame limpio del clip
aprobado y entra como referencia de imagen del siguiente. Pero `return_last_frame` solo
te da el archivo: que el clip siguiente **empiece** exactamente ahí hay que pedirlo, y en
lenguaje extremo —*"@Image3 IS the first frame of this video, frame 1 must be @Image3
itself, pixel for pixel… treat it as the locked starting state of the scene, not as a
style reference"*—. Sin esa última frase el modelo lo toma como referencia de estilo y
recompone la escena.

**3. Uniones tapadas con acción física.** El corte cae donde la información visual es
mínima: alguien cruza el lente, un hombro tapa el cuadro, el operador baja el teléfono,
motion blur fuerte. Clip A termina con una espalda ocupando el 70% del cuadro; clip B
empieza con la misma espalda medio segundo y después se aparta. El espectador lee
movimiento de cámara, no regeneración.

Dejar **0,5–1 s de solapamiento narrativo** entre clips (no hace falta usarlo todo en
el montaje).

### Referencias @Image / @Video

Se enganchan **dentro del texto del prompt** con `@Image1`, `@Video1`. El número
corresponde al **orden en el array**, no a un nombre: si se reordena el array, la
referencia se liga en silencio a otro archivo. Por eso cada entrega dice explícitamente
en qué orden cargar las referencias.

Separar siempre **referencia de estilo** (cámara, luz, locación, vestuario) de
**referencia de identidad** (cara, cuerpo, voz). La segunda necesita autorización.

---

## 5. Parámetros en kie.ai

Endpoint `POST https://api.kie.ai/api/v1/jobs/createTask`, modelo `bytedance/seedance-2-5`.
Campos de `input`: `prompt`, `reference_image_urls`, `reference_video_urls`,
`reference_audio_urls`, `generate_audio`, `resolution`, `aspect_ratio`, `duration`,
`return_last_frame`.

| Parámetro | Valor para este estilo |
|---|---|
| `aspect_ratio` | `9:16` |
| `resolution` | `1080p` (Seedance 2.5 no tiene tier 4K real, aunque algunas páginas lo publiciten) |
| `duration` | 20–30 s por beat (5/10/15/20/25/30) |
| `generate_audio` | `true` para diálogo diegético |
| `return_last_frame` | `true` si el clip siguiente continúa la escena |

Dos cosas que muerden:

- **La API es asíncrona.** Devuelve solo un `taskId`; hay que consultar estado.
- **La URL del resultado es presignada y expira a las 24 h.** Hay que descargar el
  archivo, no guardar el link.

---

## 6. El caso Walter: lo que se hizo y lo que enseñó

El pack completo está en `ejemplos/WALTER-pack-final-8-escenas.txt`, con las notas de
lectura en `ejemplos/00-LEEME.md`. Es el mejor material trabajado del método: 8 escenas,
2:45 de resultado después de cortar silencios.

**Personaje.** Walter, mozo afroamericano de 82 años en un diner americano: polo beige
con chapita roja de nombre, delantal negro de media cintura, pantalón azul marino,
zapatos negros. Descrito en el prompt como *"a proud man who does not want pity and does
not want to be a burden"* — una línea de carácter que después explica por qué rechaza la
plata en S6.

**Referencias, en orden fijo:**

| Slot | Archivo | Para qué |
|---|---|---|
| `@Image1` | `triptico_walter.png` | Identidad — tres vistas |
| `@Image2` | `model_sheet_diner.png` | Locación |
| `@Image3` | último frame de la escena previa | First frame clavado |
| `@Video1` | `video_pantalla_celular.mp4` | Solo S7 y S8, dentro de la pantalla |
| `@Audio1` / `@Audio2` | `walter_14s.mp3` / `pov_14s.mp3` | Timbre de cada voz |

**Estructura modular.** Un `BLOQUE BASE` idéntico al inicio de las 8 escenas, y cada
escena declara qué **agrega**, qué **reemplaza** y qué **quita** de ese bloque. Es más
robusto que reescribir el bloque entero cada vez, y deja ver de un vistazo qué cambia
entre clips.

**Arco:** hook ("Eighty-two") → la hija → el peso → el pedido y los $100 → la reacción →
afuera, el rechazo y el abrazo → el teléfono → los $1700 y el apretón de manos.

**Tres personajes etiquetados** —P1 Walter, P2 el que filma, P3 la moza— cada uno con su
bloque de voz. Eso es lo que evita que el modelo le dé la línea al personaje equivocado.

**El POV declarado como causa, no como encuadre:** *"The camera IS P2's phone"*, y su voz
suena más fuerte que las demás **porque el teléfono está en su mano**.

### Las cuatro cosas que el pack agregó al método

1. **Densidad de diálogo** (§3): ~70 palabras por 30 s, o el clip se llena de aire.
2. **Dirección entre paréntesis** como mecánica física, con la declaración de que no se
   pronuncia.
3. **Audios de referencia = solo timbre**, con los dos bloqueos explícitos.
4. **First frame clavado**: `return_last_frame` te da el archivo, pero que el clip
   *empiece* ahí hay que pedirlo en lenguaje extremo —*"@Image3 IS the first frame…
   treat it as the locked starting state of the scene, not as a style reference"*—. Y
   cuando una escena no continúa (S6, que salta afuera), el bloque se quita entero y se
   reemplaza por una instrucción de ruptura.

Y dos técnicas finas: **el corte pedido como hecho diegético** (el jump cut de S4 es "el
que filma paró la grabación mientras comía", no un corte de montaje) y **el aislamiento
de `@Video1` dentro de la pantalla del teléfono**, para que la referencia de video no
contamine el estilo del plano.

### Dos cosas para corregir

**El idioma.** La skill dice "escribir en español salvo que pida lo contrario", y el pack
que funcionó está íntegramente en inglés. Tiene sentido y conviene volverlo regla: **el
prompt se escribe en el idioma en que se habla en el video.** Mezclar idioma de
instrucción con idioma de diálogo invita a que el modelo cruce los dos.

**La resolución.** Los `.mp4` en `Downloads/Walter` miden **480×854**. Es 9:16 correcto,
pero un cuarto de 1080p. Si los jobs se mandaron con `resolution: "1080p"`, lo que bajó es
un preview y hay que buscar el asset bueno antes de que expire la URL a las 24 h. Es la
causa más barata y más grande de que un video se vea mal, y ningún prompt la compensa.

## 7. Cómo se escribe un prompt, paso a paso

El procedimiento real, en orden:

1. **Ubicar el beat en el arco.** Hook → contexto → rapport → construcción →
   preparación → revelación → procesamiento → emoción → abrazo → aftermath →
   significado → resolución → cierre imperfecto. Cada beat tiene una función
   dramática, y el prompt se escribe para cumplir esa función, no para "mostrar la
   escena".
2. **Pegar la biblia de continuidad** sin tocar una palabra.
3. **Escribir el timeline por bloques**, repartiendo la acción física en 25–30 s.
   Un beat físico cada 5–8 s; más que eso se atropella.
4. **Contar las palabras de diálogo.** ~2,47 por segundo de clip. Si faltan, el modelo
   devuelve silencio; si sobran, atropella. Los bloques de tiempo tienen que coincidir
   con el tiempo real de habla.
5. **Poner latencia en la reacción.** Si hay emoción, va en etapas y se declara en qué
   segundo pasa cada etapa.
6. **Escribir el diálogo entre comillas**, con la dirección emocional entre paréntesis
   antes de cada línea, escrita como mecánica física. Nadie mira a cámara.
7. **Elegir las causas de imperfección** que correspondan a lo que pasa en el plano:
   si alguien se levanta, hay reencuadre tardío y motion blur; si se cruza la ventana,
   caza el autoexposure.
8. **Cerrar con restricciones** en lenguaje natural, ajustadas a lo que sí pasa en ese
   clip (si hay un corte diegético, la restricción "no cuts" hay que reemplazarla).
9. **Decir en qué orden van las referencias.**
10. **Pasar el checklist.**

---

## 8. Lo que hay que cambiar para ChatGPT

Ver `chatgpt/COMO-ADAPTAR.md` para el detalle y los archivos listos para pegar. El
resumen:

- **El método es portable, el formato no.** Nada de lo que hace funcionar estos prompts
  depende de Claude: son reglas sobre cómo responde el modelo de **video**, no el de
  texto.
- **ChatGPT no tiene carga automática por descripción.** No existe el equivalente exacto
  de una skill que se activa sola. El cuerpo del `SKILL.md` va en el campo
  **Instructions** de un GPT personalizado (límite ~8.000 caracteres) o en las
  instrucciones de un Proyecto.
- **El `SKILL.md` de UGC entra justo; el de Arthas no.** Ese hay que partirlo:
  instrucciones = método, y las tablas de personajes y lore van como archivos de
  **Knowledge**.
- **El encabezado YAML se tira.** En ChatGPT no cumple ninguna función.
