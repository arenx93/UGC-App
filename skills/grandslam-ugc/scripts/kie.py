#!/usr/bin/env python3
"""Seedance 2.5 en kie.ai para videos grand slam.

Uso:
  python3 kie.py revisar plan_escenas.json [S2]      lint de prompts (palabras, @, bloques, prohibidas)
  python3 kie.py costo   plan_escenas.json [S2]      costo estimado
  python3 kie.py crear   plan_escenas.json S2 [--si] sube referencias y crea la tarea
  python3 kie.py esperar plan_escenas.json S2        espera, descarga clips/S2.mp4 y clips/S2_last.jpg
  python3 kie.py cadena  plan_escenas.json [--si]    todas las escenas en orden (encadena el último frame)
  python3 kie.py subir   archivo                     sube un archivo y muestra la URL (dura 3 días)
  python3 kie.py frame   video.mp4 [--t -0.4]        extrae un frame (t negativo = desde el final)

Requiere KIE_API_KEY en el entorno (salvo revisar/costo/frame) y ffmpeg en el PATH.
Solo usa la biblioteca estándar de Python.
"""
import hashlib, json, mimetypes, os, re, shutil, subprocess, sys, time, urllib.request, urllib.error, uuid

API = "https://api.kie.ai"
UPLOAD = "https://kieai.redpandaai.co/api/file-stream-upload"
MODEL = "bytedance/seedance-2-5"
WPS = 2.47                      # palabras por segundo medidas en salidas reales
TARIFA = {"480p": 0.14, "720p": 0.315, "1080p": 0.79}   # US$/s orientativo (confirmar en la cuenta)
BLOQUES = ["FORMAT:", "VOICE DIRECTION:", "PACING", "POV:", "CHARACTERS", "LOCATION:", "CAMERA:",
           "IMAGE:", "AUDIO:", "ACTING:", "ACTION:", "RESTRICTIONS:"]
PROHIBIDAS = ["cinematic", "bokeh", "authentic", "ugc style", "ugc-style", "realistic ugc", "4k", "8k",
              "masterpiece", "hyperrealistic", "photorealistic", "film grain", "shallow depth of field",
              "teal and orange", "anamorphic", "studio lighting", "beauty filter", "slow motion", "gimbal",
              "steadicam", "drone", "dolly", "rack focus", "lens flare"]


# ---------------------------------------------------------------- utilidades
def die(msg):
    sys.exit("ERROR: " + msg)

def clave():
    k = os.environ.get("KIE_API_KEY", "").strip()
    if not k: die("falta la variable de entorno KIE_API_KEY")
    return k

def http(url, data=None, headers=None, metodo=None, timeout=120):
    req = urllib.request.Request(url, data=data, headers=headers or {}, method=metodo)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        cuerpo = e.read().decode(errors="replace")[:500]
        die(f"HTTP {e.code} en {url.split('?')[0]}: {cuerpo}")

def kie(path, cuerpo=None):
    h = {"Authorization": "Bearer " + clave(), "Content-Type": "application/json"}
    d = http(API + path, json.dumps(cuerpo).encode() if cuerpo is not None else None, h,
             "POST" if cuerpo is not None else "GET")
    if d.get("code") not in (None, 200):
        die(f"kie respondió {d.get('code')}: {d.get('msg')}")
    return d

def cargar(plan_path):
    plan = json.load(open(plan_path, encoding="utf-8"))
    plan["_dir"] = os.path.dirname(os.path.abspath(plan_path))
    return plan

def ruta(plan, p):
    return p if os.path.isabs(p) else os.path.join(plan["_dir"], p)

def estado_path(plan): return os.path.join(plan["_dir"], ".kie_estado.json")
def leer_estado(plan):
    p = estado_path(plan)
    return json.load(open(p)) if os.path.exists(p) else {}
def guardar_estado(plan, est):
    json.dump(est, open(estado_path(plan), "w"), indent=1)

def escena(plan, sid):
    for e in plan["escenas"]:
        if e["id"] == sid: return e
    die(f"no existe la escena {sid}")

