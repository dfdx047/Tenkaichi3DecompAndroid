#include "tag_team.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static TtRoom state;
static TtInputs received;
static void tag_update_catchup(void){}
#include "epoch_under_test.inc"
int main(void){
    unsigned char old[TT_PAD_BYTES]={0x11},next[TT_PAD_BYTES]={0x22},out[TT_MAX_PLAYERS][TT_PAD_BYTES];
    for(unsigned epoch=2;epoch<34;++epoch){
        Tt_InputsReset(&received,epoch-1,2);
        assert(Tt_InputPut(&received,epoch-1,0,0,old)==TT_OK);
        assert(Tt_InputPut(&received,epoch-1,1,0,old)==TT_OK);
        state.phase=TT_FIGHT;state.epoch=epoch;
        memset(out,0x77,sizeof(out));
        assert(!Port_TagTakeFrame(out));
        assert(received.next==0 && out[0][0]==0x77);
        Tt_InputsReset(&received,epoch,2);
        assert(Tt_InputPut(&received,epoch,0,0,next)==TT_OK);
        assert(!Port_TagTakeFrame(out));
        assert(Tt_InputPut(&received,epoch,1,0,next)==TT_OK);
        assert(Port_TagTakeFrame(out));
        assert(received.next==1 && out[0][0]==0x22 && out[1][0]==0x22);
    }
    puts("Round input epochs: 32 rematch transitions reject stale buffered input and wait for the new shared frame.");
    return 0;
}
