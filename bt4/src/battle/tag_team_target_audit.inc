/* Hidden native regression: select through the production targeting function,
 * using the actual fighter camera, team checks, health and private pair state. */
s32 TagBattle_RunTargetAudit(void){
    extern void *BtlChar_Get(s32);
    extern void BtlChar_Place(void *,const Vec4 *,const Vec4 *,s32);
    extern s32 BtlStage_FindZoneNear(s32,const Vec4 *);
    extern f32 sinf(f32),cosf(f32);
    extern void BtlCharApi_GetCamPose(s32,Vec4 *,Vec4 *),BtlCharApi_GetPos(s32,Vec4 *);
    extern void *BtlMember_GetActiveGauge(void *);
    Vec4 eye,rot,origin,pos;f32 sy,cy,sp,cp;
    s32 i,fail=0,old=sTagTargets[0];
    static const s32 expected[4]={7,9,5,3};
    static const f32 offsets[10][2]={{0,0},{0,0},{20,0},{100,0},{0,0},{-100,0},{0,0},{0,100},{0,0},{0,-100}};
    if(sTagActors!=10)return 1;
    TagBattle_ResetPairs();
    BtlCharApi_GetCamPose(BattleSide_GetObjId(0),&eye,&rot);
    BtlCharApi_GetPos(BattleSide_GetObjId(1),&origin);
    sy=sinf(rot.y);cy=cosf(rot.y);sp=sinf(rot.x);cp=cosf(rot.x);
    for(i=1;i<10;++i){f32 x=offsets[i][0],y=offsets[i][1];
        pos=origin;pos.x+=x*cy+y*sp*sy;pos.y-=y*cp;pos.z+=-x*sy+y*sp*cy;
        BtlChar_Place(BtlChar_Get(i),&pos,&rot,BtlStage_FindZoneNear(-1,&pos));
    }
    for(i=1;i<=4;++i){sTagTargets[0]=1;
        if(!TagBattle_TargetDirection(0,i)||TagBattle_Target(0)!=expected[i-1])++fail;
        fprintf(stderr,"tag target direction=%d expected=%d actual=%d\n",i,expected[i-1],TagBattle_Target(0));
    }
    {s32 *hp=BtlMember_GetActiveGauge(BtlChar_Get(3));s32 saved=*hp;*hp=0;sTagTargets[0]=1;
        if(TagBattle_TargetDirection(0,4))++fail;*hp=saved;}
    sTagTargets[0]=1;TagBattle_ReservePair(0,1);
    if(TagBattle_TargetDirection(0,4)||TagBattle_Target(0)!=1)++fail;
    TagBattle_ResetPairs();sTagTargets[0]=old;
    fprintf(stderr,"tag target audit: four directions, allies, KO and pair lock %s\n",fail?"FAIL":"PASS");
    return fail;
}
