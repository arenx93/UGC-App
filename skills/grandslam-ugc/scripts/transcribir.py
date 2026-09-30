#!/usr/bin/env python3
"""Transcribe clips con Whisper (faster-whisper) -> .srt con frases cortas + .txt con tiempos.

Uso:
  python3 transcribir.py clips/                  todos los .mp4/.mov/.wav/.mp3 de la carpeta
  python3 transcribir.py clips/S2.mp4 [--modelo small.en] [--salida transcripciones]

Primera vez: pip install faster-whisper   (baja el modelo ~500 MB).
Frases de máx. 7 palabras o 2,5 s, como los subtítulos de los grand slams.
Saltea los archivos que ya tienen .srt (borrarlo para rehacer).
"""
import glob, os, sys, time

def ts(t):
    h = int(t // 3600); m = int(t % 3600 // 60); s = t % 60
    return f"{h:02d}:{m:02d}:{int(s):02d},{int((s - int(s)) * 1000):03d}"

def main():
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(0)
    modelo = a[a.index("--modelo") + 1] if "--modelo" in a else "small.en"
    salida = a[a.index("--salida") + 1] if "--salida" in a else None
    objetivo = a[0]
    if os.path.isdir(objetivo):
        archivos = sorted(f for e in ("mp4", "mov", "wav", "mp3", "m4a") for f in glob.glob(os.path.join(objetivo, "*." + e)))
        salida = salida or os.path.join(os.path.dirname(os.path.abspath(objetivo.rstrip("/"))), "transcripciones")
    else:
        archivos = [objetivo]
        salida = salida or os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(objetivo))), "transcripciones")
    os.makedirs(salida, exist_ok=True)
    try:
        from faster_whisper import WhisperModel
    except ImportError:
        sys.exit("Falta faster-whisper: pip install faster-whisper")
    print(f"Cargando {modelo}…")
    model = WhisperModel(modelo, device="cpu", compute_type="int8")
    for i, v in enumerate(archivos, 1):
        base = os.path.join(salida, os.path.splitext(os.path.basename(v))[0])
        if os.path.exists(base + ".srt"):
            print(f"[{i}/{len(archivos)}] {v}: ya estaba"); continue
        t0 = time.time()
        segs, _ = model.transcribe(v, language="en", word_timestamps=True, vad_filter=True)
        frases = []
        for s in segs:
            cur = []
            for w in (s.words or []):
                cur.append(w)
                if len(cur) >= 7 or (cur[-1].end - cur[0].start) >= 2.5 or w.word.strip()[-1:] in ".?!":
                    frases.append(cur); cur = []
            if cur: frases.append(cur)
        with open(base + ".srt", "w", encoding="utf-8") as f:
            for k, fr in enumerate(frases, 1):
                f.write(f"{k}\n{ts(fr[0].start)} --> {ts(fr[-1].end)}\n{''.join(w.word for w in fr).strip()}\n\n")
        with open(base + ".txt", "w", encoding="utf-8") as f:
            for fr in frases:
                t = fr[0].start
                f.write(f"{int(t // 60)}:{t % 60:04.1f}  {''.join(w.word for w in fr).strip()}\n")
        print(f"[{i}/{len(archivos)}] {v} -> {base}.srt ({time.time() - t0:.0f} s)")

if __name__ == "__main__":
    main()
