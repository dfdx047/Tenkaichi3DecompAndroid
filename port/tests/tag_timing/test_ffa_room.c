#include "tag_team_wire.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
    TtRoom room,decoded;unsigned char wire[512];unsigned member;int result;char code[7];
    Tt_Open(&room,101,"Host",3,0x1234);room.delay=0;
    assert(Tt_SetMode(&room,102,room.epoch,1)==TT_BAD_MEMBER);
    assert(Tt_SetMode(&room,101,room.epoch,1)==TT_OK);
    for(int i=1;i<6;++i)assert(Tt_Join(&room,101+i,"Guest",room.content)==TT_OK);
    assert(Tt_Count(&room)==6);
    assert(Tt_Join(&room,107,"Extra",room.content)==TT_FULL);
    assert(Tt_SetTeams(&room,101,room.epoch,2,4)==TT_BAD_CHOICE);
    assert(Tt_BeginSelect(&room,101,room.epoch)==TT_OK);
    for(int i=0;i<6;++i)assert(Tt_ChooseSeat(&room,101+i,room.epoch,i,100+i,0)==TT_OK);
    unsigned long long digest=Tt_LoadDigest(&room);
    room.free_for_all=0;assert(Tt_LoadDigest(&room)!=digest);room.free_for_all=1;
    assert(Tt_BeginLoad(&room,101,room.epoch,digest)==TT_OK);
    for(int i=0;i<6;++i)assert(Tt_Loaded(&room,101+i,room.epoch,digest)==TT_OK);
    assert(Tt_Arm(&room,101,room.epoch)==TT_OK);
    for(int i=0;i<6;++i)assert(Tt_ArmAck(&room,101+i,room.epoch)==TT_OK);
    Ttw_Snapshot(wire,&room,101,0,"ABC234");
    assert(Ttw_Decode(wire,&decoded,&member,&result,code));
    assert(decoded.free_for_all==1&&member==101&&Tt_Count(&decoded)==6);
    Ttw_Write(wire+496,2);assert(!Ttw_Decode(wire,&decoded,&member,&result,code));
    Ttw_Write(wire+496,1);Ttw_Write(wire,TT_PROTOCOL-1);assert(!Ttw_Decode(wire,&decoded,&member,&result,code));
    assert(Tt_Start(&room,101,room.epoch)==TT_OK);
    puts("PASS six-player FFA admission, ownership, draft, digest, armed barrier and wire validation");
}
