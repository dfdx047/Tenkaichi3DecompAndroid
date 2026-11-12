#ifndef BT4_DEV_RULES_H
#define BT4_DEV_RULES_H
#include <stdint.h>
/* Synchronized test rules. The public client receives these rules but exposes
   no developer controls. All fighters use the same resource rules. */
enum { BT4_DEV_HP=1, BT4_DEV_KI=2, BT4_DEV_STOCK=4, BT4_DEV_PAUSE=8, BT4_DEV_STEP=16 };
typedef struct {uint32_t sequence[10];unsigned flags,step;} Bt4DevRules;
static unsigned Bt4Dev_Check(const unsigned char *p){unsigned v=0x5a;int i;for(i=0;i<7;i++)v^=p[i];return v;}
static void Bt4Dev_Pack(unsigned char p[8],unsigned flags,uint32_t seq){
 int i;for(i=0;i<8;i++)p[i]=0;if(!seq)return;p[0]=0xd3;p[1]=1;p[2]=flags&31;
 for(i=0;i<4;i++)p[3+i]=(unsigned char)(seq>>(8*i));p[7]=(unsigned char)Bt4Dev_Check(p);
}
static int Bt4Dev_Unpack(const unsigned char p[8],unsigned *flags,uint32_t *seq){
 int i;uint32_t n=0;if(p[0]!=0xd3||p[1]!=1||(p[2]&~31)||p[7]!=Bt4Dev_Check(p))return 0;
 for(i=0;i<4;i++)n|=(uint32_t)p[3+i]<<(8*i);if(!n)return 0;*seq=n;*flags=p[2];return 1;
}
static int Bt4Dev_Consume(Bt4DevRules *s,int actor,const unsigned char p[8]){
 unsigned flags;uint32_t n;if(actor<0||actor>=10||!Bt4Dev_Unpack(p,&flags,&n))return 0;
 if(s->sequence[actor]&&(int32_t)(n-s->sequence[actor])<=0)return 0;
 s->sequence[actor]=n;s->flags=flags&15;s->step=(flags&BT4_DEV_STEP)!=0;return 1;
}
#endif
