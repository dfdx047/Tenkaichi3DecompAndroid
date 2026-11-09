"""Cuts the game's logo out of the README banner for the launcher (docs/plans/launcher/LAUNCHER_PLAN.md).

The banner (assets/BT2 Recomp Banner.png) shows the logo over a dark background; the logo has a dark
outline all around it. The background is what can be reached from the crop's border without crossing
dark pixels; an opening removes thin bits of background left touching the logo, the largest piece is
kept with its holes filled, and the edge is softened by a pixel. The result is a PNG with transparency,
assets/launcher/logo.png, which the build copies into the assets folder next to the launcher.

    python tools/launcher/cut_logo.py [--out assets/launcher/logo.png]
"""
import argparse
from pathlib import Path
import numpy as np
from PIL import Image
from scipy import ndimage as ndi

ROOT=Path(__file__).resolve().parents[2]
BANNER=ROOT/'assets'/'BT2 Recomp Banner.png'
CROP=(990,120,1810,660) # The logo's part of the banner (left, top, right, bottom).


def cut(rgb,dark_below=45,open_radius=8):
    """The logo's alpha (0..1) in an RGB array."""
    dark=rgb.max(axis=2)<dark_below
    pieces,_=ndi.label(~dark)
    edge=set(np.unique(np.concatenate([pieces[0],pieces[-1],pieces[:,0],pieces[:,-1]])))-{0}
    logo=~np.isin(pieces,list(edge))
    disk=np.hypot(*np.mgrid[-open_radius:open_radius+1,-open_radius:open_radius+1])<=open_radius
    logo=ndi.binary_opening(logo,structure=disk)
    pieces,count=ndi.label(logo)
    logo=pieces==1+int(np.argmax(ndi.sum(logo,pieces,range(1,count+1))))
    logo=ndi.binary_closing(ndi.binary_fill_holes(logo),structure=disk[::2,::2])
    return ndi.gaussian_filter(logo.astype(float),1.0)


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--out',type=Path,default=ROOT/'assets'/'launcher'/'logo.png')
    a=p.parse_args()
    rgb=np.array(Image.open(BANNER).convert('RGB').crop(CROP))
    alpha=cut(rgb)
    ys,xs=np.where(alpha>0.5)
    top,bottom,left,right=ys.min(),ys.max()+1,xs.min(),xs.max()+1
    rgba=np.dstack([rgb,(alpha*255).round().astype(np.uint8)])[top:bottom,left:right]
    a.out.parent.mkdir(parents=True,exist_ok=True)
    Image.fromarray(rgba).save(a.out)
    print(f'{a.out}: {rgba.shape[1]} x {rgba.shape[0]}')


if __name__=='__main__':main()
