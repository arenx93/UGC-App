# Elenco, lore y filtros — Arthas y Cachito

Archivo de **Knowledge** para el GPT. Complementa el campo Instructions: ahí está el
método, acá están los datos fijos de la saga.

---

## Elenco canónico (claves de `personajes_db.json`)

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

## Lore y capítulos

**El gran secreto (no revelar antes del Cap 19):** la banda no pelea entre sí, son aliados recorriendo las curias reclutando gente y artillería. Creen que **Zovaal secuestró a Cachito**: es falso. **El Primus es el titiritero**, controla a Cachito con el Sello del Primus para robar los 4 sellos y abrir **Zereth Mortis**. Zovaal y Denathrius sólo fingían ser los villanos para frenarlo.

01 Bastión · 02 Cachito roba el Sello · 03 Nace el Dreamteam · 04 Revendreth, el Orco · 05 Salones de Expiación · 06 Castillo Nathria · 07 Maldraxxus · 08 Tanques de hueso · 09 Trono vacío · 10 Ardenweald · 11 La Reina del Invierno · 12 El Colorado · 13 Profesor X · 14 Las Fauces · 15 Río de Almas · 16 Torghast · 17 Illidan en pantuflas · 18 Batalla contra el Carcelero · 19 EL PLOT TWIST · 20 Zereth Mortis.

Desde el Cap 12, estética 80s analógica (Eastman 5247, prótesis prácticas). Antes, Kodak Vision3 5219.

## Filtros de seguridad

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

## Drama: que la escena tenga una idea

Una escena técnicamente correcta puede ser dramáticamente vacía. Es una comedia: cada toma necesita **una idea que actuar**, no sólo una maniobra que ejecutar.

- **Poner el chiste en escena, no narrarlo.** "De tres maneras descoordinadas" es contarle el chiste al modelo; escalonar las reacciones y dejar que se vea.
- **Usar el contraste entre personajes.** Svetlana trabaja como cirujana, sin un gesto de más; el Orco hace lo contrario en todo y la mira con devoción. Ella no le devuelve la mirada.
- **Un gesto vale más que una línea.** Illidan quieto, gujas al piso, girando despacio la cabeza hacia Marcus, es el personaje entero y además le ahorra al modelo una acción compleja.
- **Dirección de actuación explícita**: *"her face does not change"*, *"open helpless devotion"*.

## Reglas del proyecto refutadas

No aplicarlas sin avisar que están en duda:

- **"Las comillas queman subtítulos"** — la sintaxis oficial las usa; el render con comillas no produjo subtítulos.
- **"Estructura de 6 o 15 bloques con timecodes"** — es la gramática de multi-plano; causa cortes.
- **"Muro de negativos"** — desaconsejado oficialmente, sin aporte medible.
- **"16 a 20 palabras de diálogo"** — sin respaldo y choca con los factores WPS del propio documento. Entran 10 a 14.

Lo que **sí** sobrevive: el lore, los acentos, la base de personajes, la tabla de seguridad, el tope de 3 personajes, la variedad de encuadres (nada de tomas dorsales repetidas) y el anclaje gravitatorio al describir pisadas y peso.

## Entrega y medición

Prompt completo en bloque de código listo para copiar, y escrito a `prompts/capNN_escMM.md` con `device_commit_files`. Decir siempre **en qué orden cargar las referencias de imagen**. Cerrar con el conteo de palabras del prompt y del diálogo.

Después de cada render que mande el usuario, **medirlo**:
- cortes: `ffmpeg -i v.mp4 -vf "select='gt(scene,0.12)',metadata=print:file=-" -f null -`
- ventana de silencio: extraer wav, RMS y energía 300-3000 Hz por ventanas de 0,5s
- identidad y geometría: extraer fotogramas y revisarlos de a uno
- resolución real con `ffprobe`: si dice 640x360, es borrador

Anotar el resultado en `BITACORA_OMNI.md` separando lo verificado de lo supuesto. Nunca prometer consistencia garantizada: es una red estocástica y lo único que se garantiza es la validación formal.
