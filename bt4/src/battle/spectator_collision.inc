/* Read-only camera rays. Native StgCol queries overwrite shared collision
 * scratch globals, so local spectator cameras must not use those queries. */
#include "battle/col_a.h"
#include "sys/heap.h"
#define BT4_SPECTATOR_GAME_MATH
#include "../../port/src/gs/spectator_camera.h"
#undef BT4_SPECTATOR_GAME_MATH
typedef struct SpecStageRec { s32 broken,obj;ColMesh *mesh;s32 unused; } SpecStageRec;
typedef struct SpecStageZone {
    s32 mark;ColMesh *mesh;s32 nearCount;void *near;s32 unused,recCount;SpecStageRec *recs;s32 unused1;
    Vec4 center,half;
} SpecStageZone;
typedef struct SpecStageObject { s32 state;u8 unused[0x4c]; } SpecStageObject;
typedef struct SpecStage {
    u8 unused[0x38];u32 count;SpecStageZone *zones;void *bounds;u32 objectCount;SpecStageObject *objects;
} SpecStage;
#ifdef BATTLE_STG_A_H
#define SPEC_CAMERA_STAGE ((SpecStage *)gBtlStage)
#else
extern SpecStage *gBtlStage;
#define SPEC_CAMERA_STAGE gBtlStage
#endif
static s32 SpecCamera_Box(const ColBox *box,const f32 *from,const f32 *to,f32 end) {
    f32 begin=0;int k;
    for(k=0;k<3;k++) {
        f32 d=to[k]-from[k],a,b,t;
        if(fabsf(d)<.000001f){if(from[k]<box->min[k]-1 || from[k]>box->max[k]+1)return 0;continue;}
        a=(box->min[k]-1-from[k])/d;b=(box->max[k]+1-from[k])/d;
        if(a>b){t=a;a=b;b=t;}if(a>begin)begin=a;if(b<end)end=b;if(begin>end)return 0;
    }
    return 1;
}
/* Camera-only readers may visit zones the native collision query never
   selects. Reject empty/unbound records and keep all mesh offsets inside the
   stage allocation, without changing native collision scratch variables. */
