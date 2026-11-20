import sys,os,numpy as np,cv2
from PIL import Image
S=sys.argv[1]; PX=int(sys.argv[2]); W,H=1100,495
t=np.asarray(Image.open(S+'/telas/10-fim-da-partida.png').convert('RGB')).copy()
m=np.zeros(t.shape[:2],np.uint8)
m[88:344,547:554]=1; m[88:344,688:696]=1; m[89:97,546:697]=1; m[335:343,546:697]=1; m[77:91,565:685]=1
m[340:,688:696]=0   # não mexe no botão Confirmar
t=cv2.inpaint(t,m*255,4,cv2.INPAINT_TELEA)
bg=np.asarray(Image.fromarray(t).resize((W,H),Image.LANCZOS)).astype(np.float32)
orig=np.asarray(Image.open(S+'/telas/10-fim-da-partida.png').convert('RGB').resize((W,H),Image.LANCZOS)).astype(np.float32)
sc=1.48*0.55; py=round(16*0.55)
hb=np.zeros((H,W),np.float32); y0,y1,x0,x1=462,490,960,1085
mm=cv2.dilate((orig[y0:y1,x0:x1].mean(2)>150).astype(np.uint8),np.ones((3,3),np.uint8))
hb[y0:y1,x0:x1]=cv2.GaussianBlur(mm.astype(np.float32),(0,0),0.7)
out=[]
for n in sorted(os.listdir(S+'/trunks-recorte')):
    f=Image.open(S+'/trunks-recorte/'+n); f=f.resize((round(f.width*sc),round(f.height*sc)),Image.LANCZOS)
    L=Image.new('RGBA',(W,H),(0,0,0,0)); L.paste(f,(PX,py)); fr=np.asarray(L).astype(np.float32); a=fr[...,3:]/255
    im=bg*(1-a)+fr[...,:3]*a; im=im*(1-hb[...,None])+orig*hb[...,None]
    out.append(Image.fromarray(np.clip(im,0,255).astype(np.uint8)))
out[0].save(S+'/out/10-fim-da-partida.png')
if len(sys.argv)>3:
    out[0].save(S+'/out/10-fim-da-partida-animada.webp',save_all=True,append_images=out[1:],duration=50,loop=0,quality=88,method=4)
    tiles=[out[i].resize((550,248)) for i in (0,25,50,75)]
    C=Image.new('RGB',(1100,496)); [C.paste(x,((k%2)*550,(k//2)*248)) for k,x in enumerate(tiles)]; C.save(S+'/out/quadros10.jpg')
