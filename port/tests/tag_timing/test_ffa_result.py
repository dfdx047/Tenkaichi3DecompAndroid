"""Exercise the production battle-end function with deterministic actor states.

This isolates the native result gate from rendering, animation and disc data.
The function body and result constants are read from the production sources.
"""
from pathlib import Path
import os, re, shutil, subprocess, tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/battle/btl_seq.c').read_text(encoding='utf-8')
start = source.index('s32 BtlSeq_CheckBattleEnd(void)')
end = source.index('\n}\n', start) + 3
header = re.sub(r'/\*.*?\*/', '', (root / 'include/battle/battle.h').read_text(), flags=re.S)
defines = '\n'.join(line for line in header.splitlines()
                    if re.match(r'#define (BTL_RESULT_|BTL_REASON_|BATTLE_FLAG_)', line))
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int s32;
typedef struct {int winner,reason,health[2];} BattleResult;
typedef struct {int flags;} BattleWork;
static BattleResult result;
static BattleWork work;
static int hp[10],teams[10],count,ffa,tag,timeup,winner,blocked,mode;
static int dead[2],flag7[2];
static BattleResult *Battle_GetResult(void){return &result;}
static BattleWork *Battle_GetWork(void){return &work;}
static int TagBattle_FreeForAll(void){return ffa;}
static int TagBattle_Active(void){return tag;}
static int TagBattle_Count(void){return count;}
static int TagBattle_Team(int a){return teams[a];}
static void TagBattle_SetWinningActor(int a){winner=a;}
static int BtlCharApi_GetHp(int a){return hp[a];}
static int BtlSeq_GetEndCheckOff(void){return blocked;}
static int BtlCharApi_AnyHasFlag128(void){return 0;}
static int BtlSeq_TickClocksExt(void){return timeup;}
static int Battle_GetMode(void){return mode;}
static int BtlCtrl_TestFlag7(int a){return flag7[a];}
static int BtlCtrl_IsTeamDead(int a){return dead[a];}
static int BattleReplay_IsActive(void){return 0;}
static int BattleReplay_TestDataFlag(void){return 0;}
static void BtlSeq_JudgeByHealth(BattleResult *r){r->winner=BTL_RESULT_DRAW;}
static void reset(void){
    memset(&result,0,sizeof(result));memset(&work,0,sizeof(work));
    memset(hp,0,sizeof(hp));memset(dead,0,sizeof(dead));memset(flag7,0,sizeof(flag7));
    ffa=tag=1;count=6;timeup=blocked=mode=0;winner=-1;
    for(int i=0;i<10;i++)teams[i]=i>=3;
}
'''
checks = r'''
int main(void){
    for(int survivor=0;survivor<6;survivor++)for(int preflag=0;preflag<3;preflag++){
        reset();hp[survivor]=123;
        result.winner=preflag==1?BTL_RESULT_WIN_P1:preflag==2?BTL_RESULT_WIN_P2:0;
        assert(BtlSeq_CheckBattleEnd()==1);assert(winner==survivor);
        assert(result.winner==(teams[survivor]?BTL_RESULT_WIN_P2:BTL_RESULT_WIN_P1));
        assert(result.reason==BTL_REASON_KO);
    }
    reset();hp[0]=hp[2]=hp[4]=100;result.winner=BTL_RESULT_WIN_P1;
    assert(!BtlSeq_CheckBattleEnd());assert(!result.winner&&winner==-1);
    reset();hp[0]=hp[2]=100;hp[5]=500;timeup=1;
    assert(BtlSeq_CheckBattleEnd());assert(winner==5&&result.reason==BTL_REASON_TIME_UP);
    reset();hp[0]=hp[5]=500;timeup=1;
    assert(BtlSeq_CheckBattleEnd());assert(winner==-1&&result.winner==BTL_RESULT_DRAW);
    reset();assert(BtlSeq_CheckBattleEnd());assert(winner==-1&&result.winner==BTL_RESULT_DRAW);
    reset();hp[2]=100;work.flags=BATTLE_FLAG_LOADING;
    assert(!BtlSeq_CheckBattleEnd());assert(winner==-1);
    work.flags=BATTLE_FLAG_PAUSE;assert(!BtlSeq_CheckBattleEnd());
    reset();ffa=0;hp[0]=100;hp[3]=100;result.winner=BTL_RESULT_WIN_P1;
    assert(!BtlSeq_CheckBattleEnd());assert(!result.winner);
    hp[3]=0;assert(BtlSeq_CheckBattleEnd());assert(result.winner==BTL_RESULT_WIN_P1);
    reset();ffa=tag=0;dead[1]=1;
    assert(BtlSeq_CheckBattleEnd());assert(result.winner==BTL_RESULT_WIN_P1);
    puts("PASS FFA survivors, native KO flags, timeout, ties, loading/pause gates, teams and duel results");
}
'''
compiler = os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
if not compiler:
    raise SystemExit('A C compiler is required')
with tempfile.TemporaryDirectory(prefix='bt4-result-') as directory:
    folder=Path(directory);path=folder/'result.c';exe=folder/('result.exe' if os.name=='nt' else 'result')
    path.write_text(defines+'\n'+fixture+'\n'+source[start:end]+'\n'+checks)
    subprocess.run([compiler,'-std=c99','-Wall','-Werror',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
