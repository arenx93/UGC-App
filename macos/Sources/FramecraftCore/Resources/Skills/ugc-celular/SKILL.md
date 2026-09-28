---
name: "prompts-ugc-celular"
description: "Escribe prompts de video para Seedance 2.5 en kie.ai con estética de celular real (UGC handheld, gran angular, imperfecciones causales). Usar cuando se pida un prompt, escena o clip de video realista grabado con teléfono."
---

# Prompts UGC de celular para Seedance 2.5 (kie.ai)

El usuario genera siempre con **Seedance 2.5 vía kie.ai**. Todo prompt de video que se le entregue debe verse como grabado con un teléfono real por una persona común, nunca como publicidad o cine. Escribir el prompt en español salvo que pida lo contrario.

## Principio rector

**La estética de celular no se pide con adjetivos, se pide con causas físicas.**

"Realista, estilo UGC, auténtico" son etiquetas que el modelo interpreta como "publicidad que imita UGC". Lo que sí ejecuta son causas concretas:

- el operador reencuadra DESPUÉS de que pasa la acción, nunca antes;
- el autoexposure se ajusta visiblemente cuando cambia la luz en cuadro;
- el horizonte se desvía y no se corrige;
- el brazo respira y tiembla;
- durante movimientos rápidos aparece motion blur.

Regla mental: *"alguien vio algo y levantó el teléfono"*, no *"una producción que intenta parecer casera"*.

## Estructura del prompt (en este orden)

```
FORMATO → ESCENA/LOCACIÓN → PERSONAJE(S) → ACCIÓN por bloques de tiempo →
CÁMARA → IMAGEN (color y textura) → AUDIO → RESTRICCIONES
```

Los prompts oficiales de ByteDance declaran formato y estilo al inicio y después segmentan la acción por timestamps (`0–5s`, `6–10s`, `11–20s`). Usar siempre esa segmentación: sin ella el modelo adivina el timeline y produce saltos de cámara, drift de personaje y manos rotas.

## Banco de lenguaje de cámara (usar)

**Cámara trasera (observador):**
- cámara trasera 1x, lente gran angular equivalente a ~24–28 mm
- sostenida con una mano, altura de pecho a ojos, a 1,5–2,5 m del sujeto
- microtemblor de respiración constante
- reencuadres pequeños y medio segundo tardíos, motivados por lo que hace la gente
- paneos de pocos grados, nunca geométricos
- profundidad de campo amplia: el fondo se lee entero
- motion blur natural en movimientos rápidos
- el autoexposure caza la luz al pasar frente a ventana o lámpara
- alguien puede tapar parcialmente el lente por un instante

**Cámara frontal (selfie):**
- cámara frontal, brazo extendido, leve distorsión de gran angular
- encuadre apenas por encima del nivel de los ojos, composición ladeada
- el brazo se cansa y el encuadre baja de a poco
- textura de piel visible: poros, líneas finas, brillo natural

**Prohibido siempre** (empuja al modo anuncio): cinematic, shallow depth of field, bokeh, gimbal, steadicam, dolly, slider, crane, drone, orbit, push-in dramático, rack focus, teal and orange, film grain, LUT, halation, anamorphic flare, beauty filter, professional color grade, studio lighting, rim light, slow motion.

## Luz y color

Pedir siempre luz práctica del lugar (ventana, plafón, tubo LED, luz de local) y aceptar mezclas feas de temperatura: un LED verdoso contra luz de ventana es más creíble que un cálido perfecto. Contraste medio, saturación moderada, sin grading.

## Actuación

El error más común es emoción instantánea. Programar latencia: mirar → no entender del todo → verificar con la otra persona → recién ahí reaccionar. Nadie mira fijo a cámara buscando aprobación. Nadie posa.

## Diálogo y audio

- El diálogo va **entre comillas** dentro del prompt: eso dispara el lip-sync.
- Pedir audio diegético: voces captadas por el mismo micrófono del teléfono, con sala y distancia, más ambiente del lugar. Sin música (la música va en post).
- `generate_audio` viene en `true` por defecto en la API: si se quiere clip mudo, hay que apagarlo explícitamente.

## Referencias (@Image / @Video)

Las referencias se enganchan **dentro del texto del prompt** con `@Image1`, `@Video1`, etc. El número corresponde al **orden de los ítems en el array**, no a un nombre: si se reordena el array, la referencia se liga silenciosamente a otro archivo. Avisarle esto al usuario cuando pase referencias.

Usar referencias para estilo, encuadre, locación, vestuario y props. Para identidad de personas reales hace falta autorización: separar siempre "referencia de estilo" de "referencia de identidad".

## Parámetros en kie.ai

Endpoint `POST https://api.kie.ai/api/v1/jobs/createTask`, modelo `bytedance/seedance-2-5`. Campos de `input`: `prompt`, `reference_image_urls`, `reference_video_urls`, `reference_audio_urls`, `generate_audio`, `resolution`, `aspect_ratio`, `duration`, `return_last_frame`.

Preset para este estilo:

| Parámetro | Valor |
|---|---|
| aspect_ratio | `9:16` |
| resolution | `1080p` — Seedance 2.5 no tiene tier 4K real, aunque algunas páginas lo publiciten |
| duration | 20–30 s por beat (5/10/15/20/25/30) |
| generate_audio | `true` para diálogo diegético |
| return_last_frame | `true` si el clip siguiente continúa la escena |

La API es asíncrona: devuelve solo un `taskId` y hay que consultar el estado. La URL del resultado es presignada y **expira a las 24 h**, así que hay que descargar el archivo, no guardar el link.

## Video largo (varios minutos)

Un solo archivo de 5+ minutos excede la generación. Armar por beats de 20–30 s con una "biblia de continuidad" fija (personajes, ropa, local, luz, cámara, sonido) que se repite casi palabra por palabra en cada prompt, y esconder las uniones con acción física: alguien cruza el lente, un hombro tapa el cuadro, el operador baja el teléfono. Dejar 0,5–1 s de solapamiento narrativo entre clips.

## Checklist antes de entregar un prompt

- ¿Hay bloques de tiempo con timestamps?
- ¿Cada imperfección tiene una causa declarada, no solo un adjetivo?
- ¿El diálogo está entre comillas?
- ¿Está la lista de restricciones (sin gimbal, sin cortes, sin música, sin texto)?
- ¿La reacción emocional tiene latencia?
- ¿Se declaró profundidad de campo amplia?
- ¿Nadie posa ni mira a cámara buscando aprobación?