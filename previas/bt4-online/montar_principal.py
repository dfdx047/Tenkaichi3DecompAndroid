import sys,os,numpy as np,cv2
from PIL import Image
S=sys.argv[1]; DX=int(sys.argv[2]); W,H=1100,495
bt4=np.asarray(Image.open(S+'/telas/01-principal.jpg').convert('RGB')).astype(np.float32)
ref=np.asarray(Image.open(S+'/telas/tagteam-ref.jpg').convert('RGB').resize((W,H),Image.LANCZOS)).astype(np.float32)
sc=1.48*0.55; px0,py=round(776*0.55),round(16*0.55)
def layer(n,px):
    f=Image.open(S+'/trunks-recorte/'+n); f=f.resize((round(f.width*sc),round(f.height*sc)),Image.LANCZOS)
    L=Image.new('RGBA',(W,H),(0,0,0,0)); L.paste(f,(px,py)); return np.asarray(L).astype(np.float32)
names=sorted(os.listdir(S+'/trunks-recorte'))
oldf=[layer(n,px0)[...,3] for n in names[::5]]
old=cv2.dilate((np.max(oldf,0)>10).astype(np.uint8),np.ones((11,11),np.uint8))
# fundo: ref (limpo) à direita do menu, fora do Trunks antigo; BT4 no resto
xs=np.arange(W)[None,:].repeat(H,0)
wr=np.clip((xs-600)/30,0,1)*(1-cv2.GaussianBlur(old.astype(np.float32),(0,0),2))
wr[455:,720:]=0
clean=bt4*(1-wr[...,None])+ref*wr[...,None]
# restos do lugar reservado dentro do Trunks antigo: pintar por cima
hole=np.zeros((H,W),np.uint8); hole[80:450,640:915]=1; hole&=old
clean8=np.clip(clean,0,255).astype(np.uint8)
clean8=cv2.inpaint(clean8,hole*255,6,cv2.INPAINT_TELEA)
clean=clean8.astype(np.float32)
hb=np.zeros((H,W),np.float32); box=bt4[456:492,730:1090]
m=cv2.dilate((box.mean(2)>150).astype(np.uint8),np.ones((3,3),np.uint8))
hb[456:492,730:1090]=cv2.GaussianBlur(m.astype(np.float32),(0,0),0.7)
out=[]
for n in names:
    fr=layer(n,px0+DX); a=fr[...,3:]/255
    im=clean*(1-a)+fr[...,:3]*a
    im=im*(1-hb[...,None])+bt4*hb[...,None]
    out.append(Image.fromarray(np.clip(im,0,255).astype(np.uint8)))
os.makedirs(S+'/out',exist_ok=True)
out[0].save(S+'/out/00-principal.png')
Image.fromarray(clean8).save(S+'/out/fundo-limpo.png')
if len(sys.argv)>3:
    out[0].save(S+'/out/00-principal-animada.webp',save_all=True,append_images=out[1:],duration=50,loop=0,quality=88,method=4)
    tiles=[out[i].resize((550,248)) for i in (0,25,50,75)]
    C=Image.new('RGB',(1100,496)); [C.paste(t,((k%2)*550,(k//2)*248)) for k,t in enumerate(tiles)]; C.save(S+'/out/quadros.jpg')
