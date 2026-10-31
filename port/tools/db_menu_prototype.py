import math, random, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont
R='/home/lawliet/Projetos-Recomp/Tenkaichi3DecompAndroid-tag/port/third_party/fonts/'
UI='/home/lawliet/Projetos-Recomp/Tenkaichi3DecompAndroid-tag/port/tbo/ui/'
W,H=2712,1220
KANIT=lambda s:ImageFont.truetype(R+'kanit/Kanit-ExtraBoldItalic.ttf',int(s))
FIRA=lambda s:ImageFont.truetype(R+'fira/FiraSansCondensed-Bold.ttf',int(s))
random.seed(3)
img=Image.new('RGBA',(W,H))
# --- sky: deep blue at the top to a warm orange horizon (the game's dusk)
sky=Image.new('RGBA',(1,H))
for y in range(H):
    t=y/H
    stops=[(0,(18,52,140)),(0.45,(70,140,220)),(0.72,(255,190,110)),(1,(255,120,40))]
    for (a,ca),(b,cb) in zip(stops,stops[1:]):
        if a<=t<=b:
            u=(t-a)/(b-a); sky.putpixel((0,y),tuple(int(ca[i]+(cb[i]-ca[i])*u) for i in range(3))+(255,))
img.alpha_composite(sky.resize((W,H)))
# sun glow behind the right window
g=Image.new('RGBA',(W,H)); ImageDraw.Draw(g).ellipse([1700,520,2500,1100],fill=(255,240,180,170))
img.alpha_composite(g.filter(ImageFilter.GaussianBlur(120)))
# clouds: puffs in layers (Nimbus-like, white with a warm underside)
def cloud(cx,cy,s,alpha):
    c=Image.new('RGBA',(W,H)); d=ImageDraw.Draw(c)
    for k in range(9):
        r=random.uniform(0.5,1.0)*s; x=cx+random.uniform(-1.6,1.6)*s; y=cy+random.uniform(-0.35,0.25)*s
        d.ellipse([x-r,y-r*0.75,x+r,y+r*0.75],fill=(255,250,240,alpha))
    sh=Image.new('RGBA',(W,H)); ds=ImageDraw.Draw(sh)
    ds.rectangle([cx-3*s,cy+0.05*s,cx+3*s,cy+s],fill=(255,170,120,alpha//2))
    c2=Image.composite(sh,c,Image.eval(c.split()[3],lambda v:0))
    img.alpha_composite(c.filter(ImageFilter.GaussianBlur(4)))
for cx,cy,s,a in [(300,980,190,230),(1100,1060,230,235),(2300,1000,220,230),(1700,160,120,140),(2500,260,100,130),(900,140,90,110),(2050,1120,180,240)]:
    cloud(cx,cy,s,a)
# speed streaks
st=Image.new('RGBA',(W,H)); d=ImageDraw.Draw(st)
for k in range(40):
    y=random.uniform(80,900); x=random.uniform(-200,W); l=random.uniform(200,700)
    d.line([(x,y),(x+l,y-l*0.18)],fill=(255,255,255,random.randint(25,60)),width=random.choice([3,4,6]))
img.alpha_composite(st.filter(ImageFilter.GaussianBlur(1)))
D=lambda:ImageDraw.Draw(img)

def text(xy,s,font,fill,anchor='la',stroke=0,sfill=(0,0,0,255),shadow=0):
    if shadow:
        sh=Image.new('RGBA',(W,H)); ImageDraw.Draw(sh).text((xy[0]+shadow,xy[1]+shadow),s,font=font,fill=(0,0,0,170),anchor=anchor,stroke_width=stroke,stroke_fill=(0,0,0,170))
        img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(4)))
    D().text(xy,s,font=font,fill=fill,anchor=anchor,stroke_width=stroke,stroke_fill=sfill)
