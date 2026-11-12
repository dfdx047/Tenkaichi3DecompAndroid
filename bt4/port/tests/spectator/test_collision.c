#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
typedef struct Vec4 { float x,y,z,w; } Vec4;
static struct { unsigned flags; } battle;
#define BATTLE_FLAG_LOADING 1u
#define Battle_GetWork() (&battle)
#include "../../../src/battle/spectator_collision.inc"
s32 *gColMeshBase;
SpecStage *gBtlStage;
static HeapBlock allocation;
HeapBlock *Heap_FindBlock(u32 data){return data==(u32)gColMeshBase?&allocation:NULL;}

int main(void) {
    struct {int storage[64];ColMesh mesh;SpecStageZone zone;SpecStageRec record;} blob __attribute__((aligned(16)))={0};
    int *storage=blob.storage,saved[64];
    ColMesh *mesh=&blob.mesh;*mesh=(ColMesh){1,1,3,0,8,16};
    SpecStageZone *zone=&blob.zone;SpecStageRec *record=&blob.record;
    allocation.dataSize=sizeof(blob);
    ColNode *node=(ColNode *)storage;ColPoly *poly=(ColPoly *)(storage+8);ColVec *v=(ColVec *)(storage+16);
    SpecStage stage={0};SpecStageObject object={0};
    float from[4]={0,0,-3,1},to[4]={0,0,3,1};Vec4 eye={0,0,3,1},previous={0,0,-3,1};
    node->box=(ColBox){{-1,-1,-.01f},{1,1,.01f}};node->left=0;node->right=-1;
    poly->vtx[0]=0;poly->vtx[1]=1;poly->vtx[2]=2;
    v[0]=(ColVec){-1,-1,0,1};v[1]=(ColVec){1,-1,0,1};v[2]=(ColVec){0,1,0,1};
    gColMeshBase=storage;gBtlStage=&stage;stage.count=1;stage.zones=zone;zone->mesh=mesh;
    memcpy(saved,storage,sizeof(saved));
    assert(fabsf(SpecCamera_StageRay(from,to)-.5f)<.001f);
    assert(fabsf(SpecCamera_StageRay(to,from)-.5f)<.001f);
    SpecCamera_AvoidStage(&eye,&previous);assert(eye.z<0);
    assert(!memcmp(saved,storage,sizeof(saved)) && zone->mark==0 && battle.flags==0);
    poly->flags=0x08000000;assert(SpecCamera_StageRay(from,to)==1);poly->flags=0;
    zone->mesh=0;zone->recCount=1;zone->recs=record;record->mesh=mesh;stage.objectCount=1;stage.objects=&object;
    assert(SpecCamera_StageRay(from,to)<1);
    object.state=1;assert(SpecCamera_StageRay(from,to)==1);
    record->broken=1;assert(SpecCamera_StageRay(from,to)<1);
    battle.flags=BATTLE_FLAG_LOADING;assert(SpecCamera_StageRay(from,to)==1);
    puts("PASS: native BVH leaves; two-sided walls; camera clearance; non-solid filtering; intact/broken object meshes; loading guard; no collision-world writes.");
    return 0;
}
