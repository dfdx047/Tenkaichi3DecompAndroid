/* Execute a common binary event stream against the production native kernel. */
#include "tag_team_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
unsigned char *Kernel_Input(void),*Kernel_Output(void);
int Kernel_ImportArmed(const unsigned char *),Kernel_Command(unsigned),Kernel_Phase(void),Kernel_TakeFrame(void);
unsigned Kernel_Epoch(void),Kernel_PendingFrame(void),Kernel_DeadlineFrame(void);
void Kernel_Leave(unsigned),Kernel_Snapshot(unsigned,int,const char*),Kernel_Abort(void);

static void check(int rc,const char *step){if(rc){fprintf(stderr,"fixture %s rejected: %d\n",step,rc);exit(2);}}
static int fixture(const char *name,int humans,int slots,int ffa){
    TtRoom room;unsigned char packet[512];int i;FILE *out;
    if(humans<1||humans>slots||slots<2||slots>10)return 2;
    Tt_Open(&room,101,"Human 101",slots>4?(slots+1)/2:2,0xc03619a07f5e8d9aULL);
    room.delay=0;room.seed=0x12345678;
    check(Tt_SetTeams(&room,101,room.epoch,slots/2,slots-slots/2),"teams");
    if(ffa)check(Tt_SetMode(&room,101,room.epoch,1),"Free for All");
    for(i=1;i<humans;++i){char text[20];snprintf(text,sizeof(text),"Human %d",101+i);check(Tt_Join(&room,101+i,text,room.content),"join");}
    check(Tt_FillCPU(&room,101,room.epoch,2),"CPU");
    check(Tt_BeginSelect(&room,101,room.epoch),"select");
    for(i=0;i<slots;++i)check(Tt_ChooseSeat(&room,room.players[i].cpu?101:room.players[i].member,room.epoch,i,100+i,i%3),"choose");
    check(Tt_SetArena(&room,101,room.epoch,23,54,1),"arena");
    check(Tt_BeginLoad(&room,101,room.epoch,Tt_LoadDigest(&room)),"load");
    for(i=0;i<humans;++i)check(Tt_Loaded(&room,101+i,room.epoch,room.load_digest),"loaded");
    check(Tt_Arm(&room,101,room.epoch),"arm");
    for(i=0;i<humans;++i)check(Tt_ArmAck(&room,101+i,room.epoch),"armed");
    Ttw_Snapshot(packet,&room,101,0,"ABC234");
    out=fopen(name,"wb");if(!out)return 3;
    if(fwrite(packet,1,sizeof(packet),out)!=sizeof(packet)){fclose(out);return 3;}fclose(out);return 0;
}
int main(int argc,char **argv){
    FILE *events,*trace,*wire;unsigned char record[520],result[20];size_t got;
    if((argc==5||argc==6)&&!strcmp(argv[1],"--fixture"))return fixture(argv[2],atoi(argv[3]),atoi(argv[4]),argc==6?atoi(argv[5]):0);
    if(argc!=4)return 1;
    events=fopen(argv[1],"rb");trace=fopen(argv[2],"wb");wire=fopen(argv[3],"wb");
    if(!events||!trace||!wire)return 3;
    while((got=fread(record,1,sizeof(record),events))){
        unsigned op=Ttw_Read(record),member=Ttw_Read(record+4),value=0;
        if(got!=sizeof(record))return 4;
        memcpy(Kernel_Input(),record+8,512);
        switch(op){
        case 1:value=(unsigned)Kernel_Command(member);break;
        case 2:value=Kernel_DeadlineFrame()<<1;value|=(unsigned)Kernel_TakeFrame();break;
        case 3:Kernel_Leave(member);break;
        case 4:Kernel_Snapshot(member,0,"ABC234");break;
        case 5:value=(unsigned)Kernel_ImportArmed(Kernel_Input());break;
        case 6:Kernel_Abort();break;
        case 7:value=(unsigned)Kernel_ImportArmed(NULL);break;
        default:return 5;
        }
        Ttw_Write(result,op);Ttw_Write(result+4,value);Ttw_Write(result+8,(unsigned)Kernel_Phase());
        Ttw_Write(result+12,Kernel_Epoch());Ttw_Write(result+16,Kernel_PendingFrame());
        if(fwrite(result,1,sizeof(result),trace)!=sizeof(result))return 6;
        if((op==2||op==4)&&fwrite(Kernel_Output(),1,512,wire)!=512)return 6;
    }
    fclose(events);fclose(trace);fclose(wire);return 0;
}
