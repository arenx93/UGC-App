# Parámetros de kie.ai y arco narrativo

Archivo de **Knowledge** para el GPT de UGC. En Instructions está el método; acá los
datos de referencia.

## Parámetros kie.ai

`POST https://api.kie.ai/api/v1/jobs/createTask`, modelo `bytedance/seedance-2-5`.
Campos de `input`: prompt, reference_image_urls, reference_video_urls,
reference_audio_urls, generate_audio, resolution, aspect_ratio, duration,
return_last_frame.

Preset: `aspect_ratio` 9:16 · `resolution` 1080p (no hay tier 4K real) · `duration`
20–30 s por beat (valores 5/10/15/20/25/30) · `generate_audio` true · `return_last_frame`
true si el clip siguiente continúa la escena.

La API es asíncrona: devuelve un `taskId` y hay que consultar estado. La URL del
resultado es presignada y **expira a las 24 h**: hay que descargar el archivo, no
guardar el link.


## Video largo

Un archivo de 5+ minutos excede la generación. Armar por beats de 20–30 s con una
**biblia de continuidad** fija (personajes, ropa, local, luz, cámara, sonido) que se
repite casi palabra por palabra en cada prompt: copiar y pegar, nunca reescribir, porque
cambiar descriptores entre escenas es la causa documentada de drift.

Encadenar por último fotograma: exportar el último frame limpio del clip aprobado y
usarlo como referencia de imagen del siguiente. `return_last_frame` te da el archivo,
pero que el clip **empiece** ahí hay que pedirlo en lenguaje extremo: *"@ImageN IS the
first frame of this video. Frame 1 must be @ImageN itself, pixel for pixel… Do not
reinterpret it, do not recompose it. Treat it as the locked starting state of the scene,
not as a style reference."* Si la escena NO continúa (cambio de locación u hora), quitá
ese bloque entero y reemplazalo por una instrucción de ruptura explícita: *"build this
opening from the description, do NOT continue any previous framing"*.

Esconder las uniones con acción física: alguien cruza el lente, un hombro tapa el cuadro,
el operador baja el teléfono, motion blur fuerte. El corte cae donde la información
visual es mínima. Dejar 0,5–1 s de solapamiento entre clips.

Un corte dentro de un clip se puede pedir, pero como **hecho diegético** ("el que filma
paró la grabación y la volvió a empezar"), declarando el segundo exacto y cambiando la
restricción de "no cuts" por "exactly ONE hard jump cut".

## Arco narrativo de una pieza larga

Arco típico de una pieza larga: hook → contexto → rapport → construcción → preparación →
revelación → procesamiento → emoción → abrazo → aftermath → significado → resolución →
cierre imperfecto. El cierre no lleva fade to black, end card, CTA ni logo: termina de
forma accidental y humana.

Cada beat tiene una función dramática y el prompt se escribe para cumplir esa función,
no para "mostrar la escena". Los beats intermedios ("base") se escriben con la misma
plantilla cambiando solo el bloque de acción.

## Estructura modular recomendada

Un **BLOQUE BASE** idéntico al inicio de cada escena —formato, first frame, ritmo, POV,
personajes, voces, locación, cámara, imagen, audio, actuación, restricciones— y después,
por escena, solo lo que **se agrega**, lo que **se reemplaza** y lo que **se quita** de
ese bloque. Es más robusto que reescribir el bloque entero cada vez y deja ver de un
vistazo qué cambia entre clips.

## Overlays en post

Texto largo donde la imagen está vacía (un hombro tapando el lente, movimiento, manos,
objetos, transiciones). Texto corto o nada sobre caras y reacciones: un primer plano
emocional va limpio, la cara lo dice todo.
