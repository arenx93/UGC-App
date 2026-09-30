# Ejemplo completo — "74YO STILL DELIVERING GROCERIES"

Video nuevo armado con todo el método. Los 5 prompts están en `ejemplo/prompts/S1–S5.txt`, el
`ejemplo/plan_escenas.json` pasa `kie.py revisar` sin fallas y `ejemplo/plan.json` es el montaje
previsto. Usarlo como modelo de forma, no como texto a copiar (cada video lleva su propio personaje).

## 1. Plan

```
PERSONAJE P1: 74, abuela que hace entregas de súper para criar a su nieto de 7 (su hijo murió el año pasado)
              — situación visible: carga bolsas pesadas al sol en el estacionamiento
              — auto: Toyota Corolla 2009 plateado, paragolpes abollado — paga hoy: $240/mes
P2: el que filma (manos: piel morena clara, reloj negro de silicona, buzo gris)   P3: nieto, no habla
GESTO 1: 5 billetes de $100 "for today's orders"      CIFRAS: "sixteen hundred dollars back" · "fifty-one a month, full coverage"
HOOK: largo observacional (desde el auto) + narración de P2
      texto: "74YO STILL DELIVERING GROCERIES / TO RAISE HER GRANDSON / SO I DID THIS FOR HER" (L3 amarilla) ❤️🥺, y=0.28 (abajo está el capó)
COLA: asset $52.90 existente + testimonio A

| Clip | Dur gen. | Tramo                              | Queda | % final   | Palabras | Locación / hora          | @Image3 |
|------|----------|------------------------------------|-------|-----------|----------|--------------------------|---------|
| S1   | 15 s     | HOOK largo + narración             | ~13 s | 0–14%     | 40/37    | estacionamiento, 13 h    | no      |
| S2   | 25 s     | ENCUENTRO + DOLOR                  | ~18 s | 14–33%    | 65/62    | idem                     | S1      |
| S3   | 25 s     | CONTEXTO + GESTO $500              | ~18 s | 33–52%    | 61/62    | idem                     | S2      |
| S4   | 30 s     | PUENTE + TELÉFONO + $1,600 back    | ~22 s | 52–75%    | 72/74    | idem, junto al Corolla   | S3      |
| S5   | 10 s     | $51 + reacción + abrazo (lente)    | ~8 s  | 75–83%    | 26/25    | idem                     | S4      |
| COLA | —        | papel $52.90 + testimonio (VO)     | 20 s  | 83–100%   | VO ~52   | interior                 | —       |
Generado: 105 s (≈US$15 en 480p) · FINAL estimado: ~1:39 · GESTO ~0:40 (40%) · TELÉFONO ~0:57 (58%)
```

Chequeo contra los ganadores: duración ✓ (1:20–2:00) · gesto 33–46% ✓ · teléfono 48–74% ✓ · cifra
dicha dos veces con reacción ✓ ("Sixteen hundred dollars back" → "What? Shut up!"; "Fifty-one a
month, full coverage" → "Instead of two-forty? Oh my gosh.") · paga más de $80 ✓ · auto con marca y
año ✓ · primer plano emocional sin texto (S3 21–25 s) ✓ · abrazo que tapa el lente para texto (S5) ✓.

## 2. Referencias a producir

| Archivo | Cómo | Nota |
|---|---|---|
| `refs/P1_triptico.png` | GPT Image 2 con el prompt de tríptico (`03` §2): 74-year-old white American woman, small, slightly hunched, short curly grey hair, thin gold-rimmed glasses, freckles; faded navy polo, lime-green reflective delivery vest, light-wash jeans, white velcro sneakers, black fanny pack across the chest | La ropa = la del bloque CHARACTERS |
| `refs/parking_sheet.png` | Model sheet 2×2 de estacionamiento de súper a las 13 h, con el Corolla 2009 plateado abollado y el baúl abierto con bolsas de papel. Sin carteles legibles | Incluye el auto: se nombra en S4 |
| `refs/pantalla_10s.mp4` | Screen recording del cotizador: marcas → Toyota → año 2009 → descuentos → WHAT WE FOUND FOR YOU → missed savings **$1,6xx** → cuota **$51** | Cifras = las del diálogo |
| `refs/voz_P1.mp3` | 14 s, mujer de 70+, acento general americano, lectura neutra | total audios ≤ 30 s |
| `refs/voz_P2.mp3` | 14 s, hombre de 30, cálido, lectura neutra, mismo nivel | |

## 3. Generación

```bash
cd ejemplo
python3 ../scripts/kie.py revisar plan_escenas.json      # ✓ en las 5
python3 ../scripts/kie.py costo plan_escenas.json        # 105 s en 480p ≈ US$14.70
python3 ../scripts/kie.py cadena plan_escenas.json       # pide confirmación; encadena último frame
# revisar cada clip; aprobado el guion, cambiar "resolution" a "1080p" y regenerar los aprobados
```

## 4. Montaje previsto (`ejemplo/plan.json`)

| # | Tramo | Clip in–out | Texto encima | Voz |
|---|---|---|---|---|
| 1 | HOOK | S1 0.0–13.5 | hook (0–13) | narración P2 (en el clip) |
| 2 | ENCUENTRO | S2 0.4–11.2 en 2 tramos (1.0 / 1.15) | Watch this... (13–21) | diálogo |
| 3 | DOLOR | S2 11.2–24.9 en 2 tramos | This broke my heart (21–31) | diálogo |
| 4 | CONTEXTO | S3 0.3–12.0 en 2 tramos | "The car needs gas, he needs shoes" 🥺 (31–38) | diálogo |
| 5 | GESTO | S3 12.0–21.0 en 2 tramos | I gave her $500 for today's orders.. 💚 (38–44) | diálogo |
| 6 | LLANTO | S3 21.0–24.9 | (nada) | diálogo |
| 7 | PUENTE | S4 0.3–18.0 en 3 tramos | I have a big surprise for her (47–60) | diálogo |
| 8 | TELÉFONO | S4 18.0–29.9 | (nada, solo subtítulos) | diálogo |
| 9 | CUOTA + ABRAZO | S5 0.0–9.8 | If your bill is over $80 you can do this too (sobre el abrazo) | diálogo |
| 10 | PLACA | 1,5 s | IF YOU PAY MORE THAN $80MO CHECK BELOW ❤️🥺 | — |
| 11 | COLA | cola_52.90 0–20 | (nada) | testimonio A (VO) |

Los tiempos de textos del `plan.json` son estimados: se ajustan después de mirar la hoja de `base.mp4`.
