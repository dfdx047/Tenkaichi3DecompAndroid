/* Geometry contract: all seat pairs, both three-player styles, simultaneous
   fusions, no accidental spectator merging, and display-list round trips. */
#include <assert.h>
#include <stdio.h>
#include "../../include/battle/tag_view_layout.h"
static void check(int n,int wide,int *group,int expected) {
    int r[4][4],visible[4],i,j,k,area=0,views=0;
    TagView_Layout(n,wide,group,r,visible);
    for(i=0;i<n;i++) if(visible[i]) {
        int copy[4];
        views++;area+=r[i][2]*r[i][3];
        assert(r[i][0]>=0&&r[i][1]>=0&&r[i][0]+r[i][2]<=512&&r[i][1]+r[i][3]<=448);
        for(k=0;k<2;k++) {
            int marker=TagView_HudMarker(r[i],k);
            assert(marker>=0x80&&marker<=0xBF);
            TagView_HudRect(marker,copy);
            for(j=0;j<4;j++) assert(r[i][j]==copy[j]);
        }
        for(j=0;j<i;j++) if(visible[j]) assert(
            r[i][0]+r[i][2]<=r[j][0]||r[j][0]+r[j][2]<=r[i][0]||
            r[i][1]+r[i][3]<=r[j][1]||r[j][1]+r[j][3]<=r[i][1]);
    }
    assert(views==expected);
    assert(area==512*448 || (n==3&&!wide&&visible[2]&&r[2][0]==128&&area==3*256*224));
}
int main(void) {
    int n,wide,a,b;
    for(n=1;n<=4;n++) for(wide=0;wide<=1;wide++) {
        int group[4]={-1,-1,-1,-1};
        check(n,wide,group,n);
        for(a=0;a<n;a++) for(b=a+1;b<n;b++) {
            group[a]=group[b]=9;
            check(n,wide,group,n-((n==4&&(a^b)==3)?0:1));
            group[a]=group[b]=-1;
            check(n,wide,group,n); /* defusion */
        }
    }
    {int g[4]={0,0,2,2};check(4,1,g,2);}
    {int g[4]={0,1,0,1};check(4,1,g,2);}
    {int g[4]={0,1,1,0};check(4,1,g,4);}
    puts("PASS all fused seat layouts, bounds, non-overlap and HUD rectangle packets");
    return 0;
}
