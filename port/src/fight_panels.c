/*
 * The fight panels (gs/ui.cpp): the side bars of a 4:3 fight in a wide window show each side's fighter, its team and
 * its gauges. This file reads them once per blank on the game's thread (Port_VBlank, plat_stub.c) into a copy the
 * render thread takes (PortPanels_Read).
 *
 * It only reads: nothing here writes the game's memory or draws a random number, so the game runs the same with it
 * on or off and an online session stays in step. Blanks being run again after a rollback are skipped. Its own
 * variables are not game state.
 *
 * The fighter is read at its offsets (include/battle/btl_char_member.h: BtlMemberChr, BtlMember, BtlMemberGauge);
 * the port's files do not include the game's headers.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gs/fight_panels.h"
#include "port_addr.h" /* (Port_Ptr: a game address as a pointer) */

#if (defined(__x86_64__) || defined(__aarch64__)) && defined(__clang__)
#undef __ptr32 /* the Windows headers of mingw define these two away */
#undef __uptr
#define GAME_PTR *__ptr32 __uptr
#else
#define GAME_PTR *
#endif

extern int gPortResim; /* gs/state.c: blanks being run again */

/* src/battle/btl_char_mgr.c / btl_seq.c: there from Battle_Init to Battle_Term, NULL outside (see a11y_game.c) */
extern void GAME_PTR gBtlChars;
extern void GAME_PTR gBtlSeq;
typedef struct {
    char unk0[0x30];
    uint32_t flags; /* 0x30: HUD_SHOW_* */
} FpHud;
extern FpHud GAME_PTR gHud; /* include/battle/hud_a.h; read only while this battle's roster and sequence are there */
#define HUD_SHOW_GAUGES 0x01

extern void GAME_PTR BtlChar_FindBySide(int side);
extern int BtlCharApi_GetChara(int objId);
extern int BtlChar_TestFlag(void GAME_PTR chr, unsigned n);
#define CHR_POWERED_UP 6

extern int Port_FilePath(int ptid, int flid, const char *fname, char *out, int size); /* plat_file.c */
#define FACE_FILE 0x2F9 /* the large portraits (the character select's): file FACE_FILE + character of partition 1 */
#define CHARA_MAX 0xA1 /* the grid's marker cells and up are not characters */

/* include/battle/btl_char_member.h: BtlMemberChr */
#define CHR_ACTIVE 0x994
#define CHR_MEMBER_COUNT 0x998
#define CHR_SWITCH_TARGET 0x9A0
#define CHR_MEMBERS 0x9A4
#define MEMBER_SIZE 0xA4
#define MEMBER_CHARA 0x00
#define MEMBER_COSTUME 0x04
#define MEMBER_PRESENT 0x08
#define MEMBER_GAUGE 0x40 /* hp, hpMax, unk8, ki, kiMax, blast, blastMax, maxPower */
#define CHR_COMBO 0xD40   /* damage, hits, timer: the combo landing on this fighter */

/* The moves (HUD mods): include/battle/btl_tech_b.h. Read only: the cost readers and the action / input queries
   only read the fighter; the names are read here from the fighter's own name list (battle object + 0xBC, what
   HudPrompt_DrawNames gives BtlMenu_SetScript2; entry n of it is technique slot n, as BtlCtrl_GetTechPromptPad1's
   index), parsed as BtlText_FindEntry does but without its shared state (the menu's script pointer). */
extern int BtlAct_GetCurrent(void GAME_PTR chr);
extern int BtlInput_IsHeld(void GAME_PTR chr, unsigned mask);
extern int BattleSide_IsCpu(int side); /* (a side's control word: 2 the computer; read only) */
extern int BtlSuper_GetKiCost(void GAME_PTR chr, int slot);
extern int BtlSkill_GetBlastCost(void GAME_PTR chr, int slot);
extern void GAME_PTR BtlChar_GetObj(void GAME_PTR chr);
#define BTLB_CHARGE 0x200
#define OBJ_NAMES 0xBC

