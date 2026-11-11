/* Local presentation preferences. Deliberately outside game/rollback memory. */
#include "presentation.h"
#include <stdlib.h>
#include <stdio.h>
static int sHudHidden,sHudInitialized;
int PortHud_Hidden(void){
    if(!sHudInitialized){sHudInitialized=1;
        if(getenv("BT4_RENDER_TEST") && getenv("BT4_HUD_HIDDEN"))
            sHudHidden=atoi(getenv("BT4_HUD_HIDDEN"))!=0;
    }
    return sHudHidden;
}
void PortHud_SetHidden(int hidden){
    sHudInitialized=1;sHudHidden=hidden!=0;
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"presentation HUD: hidden=%d\n",sHudHidden);
}
void PortHud_Toggle(void){PortHud_SetHidden(!PortHud_Hidden());}

