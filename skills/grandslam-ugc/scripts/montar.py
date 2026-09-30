#!/usr/bin/env python3
"""Montaje de videos UGC grand slam (vertical auto).
Clips: cada entrada de "clips" puede llevar "in"/"out", "zoom" (punch-in, ej. 1.15) y "ancla_y"
(0-1, dónde centrar el recorte al hacer zoom), o "tramos": [[in,out], [in,out,zoom], ...] para
cortar un mismo clip en varios jump cuts. "cola" acepta "file" (video) o "imagen" (+ "dur").
Uso:
  python3 montar.py base   plan.json   -> arma base.mp4 (clips + recorte de silencios + placa + cola)
  python3 montar.py textos plan.json   -> quema hook, comentarios y subtitulos sobre base.mp4 -> final.mp4
  python3 montar.py hoja   video.mp4   -> hoja de fotogramas cada 2 s para revisar
  python3 montar.py srt    plan.json   -> solo rehace base.srt (si cambiaste los .srt de los clips)
"""
import json, os, re, shutil, subprocess, sys, tempfile
from PIL import Image, ImageDraw, ImageFont

W, H, FPS = 1080, 1920, 30
EMOJI_FONT = next((p for p in ["/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
                                "/System/Library/Fonts/Apple Color Emoji.ttc"] if os.path.exists(p)), None)