def refs_de(plan, e, local=True):
    """Devuelve (imagenes, videos, audios) como rutas locales, en el orden de los arrays."""
    R = plan.get("refs", {})
    def res(nombre):
        if nombre not in R: die(f"{e['id']}: la referencia '{nombre}' no está en refs")
        return ruta(plan, R[nombre])
    imgs = [res(n) for n in e.get("images", [])]
    if e.get("continua"):
        imgs.append(os.path.join(plan["_dir"], "clips", f"{e['continua']}_last.jpg"))
    return imgs, [res(n) for n in e.get("videos", [])], [res(n) for n in e.get("audios", [])]


# ---------------------------------------------------------------- revisar (lint)
def comillas(txt):
    return re.findall(r'"([^"]*)"|“([^”]*)”', txt)

def palabras(s):
    return len(re.findall(r"[A-Za-z0-9'’$,.\-]+", s))

def revisar_escena(plan, e):
    fallas, avisos = [], []
    p = ruta(plan, e["prompt"])
    if not os.path.exists(p): return [f"no existe {e['prompt']}"], []
    txt = open(p, encoding="utf-8").read()
    dur = int(e["duration"])
    if not 4 <= dur <= 30: fallas.append(f"duration {dur} fuera de 4–30")

    for b in BLOQUES:
        if b not in txt: fallas.append(f"falta el bloque {b}")
    if "NEVER spoken aloud" not in txt: fallas.append("VOICE DIRECTION sin 'NEVER spoken aloud'")
    if "never sound bored" not in txt.lower(): fallas.append("falta la línea de P2 'never sound bored…'")
    for n in re.findall(r"@Audio(\d+)", txt):
        if f"Do not reproduce any words from @Audio{n}" not in txt:
            fallas.append(f"@Audio{n} sin bloqueo de palabras"); break
    if "do not copy its emotion" not in txt.lower() and "@Audio" in txt:
        fallas.append("audios sin bloqueo de emoción ('do not copy its emotion')")

    # primer frame vs continuidad
    tiene_ff = "FIRST FRAME — MANDATORY" in txt or "FIRST FRAME - MANDATORY" in txt
    if e.get("continua") and not tiene_ff: fallas.append("continúa otra escena pero falta el bloque FIRST FRAME")
    if not e.get("continua") and tiene_ff: fallas.append("tiene FIRST FRAME pero no 'continua' (no hay último frame en el array)")
    if e.get("continua") and "not as a style reference" not in txt:
        fallas.append("FIRST FRAME sin la frase 'not as a style reference'")

    # referencias @ vs arrays
    imgs, vids, auds = refs_de(plan, e)
    for tipo, lista in (("Image", imgs), ("Video", vids), ("Audio", auds)):
        usados = sorted({int(n) for n in re.findall(rf"@{tipo}(\d+)", txt)})
        for n in usados:
            if n > len(lista): fallas.append(f"@{tipo}{n} usado pero el array tiene {len(lista)}")
        for n in range(1, len(lista) + 1):
            if n not in usados: fallas.append(f"@{tipo}{n} ({os.path.basename(lista[n-1])}) cargado pero no usado en el texto")

    # palabras dentro de ACTION
    ini = txt.find("ACTION:"); fin = txt.find("RESTRICTIONS:")
    accion = txt[ini:fin] if ini >= 0 and fin > ini else txt
    lineas = [a or b for a, b in comillas(accion)]
    total = sum(palabras(l) for l in lineas)
    objetivo = e.get("palabras_objetivo") or round(dur * WPS)
    if not objetivo * 0.9 <= total <= objetivo * 1.1:
        fallas.append(f"palabras de diálogo {total} fuera de {objetivo} ±10% ({round(objetivo*0.9)}–{round(objetivo*1.1)})")
    for l in lineas:
        if palabras(l) > 12: avisos.append(f"línea de {palabras(l)} palabras (>12): \"{l[:60]}…\"")
        if re.search(r"[$\d]", l): avisos.append(f"cifra con números dentro de comillas (escribirla en palabras): \"{l[:50]}\"")

    # tiempos por bloque de ACTION
    bloques = re.findall(r"^\s*(\d+(?:\.\d+)?)\s*[–-]\s*(\d+(?:\.\d+)?)\s*s\b(.*?)(?=^\s*\d+(?:\.\d+)?\s*[–-]|\Z)",
                         accion, re.S | re.M)
    fin_max = 0
    for a, b, cuerpo in bloques:
        a, b = float(a), float(b); fin_max = max(fin_max, b)
        w = sum(palabras(x or y) for x, y in comillas(cuerpo))
        if w and abs((b - a) - w / 2.5) > 1.0:
            avisos.append(f"bloque {a:g}–{b:g} s: {w} palabras ≈ {w/2.5:.1f} s de habla, el bloque dura {b-a:g} s")
    if bloques and abs(fin_max - dur) > 1: fallas.append(f"ACTION termina en {fin_max:g} s y el clip dura {dur} s")
    if not bloques: avisos.append("ACTION sin bloques de tiempo 'a–b s —'")

    # palabras prohibidas fuera de RESTRICTIONS
    fuera = txt[:fin] if fin > 0 else txt
    for pw in PROHIBIDAS:
        for m in re.finditer(re.escape(pw), fuera, re.I):
            antes = fuera[max(0, m.start() - 25):m.start()].lower()
            if not re.search(r"\b(no|not|never|without)\b", antes):
                fallas.append(f"palabra prohibida fuera de RESTRICTIONS: '{pw}'"); break

    if "0.5x" not in txt and "ultra-wide" not in txt.lower() and "HOOK" not in txt and "2x digital zoom" not in txt:
        avisos.append("CAMERA no pide el ultra gran angular 0.5x (lo que usan los ganadores)")
    es_ultima = e is plan["escenas"][-1]
    rlf = e.get("return_last_frame", not es_ultima)
    if es_ultima and rlf: avisos.append("última escena con return_last_frame true")
    return fallas, avisos, {"palabras": total, "objetivo": objetivo, "imgs": imgs, "vids": vids, "auds": auds}