def grad_text(xy,s,font,top,bot,stroke,sfill,anchor='la'):
    mask=Image.new('L',(W,H)); ImageDraw.Draw(mask).text(xy,s,font=font,fill=255,anchor=anchor)
    bb=mask.getbbox()
    out=Image.new('RGBA',(W,H)); ImageDraw.Draw(out).text(xy,s,font=font,fill=sfill,anchor=anchor,stroke_width=stroke,stroke_fill=sfill)
    gl=Image.new('RGBA',(W,H)); gd=ImageDraw.Draw(gl)
    for y in range(bb[1],bb[3]+1):
        t=(y-bb[1])/max(1,bb[3]-bb[1]); gd.line([(bb[0],y),(bb[2],y)],fill=tuple(int(top[i]*(1-t)+bot[i]*t) for i in range(3))+(255,))
    out.paste(gl,(0,0),mask)
    sh=Image.new('RGBA',(W,H)); ImageDraw.Draw(sh).text((xy[0]+7,xy[1]+9),s,font=font,fill=(40,10,0,200),anchor=anchor,stroke_width=stroke,stroke_fill=(40,10,0,200))
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(5))); img.alpha_composite(out)
def poly(pts,fill=None,outline=None,width=0,blur=0):
    l=Image.new('RGBA',(W,H)); ImageDraw.Draw(l).polygon(pts,fill=fill,outline=None)
    if outline: ImageDraw.Draw(l).line(pts+[pts[0]],fill=outline,width=width,joint='curve')
    img.alpha_composite(l.filter(ImageFilter.GaussianBlur(blur)) if blur else l)
def grad_poly(pts,top,bot):
    ys=[p[1] for p in pts]; y0,y1=min(ys),max(ys)
    m=Image.new('L',(W,H)); ImageDraw.Draw(m).polygon(pts,fill=255)
    gl=Image.new('RGBA',(W,H)); gd=ImageDraw.Draw(gl)
    for y in range(int(y0),int(y1)+1):
        t=(y-y0)/max(1,y1-y0); gd.line([(0,y),(W,y)],fill=tuple(int(top[i]*(1-t)+bot[i]*t) for i in range(4)))
    out=Image.new('RGBA',(W,H)); out.paste(gl,(0,0),m); img.alpha_composite(out)
def star(cx,cy,r,fill):
    pts=[]
    for k in range(10):
        rr=r if k%2==0 else r*0.42; a=-math.pi/2+k*math.pi/5
        pts.append((cx+math.cos(a)*rr,cy+math.sin(a)*rr))
    D().polygon(pts,fill=fill)
def dragonball(cx,cy,r,stars,glow=True):
    if glow:
        g=Image.new('RGBA',(W,H)); ImageDraw.Draw(g).ellipse([cx-r*1.5,cy-r*1.5,cx+r*1.5,cy+r*1.5],fill=(255,170,40,140)); img.alpha_composite(g.filter(ImageFilter.GaussianBlur(r*0.5)))
    b=Image.new('RGBA',(W,H)); bd=ImageDraw.Draw(b)
    for k in range(int(r),0,-1):
        t=k/r; c=(255,int(120+110*(1-t)),int(20+120*(1-t)**2),255)
        ox=-r*0.28*(1-t); oy=-r*0.3*(1-t)
        bd.ellipse([cx+ox-k,cy+oy-k,cx+ox+k,cy+oy+k],fill=c)
    img.alpha_composite(b)
    D().ellipse([cx-r,cy-r,cx+r,cy+r],outline=(170,70,0,255),width=max(2,int(r*0.06)))
    pos={1:[(0,0)],2:[(-.3,0),(.3,0)],3:[(0,-.3),(-.3,.22),(.3,.22)],4:[(-.28,-.28),(.28,-.28),(-.28,.28),(.28,.28)],
         5:[(0,-.38),(-.36,-.08),(.36,-.08),(-.22,.34),(.22,.34)],6:[(-.3,-.32),(.3,-.32),(-.38,.05),(.38,.05),(-.18,.38),(.18,.38)],7:[(0,0),(0,-.4),(-.36,-.18),(.36,-.18),(-.36,.22),(.36,.22),(0,.42)]}[stars]
    for px,py in pos: star(cx+px*r,cy+py*r,r*(0.26 if stars>1 else 0.42),(220,30,20,255))
    hl=Image.new('RGBA',(W,H)); ImageDraw.Draw(hl).ellipse([cx-r*0.62,cy-r*0.7,cx-r*0.1,cy-r*0.32],fill=(255,255,255,170)); img.alpha_composite(hl.filter(ImageFilter.GaussianBlur(r*0.06)))

