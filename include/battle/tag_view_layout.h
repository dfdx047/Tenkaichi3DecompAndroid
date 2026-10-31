#ifndef TAG_VIEW_LAYOUT_H
#define TAG_VIEW_LAYOUT_H
/* Value-only geometry shared by the game and display-list renderer. Player
   seats never move; a fused pair occupies one rectangle without renumbering
   controllers, camera records, particle banks or HUD widgets. */
static inline void TagView_Layout(int count,int wide,const int *fusion,
                                  int rects[4][4],int visible[4]) {
    int i,j,k;
    for(i=0;i<4;i++) {
        int *r=rects[i];
        visible[i]=i<count;
        r[0]=count>=2?(i%2)*256:0; r[1]=count>=3?(i/2)*224:0;
        r[2]=count>=2?256:512; r[3]=count>=3?224:448;
        if(count==3 && i==2) {r[0]=wide?0:128;r[2]=wide?512:256;}
    }
    for(i=0;i<count;i++) for(j=i+1;j<count;j++) {
        int *a=rects[i],*b=rects[j];
        if(fusion[i]<0 || fusion[i]!=fusion[j] || !visible[i] || !visible[j]) continue;
        if(count==4 && (i^j)==3) continue; /* diagonal seats retain both views */
        visible[j]=0;
        if(count==2) {a[0]=a[1]=0;a[2]=512;a[3]=448;}
        else if(count==3 && j==2) {
            /* A lower seat touches both upper seats. With two bodies left,
               give the fused pair its upper player's full-height column. */
            int other=1-i;
            a[0]=i*256;a[1]=0;a[2]=256;a[3]=448;
            rects[other][0]=other*256;rects[other][1]=0;
            rects[other][2]=256;rects[other][3]=448;
        } else if((i^j)==1) {a[0]=0;a[2]=512;}
        else {a[1]=0;a[3]=448;}
        /* Valid geometry even for a suspended camera record. */
        for(k=0;k<4;k++) b[k]=a[k];
    }
}

/* Carry the actual rectangle in the display list, so a later game update
   cannot change the HUD layout while the renderer is consuming this frame. */
static inline int TagView_HudMarker(const int *r,int bottom) {
    return 0x80|(r[0]/128)|((r[1]/224)<<2)|((r[2]==512)<<3)|
        ((r[3]==448)<<4)|(bottom?0x20:0);
}
static inline void TagView_HudRect(int marker,int *r) {
    r[0]=(marker&3)*128;r[1]=((marker>>2)&1)*224;
    r[2]=(marker&8)?512:256;r[3]=(marker&16)?448:224;
}
#endif
