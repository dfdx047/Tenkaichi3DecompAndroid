#!/usr/bin/env python3
"""Z Warriors online, the search screen's flying cloud (our own drawing, approved 2026-11-27): the cloud going round
with its trail, port/tbo/ui/zw_cloud.mpg and its first picture. Film with alpha, as gs/gs_draw.c's (colours on top,
how solid below, MPEG-1; see menu_icon_films.py)."""
import math
import pathlib

import av
import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

OUT = pathlib.Path(__file__).resolve().parents[2] / "port/tbo/ui"
N_FILM = 448
S=3
EDGE=(168,84,8,255); Y=(255,206,40,255); HI=(255,238,130,255); SH=(244,160,18,255)
def cloud():
    W,Hh=300,200; K=4
    LINE=(150,66,6,255); BASE=(255,204,32,255); SHADE=(240,146,14,255); LIGHT=(255,240,120,255)
    puffs=[(214,92,36),(180,64,34),(140,66,32),(188,126,34),(146,134,32),(106,88,30),(108,124,28),(234,122,22),(160,100,48),(76,104,20)]
    def mask(grow=0,dx=0,dy=0,k=1.0):
        m=Image.new('L',(W*K,Hh*K)); d=ImageDraw.Draw(m)
        for x,y,r in puffs:
            rr=r*k+grow; d.ellipse(((x+dx-rr)*K,(y+dy-rr)*K,(x+dx+rr)*K,(y+dy+rr)*K),fill=255)
        # as pontas de tras, finas e viradas
        for pts in (((90,78),(58,86),(10,76),(44,100),(86,104)),((92,104),(50,112),(4,126),(48,130),(100,134))):
            d.polygon([((x+dx)*K,(y+dy)*K) for x,y in pts],fill=255)
        return m
    m=mask()
    line=m.filter(ImageFilter.MaxFilter(17))
    def paint(msk,col): return Image.composite(Image.new('RGBA',msk.size,col),Image.new('RGBA',msk.size),msk)
    im=paint(line,LINE)
    im.alpha_composite(paint(m,SHADE))
    top=ImageChops.multiply(m,mask(dx=-5,dy=-17,k=0.97)); im.alpha_composite(paint(top,BASE))
    d=ImageDraw.Draw(im)
    def curl(cx,cy,r,turn=1):
        pts=[]
        for t in range(0,75):
            u=t/74; a=math.radians(-20+turn*u*440); rr=r*(1-0.68*u); pts.append(((cx+rr*math.cos(a))*K,(cy+rr*math.sin(a))*K))
        d.line(pts,fill=LINE,width=int(3.2*K),joint='curve')
    curl(172,104,22); curl(128,102,14,-1); curl(208,116,12)
    for x,y,r in ((180,64,34),(140,66,32),(214,92,36),(106,88,30)):
        d.arc(((x-r+7)*K,(y-r+7)*K,(x+r-7)*K,(y+r-7)*K),200,285,fill=LIGHT,width=8*K)
        d.arc(((x-r)*K,(y-r)*K,(x+r)*K,(y+r)*K),208,300,fill=LINE,width=int(2.4*K))
    d.line([(24*K,80*K),(60*K,92*K),(92*K,92*K)],fill=LIGHT,width=5*K,joint='curve')
    return im.resize((W//2+36,Hh//2+24),Image.LANCZOS)
CL=cloud()
R=178
def frame(i,total=60):
    N=620; C=N//2; head=-math.pi/2+2*math.pi*i/total
    big=Image.new('RGBA',(N*S,N*S)); d=ImageDraw.Draw(big)
    def P(r,th): return ((C+r*math.cos(th))*S,(C+r*math.sin(th))*S)
    L=math.radians(236); n=140
    def band(off,wmax,fill,outline=None,a0=0.14,length=1.0,wave=6,ph=0.0):
        out=[]; inn=[]
        for k in range(n+1):
            u=k/n; th=head-a0-u*L*length; r=R+off*(1-0.35*u)+wave*math.sin(u*13-i*2*math.pi*3/total+ph)*min(1,u*3)
            w=wmax*(1-u)**0.9/2
            out.append(P(r+w,th)); inn.append(P(r-w,th))
        poly=out+inn[::-1]
        d.polygon(poly,fill=fill)
        if outline: d.line(poly+[poly[0]],fill=outline,width=int(2.6*S),joint='curve')
    LN=(150,66,6,255)
    band(15,11,(240,146,14,255),LN,0.20,0.56,3,2.1)
    band(-16,9,(255,204,32,255),LN,0.18,0.70,3,4.0)
    band(0,32,(255,204,32,255),LN,0.08,0.96,4)
    band(5,12,(240,146,14,255),None,0.08,0.70,4)
    band(-6,7,(255,240,120,255),None,0.08,0.55,4)
    for k in range(3):
        u=0.48+k*0.17; th=head-u*L-0.08-0.02*math.sin(i*0.4+k); r=R+(25 if k%2 else -27); w=(7-k*1.5)*S; x,y=P(r,th)
        d.ellipse((x-w,y-w,x+w,y+w),fill=(255,204,32,255),outline=LN,width=2*S)
    im=big.resize((N,N),Image.LANCZOS)
    ang=math.degrees(head)+90     # a direcao do voo
    c=CL.rotate(-ang,resample=Image.BICUBIC,expand=True)
    bob=4*math.sin(i*0.7); x=C+(R+bob)*math.cos(head); y=C+(R+bob)*math.sin(head)
    pad=Image.new('RGBA',im.size); pad.paste(c,(int(x-c.width/2),int(y-c.height/2)),c); im.alpha_composite(pad)
    glow=Image.new('RGBA',im.size,(255,196,40,0)); glow.putalpha(im.split()[3].filter(ImageFilter.GaussianBlur(11)).point(lambda v:int(v*0.18)))
    glow.alpha_composite(im); return glow


def main():
    total = 60  # 2.5 s a turn at 24 pictures a second
    pictures = []
    for i in range(total):
        # the 620 canvas has room to spare: the cloud and its trail fill the middle
        pictures.append(frame(i, total).crop((40, 40, 580, 580)).resize((N_FILM, N_FILM), Image.LANCZOS))
        print(i, end=" ", flush=True)
    c = av.open(str(OUT / "zw_cloud.mpg"), "w", format="mpeg")
    s = c.add_stream("mpeg1video", rate=24)
    s.width, s.height, s.pix_fmt, s.bit_rate = N_FILM, 2 * N_FILM, "yuv420p", 6_000_000
    s.codec_context.options = {"qmin": "1", "qmax": "5", "g": "12"}
    for p in pictures:
        a = np.asarray(p, np.uint8)
        stacked = np.concatenate([a[:, :, :3], np.repeat(a[:, :, 3:], 3, axis=2)])
        for pkt in s.encode(av.VideoFrame.from_ndarray(stacked, format="rgb24")):
            c.mux(pkt)
    for pkt in s.encode():
        c.mux(pkt)
    c.close()
    still = np.array(pictures[0]).astype(np.uint32)
    still[:, :, 3] = (still[:, :, 3] * 128 + 127) // 255  # (a picture's alpha: 0x80 opaque)
    Image.fromarray(still.astype(np.uint8), "RGBA").save(OUT / "zw_cloud.png")
    print("\nzw_cloud", (OUT / "zw_cloud.mpg").stat().st_size, "bytes")


if __name__ == "__main__":
    main()
