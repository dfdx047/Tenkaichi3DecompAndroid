#include "../include/tag_team_wire.h"
#include <string.h>
uint32_t Ttw_Read(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
void Ttw_Write(unsigned char *p, uint32_t v) {
    p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);
}
void Ttw_Request(unsigned char *p, int command, uint32_t epoch) {
    memset(p,0,TT_WIRE_BYTES);Ttw_Write(p,TT_PROTOCOL);Ttw_Write(p+4,command);Ttw_Write(p+8,epoch);Ttw_Write(p+20,0xffffffffu);
}
void Ttw_Snapshot(unsigned char *p, const TtRoom *r, uint32_t member, int result, const char *code) {
    int i;
    Ttw_Request(p,TTW_STATE,r->epoch);
    Ttw_Write(p+12,member);Ttw_Write(p+16,result);Ttw_Write(p+20,r->revision);
    Ttw_Write(p+24,r->host);Ttw_Write(p+28,r->phase);Ttw_Write(p+32,r->team_size);
    Ttw_Write(p+36,r->time);Ttw_Write(p+40,r->delay);Ttw_Write(p+44,r->rollback);
    Ttw_Write(p+48,(uint32_t)r->content);Ttw_Write(p+52,(uint32_t)(r->content>>32));
    Ttw_Write(p+56,(uint32_t)r->load_digest);Ttw_Write(p+60,(uint32_t)(r->load_digest>>32));
    if(code)memcpy(p+64,code,6);
    for(i=0;i<TT_MAX_PLAYERS;++i){
        const TtPlayer *v=&r->players[i];unsigned char *o=p+72+i*40;
        Ttw_Write(o,v->member);memcpy(o+4,v->name,20);
        Ttw_Write(o+24,(uint32_t)v->chara);Ttw_Write(o+28,v->costume);
        Ttw_Write(o+32,v->confirmed|v->loaded<<1|v->armed<<2|v->cpu<<3|v->team<<4|v->lobby_slot<<5|v->ended<<9);
        Ttw_Write(o+36,v->difficulty);
    }
    Ttw_Write(p+472,r->slot_count);Ttw_Write(p+476,r->stage);Ttw_Write(p+480,r->bgm);Ttw_Write(p+484,r->attacks);Ttw_Write(p+488,r->seed);Ttw_Write(p+492,r->result_choice);
    Ttw_Write(p+496,r->free_for_all);
}
int Ttw_Decode(const unsigned char *p, TtRoom *r, uint32_t *member, int *result, char *code) {
    TtRoom next;int i,phase=(int)Ttw_Read(p+28),size=(int)Ttw_Read(p+32);
    if(Ttw_Read(p)!=TT_PROTOCOL||Ttw_Read(p+4)!=TTW_STATE||phase<0||phase>TT_ABORTED) return 0;
    if(phase!=TT_CLOSED&&(size<2||size>TT_TEAM_MAX||Ttw_Read(p+472)<2||Ttw_Read(p+472)>10))return 0;
    if(phase==TT_CLOSED&&(size!=0||Ttw_Read(p+472)!=0))return 0;
    memset(&next,0,sizeof(next));
    next.epoch=Ttw_Read(p+8);next.revision=Ttw_Read(p+20);next.host=Ttw_Read(p+24);
    next.phase=phase;next.team_size=size;next.time=(int)Ttw_Read(p+36);
    next.free_for_all=(int)Ttw_Read(p+496);
    if(next.free_for_all<0||next.free_for_all>1||(next.free_for_all && Ttw_Read(p+472)!=6))return 0;
    next.slot_count=(int)Ttw_Read(p+472);next.stage=(int)Ttw_Read(p+476);next.bgm=(int)Ttw_Read(p+480);next.attacks=(int)Ttw_Read(p+484);next.seed=Ttw_Read(p+488);next.result_choice=(int32_t)Ttw_Read(p+492);
    if(next.stage<0||next.stage>=0x63||next.bgm<0||next.bgm>65535||next.attacks<0||next.attacks>1)return 0;
    next.delay=(int)Ttw_Read(p+40);next.rollback=(int)Ttw_Read(p+44);
    next.content=Ttw_Read(p+48)|(uint64_t)Ttw_Read(p+52)<<32;
    next.load_digest=Ttw_Read(p+56)|(uint64_t)Ttw_Read(p+60)<<32;
    for(i=0;i<TT_MAX_PLAYERS;++i){
        TtPlayer *v=&next.players[i];const unsigned char *o=p+72+i*40;uint32_t flags=Ttw_Read(o+32);
        v->member=Ttw_Read(o);memcpy(v->name,o+4,19);v->name[19]=0;
        v->chara=(int32_t)Ttw_Read(o+24);v->costume=(int)Ttw_Read(o+28);
        v->confirmed=flags&1;v->loaded=(flags>>1)&1;v->armed=(flags>>2)&1;
        v->cpu=(flags>>3)&1;v->difficulty=(int)Ttw_Read(o+36);
        v->team=(flags>>4)&1;v->lobby_slot=(flags>>5)&15;
        v->ended=(flags>>9)&1;
        if(flags>1023||v->lobby_slot>9||v->difficulty<0||v->difficulty>4||v->costume<0||v->costume>=10||v->chara < -1||v->chara>=512)return 0;
        if((flags&7)&&(!v->member||!v->confirmed||v->chara<0))return 0;
        if(v->member&&((v->member&TT_CPU_MEMBER)!=0)!=v->cpu)return 0;
        if(v->member&&i>=next.slot_count)return 0;
        if(v->member&&Tt_Seat(&next,v->member)!=i)return 0;
        if(i<next.slot_count){int j;for(j=0;j<i;++j)if(next.players[j].lobby_slot==v->lobby_slot)return 0;}
    }
    if(phase!=TT_CLOSED&&phase!=TT_ABORTED&&Tt_Seat(&next,next.host)<0)return 0;
    *r=next;*member=Ttw_Read(p+12);*result=(int)Ttw_Read(p+16);
    memcpy(code,p+64,6);code[6]=0;
    return 1;
}