def cmd_revisar(plan, sid=None):
    malas = 0
    for e in plan["escenas"]:
        if sid and e["id"] != sid: continue
        r = revisar_escena(plan, e)
        fallas, avisos = r[0], r[1]
        print(f"\n== {e['id']}  ({e['duration']} s, {e['prompt']})")
        if len(r) > 2:
            info = r[2]
            print(f"   palabras {info['palabras']} / objetivo {info['objetivo']}")
            for tipo, lista in (("Image", info["imgs"]), ("Video", info["vids"]), ("Audio", info["auds"])):
                for i, f in enumerate(lista, 1):
                    falta = "" if os.path.exists(f) or f.endswith("_last.jpg") else "   <-- NO EXISTE"
                    print(f"   @{tipo}{i} -> {os.path.relpath(f, plan['_dir'])}{falta}")
        for f in fallas: print("   ✗", f)
        for a in avisos: print("   !", a)
        if not fallas: print("   ✓ sin fallas")
        malas += bool(fallas)
    return malas

def cmd_costo(plan, sid=None):
    res = plan.get("resolution", "1080p"); t = TARIFA.get(res, TARIFA["1080p"])
    seg = sum(int(e["duration"]) for e in plan["escenas"] if not sid or e["id"] == sid)
    print(f"{seg} s en {res} ≈ US${seg*t:.2f} (tarifa orientativa {t}/s; confirmar en kie)")
    return seg * t


