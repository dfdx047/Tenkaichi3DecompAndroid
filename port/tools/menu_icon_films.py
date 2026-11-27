#!/usr/bin/env python3
"""The main menu's animated icons (port/tbo/ui/menu_ball.mpg, menu_radar.mpg and their first pictures): Z Warriors'
four-star ball, from the user's clip (on black), and Tag Team's Scouter (drawn here: its sight turns, its readings
change). Films with alpha, as gs/gs_draw.c's: colours on top, how solid each pixel is below, MPEG-1.
    port/tools/menu_icon_films.py [BALL.mp4]"""
import math
import random
import pathlib
import sys

import av
import numpy as np
from PIL import Image, ImageDraw

OUT = pathlib.Path(__file__).resolve().parents[2] / "port/tbo/ui"
N = 128


def film(name, pictures):
    c = av.open(str(OUT / (name + ".mpg")), "w", format="mpeg")
    s = c.add_stream("mpeg1video", rate=24)
    s.width, s.height, s.pix_fmt, s.bit_rate = N, 2 * N, "yuv420p", 2_500_000
    s.codec_context.options = {"qmin": "1", "qmax": "5", "g": "12"}
    for p in pictures:
        a = np.asarray(p, np.uint8)
        stacked = np.concatenate([a[:, :, :3], np.repeat(a[:, :, 3:], 3, axis=2)])
        for pkt in s.encode(av.VideoFrame.from_ndarray(stacked, format="rgb24")):
            c.mux(pkt)
    for pkt in s.encode():
        c.mux(pkt)
    c.close()
    pictures[0].save(OUT / (name + ".png"))
    print(name, len(pictures), "pictures", (OUT / (name + ".mpg")).stat().st_size, "bytes")


def ball(clip):
    box = None
    out = []
    mask = Image.new("L", (N * 4, N * 4), 0)
    ImageDraw.Draw(mask).ellipse((4, 4, N * 4 - 4, N * 4 - 4), fill=255)
    mask = mask.resize((N, N), Image.LANCZOS)
    for x in av.open(clip).decode(video=0):
        f = x.to_image().convert("RGB")
        if box is None:  # the ball: the bright pixels of the first picture
            x0, y0, x1, y1 = f.convert("L").point(lambda v: 255 if v > 60 else 0).getbbox()
            cx, cy, r = (x0 + x1) / 2, (y0 + y1) / 2, max(x1 - x0, y1 - y0) / 2 * 1.02
            box = (int(cx - r), int(cy - r), int(cx + r), int(cy + r))
        p = f.crop(box).resize((N, N), Image.LANCZOS).convert("RGBA")
        p.putalpha(mask)
        out.append(p)
    return out


N_S = 512
INK = (34, 38, 66, 255)


def scouter_frame(i):
    t=i/48
    im=Image.new('RGBA',(N_S,N_S))
    d=ImageDraw.Draw(im)
    # haste que passa por cima da orelha e braco ate a lente
    d.line([(112,196),(150,96),(236,74),(300,104)],fill=INK,width=46,joint='curve')
    d.line([(112,196),(150,96),(236,74),(300,104)],fill=(214,220,234,255),width=22,joint='curve')
    # lente verde, translucida
    lens=[(214,150),(478,128),(492,196),(452,330),(330,372),(236,330)]
    glass=Image.new('RGBA',(N_S,N_S)); g=ImageDraw.Draw(glass)
    g.polygon(lens,fill=(60,226,120,225))
    m=Image.new('L',(N_S,N_S)); ImageDraw.Draw(m).polygon(lens,fill=255)
    hud=Image.new('RGBA',(N_S,N_S)); h=ImageDraw.Draw(hud)
    for y in range(140,380,22): h.line((200,y,500,y),fill=(20,150,80,110),width=4)
    # mira que gira e fecha no alvo
    cx,cy=372,226; r=58+18*max(0,math.cos(t*2*math.pi*2))
    for k in range(4):
        a=t*360+k*90; h.arc((cx-r,cy-r,cx+r,cy+r),a,a+52,fill=(235,255,235,255),width=12)
    h.polygon([(cx,cy-20),(cx+20,cy),(cx,cy+20),(cx-20,cy)],outline=(255,240,90,255),width=8)
    # leitura do poder de luta: barras que mudam
    rnd=random.Random(i//3)
    for k in range(4):
        w=rnd.randint(26,78); y=290+k*20 if k<3 else 164
        h.rectangle((262,y,262+w,y+10),fill=(12,100,52,255))
    # linha de varredura
    sy=130+int(250*((t*2)%1))
    h.rectangle((200,sy-14,500,sy),fill=(190,255,200,90)); h.rectangle((200,sy,500,sy+7),fill=(235,255,235,220))
    glass.alpha_composite(hud)
    glass.putalpha(Image.composite(glass.getchannel('A'),Image.new('L',(N_S,N_S)),m))
    im.alpha_composite(glass)
    d=ImageDraw.Draw(im)
    d.line(lens+[lens[0],lens[1]],fill=INK,width=18,joint='curve')
    shine=Image.new('RGBA',(N_S,N_S)); ImageDraw.Draw(shine).polygon([(262,162),(306,158),(262,262),(244,240)],fill=(255,255,255,130))
    im.alpha_composite(shine)
    d=ImageDraw.Draw(im)
    # peca da orelha
    d.rounded_rectangle((34,150,232,392),84,fill=(226,231,242,255),outline=INK,width=18)
    d.rounded_rectangle((58,168,150,250),40,fill=(248,250,255,255))
    d.ellipse((72,214,194,336),fill=(170,178,200,255),outline=INK,width=14)
    p=0.5+0.5*math.sin(t*2*math.pi*3)   # o botao vermelho pulsa
    d.ellipse((98,240,168,310),fill=(int(150+105*p),int(30+50*p),int(40+40*p),255),outline=INK,width=10)
    d.ellipse((112,252,132,270),fill=(255,210,205,int(120+135*p)))
    d.rounded_rectangle((176,214,252,300),22,fill=(190,198,218,255),outline=INK,width=14)
    for k in range(3): d.line((96+k*30,358,110+k*30,358),fill=(90,98,130,255),width=12)
    return im.rotate(-4,resample=Image.BICUBIC,center=(256,256))

def scouter():
    return [scouter_frame(i).crop((24, 0, 492, 468)).resize((N, N), Image.LANCZOS) for i in range(48)]


if len(sys.argv) > 1:  # (without the clip: only Tag Team's)
    film("menu_ball", ball(sys.argv[1]))
film("menu_radar", scouter())  # (the name kept: the engine's MENU_RADAR is Tag Team's icon)
