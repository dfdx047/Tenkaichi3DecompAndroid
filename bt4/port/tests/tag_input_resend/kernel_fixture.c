/* The owner's TAG TEAM kernel (shared/tag_kernel.c, the one tag_team_p2p.cpp's owner runs), set into a fight of six
   players by its own room functions (as tests/tag_timing/test_ffa_room.c does), for test.cpp. */
#define TT_KERNEL_NATIVE 1
#include <stdio.h>
#include "../../../shared/tag_kernel.c"

/* A six-player room in its fight; `delay` frames of input lead. Returns the round (epoch). */
unsigned fixture_fight(unsigned delay) {
    int i;unsigned long long digest;
    Kernel_Reset();Kernel_Pacing(1);
    Tt_Open(&room,101,"Host",3,0x1234);
    Tt_SetMode(&room,101,room.epoch,1);
    for(i=1;i<6;++i)Tt_Join(&room,101+(unsigned)i,"Guest",room.content);
    Tt_BeginSelect(&room,101,room.epoch);
    for(i=0;i<6;++i)Tt_ChooseSeat(&room,101+(unsigned)i,room.epoch,i,100+i,0);
    digest=Tt_LoadDigest(&room);
    Tt_BeginLoad(&room,101,room.epoch,digest);
    Tt_InputsReset(&frames,room.epoch,Tt_Count(&room));
    for(i=0;i<6;++i)Tt_Loaded(&room,101+(unsigned)i,room.epoch,digest);
    Tt_Arm(&room,101,room.epoch);
    for(i=0;i<6;++i)Tt_ArmAck(&room,101+(unsigned)i,room.epoch);
    room.delay=(int)delay;
    Ttw_Request(Kernel_Input(),TTW_START,room.epoch);
    if(Kernel_Command(101)!=TT_OK){fprintf(stderr,"fixture: the fight did not start\n");return 0;}
    return room.epoch;
}
/* A TTW_INPUT packet (512 bytes) from `member`, as the owner's tag_team_p2p.cpp passes it on: its result */
int fixture_input(unsigned member,const unsigned char *packet) {
    memcpy(Kernel_Input(),packet,512);
    return Kernel_Command(member);
}
/* One of the owner's ticks: the frame's deadline, then the frame. `late`: the seats the deadline filled; `pads`: the frame. */
int fixture_tick(unsigned *late,unsigned char pads[TT_MAX_PLAYERS][TT_PAD_BYTES]) {
    *late=Kernel_DeadlineFrame();
    if(!Kernel_TakeFrame())return 0;
    memcpy(pads,Kernel_Output()+16,TT_MAX_PLAYERS*TT_PAD_BYTES);
    return 1;
}
unsigned fixture_next(void) { return frames.next; }
