#include "common.h"
#include "battle/hud_a.h"
#include "sys/gfx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Gfx gGfx;
void Dma_AddData(void *data,s32 size){(void)data;(void)size;assert(!"Unexpected renderer call in health-state test");}
void HudSprite_Show(HudSprite *s,s32 on){s->flags=(s->flags&~1u)|!on;}
void HudSprite_SetTex(HudSprite *s,s32 tex,s32 palette){s->tex=tex;s->texSub=palette;}
void HudSprite_Move(HudSprite *s,s32 x,s32 y){s->pos.x0+=x;s->pos.x1+=x;s->pos.y0+=y;s->pos.y1+=y;}
extern void HudGauge_UpdateHp(void);
int main(void) {
    static const int cases[][3]={
        {5000,3,-1},{10000,3,3},{15000,4,3},{20000,4,4},
        {25000,1,4},{30000,1,1},{35000,1,1},{40000,1,1},
        {42000,5,1},{45000,5,1},{48000,5,1},{50000,5,5}
    };
    HudGauge work;HudSprite sprites[9];unsigned i;int side;
    memset(&work,0,sizeof(work));memset(sprites,0,sizeof(sprites));
    work.sprites=sprites;gHudGauge=&work;
    for(side=0;side<2;++side)for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        work.side=side;work.hpShown[side]=work.cur.hp[side]=cases[i][0];
        HudGauge_UpdateHp();
        assert(sprites[8].texSub==cases[i][1]);
        if(cases[i][2]<0)assert(sprites[3].flags&1);
        else{assert(!(sprites[3].flags&1));assert(sprites[3].texSub==cases[i][2]);}
    }
    puts("Health regression: both sides, all tiers, full blue and progressive green backing PASS");return 0;
}
