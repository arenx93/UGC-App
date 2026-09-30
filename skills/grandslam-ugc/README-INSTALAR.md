# grandslam-ugc — instalación en Claude Code local

Skill para que un agente de Claude en tu Mac haga **todo el ciclo** de un video grand slam de
seguros de auto: guion → referencias → prompts de Seedance 2.5 → generación en kie.ai → edición con
FFmpeg → control de calidad contra los 9 ganadores.

Reemplaza y une las dos skills anteriores (`prompts-ugc-celular` y `editar-videos-ugc`). Si las tenés
instaladas, podés dejarlas, pero conviene desactivarlas para que no compitan.

## 1. Instalar

```bash
# desde la raíz de este repo
mkdir -p ~/.claude/skills
cp -R skills/grandslam-ugc ~/.claude/skills/
# o, para que se actualice con git pull:
ln -s "$(pwd)/skills/grandslam-ugc" ~/.claude/skills/grandslam-ugc
```

Dependencias (una vez):
```bash
brew install ffmpeg                 # trae ffmpeg y ffprobe
python3 -m pip install pillow faster-whisper
```
Fuentes (recomendado): bajar **Anton** y **Montserrat** de Google Fonts y dejar
`Anton-Regular.ttf` y `Montserrat-SemiBold.ttf` en `fuentes/` dentro de la carpeta de trabajo.

Clave de kie: en `~/.zshrc` → `export KIE_API_KEY=...` (no la pegues en el chat ni en archivos del proyecto).

## 2. Carpeta de trabajo sugerida

```
~/GrandSlams/
  CLAUDE.md                 (el bloque de abajo)
  fuentes/  Anton-Regular.ttf  Montserrat-SemiBold.ttf
  cola/     cola_52.90.mp4           ← asset reutilizable
  vo/       testimonio_52.90.mp3
  videos/<slug>/  guion.md  plan_escenas.json  prompts/  refs/  clips/  transcripciones/  plan.json
```

`CLAUDE.md` de esa carpeta:
```markdown
# Grand slams UGC
- Usá siempre la skill grandslam-ugc para guiones, prompts de Seedance, generación en kie y edición.
- Conversación en español; todo lo que se dice o se lee en el video, en inglés.
- Nunca corras `kie.py crear` ni `cadena` sin que yo confirme el costo en el chat. Después de mi "dale", usá `--si`.
- Primero 480p para validar; 1080p solo en clips aprobados.
- Los scripts de la skill están en ~/.claude/skills/grandslam-ugc/scripts/.
```

## 3. Cómo pedirle cosas al agente

- "Armame un grand slam nuevo con una abuela que hace entregas" → plan + guion (paso 1).
- "Escribí los prompts de S1 a S5" → prompts con verificación y `kie.py revisar`.
- "Generá S1 y S2 en 480p" → muestra costo, espera tu OK, genera, descarga y revisa.
- "Editá el video con los clips de videos/abuela" → transcribe, monta, mide y entrega `final.mp4` con la tabla QA.
- "Medí este ganador nuevo" → `medir.py` y agrega sus números a la anatomía.

## 4. Qué cambió respecto de las skills anteriores

| Tema | Antes | Ahora (medido en los ganadores) |
|---|---|---|
| Entrada del producto | "no antes del 70%" | teléfono al **48–74%** (mediana 65%), primer regalo al 33–46% |
| Cámara del prompt | 1x, 24–28 mm, a 1,5–2,5 m | **0.5x ultra gran angular**, 0,6–1,5 m, **manos del que filma en cuadro** |
| Hook largo | — | plano escondido (parabrisas/mesa), zoom 2x, **narración susurrada generada en S1** |
| Densidad | presupuesto 2,47 pal/seg al generar | + objetivo **≥ 3,3 pal/seg en el final** (reales 3,5–4,2) → generar ~30% de más y cortar |
| Audios de referencia | 10–20 s c/u | **12–15 s c/u** (el total de audios no puede pasar 30 s) |
| Resolución | 1080p siempre | **480p para validar**, 1080p para los aprobados |
| Diálogo | — | líneas ≤ 12 palabras, cifras en palabras, auto con marca y año, "There's one more thing I want to do for you" |
| Montaje | cortes por clip | `tramos` (jump cuts dentro de un clip) + `zoom` (punch-in) + cola desde imagen |
| Verificación | a mano | `kie.py revisar` (lint) y `medir.py` (contra los ganadores) |
| Bug | `montar.py` re-temporizaba el video a 25 fps al recortar silencios → audio y video se desfasaban | corregido (30 fps fijo) |

## 5. Límites conocidos

- La doc oficial de kie no se pudo abrir al escribir esto: los nombres de campos salen de la doc
  indexada, de guías públicas y de la integración que ya usa Framecraft. `references/05-kie-api.md`
  pide confirmarlos en el primer uso. La subida y la creación de tareas de `kie.py` no se probaron
  contra el servidor real.
- La detección automática de cortes de `medir.py` no ve jump cuts dentro del mismo plano (±2).
- Las reglas pesan igual para los 9 ganadores: no había métricas de rendimiento por video.