# --- title (the game's own orange lettering style) and the TAG TEAM capsule
grad_text((110,40),'MODOS MODIFICADOS',KANIT(120),(255,250,190),(255,130,20),9,(60,14,0,255))
poly([(130,198),(560,198),(535,262),(105,262)],fill=(200,20,20,240),outline=(255,230,140,255),width=5)
text((330,230),'TAG TEAM MOD',KANIT(48),(255,255,255,255),'mm',4,(80,0,0,255))
# --- the plates: the main menu's ring, slanted, the chosen one long and lit
items=['BATALHA EM EQUIPE','TODOS CONTRA TODOS','TREINO MODIFICADO','DESAFIO DE LUTADORES','TORNEIO TAG TEAM','SALA ONLINE','CONFIGURAÇÕES DO MOD','VOLTAR']
sel=2
y0=310; pitch=92
for i,lab in enumerate(items[:7]):
    y=y0+i*pitch; off=abs(i-sel)*26
    x0=70+off; slant=40
    if i==sel:
        x1=1220
        poly([(x0+slant-20,y-18),(x1+20,y-18),(x1-slant+20,y+86),(x0-20,y+86)],fill=(255,180,40,200),blur=26)
        grad_poly([(x0+slant,y-8),(x1,y-8),(x1-slant,y+78),(x0,y+78)],(255,240,150,255),(240,120,10,255))
        D().line([(x0+slant,y-8),(x1,y-8),(x1-slant,y+78),(x0,y+78),(x0+slant,y-8)],fill=(120,30,0,255),width=6)
        dragonball(x0+82,y+35,46,4)
        text((x0+150,y+35),lab,KANIT(66),(255,255,255,255),'lm',6,(110,30,0,255),shadow=4)
    else:
        x1=1050-off
        grad_poly([(x0+slant,y),(x1,y),(x1-slant,y+70),(x0,y+70)],(235,245,255,235),(150,190,240,235))
        D().line([(x0+slant,y),(x1,y),(x1-slant,y+70),(x0,y+70),(x0+slant,y)],fill=(20,40,110,255),width=5)
        dragonball(x0+70,y+35,26,(i%7)+1,glow=False)
        text((x0+118,y+35),lab,KANIT(50),(30,50,120,255),'lm',3,(255,255,255,255))
# arrows of the ring (more above/below)
for yy,up in [(y0+7*pitch-4,False)]:
    cx=520; s=1 if up else -1
    D().polygon([(cx,yy-18*s),(cx-26,yy+12*s),(cx+26,yy+12*s)],fill=(255,230,120,255),outline=(120,40,0,255))
# --- the picture window on the right (the game's icon window), gold frame, the mode's picture
fx0,fy0,fx1,fy1=1400,250,2600,830
poly([(fx0-10,fy0-10),(fx1+10,fy0-10),(fx1+10,fy1+10),(fx0-10,fy1+10)],fill=(0,0,0,120),blur=20)
D().rounded_rectangle([fx0,fy0,fx1,fy1],radius=36,fill=(255,200,70,255),outline=(120,40,0,255),width=6)
D().rounded_rectangle([fx0+18,fy0+18,fx1-18,fy1-18],radius=26,fill=(20,30,80,255))
# inside: a Kami's-lookout-like training target scene in the window
inner=Image.new('RGBA',(W,H)); di=ImageDraw.Draw(inner)
for y in range(fy0+18,fy1-18):
    t=(y-fy0)/(fy1-fy0); di.line([(fx0+18,y),(fx1-18,y)],fill=(int(60+150*t),int(110+60*t),int(200-80*t),255))
