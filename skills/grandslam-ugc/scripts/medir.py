#!/usr/bin/env python3
"""Mide un video contra los números de los 9 grand slams.

Uso:
  python3 medir.py final.mp4 [--srt base.srt] [--cola 20] [--gesto 44.5] [--telefono 71] [--json]

--cola      segundos de cola al final (se excluyen del silencio y de la densidad de habla)
--gesto     segundo del video en que se entrega la plata/ayuda  -> % del total
--telefono  segundo en que aparece el teléfono/app             -> % del total
Sirve también para medir un ganador nuevo y sumar sus números a references/01-anatomia-grandslam.md.
"""
import json, re, shutil, statistics, subprocess, sys

REF = {  # rango aceptable, medido en los ganadores
    "duracion": (80, 120, "1:20–2:00 (ganadores 0:59–2:52, mediana 1:48)"),
    "silencio_pct": (0, 2.0, "0–2% bajo −30 dB"),
    "hueco_max": (0, 0.6, "ningún hueco > 0,6 s"),
    "cortes": (10, 21, "10–21 cortes"),
    "toma_mediana": (2.9, 6.3, "toma mediana 2,9–6,3 s"),
    "lufs": (-19, -13, "−13 a −19 LUFS (objetivo −16)"),
    "pal_seg": (3.3, 4.3, "≥3,3 pal/seg (reales 3,5–4,2)"),
    "gesto_pct": (33, 46, "gesto al 33–46%"),
    "telefono_pct": (48, 74, "teléfono al 48–74%"),
}

def ff(args):
    return subprocess.run(["ffmpeg", "-hide_banner", "-nostats"] + args, capture_output=True, text=True).stderr

def info(v):
    e = ff(["-i", v])
    h, m, s = re.search(r"Duration: (\d+):(\d+):([\d.]+)", e).groups()
    d = int(h) * 3600 + int(m) * 60 + float(s)
    wh = re.search(r"Video:.*?(\d{3,5})x(\d{3,5})", e)
    fps = re.search(r"([\d.]+) fps", e)
    return d, (wh.group(1) + "x" + wh.group(2)) if wh else "?", float(fps.group(1)) if fps else 0

def silencios(v, hasta):
    e = ff(["-t", str(hasta), "-i", v, "-vn", "-af", "silencedetect=n=-30dB:d=0.3", "-f", "null", "-"])
    s = [float(x) for x in re.findall(r"silence_start: (-?[\d.]+)", e)]
    t = [float(x) for x in re.findall(r"silence_end: ([\d.]+)", e)]
    t += [hasta] * (len(s) - len(t))
    gaps = [b - a for a, b in zip(s, t)]
    return sum(gaps), max(gaps) if gaps else 0.0

def cortes(v, d):
    e = ff(["-i", v, "-vf", "select='gt(scene,0.35)',showinfo", "-an", "-f", "null", "-"])
    ts = [float(x) for x in re.findall(r"pts_time:([\d.]+)", e)]
    ts = [t for i, t in enumerate(ts) if i == 0 or t - ts[i - 1] > 0.5]
    bordes = [0.0] + ts + [d]
    tomas = [b - a for a, b in zip(bordes, bordes[1:])]
    return len(ts), statistics.median(tomas), ts

def lufs(v):
    e = ff(["-i", v, "-vn", "-af", "ebur128", "-f", "null", "-"])
    m = re.findall(r"I:\s+(-?[\d.]+) LUFS", e)
    return float(m[-1]) if m else None

def leer_srt(p):
    def sec(x):
        h, m, r = x.strip().replace(",", ".").split(":"); return int(h) * 3600 + int(m) * 60 + float(r)
    ev = []
    for b in re.split(r"\n\s*\n", open(p, encoding="utf-8").read().strip()):
        l = b.strip().splitlines()
        if len(l) >= 3 and "-->" in l[1]:
            a, c = l[1].split("-->"); ev.append((sec(a), sec(c), " ".join(l[2:])))
    return ev

def main():
    a = sys.argv[1:]
    if not a or a[0].startswith("-"): print(__doc__); sys.exit(0)
    if not shutil.which("ffmpeg"): sys.exit("falta ffmpeg")
    v = a[0]
    opt = lambda k, f=float: f(a[a.index(k) + 1]) if k in a else None
    cola = opt("--cola") or 0.0
    d, wh, fps = info(v)
    escena = d - cola
    sil, hueco = silencios(v, escena)
    n, med, ts = cortes(v, d)
    r = {"duracion": round(d, 1), "resolucion": wh, "fps": fps, "silencio_pct": round(100 * sil / escena, 1),
         "hueco_max": round(hueco, 2), "cortes": n, "toma_mediana": round(med, 1), "lufs": lufs(v),
         "cortes_en": [round(t, 1) for t in ts]}
    srt = opt("--srt", str)
    if srt:
        ev = [e for e in leer_srt(srt) if e[0] < escena]
        if ev:
            w = sum(len(t.split()) for *_, t in ev); span = ev[-1][1] - ev[0][0]
            r["palabras"] = w; r["pal_seg"] = round(w / span, 2)
    for k, nombre in (("--gesto", "gesto_pct"), ("--telefono", "telefono_pct")):
        t = opt(k)
        if t is not None: r[nombre] = round(100 * t / d, 1)

    if "--json" in a: print(json.dumps(r, ensure_ascii=False, indent=1)); return
    print(f"\n{v}\n{'-'*72}")
    print(f"{'medida':16s}{'valor':>12s}   ✓/✗   referencia")
    for k, (lo, hi, txt) in REF.items():
        if r.get(k) is None: continue
        ok = lo <= r[k] <= hi
        print(f"{k:16s}{str(r[k]):>12s}    {'✓' if ok else '✗'}    {txt}")
    print(f"{'resolución':16s}{wh:>12s}    {'✓' if wh=='1080x1920' else '✗'}    1080x1920 @30 fps ({fps:g} fps)")
    print(f"cortes detectados en: {r['cortes_en']}")
    print("Nota: la detección de cortes no ve los jump cuts dentro del mismo plano (±2).")

if __name__ == "__main__":
    main()