# ---------------------------------------------------------------- subir
def subir(archivo, plan=None):
    if not os.path.exists(archivo): die(f"no existe {archivo}")
    cache_p = os.path.join(plan["_dir"] if plan else ".", ".kie_cache.json")
    cache = json.load(open(cache_p)) if os.path.exists(cache_p) else {}
    h = hashlib.sha1(open(archivo, "rb").read()).hexdigest()
    if h in cache and time.time() - cache[h]["t"] < 2.5 * 86400:
        return cache[h]["url"]
    mime = mimetypes.guess_type(archivo)[0] or "application/octet-stream"
    carpeta = "images" if mime.startswith("image/") else "videos" if mime.startswith("video/") else "audio"
    nombre = h[:12] + os.path.splitext(archivo)[1].lower()
    b = "----kie" + uuid.uuid4().hex
    partes = []
    for k, v in (("uploadPath", carpeta + "/grandslam"), ("fileName", nombre)):
        partes.append(f"--{b}\r\nContent-Disposition: form-data; name=\"{k}\"\r\n\r\n{v}\r\n".encode())
    partes.append(f"--{b}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"{nombre}\"\r\n"
                  f"Content-Type: {mime}\r\n\r\n".encode() + open(archivo, "rb").read() + b"\r\n")
    partes.append(f"--{b}--\r\n".encode())
    d = http(UPLOAD, b"".join(partes), {"Authorization": "Bearer " + clave(),
                                        "Content-Type": f"multipart/form-data; boundary={b}"}, "POST", 300)
    url = (d.get("data") or {}).get("downloadUrl")
    if not url: die(f"subida fallida: {d}")
    cache[h] = {"url": url, "t": time.time(), "archivo": os.path.basename(archivo)}
    json.dump(cache, open(cache_p, "w"), indent=1)
    return url


# ---------------------------------------------------------------- crear / esperar
def cmd_crear(plan, sid, si=False):
    e = escena(plan, sid)
    r = revisar_escena(plan, e)
    if r[0]:
        cmd_revisar(plan, sid); die(f"{sid} tiene fallas: corregir antes de gastar créditos")
    imgs, vids, auds = r[2]["imgs"], r[2]["vids"], r[2]["auds"]
    for f in imgs + vids + auds:
        if not os.path.exists(f): die(f"falta el archivo {f} (¿se generó la escena anterior?)")
    res = e.get("resolution", plan.get("resolution", "1080p"))
    costo = int(e["duration"]) * TARIFA.get(res, TARIFA["1080p"])
    print(f"{sid}: {e['duration']} s en {res} ≈ US${costo:.2f}")
    if not si and input("¿Confirmás la generación? [s/N] ").strip().lower() not in ("s", "si", "sí", "y"):
        die("cancelado")
    inp = {"prompt": open(ruta(plan, e["prompt"]), encoding="utf-8").read().strip(),
           "aspect_ratio": e.get("aspect_ratio", plan.get("aspect_ratio", "9:16")),
           "resolution": res, "duration": int(e["duration"]), "generate_audio": True,
           "return_last_frame": e.get("return_last_frame", e is not plan["escenas"][-1])}
    if imgs: inp["reference_image_urls"] = [subir(f, plan) for f in imgs]
    if vids: inp["reference_video_urls"] = [subir(f, plan) for f in vids]
    if auds: inp["reference_audio_urls"] = [subir(f, plan) for f in auds]
    d = kie("/api/v1/jobs/createTask", {"model": e.get("model", plan.get("model", MODEL)), "input": inp})
    tid = d["data"]["taskId"]
    est = leer_estado(plan); est[sid] = {"taskId": tid, "creado": time.time(), "resolution": res}
    guardar_estado(plan, est)
    print(f"{sid}: tarea {tid} creada")
    return tid

def urls_de(obj):
    out = []
    if isinstance(obj, str) and obj.startswith("http"): out.append(obj)
    elif isinstance(obj, dict): [out.extend(urls_de(v)) for v in obj.values()]
    elif isinstance(obj, list): [out.extend(urls_de(v)) for v in obj]
    return out

def descargar(url, dst):
    with urllib.request.urlopen(url, timeout=600) as r, open(dst, "wb") as f:
        shutil.copyfileobj(r, f)