/* The name of entry n of a name list (u16 text: lines "xx[&NNN]*...name" after a first character): UTF-8 */
static void entry_name(const uint16_t *p, int n, char *out, int size) {
    int k = 0;
    out[0] = '\0';
    if (p == NULL) {
        return;
    }
    p += 1;
    while (*p != 0) {
        const uint16_t *line;
        p += 2;
        if (p[-1] == 0 || p[-2] == 0) {
            return;
        }
        if (p[0] == '&') {
            p += 4; /* (an unlock condition: the entry counts whatever it says) */
        }
        if (p[0] == '@') {
            return;
        }
        if (p[0] == '*') {
            if (n <= 0) {
                int i;
                for (i = 0; i < 4 && p[i] != 0; i++) {
                }
                for (line = p + i; *line != 0 && *line != 10 && *line != 13 && k < size - 4; line++) {
                    unsigned c = *line;
                    if (c < 0x80) {
                        out[k++] = (char)c;
                    } else if (c < 0x800) {
                        out[k++] = (char)(0xC0 | (c >> 6));
                        out[k++] = (char)(0x80 | (c & 0x3F));
                    } else {
                        out[k++] = (char)(0xE0 | (c >> 12));
                        out[k++] = (char)(0x80 | ((c >> 6) & 0x3F));
                        out[k++] = (char)(0x80 | (c & 0x3F));
                    }
                }
                while (k > 0 && out[k - 1] == ' ') {
                    k--;
                }
                out[k] = '\0';
                return;
            }
            n--;
        }
        while (*p != 0 && *p != 10 && *p != 13) {
            p++;
        }
        if (*p != 0) {
            p++;
        }
    }
}


static FpState sState;           /* the copy the render thread reads */
static volatile unsigned sSeq;   /* odd while it is written */
static int sWritten;

/* Which portrait each slot's path is for, so the files are looked up only when a member changes. */
static int sPathKey[2][FP_MEMBERS];
static char sPath[2][FP_MEMBERS][FP_PATH];

static int rd(uint8_t GAME_PTR p, int off) {
    return *(int GAME_PTR)(p + off);
}

static void portrait_path(int side, int slot, int chara, int costume) {
    int key = (chara << 8 | (costume & 0xFF)) + 1, id;

    if (sPathKey[side][slot] == key) {
        return;
    }
    sPathKey[side][slot] = key;
    sPath[side][slot][0] = '\0';
    if (chara < 0 || chara >= CHARA_MAX) {
        return;
    }
    id = FACE_FILE + chara; /* (a file under mods/ wins, as for the game's own reads) */
    if (!Port_FilePath(1, id - 1, NULL, sPath[side][slot], FP_PATH)) {
        sPath[side][slot][0] = '\0';
    }
}

static void read_side(FpState *st, int side) {
    FpSide *s = &st->side[side];
    uint8_t GAME_PTR chr = (uint8_t GAME_PTR)BtlChar_FindBySide(side);
    uint8_t GAME_PTR foe = (uint8_t GAME_PTR)BtlChar_FindBySide(side ^ 1);
    uint8_t GAME_PTR m;
    int i, objChara;

    memset(s, 0, sizeof(*s));
    s->next = -1;
    if (chr == NULL) {
        return;
    }
    s->active = rd(chr, CHR_ACTIVE);
    s->count = rd(chr, CHR_MEMBER_COUNT);
    if (s->count < 1 || s->count > FP_MEMBERS || s->active < 0 || s->active >= s->count) {
        return;
    }
    s->found = 1;
    objChara = BtlCharApi_GetChara(side);
    for (i = 0; i < s->count; i++) {
        FpMember *o = &s->m[i];
        m = chr + CHR_MEMBERS + i * MEMBER_SIZE;
        o->present = rd(m, MEMBER_PRESENT) != 0;
        o->chara = i == s->active && objChara >= 0 && objChara < CHARA_MAX ? objChara : rd(m, MEMBER_CHARA);
        o->costume = rd(m, MEMBER_COSTUME);
        o->hp = rd(m, MEMBER_GAUGE + 0x00);
        o->hpMax = rd(m, MEMBER_GAUGE + 0x04);
        if (o->present) {
            portrait_path(side, i, o->chara, o->costume);
            memcpy(o->path, sPath[side][i], FP_PATH);
        }
    }
    m = chr + CHR_MEMBERS + s->active * MEMBER_SIZE;
    s->ki = rd(m, MEMBER_GAUGE + 0x0C);
    s->kiMax = rd(m, MEMBER_GAUGE + 0x10);
    s->blast = rd(m, MEMBER_GAUGE + 0x14);
    s->blastMax = rd(m, MEMBER_GAUGE + 0x18);
    s->maxPower = rd(m, MEMBER_GAUGE + 0x1C);
    s->poweredUp = BtlChar_TestFlag(chr, CHR_POWERED_UP) != 0;
    i = rd(chr, CHR_SWITCH_TARGET); /* (raw: BtlMember_GetSwitchTarget would move it) */
    if (i >= 0 && i < s->count && i != s->active && s->m[i].present && s->m[i].hp > 0) {
        s->next = i;
    }
    s->action = BtlAct_GetCurrent(chr);
    s->chargeHeld = BtlInput_IsHeld(chr, BTLB_CHARGE) != 0;
    s->cpu = BattleSide_IsCpu(side) != 0;
    {
        uint8_t GAME_PTR obj = (uint8_t GAME_PTR)BtlChar_GetObj(chr);
        uint32_t at = obj != NULL ? (uint32_t)rd(obj, OBJ_NAMES) : 0;
        const uint16_t *names = at != 0 ? (const uint16_t *)Port_Ptr(at) : NULL;
        for (i = 0; i < FP_MOVES; i++) {
            entry_name(names, i, s->moves[i].name, (int)sizeof(s->moves[i].name));
            s->moves[i].cost = s->moves[i].name[0] == '\0' ? 0 : i < 2 ? BtlSkill_GetBlastCost(chr, i) : BtlSuper_GetKiCost(chr, i);
        }
    }
    if (foe != NULL) {
        s->comboDamage = rd(foe, CHR_COMBO + 0x00);
        s->comboHits = rd(foe, CHR_COMBO + 0x04);
        s->comboTimer = rd(foe, CHR_COMBO + 0x08);
    }
}

