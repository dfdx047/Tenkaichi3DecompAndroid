/* One screen-space mask for a whole portrait, including atlas pieces.
   No new bitmaps, render targets or per-frame heap allocations. */
#ifndef GS_PORTRAIT_FADE_H
#define GS_PORTRAIT_FADE_H
static void GsPortrait_Finalize(GsDraw *draws,unsigned count,const Vtx *vertices,float scale) {
    float lo[2]={1e9f,1e9f},hi[2]={-1e9f,-1e9f};unsigned i,k;
    for(i=0;i<count;i++) {
        GsDraw *d=&draws[i];
        if(d->vu || (d->portrait[0]==0 && d->portrait[1]==0))continue;
        int side=d->portrait[1]<0 || (d->portrait[1]==0 && d->portrait[0]>0);
        for(k=d->first;k<d->first+d->count;k++) {
            float x=vertices[k].x;
            if(x<lo[side])lo[side]=x;
            if(x>hi[side])hi[side]=x;
        }
    }
    for(i=0;i<count;i++) {
        GsDraw *d=&draws[i];
        if(d->vu || (d->portrait[0]==0 && d->portrait[1]==0))continue;
        int side=d->portrait[1]<0 || (d->portrait[1]==0 && d->portrait[0]>0);
        float width=(hi[side]-lo[side])*0.12f*scale;
        d->portrait[0]=(side?hi[side]:lo[side])*scale;
        d->portrait[1]=side?-width:width;
    }
}
#endif
