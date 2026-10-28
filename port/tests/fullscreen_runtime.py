"""Check fullscreen shortcuts with real X11 input; requires an EWMH window manager."""
import argparse
import ctypes as c
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--renderer', choices=('gpu', 'gl'), default='gpu')
a = p.parse_args()
out = ROOT / 'port/build/fullscreen-check' / a.renderer
out.mkdir(parents=True, exist_ok=True)
settings = out / 'settings.txt'
settings.write_text('fullscreen=0\ngpu_api='+str(int(a.renderer=='gl'))+'\n')
env = {k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='gpu', BT3_TAG_TEAM='1', BT3_TAG_MENU_TEST='main', BT3_WINDOW='800x600',
           BT3_NOSOUND='1', BT3_PACED='1', BT3_UPDATE_CHECK='0', BT3_FULLSCREEN='0',
           BT3_SETTINGS=str(settings), BT3_SAVES=str(out/'saves'), SDL_VIDEODRIVER='x11')
x = c.CDLL('libX11.so.6'); xt = c.CDLL('libXtst.so.6')
D, W, I, U = c.c_void_p, c.c_ulong, c.c_int, c.c_uint
def bind(lib, name, restype, *args):
    f=getattr(lib,name);f.restype=restype;f.argtypes=args;return f
display=bind(x,'XOpenDisplay',D,c.c_char_p)(None)
assert display, 'An X11 display is required (use xvfb-run).'
root=bind(x,'XDefaultRootWindow',W,D)(display)
query=bind(x,'XQueryTree',I,D,W,c.POINTER(W),c.POINTER(W),c.POINTER(c.POINTER(W)),c.POINTER(U))
name=bind(x,'XFetchName',I,D,W,c.POINTER(c.c_char_p))
free=bind(x,'XFree',I,D)
sync=bind(x,'XSync',I,D,I)
focus=bind(x,'XSetInputFocus',I,D,W,I,W)
geometry=bind(x,'XGetGeometry',I,D,W,c.POINTER(W),c.POINTER(I),c.POINTER(I),c.POINTER(U),c.POINTER(U),c.POINTER(U),c.POINTER(U))
keycode=bind(x,'XKeysymToKeycode',c.c_ubyte,D,W)
key=bind(xt,'XTestFakeKeyEvent',I,D,U,I,W)
button=bind(xt,'XTestFakeButtonEvent',I,D,U,I,W)
move=bind(xt,'XTestFakeMotionEvent',I,D,I,I,I,W)
translate=bind(x,'XTranslateCoordinates',I,D,W,W,I,I,c.POINTER(I),c.POINTER(I),c.POINTER(W))
def window(parent=None):
    rr,pp,n=W(),W(),U();children=c.POINTER(W)()
    query(display,root if parent is None else parent,c.byref(rr),c.byref(pp),c.byref(children),c.byref(n))
    found=None
    for i in range(n.value):
        title=c.c_char_p()
        if name(display,children[i],c.byref(title)) and title.value:
            if b'Native Tag Team' in title.value: found=children[i]
            free(title)
        if not found: found=window(children[i])
        if found: break
    if children:free(children)
    return found
def size(w):
    rr,xx,yy,ww,hh,b,d=W(),I(),I(),U(),U(),U(),U()
    assert geometry(display,w,c.byref(rr),c.byref(xx),c.byref(yy),c.byref(ww),c.byref(hh),c.byref(b),c.byref(d))
    return ww.value,hh.value
def press(symbol,on):
    key(display,keycode(display,symbol),on,0);sync(display,False)
def mode(expected):
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        if re.search(r'^fullscreen='+str(expected)+r'$',settings.read_text(),re.M):
            time.sleep(.4);return
        time.sleep(.05)
    raise AssertionError(('fullscreen',expected,settings.read_text()))
def alt_enter(enter=0xff0d):
    press(0xffe9,1);press(enter,1);time.sleep(.2);press(enter,0);press(0xffe9,0)
def double_click():
    xx,yy,child=I(),I(),W()
    translate(display,w,root,400,300,c.byref(xx),c.byref(yy),c.byref(child))
    move(display,0,xx.value,yy.value,0)
    for _ in range(2):
        button(display,1,1,0);sync(display,False);time.sleep(.08)
        button(display,1,0,0);sync(display,False);time.sleep(.1)
with (out/'runtime.log').open('w') as log:
    proc=subprocess.Popen([str(ROOT/'port/build/bt3_64')],cwd=ROOT,env=env,stdout=log,stderr=log)
    try:
        end=time.monotonic()+60;w=None
        while time.monotonic()<end and not w:
            assert proc.poll() is None, 'Native game exited'
            w=window();time.sleep(.1)
        assert w, 'Native window missing'
        focus(display,w,1,0);sync(display,False);time.sleep(4)
        initial=size(w)
        alt_enter();mode(1);assert size(w)==size(root),(size(w),size(root))
        # Holding Enter must not repeatedly toggle; releasing Alt first must
        # not turn the same Enter press into a gameplay input.
        press(0xffe9,1);press(0xff0d,1);mode(0)
        time.sleep(1.3);mode(0);press(0xffe9,0);time.sleep(.3);press(0xff0d,0)
        assert size(w)==initial,(size(w),initial)
        alt_enter(0xff8d);mode(1)
        double_click();mode(0)
        double_click();mode(1)
        press(0xffc8,1);press(0xffc8,0);mode(0)  # F11
        # Fullscreen remains available while the settings window owns input.
        press(0xffbe,1);press(0xffbe,0);time.sleep(.4)  # F1
        alt_enter();mode(1);alt_enter();mode(0)
        assert proc.poll() is None
        print(a.renderer+': Alt+Enter, keypad Enter, held keys, double-click, F11, settings and window size restoration passed.')
    finally:
        from PIL import ImageGrab
        ImageGrab.grab().save(out/'last-screen.png')
        proc.terminate()
        try:proc.wait(timeout=5)
        except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=5)
bind(x,'XCloseDisplay',I,D)(display)