extern HeapBlock *Heap_FindBlock(u32 data);
static u32 sSpecBlobBase,sSpecBlobBytes;
static s32 SpecCamera_Range(const void *ptr,u32 bytes) {
    u32 address=(u32)ptr;
    return address>=sSpecBlobBase && bytes<=sSpecBlobBytes &&
           address-sSpecBlobBase<=sSpecBlobBytes-bytes;
}
static void SpecCamera_Mesh(ColMesh *mesh,const f32 *from,const f32 *to,f32 *fraction) {
    s32 stack[128],count=1,visits=0;
    ColNode *nodes;ColPoly *polys;ColVec *vertices;
    if(!mesh || !gColMeshBase || !SpecCamera_Range(mesh,sizeof(*mesh)) || mesh->nodeCount<=0 ||
       (u32)mesh->nodeCount>sSpecBlobBytes/sizeof(ColNode) || mesh->nodeOfs<0 || mesh->polyOfs<0 || mesh->vtxOfs<0 ||
       (u32)mesh->nodeOfs>sSpecBlobBytes/4 || (u32)mesh->polyOfs>sSpecBlobBytes/4 || (u32)mesh->vtxOfs>sSpecBlobBytes/4)return;
    nodes=(ColNode *)(gColMeshBase+mesh->nodeOfs);polys=(ColPoly *)(gColMeshBase+mesh->polyOfs);
    vertices=(ColVec *)(gColMeshBase+mesh->vtxOfs);stack[0]=0;
    if(!SpecCamera_Range(nodes,(u32)mesh->nodeCount*sizeof(ColNode)) || !SpecCamera_Range(polys,sizeof(ColPoly)) || !SpecCamera_Range(vertices,sizeof(ColVec)))return;
    while(count && ++visits<=mesh->nodeCount) {
        s32 index=stack[--count];ColNode *node;
        if(index<0 || index>=mesh->nodeCount)continue;
        node=&nodes[index];
        if(!SpecCamera_Box(&node->box,from,to,*fraction))continue;
        if(node->right==-1) {
            ColPoly *poly;if(node->left<0 || (u32)node->left>=sSpecBlobBytes/sizeof(ColPoly))continue;poly=&polys[node->left];
            if(!SpecCamera_Range(poly,sizeof(*poly)) || poly->vtx[0]<0 || poly->vtx[1]<0 || poly->vtx[2]<0 ||
               (u32)poly->vtx[0]>=sSpecBlobBytes/sizeof(ColVec) || (u32)poly->vtx[1]>=sSpecBlobBytes/sizeof(ColVec) ||
               (u32)poly->vtx[2]>=sSpecBlobBytes/sizeof(ColVec) ||
               !SpecCamera_Range(&vertices[poly->vtx[0]],sizeof(ColVec)) || !SpecCamera_Range(&vertices[poly->vtx[1]],sizeof(ColVec)) ||
               !SpecCamera_Range(&vertices[poly->vtx[2]],sizeof(ColVec)))continue;
            if(poly->flags&(0x0c000000|0x60))continue;
            spec_triangle(from,to,(f32 *)&vertices[poly->vtx[0]],(f32 *)&vertices[poly->vtx[1]],(f32 *)&vertices[poly->vtx[2]],fraction);
        } else if(count+2<=128) { stack[count++]=node->right;stack[count++]=node->left; }
    }
}
static f32 SpecCamera_StageRay(const f32 *from,const f32 *to) {
    f32 fraction=1;u32 i;int j;
    if(!SPEC_CAMERA_STAGE || !SPEC_CAMERA_STAGE->zones || (Battle_GetWork()->flags&BATTLE_FLAG_LOADING))return 1;
    {
        HeapBlock *allocation=Heap_FindBlock((u32)gColMeshBase);
        if(!allocation || allocation->dataSize<=0)return 1;
        sSpecBlobBase=(u32)gColMeshBase;sSpecBlobBytes=(u32)allocation->dataSize;
        if(SPEC_CAMERA_STAGE->count>sSpecBlobBytes/sizeof(SpecStageZone) ||
           !SpecCamera_Range(SPEC_CAMERA_STAGE->zones,SPEC_CAMERA_STAGE->count*sizeof(SpecStageZone)))return 1;
    }
    for(i=0;i<SPEC_CAMERA_STAGE->count;i++) {
        SpecStageZone *zone=&SPEC_CAMERA_STAGE->zones[i];
        ColBox bounds;
        bounds.min[0]=zone->center.x-zone->half.x;bounds.max[0]=zone->center.x+zone->half.x;
        bounds.min[1]=-1e10f;bounds.max[1]=1e10f;
        bounds.min[2]=zone->center.z-zone->half.z;bounds.max[2]=zone->center.z+zone->half.z;
        if(!SpecCamera_Box(&bounds,from,to,fraction))continue;

        SpecCamera_Mesh(zone->mesh,from,to,&fraction);
        if(zone->recCount<0 || (u32)zone->recCount>sSpecBlobBytes/sizeof(SpecStageRec) ||
           !SpecCamera_Range(zone->recs,(u32)zone->recCount*sizeof(SpecStageRec)))continue;
        for(j=0;j<zone->recCount;j++) {
            SpecStageRec *rec=&zone->recs[j];
            if(rec->obj<0 || (u32)rec->obj>=SPEC_CAMERA_STAGE->objectCount || !SPEC_CAMERA_STAGE->objects)continue;
            if((rec->broken!=0)!=((SPEC_CAMERA_STAGE->objects[rec->obj].state&1)!=0))continue;
            SpecCamera_Mesh(rec->mesh,from,to,&fraction);
        }
    }
    return fraction;
}
static void SpecCamera_AvoidStage(Vec4 *eye,const Vec4 *previous) {
    f32 hit=SpecCamera_StageRay((const f32 *)previous,(const f32 *)eye);
    if(hit<1) {
        f32 dx=eye->x-previous->x,dy=eye->y-previous->y,dz=eye->z-previous->z;
        f32 length=sqrtf(dx*dx+dy*dy+dz*dz);
        hit=length>0?fmaxf(0,hit-2/length):0;
        eye->x=previous->x+dx*hit;eye->y=previous->y+dy*hit;eye->z=previous->z+dz*hit;
    }
}
