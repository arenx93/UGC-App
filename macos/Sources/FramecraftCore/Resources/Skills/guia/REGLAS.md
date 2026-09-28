# Kit de prompts de video — Seedance 2.5 / Omni

Todo el sistema con el que se escriben los prompts, empaquetado para poder leerlo,
adaptarlo a ChatGPT o llevarlo a cualquier otro asistente.

## Por dónde empezar

1. **`00-COMO-FUNCIONA.md`** — el resumen: qué es una skill, el método completo, cómo se
   escribe un prompt paso a paso y qué enseñó el caso Walter.
2. **`ejemplos/`** — el pack real de Walter: 8 escenas que funcionaron, con las notas de
   lectura de por qué funcionaron.
3. **`chatgpt/COMO-ADAPTAR.md`** — cómo montarlo en ChatGPT, con los archivos ya cortados
   al límite de caracteres.

## Contenido

```
00-COMO-FUNCIONA.md                    El resumen del sistema

skills-claude/                         Las skills tal como están hoy
  prompts-ugc-celular/SKILL.md           UGC de celular · Seedance 2.5 en kie.ai
  prompts-arthas-cachito/SKILL.md        Saga de fantasía · Omni 1.1 Flash / Veo 3.1

ejemplos/
  00-LEEME.md                            Qué mirar del pack y por qué funcionó
  WALTER-pack-final-8-escenas.txt        El pack real: 8 escenas, 2:45 de resultado

chatgpt/
  COMO-ADAPTAR.md                        Guía de migración
  INSTRUCCIONES-GPT-ugc-celular.md       Listo para pegar (~7.700 car.)
  INSTRUCCIONES-GPT-arthas-cachito.md    Listo para pegar (~7.990 car.)
  knowledge-ugc/PARAMETROS-Y-ARCO.md     Archivo de Knowledge
  knowledge-arthas/ELENCO-LORE-FILTROS.md  Archivo de Knowledge

referencia/
  Como generar UGC con Seedance...pdf    El documento base del método
```

## Las cinco reglas, en una pantalla

1. **La estética se pide con causas físicas, no con adjetivos.** "Realista" es una
   etiqueta; "el operador reencuadra medio segundo después de que pasa la acción" es una
   causa con consecuencia visible.
2. **La acción va segmentada por timestamps.** Sin eso el modelo adivina el timeline.
3. **El diálogo tiene densidad medible: ~2,47 palabras por segundo.** El modelo rellena
   con silencio el tiempo que le sobra. Los silencios se hacen en el montaje.
4. **La emoción va entre paréntesis y escrita como mecánica física** —dónde respira,
   dónde baja el volumen—, con la aclaración de que el paréntesis no se pronuncia.
5. **La continuidad se copia y pega, no se reescribe.** Cambiar descriptores entre
   escenas es la causa documentada de drift.