m=Image.new('L',(W,H)); ImageDraw.Draw(m).rounded_rectangle([fx0+18,fy0+18,fx1-18,fy1-18],radius=26,fill=255)
o=Image.new('RGBA',(W,H)); o.paste(inner,(0,0),m); img.alpha_composite(o)
# rays
rays=Image.new('RGBA',(W,H)); dr=ImageDraw.Draw(rays); cx,cy=(fx0+fx1)/2,(fy0+fy1)/2+20
for k in range(18):
    a=k*math.pi/9; dr.polygon([(cx,cy),(cx+math.cos(a)*900,cy+math.sin(a)*900),(cx+math.cos(a+0.12)*900,cy+math.sin(a+0.12)*900)],fill=(255,255,220,45))
o=Image.new('RGBA',(W,H)); o.paste(rays,(0,0),m); img.alpha_composite(o)
# the seven balls around the big four-star one
for k in range(7):
    a=-math.pi/2+k*2*math.pi/7
    dragonball(cx+math.cos(a+math.pi/7)*400,cy+30+math.sin(a+math.pi/7)*185,40,k+1,glow=False)
dragonball(cx,cy+30,130,4)
# label strip at the window's foot
poly([(fx0+60,fy1-30),(fx1-20,fy1-30),(fx1-60,fy1+50),(fx0+20,fy1+50)],fill=(200,20,20,255),outline=(255,230,140,255),width=5)
text(((fx0+fx1)/2,fy1+10),'TREINO MODIFICADO',KANIT(56),(255,255,255,255),'mm',4,(80,0,0,255))
# 2 vs 2 chip on the window
for j,(lab,col) in enumerate([('P1',(50,110,230)),('P2',(50,110,230)),('CPU',(210,40,40)),('CPU',(210,40,40))]):
    x=fx0+50+j*150
    D().rounded_rectangle([x,fy0+40,x+130,fy0+100],radius=14,fill=col+(255,),outline=(255,255,255,255),width=4)
    text((x+65,fy0+70),lab,KANIT(40),(255,255,255,255),'mm',3,(0,0,0,255))
# --- the message window at the foot (the game's guide box): a portrait disc + the line
bx0,by0,bx1,by1=60,1000,2650,1150
poly([(bx0,by0+8),(bx1,by0+8),(bx1,by1+8),(bx0,by1+8)],fill=(0,0,0,110),blur=16)
D().rounded_rectangle([bx0,by0,bx1,by1],radius=30,fill=(250,250,255,245),outline=(30,50,140,255),width=7)
D().rounded_rectangle([bx0+10,by0+10,bx1-10,by1-10],radius=24,outline=(255,170,40,255),width=4)
# portrait slot: the four-star ball as the speaker (the guide characters stay as in the game)
D().ellipse([bx0+30,by0-60,bx0+230,by0+140],fill=(20,40,110,255),outline=(255,200,70,255),width=7)
dragonball(bx0+130,by0+40,72,4,glow=False)
text((bx0+270,by0+52),'Treine com equipes e regras de treino ajustáveis. Troque de lutador a qualquer momento.',FIRA(50),(30,40,90,255),'lm')
text((bx0+270,by0+108),'SELECT  —  voltar aos modos normais',FIRA(38),(200,90,0,255),'lm')
# prompts with the real button art, top right
def btn(path,x,y,h):
    b=Image.open(UI+path).convert('RGBA'); w=int(b.width*h/b.height); img.alpha_composite(b.resize((w,h),Image.LANCZOS),(int(x),int(y))); return w
x=W-90
for path,lab in [('btn_triangle.png','Voltar'),('btn_cross.png','Entrar')]:
    tw=D().textlength(lab,font=FIRA(46)); x-=tw
    text((x,150),lab,FIRA(46),(255,255,255,255),'lm',4,(20,30,80,255))
    w=int(Image.open(UI+path).width*60/Image.open(UI+path).height); x-=w+14; btn(path,x,120,60); x-=50
img.convert('RGB').save(sys.argv[1],quality=92); print('ok')