/* What the game's HUD is given (src/battle/hud_b.c, hud_c_b.c, hud_e_c.c), for HUDs the port draws (the script mods):
   1 timer (value, is a clock), 2 status icons (mask, raised), 3 combo hits, 4 combo damage, 5 event message. Blanks
   run again after a rollback give nothing (they were given already, or will be). */
static struct {
    int timer, timerClock, timerSet;
    int statMask[2], statUp[2], hits[2], damage[2], msg[2];
    unsigned hitSeq[2], msgSeq[2];
} sFeed = {0, 1, 0, {0, 0}, {0, 0}, {0, 0}, {-1, -1}, {0, 0}, {0, 0}, {0, 0}};

void Port_HudFeed(int kind, int side, int a, int b) {
    if (gPortResim || side < 0 || side > 1) {
        return;
    }
    switch (kind) {
    case 1: sFeed.timer = a; sFeed.timerClock = b; sFeed.timerSet = 1; break;
    case 2: sFeed.statMask[side] = a & 15; sFeed.statUp[side] = b & 15; break;
    case 3:
        if (a != sFeed.hits[side]) {
            sFeed.hitSeq[side]++;
        }
        sFeed.hits[side] = a;
        break;
    case 4: sFeed.damage[side] = a; break;
    case 5: sFeed.msg[side] = a; sFeed.msgSeq[side]++; break;
    }
}

void PortPanels_GameTick(void) {
    static FpState st;
    FpHud GAME_PTR h;

    if (gPortResim) {
        return;
    }
    memset(&st, 0, sizeof(st));
    if (gBtlChars != NULL && gBtlSeq != NULL) {
        read_side(&st, 0);
        read_side(&st, 1);
        st.battle = st.side[0].found && st.side[1].found;
        h = gHud;
        st.gauges = st.battle && h != NULL && (h->flags & HUD_SHOW_GAUGES) != 0;
    }
    if (!st.battle) {
        memset(sPathKey, 0, sizeof(sPathKey)); /* (a mod switched between fights is looked up again) */
        sFeed.timerSet = 0;
        sFeed.statMask[0] = sFeed.statMask[1] = 0;
        sFeed.hits[0] = sFeed.hits[1] = 0;
        sFeed.damage[0] = sFeed.damage[1] = -1;
    } else {
        int s;
        st.timer = sFeed.timer;
        st.timerClock = sFeed.timerClock;
        st.timerSet = sFeed.timerSet;
        for (s = 0; s < 2; s++) {
            st.side[s].statMask = sFeed.statMask[s];
            st.side[s].statUp = sFeed.statUp[s];
            st.side[s].hudHits = sFeed.hits[s];
            st.side[s].hudDamage = sFeed.damage[s];
            st.side[s].hitSeq = sFeed.hitSeq[s];
            st.side[s].msg = sFeed.msg[s];
            st.side[s].msgSeq = sFeed.msgSeq[s];
        }
    }
    __atomic_add_fetch(&sSeq, 1, __ATOMIC_ACQ_REL);
    memcpy(&sState, &st, sizeof(st));
    __atomic_add_fetch(&sSeq, 1, __ATOMIC_ACQ_REL);
    sWritten = 1;
}

int PortPanels_Read(FpState *out) {
    unsigned a, b;
    int tries;

    if (!sWritten) {
        return 0;
    }
    for (tries = 0; tries < 64; tries++) {
        a = __atomic_load_n(&sSeq, __ATOMIC_ACQUIRE);
        if (a & 1) {
            continue;
        }
        memcpy(out, &sState, sizeof(*out));
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        b = __atomic_load_n(&sSeq, __ATOMIC_ACQUIRE);
        if (a == b) {
            return 1;
        }
    }
    return 0;
}