def cmd_frame(video, t=-0.1, dst=None):
    dst = dst or os.path.splitext(video)[0] + "_last.jpg"
    if t < 0:
        cmd = ["ffmpeg", "-y", "-v", "error", "-sseof", str(min(t, -0.05) - 0.25), "-i", video,
               "-update", "1", "-q:v", "2", dst]
    else:
        cmd = ["ffmpeg", "-y", "-v", "error", "-ss", str(t), "-i", video, "-frames:v", "1", "-q:v", "2", dst]
    subprocess.run(cmd, check=True)
    print("frame:", dst)
    return dst

def cmd_esperar(plan, sid, max_min=40):
    est = leer_estado(plan)
    if sid not in est: die(f"{sid} no tiene tarea creada")
    tid = est[sid]["taskId"]; t0 = time.time(); ultimo = ""
    while True:
        d = kie("/api/v1/jobs/recordInfo?taskId=" + tid)["data"]
        st = d.get("state")
        if st != ultimo: print(f"{sid}: {st}"); ultimo = st
        if st == "success": break
        if st == "fail": die(f"{sid} falló: {d.get('failCode')} {d.get('failMsg')}")
        if time.time() - t0 > max_min * 60: die(f"{sid}: más de {max_min} min esperando; reintentar 'esperar' luego")
        time.sleep(15)
    rj = d.get("resultJson"); rj = json.loads(rj) if isinstance(rj, str) else (rj or {})
    urls = urls_de(rj)
    video = next((u for u in urls if re.search(r"\.(mp4|mov)(\?|$)", u, re.I)), urls[0] if urls else None)
    if not video: die(f"{sid}: la respuesta no trae URL: {rj}")
    os.makedirs(os.path.join(plan["_dir"], "clips"), exist_ok=True)
    dst = os.path.join(plan["_dir"], "clips", f"{sid}.mp4")
    descargar(video, dst)
    for u in urls:
        if re.search(r"\.(jpg|jpeg|png|webp)(\?|$)", u, re.I):
            descargar(u, os.path.join(plan["_dir"], "clips", f"{sid}_last_kie" + os.path.splitext(u.split("?")[0])[1]))
    cmd_frame(dst, -0.1, os.path.join(plan["_dir"], "clips", f"{sid}_last.jpg"))
    try:
        wh = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v", "-show_entries", "stream=width,height",
                             "-of", "csv=p=0", dst], capture_output=True, text=True).stdout.strip()
        print(f"{sid}: descargado {dst} ({wh})")
    except FileNotFoundError:
        print(f"{sid}: descargado {dst}")
    est[sid].update({"archivo": dst, "listo": time.time()}); guardar_estado(plan, est)
    return dst

def cmd_cadena(plan, si=False):
    if cmd_revisar(plan): die("hay escenas con fallas")
    total = cmd_costo(plan)
    if not si and input(f"¿Generar TODAS las escenas (≈US${total:.2f})? [s/N] ").strip().lower() not in ("s", "si", "sí", "y"):
        die("cancelado")
    est = leer_estado(plan)
    for e in plan["escenas"]:
        clip = os.path.join(plan["_dir"], "clips", f"{e['id']}.mp4")
        if os.path.exists(clip): print(f"{e['id']}: ya existe, salteo"); continue
        if e["id"] not in est or "archivo" in est[e["id"]]:
            cmd_crear(plan, e["id"], si=True)
        cmd_esperar(plan, e["id"])
        print(f"-> revisar clips/{e['id']}.mp4 antes de aprobar la siguiente si es importante")


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(0)
    si = "--si" in a; a = [x for x in a if x != "--si"]
    c = a[0]
    if c == "subir": print(subir(a[1]))
    elif c == "frame":
        t = float(a[a.index("--t") + 1]) if "--t" in a else -0.1
        cmd_frame(a[1], t)
    else:
        plan = cargar(a[1]); sid = a[2] if len(a) > 2 else None
        if c == "revisar": sys.exit(1 if cmd_revisar(plan, sid) else 0)
        elif c == "costo": cmd_costo(plan, sid)
        elif c == "crear": cmd_crear(plan, sid or die("falta la escena"), si)
        elif c == "esperar": cmd_esperar(plan, sid or die("falta la escena"))
        elif c == "cadena": cmd_cadena(plan, si)
        else: print(__doc__)