FALLBACK_BOLD = next((p for p in ["/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                                  "/System/Library/Fonts/Supplemental/Arial Bold.ttf"] if os.path.exists(p)), None)
FALLBACK_COND = next((p for p in ["/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf",
                                  "/System/Library/Fonts/Supplemental/Impact.ttf"] if os.path.exists(p)), FALLBACK_BOLD)

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: sys.exit("ERROR ffmpeg:\n" + r.stderr[-2000:])
    return r

HAY_FFPROBE = shutil.which("ffprobe") is not None

def dur(f):
    if HAY_FFPROBE:
        return float(subprocess.check_output(["ffprobe","-v","error","-show_entries","format=duration","-of","csv=p=0",f]))
    err = subprocess.run(["ffmpeg","-i",f],capture_output=True,text=True).stderr
    h,m,x = re.search(r"Duration: (\d+):(\d+):([\d.]+)", err).groups()
    return int(h)*3600+int(m)*60+float(x)

def tiene_audio(f):
    if HAY_FFPROBE:
        return bool(subprocess.run(["ffprobe","-v","error","-select_streams","a","-show_entries","stream=index","-of","csv=p=0",f],
                                   capture_output=True,text=True).stdout.strip())
    return "Audio:" in subprocess.run(["ffmpeg","-i",f],capture_output=True,text=True).stderr

def expandir(clips):
    """Convierte {"file", "tramos": [[a,b(,zoom)],...]} en una entrada por tramo."""
    out=[]
    for c in clips:
        if c.get("tramos"):
            for t in c["tramos"]:
                d=dict(c); d.pop("tramos"); d["in"],d["out"]=t[0],t[1]
                if len(t)>2: d["zoom"]=t[2]
                out.append(d)
        else: out.append(c)
    return out

# ---------------- BASE ----------------
def norm_clip(src, dst, t_in=None, t_out=None, zoom=1.0, ancla_y=0.4):
    has_audio = tiene_audio(src)
    cmd = ["ffmpeg","-y","-v","error"]
    if t_in is not None: cmd += ["-ss", str(t_in)]
    if t_out is not None: cmd += ["-to", str(t_out)]
    cmd += ["-i", src]
    if not has_audio: cmd += ["-f","lavfi","-i","anullsrc=r=44100:cl=stereo"]
    z = max(1.0, float(zoom or 1.0)); zw, zh = int(W*z)//2*2, int(H*z)//2*2
    punch = f",scale={zw}:{zh},crop={W}:{H}:(iw-{W})/2:(ih-{H})*{ancla_y}" if z > 1.0 else ""
    cmd += ["-vf", f"scale={W}:{H}:force_original_aspect_ratio=increase,crop={W}:{H}{punch},fps={FPS},setsar=1",
            "-map","0:v","-map","0:a" if has_audio else "1:a"]
    if not has_audio: cmd += ["-shortest"]
    cmd += ["-c:v","libx264","-crf","16","-preset","veryfast","-pix_fmt","yuv420p","-c:a","aac","-ar","44100","-ac","2", dst]
    run(cmd)

def silencios(f, db=-30, d=0.3):
    r = subprocess.run(["ffmpeg","-i",f,"-vn","-af",f"silencedetect=n={db}dB:d={d}","-f","null","-"],capture_output=True,text=True).stderr
    s = [float(x) for x in re.findall(r"silence_start: (-?[\d.]+)", r)]
    e = [float(x) for x in re.findall(r"silence_end: ([\d.]+)", r)]
    return list(zip(s, e + [dur(f)]*(len(s)-len(e))))

def recortar_silencios(src, dst, db=-30, d=0.3, pad=0.12):
    T = dur(src); keep=[]; t=0.0
    for s,e in silencios(src, db, d):
        a, b = max(0,s+pad), min(T,e-pad)
        if b-a > 0.05:
            keep.append((t,a)); t=b
    keep.append((t,T))
    keep=[(a,b) for a,b in keep if b-a>0.05]
    sel="+".join(f"between(t,{a:.3f},{b:.3f})" for a,b in keep)
    run(["ffmpeg","-y","-v","error","-i",src,
         "-vf",f"select='{sel}',setpts=N/{FPS}/TB","-af",f"aselect='{sel}',asetpts=N/SR/TB",
         "-r",str(FPS),"-c:v","libx264","-crf","16","-preset","veryfast","-c:a","aac",dst])
    return keep

def placa(texto, segundos, dst, fuentes):
    png = dst + ".png"
    img = Image.new("RGBA",(W,H),(0,0,0,255))
    capa = render_texto(texto, fuentes.get("hook") or FALLBACK_COND, 80, "caja_blanca", mayus=True)
    img.alpha_composite(capa, ((W-capa.width)//2, (H-capa.height)//2))
    img.convert("RGB").save(png)
    run(["ffmpeg","-y","-v","error","-loop","1","-t",str(segundos),"-i",png,"-f","lavfi","-t",str(segundos),"-i","anullsrc=r=44100:cl=stereo",
         "-vf",f"fps={FPS},format=yuv420p","-c:v","libx264","-crf","16","-c:a","aac","-shortest",dst])

def cola_imagen(img, segundos, dst):
    """Simula cámara en mano sobre una foto fija: zoom lento + deriva + temblor leve."""
    n = int(segundos*FPS)
    zp = (f"scale={W*2}:{H*2}:force_original_aspect_ratio=increase,crop={W*2}:{H*2},"
          f"zoompan=z='1.06+0.05*on/{n}':x='iw/2-(iw/zoom/2)+18*sin(on/23)+6*sin(on/5)':"
          f"y='ih/2-(ih/zoom/2)+22*sin(on/31)+5*cos(on/4)':d={n}:s={W}x{H}:fps={FPS},"
          f"eq=brightness=0.02:saturation=0.95,format=yuv420p")
    run(["ffmpeg","-y","-v","error","-i",img,"-f","lavfi","-t",str(segundos),"-i","anullsrc=r=44100:cl=stereo",
         "-vf",zp,"-t",str(segundos),"-c:v","libx264","-crf","16","-c:a","aac","-shortest",dst])

def concat(parts, dst):
    lst = dst + ".txt"
    open(lst,"w").write("".join(f"file '{os.path.abspath(p)}'\n" for p in parts))
    run(["ffmpeg","-y","-v","error","-f","concat","-safe","0","-i",lst,"-vf",f"fps={FPS}","-c:v","libx264","-crf","16","-preset","veryfast","-c:a","aac",dst])

def base(plan, solo_srt=False):
    plan["clips"] = expandir(plan["clips"])
    tmp = tempfile.mkdtemp(prefix="montaje_")
    fu = plan.get("fuentes", {})
    partes=[]; duraciones=[]
    for i,c in enumerate(plan["clips"]):
        p=f"{tmp}/c{i:02d}.mp4"
        if solo_srt:
            a=c.get("in") or 0; b=c.get("out") or dur(c["file"]); duraciones.append(b-a); continue
        norm_clip(c["file"], p, c.get("in"), c.get("out"), c.get("zoom",1.0), c.get("ancla_y",0.4)); partes.append(p); duraciones.append(dur(p))
    out = plan.get("base","base.mp4")
    srt_out = os.path.splitext(out)[0]+".srt"
    if solo_srt:
        keep = json.load(open(os.path.splitext(out)[0]+".keep.json")) if os.path.exists(os.path.splitext(out)[0]+".keep.json") else None
        n = srt_base(plan, duraciones, keep, srt_out); print(f"{srt_out}: {n} subtitulos"); return
    escena=f"{tmp}/escena.mp4"; concat(partes, escena)
    T0 = dur(escena); keep=None
    if plan.get("recortar_silencios", True):
        cortado=f"{tmp}/escena_cort.mp4"
        keep = recortar_silencios(escena, cortado, plan.get("silencio_db",-30), plan.get("silencio_min",0.3))
        json.dump(keep, open(os.path.splitext(out)[0]+".keep.json","w"))
        escena=cortado; print(f"Silencios recortados: {T0-dur(escena):.1f} s")
    fin_escena = dur(escena); marcas={"escena_fin": fin_escena}
    finales=[escena]; t=fin_escena
    if plan.get("placa_cta"):
        p=f"{tmp}/placa.mp4"; placa(plan["placa_cta"]["text"], plan["placa_cta"].get("dur",1.5), p, fu)
        finales.append(p); marcas["placa"]=t; t+=dur(p)
    if plan.get("cola"):
        p=f"{tmp}/cola.mp4"
        if plan["cola"].get("imagen"): cola_imagen(plan["cola"]["imagen"], plan["cola"].get("dur",20), p)
        else: norm_clip(plan["cola"]["file"], p, plan["cola"].get("in"), plan["cola"].get("out"))
        finales.append(p); marcas["cola"]=t; t+=dur(p)
    if plan.get("voz_en_off"):
        sin_vo=f"{tmp}/sin_vo.mp4"; concat(finales, sin_vo); voz_en_off(sin_vo, out, plan["voz_en_off"], marcas)
    else:
        concat(finales, out)
    if any(c.get("srt") for c in plan["clips"]):
        n = srt_base(plan, duraciones, keep, srt_out); print(f"{srt_out}: {n} subtitulos alineados a {out}")
    print(f"base: {out}  duracion {dur(out):.1f} s  | escena termina {fin_escena:.1f} s"
          + (f" | placa {marcas['placa']:.1f} s" if "placa" in marcas else "")
          + (f" | cola {marcas['cola']:.1f} s" if "cola" in marcas else ""))

# ---------------- SUBTITULOS ALINEADOS ----------------
def leer_srt(path):
    def sec(x):
        h,m,r = x.strip().replace(",",".").split(":"); return int(h)*3600+int(m)*60+float(r)
    ev=[]
    for blk in re.split(r"\n\s*\n", open(path,encoding="utf-8").read().strip()):
        ls=blk.strip().splitlines()
        if len(ls)<3 or "-->" not in ls[1]: continue
        a,b=ls[1].split("-->"); ev.append([sec(a),sec(b)," ".join(ls[2:]).strip()])
    return ev

def escribir_srt(ev, path):
    def ts(t):
        t=max(0,t); h=int(t//3600); m=int(t%3600//60); s=t%60
        return f"{h:02d}:{m:02d}:{int(s):02d},{int(round((s-int(s))*1000))%1000:03d}"
    with open(path,"w",encoding="utf-8") as f:
        for k,(a,b,t) in enumerate(ev,1): f.write(f"{k}\n{ts(a)} --> {ts(b)}\n{t}\n\n")

def mapear(t, keep):
    """Tiempo en la escena original -> tiempo en la escena con silencios recortados."""
    acc=0.0
    for a,b in keep:
        if t < a: return acc          # cae en un silencio recortado: va al inicio del tramo siguiente
        if t <= b: return acc + (t-a)
        acc += b-a
    return acc

def srt_base(plan, duraciones, keep, out_path):
    ev=[]; off=0.0
    for c,d in zip(plan["clips"], duraciones):
        if c.get("srt"):
            t_in = c.get("in") or 0.0
            for a,b,t in leer_srt(c["srt"]):
                a2,b2 = a-t_in, b-t_in
                if b2<=0 or a2>=d: continue
                solape = min(d,b2)-max(0,a2)
                if solape < 0.5*(b-a) and solape < 1.0: continue   # la frase quedó casi toda en otro tramo
                ev.append([off+max(0,a2), off+min(d,b2), t])
        off += d
    if keep: ev=[[mapear(a,keep), mapear(b,keep), t] for a,b,t in ev]
    ev=[e for e in ev if e[1]-e[0] > 0.2]
    for i in range(len(ev)):                        # cada frase queda hasta que entra la siguiente
        sig = ev[i+1][0] if i+1 < len(ev) else ev[i][1]+0.6
        ev[i][1] = sig if sig-ev[i][1] < 1.0 else ev[i][1]+0.4
        ev[i][1] = max(ev[i][1], min(ev[i][0]+0.8, sig))
    escribir_srt(ev, out_path)
    return len(ev)

# ---------------- VOZ EN OFF ----------------
def voz_en_off(src, dst, voces, marcas):
    """voces: [{"file","start" (seg o "cola"/"placa"/"escena_fin"),"vol",("duck": 0-1 volumen de la escena debajo)}]"""
    cmd=["ffmpeg","-y","-v","error","-i",src]
    fc=[]; bg="[0:a]"; mix=[]
    for i,v in enumerate(voces):
        st = marcas[v["start"]] if isinstance(v["start"],str) else float(v["start"])
        L = dur(v["file"]); cmd += ["-i", v["file"]]
        duck = v.get("duck", 0.25)
        fc.append(f"{bg}volume={duck}:enable='between(t,{st:.3f},{st+L:.3f})'[bg{i}]"); bg=f"[bg{i}]"
        ms=int(st*1000)
        fc.append(f"[{i+1}:a]aresample=44100,aformat=channel_layouts=stereo,adelay={ms}|{ms},volume={v.get('vol',1.0)}[v{i}]")
        mix.append(f"[v{i}]")
    fc.append(f"{bg}{''.join(mix)}amix=inputs={len(mix)+1}:normalize=0:duration=first[a]")
    cmd += ["-filter_complex",";".join(fc),"-map","0:v","-map","[a]","-c:v","copy","-c:a","aac",dst]
    run(cmd)

# ---------------- TEXTOS ----------------
def es_emoji(ch):
    o=ord(ch)
    return o>=0x1F000 or 0x2600<=o<=0x27BF or o in (0xFE0F,0x200D,0x2B50,0x2B55) or 0x1F1E6<=o<=0x1F1FF

def runs(linea):
    out=[]
    for ch in linea:
        k = es_emoji(ch)
        if out and out[-1][0]==k: out[-1][1]+=ch
        else: out.append([k,ch])
    return out

def medir(run_txt, emoji, font, size, efont):
    if emoji:
        n = sum(1 for c in run_txt if ord(c) not in (0xFE0F,0x200D))
        return int(n*size*1.15)
    return int(font.getlength(run_txt))

def dibujar_emoji(img, x, y, txt, size):
    if not EMOJI_FONT: return
    ef = ImageFont.truetype(EMOJI_FONT, 109)
    for ch in txt:
        if ord(ch) in (0xFE0F,0x200D): continue
        tile = Image.new("RGBA",(136,128),(0,0,0,0))
        ImageDraw.Draw(tile).text((0,0), ch, font=ef, embedded_color=True)
        bb = tile.getbbox()
        if not bb: continue
        tile = tile.crop(bb); s = size/ max(tile.height,1)
        tile = tile.resize((max(1,int(tile.width*s)), int(size)), Image.LANCZOS)
        img.alpha_composite(tile, (int(x), int(y + size*0.12)))
        x += size*1.15

def envolver(texto, font, maxw):
    """Devuelve [(linea, indice_de_parrafo_original)]."""
    lineas=[]
    for k,par in enumerate(texto.split("\n")):
        pal=par.split(" "); cur=""
        for p in pal:
            prueba=(cur+" "+p).strip()
            if font.getlength(prueba) <= maxw or not cur: cur=prueba
            else: lineas.append((cur,k)); cur=p
        lineas.append((cur,k))
    return lineas

def render_texto(texto, fuente, size, estilo, mayus=False, amarillas=(), maxw=940):
    """estilo: contorno | caja_blanca | caja_negra. Devuelve RGBA recortado."""
    if mayus: texto = texto.upper()
    font = ImageFont.truetype(fuente, size)
    pares = envolver(texto, font, maxw)
    lineas = [l for l,_ in pares]; parrafo = [k for _,k in pares]
    lh = int(size*1.18)
    anchos = [sum(medir(t,e,font,size,None) for e,t in runs(l)) for l in lineas]
    padx, pady = int(size*0.45), int(size*0.28)
    cw = max(anchos) + 2*padx + 20; ch = lh*len(lineas) + 2*pady + 20
    img = Image.new("RGBA",(cw,ch),(0,0,0,0)); d = ImageDraw.Draw(img)
    if estilo in ("caja_blanca","caja_negra"):
        fill = (255,255,255,255) if estilo=="caja_blanca" else (0,0,0,235)
        # una caja por linea, como el texto nativo de TikTok
        for i,a in enumerate(anchos):
            x0=(cw-a)//2-padx; y0=10+pady+i*lh-int(pady*0.6)
            d.rounded_rectangle([x0,y0,x0+a+2*padx,y0+lh+int(pady*0.9)], radius=int(size*0.35), fill=fill)
    color_txt = (0,0,0,255) if estilo=="caja_blanca" else (255,255,255,255)
    for i,l in enumerate(lineas):
        x=(cw-anchos[i])//2; y=10+pady+i*lh
        col = (255,230,0,255) if parrafo[i] in amarillas else color_txt
        for e,t in runs(l):
            if e: dibujar_emoji(img, x, y, t, size); x+=medir(t,True,font,size,None)
            else:
                kw = dict(stroke_width=max(3,size//12), stroke_fill=(0,0,0,255)) if estilo=="contorno" else {}
                d.text((x,y), t, font=font, fill=col, **kw); x+=font.getlength(t)
    return img.crop(img.getbbox())

def png_capa(texto, fuente, size, estilo, y_frac, dst, mayus=False, amarillas=()):
    capa = render_texto(texto, fuente, size, estilo, mayus, amarillas)
    lienzo = Image.new("RGBA",(W,H),(0,0,0,0))
    y = int(H*y_frac - capa.height/2)
    lienzo.alpha_composite(capa, ((W-capa.width)//2, max(0,min(H-capa.height,y))))
    lienzo.save(dst)

def srt_a_ass(srt, ass, fuente, color):
    fam = ImageFont.truetype(fuente, 10).getname()[0]
    col = "&H0000E6FF" if color=="amarillo" else "&H00FFFFFF"
    head=f"""[Script Info]
ScriptType: v4.00+
PlayResX: {W}
PlayResY: {H}

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Sub,{fam},50,{col},{col},&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,3,0,2,90,90,{int(H*0.21)},1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
"""
    def t(x): h,m,s=x.replace(",",".").split(":"); return f"{int(h)}:{m}:{float(s):05.2f}"
    ev=[]
    for blk in re.split(r"\n\s*\n", open(srt,encoding="utf-8").read().strip()):
        ls=blk.strip().splitlines()
        if len(ls)<3: continue
        a,b=[x.strip() for x in ls[1].split("-->")]
        txt="\\N".join(ls[2:]).lower()
        txt=re.sub(r"[.,!?;:]","",txt)
        ev.append(f"Dialogue: 0,{t(a)},{t(b)},Sub,,0,0,0,,{txt}")
    open(ass,"w",encoding="utf-8").write(head+"\n".join(ev)+"\n")
    return os.path.dirname(os.path.abspath(fuente))

def textos(plan):
    tmp = tempfile.mkdtemp(prefix="textos_")
    fu = plan.get("fuentes", {}); f_hook = fu.get("hook") or FALLBACK_COND; f_txt = fu.get("texto") or FALLBACK_BOLD
    src = plan.get("base","base.mp4"); out = plan.get("salida","final.mp4")
    capas=[]
    h = plan.get("hook")
    if h:
        txt = h["text"] + (("\n"+h["emojis"]) if h.get("emojis") else "")
        p=f"{tmp}/hook.png"
        png_capa(txt, f_hook, h.get("size",78), h.get("estilo","contorno"), h.get("y",0.55), p, mayus=True, amarillas=tuple(h.get("amarilla",[])))
        capas.append((p,h.get("start",0),h["end"]))
    for i,c in enumerate(plan.get("comentarios",[])):
        p=f"{tmp}/c{i:02d}.png"
        png_capa(c["text"], f_txt, c.get("size",60), c.get("estilo","contorno"), c.get("y",0.5), p)
        capas.append((p,c["start"],c["end"]))
    cmd=["ffmpeg","-y","-v","error","-i",src]
    for p,_,_ in capas: cmd+=["-i",p]
    fc=[]; last="0:v"
    subs = plan.get("subtitulos")
    if subs is None and os.path.exists(os.path.splitext(src)[0]+".srt"): subs = os.path.splitext(src)[0]+".srt"
    if subs:
        ass=f"{tmp}/subs.ass"; fdir=srt_a_ass(subs, ass, f_txt, plan.get("color_subs","blanco"))
        fc.append(f"[0:v]ass={ass}:fontsdir={fdir}[s]"); last="s"
    for i,(p,a,b) in enumerate(capas):
        fc.append(f"[{last}][{i+1}:v]overlay=0:0:enable='between(t,{a},{b})'[o{i}]"); last=f"o{i}"
    cmd+=["-filter_complex",";".join(fc) if fc else "null","-map",f"[{last}]" if fc else "0:v","-map","0:a",
          "-af","loudnorm=I=-16:TP=-1.5:LRA=11","-c:v","libx264","-crf","18","-preset","medium","-pix_fmt","yuv420p",
          "-r",str(FPS),"-c:a","aac","-b:a","192k","-ar","44100","-movflags","+faststart",out]
    run(cmd); print(f"final: {out}  {dur(out):.1f} s")

def hoja(video, cada=2.0):
    """Hojas de 5x4 fotogramas (uno cada 2 s) con el tiempo marcado, para revisar el montaje."""
    b=os.path.splitext(video)[0]; tmp=tempfile.mkdtemp(prefix="hoja_")
    run(["ffmpeg","-y","-v","error","-i",video,"-vf",f"fps={1/cada},scale=270:480",f"{tmp}/f_%04d.jpg"])
    fr=sorted(os.listdir(tmp)); font=ImageFont.truetype(FALLBACK_BOLD, 22); hojas=[]
    for k in range(0,len(fr),20):
        img=Image.new("RGB",(270*5,480*4),(0,0,0)); d=ImageDraw.Draw(img)
        for n,f in enumerate(fr[k:k+20]):
            x,y=(n%5)*270,(n//5)*480; img.paste(Image.open(f"{tmp}/{f}"),(x,y))
            t=(k+n)*cada; lab=f"{int(t//60)}:{t%60:04.1f}"
            d.rectangle([x+2,y+2,x+2+font.getlength(lab)+8,y+30],fill=(255,255,255)); d.text((x+6,y+4),lab,font=font,fill=(220,0,0))
        out=f"{b}_hoja_{k//20+1:02d}.jpg"; img.save(out,quality=85); hojas.append(out)
    print("hojas:", " ".join(hojas))

if __name__=="__main__":
    modo, arg = sys.argv[1], sys.argv[2]
    if modo=="hoja": hoja(arg)
    else:
        plan=json.load(open(arg, encoding="utf-8"))
        if modo=="srt": base(plan, solo_srt=True)
        else: {"base":base,"textos":textos}[modo](plan)
