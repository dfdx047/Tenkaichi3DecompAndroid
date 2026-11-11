#include "common.h"
#include "sys/late_a.h"
#include "sys/heap.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "sys/snd.h"
#include "sys/adx.h"

/*
 * Shen, 0x2BD230..0x2BEB20: the dragon-summoning screen (gProgress->mode 70) and the mode loop around it.
 * Built with -G0: no $gp addressing anywhere. See sys/late_a.h.
 *
 * The object's strings start at 0x2FBDA8 ("mc_menu_plate_%d") and end with "fl_out" (0x2FC0E8); the twelve
 * "host:data/test/shenron/..." names in between are arguments of a debug loader that compiles to nothing.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 sprintf(char *dst, const char *fmt, ...);
extern s32 sceGsSyncPath(s32 mode, u16 timeout);

extern void *File_LoadSync(s32 id, void *buf, s32 unused);
extern void File_Stub264D90(void);
extern void Pad_SetRepeat(s32 delay, s32 interval);
extern void Pad_Update(void);
extern u32 Rand_Range(u32 n);
extern void *Sprite_Unpack(void *src, void *dst, s32 *rawSize);
extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Voice_PlayWithSubtitle(void *subtitles, s32 base, s32 line);
extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 vsyncs);
extern void Dma_Flush(void);
extern void Dma_ResetBuffers(void);
extern void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_Update(void);
extern void ColorFade_Draw(void);
extern s32 ColorFade_IsFadingOut(void);

extern void Flash_Create(Flash *flash, void *data, void *tex);
extern void Flash_Destroy(Flash *flash);
extern void Flash_Advance(Flash *flash);
extern void Flash_Draw(Flash *flash);
extern void Flash_Play(Flash *flash, s32 arg);
extern void Flash_GotoLabel(Flash *flash, const char *label, s32 arg);
extern void Flash_FindLabel(Flash *flash, const char *parent, const char *name, FlashRef *out);
extern void Flash_ClipGotoLabel(Flash *flash, FlashRef *ref, const char *label);
extern void Flash_ClipSetCallbackA(Flash *flash, FlashRef *ref, void (*cb)(void), s32 arg);
extern void Flash_ClipSetCallbackB(Flash *flash, FlashRef *ref, void (*cb)(void), s32 arg);
extern void Flash_ClipSetColor(Flash *flash, FlashRef *ref, f32 v);
extern void Flash_ClipSetUv(Flash *flash, FlashRef *ref, FlashUv *uv);
/* text box module, after 0x2600B0 (not decompiled): draws a line in a clip */
extern void TextBox_AttachLine(Flash *flash, FlashRef *ref, s32 a, s32 b, s32 line, TextBox *box);
/* the screen's backdrop (0x262BC8.., not decompiled): the dragon scene */
extern s32 ShenScene_GetState(void);       /* its state: 1 = ready for input, 2 = ended */
extern void ShenScene_SetState(s32 state); /* requests a state; 3 = end */
extern void ShenScene_Update(void);      /* draws it */
extern void ShenScene_Init(s32 dragon);
extern void ShenScene_Term(void);
extern void MsgWin_Init4(u32 *pack, void *text, s32 side, void *unused) __asm__("MsgWin_Init");
extern void Dialog_Init(u32 *file, u32 *msgTbl, s32 size);
extern void Dialog_Term(void);
extern void Dialog_Draw(s32 visible);

extern void ShenCfm_Init(u32 *pack);
extern void ShenCfm_Term(void);
extern void ShenCfm_Draw(void);
extern void ShenCfm_Open(s32 kind, s32 id);
extern s32 ShenCfm_Update(s32 *answer);
extern void ShenCfm_Close(void);
extern s32 ShenSave_Run(s32 section);

/* The menu overlay's archive pointer (DBZP.BIN data): file `gProgress->unk4 + 0x18`. */
extern u32 *gMenuArc12;
extern ViewProgress *gProgress;

/* A debug build loaded each resource from the host by name; here only the names are left. */
static inline void Shen_DebugFile(const char *name) {
}

s32 Shen_Run(s32 section);
void Shen_Say(ShenWork *work, s32 line);

/* Handler of gProgress->mode 70: runs the wish screen, then the save screen, and sets mode 4. Returns 0. */
s32 Shen_Main(void) {
    s32 done = 0;
    s32 ret = 1;

    if (gMenuArc12 == NULL) {
        gMenuArc12 = File_LoadSync(gProgress->unk4 + 0x18, NULL, 0);
    }
    do {
        switch (gProgress->mode) {
        case 70:
            Bgm_Play(0x83A);
            Pad_SetRepeat(20, 1);
            Shen_Run(2);
            Adx_StopAll();
            Pad_SetRepeat(40, 3);
            gProgress->mode = 71;
            break;
        case 71:
            ShenSave_Run(2);
            ret = 0;
            Adx_StopAll();
            done = 1;
            gProgress->mode = 4;
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc12 != NULL) {
        Heap_Free(gMenuArc12);
        gMenuArc12 = NULL;
    }
    return ret;
}

/* Frees every node of a wish list except its head. */
void ShenList_Free(ShenNode *head) {
    ShenNode *node = head->next;
    ShenNode *next;

    while (node->no != 0) {
        next = node->next;
        Heap_Free(node);
        node = next;
    }
}

/* Makes `head` a circular list of `count` nodes numbered 0..count-1 (count >= 2). */
void ShenList_Build(ShenNode *head, u32 count) {
    u32 i;
    ShenNode *cur;
    s32 n;
    ShenNode *prev;
    ShenNode *first;
    ShenNode *node;

    i = 1;
    cur = head;
    cur->no = 0;
    n = 0;
    prev = NULL;
    first = head;
    node = NULL;
    for (; i < count; i++) {
        node = Heap_Alloc(sizeof(ShenNode), 0x20, 0, HEAP_ANY);
        memset(node, 0, sizeof(ShenNode));
        n++;
        node->no = i;
        cur->prev = prev;
        cur->next = node;
        prev = cur;
        if (n != count - 1) {
            cur = node;
        }
    }
    first->prev = node;
    node->next = first;
    node->prev = cur;
}

/* Returns the node with the given number. */
ShenNode *ShenList_Find(ShenNode *head, s32 no) {
    ShenNode *node = head;

    if (no < 0 && node->prev->no < no) {
        return NULL;
    }
    if (head->no != no) {
        do {
            node = node->next;
        } while (node->no != no);
    }
    return node;
}

/* Walks `steps` nodes forward (or back when negative) and returns that node's wish. */
ShenWish *ShenList_StepWish(ShenNode *node, s32 steps) {
    s32 dir = steps;
    s32 i = 0;

    if (steps == 0) {
        return &node->wish;
    }
    if (steps < 0) {
        steps = -steps;
    }
    for (; i != steps; i++) {
        if (dir < 0) {
            node = node->prev;
        } else if (dir > 0) {
            node = node->next;
        }
    }
    return &node->wish;
}

/* Walks `steps` nodes forward (or back when negative) and returns that node. */
ShenNode *ShenList_Step(ShenNode *node, s32 steps) {
    s32 dir = steps;
    s32 i = 0;

    if (steps == 0) {
        return node;
    }
    if (steps < 0) {
        steps = -steps;
    }
    for (; i != steps; i++) {
        if (dir < 0) {
            node = node->prev;
        } else if (dir > 0) {
            node = node->next;
        }
    }
    return node;
}

/* Starts one of the dragon's voice lines (voice file 0x85B7 + line) and shows its text. */
void Shen_Say(ShenWork *work, s32 line) {
    Voice_PlayWithSubtitle(NULL, 0x85B7, line);
    work->line = line;
}

/* Keeps the cursor row inside 0..3. Returns 1 when it was outside: the list scrolls instead. */
s32 Shen_ClampCursor(s32 *cursor) {
    s32 max = 3;

    if (*cursor < 0) {
        *cursor = 0;
        return 1;
    }
    if (*cursor > max) {
        *cursor = max;
        return 1;
    }
    return 0;
}

/*
 * Name format of the five row plates. A named object because Shen_DrawList, which is still assembly, refers to
 * it by address; the compiler emitted it as an ordinary string literal of Shen_PlayPlateOk (0x2FBDA8).
 */
const char gShenPlateFmt[] __attribute__((aligned(8))) = "mc_menu_plate_%d";

/* Plays "fl_ok" on the plate under the cursor. */
void Shen_PlayPlateOk(ShenWork *work) {
    FlashRef ref;
    char name[0x100];

    sprintf(name, gShenPlateFmt, work->cursor + 1);
    Flash_FindLabel(work->flash, NULL, name, &ref);
    Flash_ClipGotoLabel(work->flash, &ref, "fl_ok");
}

/* Lights or dims the plate of a row. */
void Shen_SetPlate(Flash *flash, s32 row, s32 on) {
    FlashRef ref;
    char name[0x100];

    sprintf(name, gShenPlateFmt, row + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Moves the cursor up (0) or down (1); at the ends the list scrolls by one wish. */
void Shen_MoveCursor(ShenWork *work, s32 dir) {
    Shen_SetPlate(work->flash, work->cursor, 0);
    if (dir == 0) {
        work->cursor--;
    } else if (dir == 1) {
        work->cursor++;
    }
    if (Shen_ClampCursor(&work->cursor)) {
        if (dir == 0) {
            work->top = work->top->prev;
            Flash_GotoLabel(work->flash, "fl_list_down", 1);
            work->out = ShenList_Step(work->top, 4);
        } else if (dir == 1) {
            work->top = work->top->next;
            Flash_GotoLabel(work->flash, "fl_list_up", 1);
            work->out = ShenList_Step(work->top, -1);
        }
    }
    Shen_SetPlate(work->flash, work->cursor, 1);
    Snd_PlaySe(1, 0);
}

/* Sets the texture rectangle of a named clip. */
static inline void Shen_SetClipUv(Flash *flash, const char *parent, const char *name, FlashUv *uv) {
    FlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Gives the two scroll arrows their cells of the icon sheet. */
void Shen_SetArrowUvs(Flash *flash) {
    FlashUv uv;

    uv.x1 = 0x20;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    uv.x0 = 0;
    Shen_SetClipUv(flash, "mc_yajirusi_up", "mc_yajirusi_icon_up", &uv);
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    Shen_SetClipUv(flash, "mc_yajirusi_down", "mc_yajirusi_icon_down", &uv);
}

/* Clip callback: clips drawing to the list area (x 0..0x200, y 0x6A..0x11E). */
void Shen_ScissorList(void) {
    Sprite_SetScissor(0, 0x200, 0x6A, 0x11E);
}

/* Clip callback: back to the full screen. */
void Shen_ScissorFull(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* 1 when the save already has what a wish gives (`n` is 0-based). */
static inline s32 Shen_IsOwned(s32 kind, s32 n) {
    switch (kind) {
    case SHEN_WISH_ITEM:
        return (u8)(gSaveData->item[n] & 1);
    case SHEN_WISH_STAGE:
        return (s32)((gSaveData->stageBits >> n) & 1) != 0;
    case SHEN_WISH_CHARA:
        return (s32)((SAVE_CHARA_WORD(gSaveData, n) >> (n % 64)) & 1) != 0;
    case SHEN_WISH_MONEY:
        return 0;
    }
    return 0;
}

/*
 * Clip names used only by Shen_DrawList (0x2FBE60, 0x2FBE70). Named objects for the same reason as
 * gShenPlateFmt; once the function is C again they are plain string literals.
 */
const char gShenIconClip[] __attribute__((aligned(8))) = "mc_icon_play";
const char gShenTextClip[] __attribute__((aligned(8))) = "mc_menu_text_off";

/*
 * Per frame: the icon, the clipping, the brightness and the text of the five rows of the list.
 *
 * For each row (the fifth shows the wish that is scrolling out): the icon cell by kind of wish (item: by the
 * item's type byte, 0 -> cell 3, 2 -> cell 1, else cell 2; stage, character and money have fixed cells of the
 * 64x64 sheet), the two scissor callbacks on the icon and on the plate, half brightness for a wish the save
 * already has, and the wish's name: line `node->no` of the row's text box, +4 for dragon 1, +11 for dragon 2.
 *
 * NON-MATCHING. The attempt does the same thing, with the same frame and the same instruction count (229);
 * what differs is which of the two registers that hold `&ref` each call uses. The compiler keeps two copies
 * of that address: one in a saved register for the label / callback calls, one rebuilt in front of the two
 * brightness calls and the text call. In the original the Flash_FindLabel of "mc_menu_text_off" uses the saved
 * copy (so that copy is s3 and the constant 0x40 and the row counter move to s4 / s5), and the compare with 2
 * of the line number uses the register that holds 2 (`xor v1,v1,s6`); here that call rebuilds the address and
 * the compare is `xori v1,v1,2`. No arrangement of the tail (inline helpers, a pointer variable, where the
 * line number is worked out) reproduced it: build/scratch_late_a/dl*.py.
 */
#if 0
static inline void Shen_FindClip(Flash *flash, const char *parent, const char *name, FlashRef *ref) {
    Flash_FindLabel(flash, parent, name, ref);
}

void Shen_DrawList(ShenWork *work) {
    char name[0x100];
    FlashRef ref;
    FlashUv uv;
    Flash *flash = work->flash;
    ShenNode *node;
    s32 i;

    Shen_SetArrowUvs(flash);
    node = work->top;
    for (i = 0; i < 5; i++) {
        sprintf(name, gShenPlateFmt, i + 1);
        switch (node->wish.kind) {
        case SHEN_WISH_ITEM: {
            s32 idx = node->wish.id - 1;
            s32 cell;

            if (work->items[idx].type != 0) {
                cell = work->items[idx].type != 2 ? 2 : 1;
            } else {
                cell = 3;
            }
            uv.x0 = cell << 6;
            uv.y0 = 0;
            uv.x1 = (cell << 6) + 0x40;
            uv.y1 = 0x40;
            break;
        }
        case SHEN_WISH_STAGE:
            uv.x0 = 0x40;
            uv.x1 = 0x80;
            uv.y0 = 0x40;
            uv.y1 = 0x80;
            break;
        case SHEN_WISH_CHARA:
            uv.x0 = 0;
            uv.x1 = 0x40;
            uv.y0 = 0x40;
            uv.y1 = 0x80;
            break;
        case SHEN_WISH_MONEY:
            uv.x0 = 0;
            uv.x1 = 0x40;
            uv.y0 = 0;
            uv.y1 = 0x40;
            break;
        }
        Shen_SetClipUv(flash, name, gShenIconClip, &uv);
        Flash_FindLabel(flash, name, gShenIconClip, &ref);
        Flash_ClipSetCallbackA(flash, &ref, Shen_ScissorList, 0);
        Flash_ClipSetCallbackB(flash, &ref, Shen_ScissorFull, 0);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetCallbackA(flash, &ref, Shen_ScissorList, 0);
        Flash_ClipSetCallbackB(flash, &ref, Shen_ScissorFull, 0);
        if (Shen_IsOwned(node->wish.kind, node->wish.id - 1)) {
            Flash_ClipSetColor(flash, &ref, 0.5f);
        } else {
            Flash_ClipSetColor(flash, &ref, 1.0f);
        }
        Shen_FindClip(flash, name, gShenTextClip, &ref);
        {
            s32 no = node->no;
            s32 line;

            if (work->dragon == SHEN_DRAGON_1) {
                line = no + 4;
            } else {
                line = no;
                if (work->dragon == SHEN_DRAGON_2) {
                    line += 11;
                }
            }
            TextBox_AttachLine(flash, &ref, 0, 0, line, &work->box[i]);
        }
        if (i == 3) {
            node = work->out;
        } else {
            node = node->next;
        }
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/sys/late_a", Shen_DrawList);

/* Sets the reward window up for the wish under the cursor. */
void Shen_SetupGetWin(ShenWork *work) {
    ShenWish *wish = &ShenList_Step(work->top, work->cursor)->wish;

    switch (wish->kind) {
    case SHEN_WISH_ITEM:
        GetWin_Setup(GETWIN_KIND_ITEM, wish->id - 1);
        break;
    case SHEN_WISH_STAGE:
        GetWin_Setup(GETWIN_KIND_1, wish->id - 1);
        break;
    case SHEN_WISH_CHARA:
        GetWin_Setup(GETWIN_KIND_0, wish->id - 1);
        break;
    case SHEN_WISH_MONEY:
        GetWin_Setup(GETWIN_KIND_MONEY, wish->id);
        break;
    }
}

/*
 * Draws which dragon comes and copies its wishes from the wish file into the list.
 *
 * One Rand_Range(100) draw `r`. With bit 0 of gSaveData->slot[8].flags set: r < 40 dragon 0, 40..59 dragon 1,
 * 60..99 dragon 2. Without it: r < 50 dragon 0, else dragon 1 (dragon 2 cannot come). Dragon 1 lists seven
 * wishes and grants three; the others list four and grant one. The dragon number also goes to the backdrop
 * (ShenScene_Init).
 *
 * NON-MATCHING: 4 of 121 instructions, a swap of two registers. Both hold `&work->list`: s2, computed once per
 * arm, and s4, a copy made after ShenList_Build. The original passes the copy to ShenList_Find in the loop and
 * stores s2 to work->out / work->top; this attempt passes s2 and stores the copy to work->top. Everything else
 * is identical. Some 4000 arrangements of the three arms were tried (build/scratch_late_a/bl*.py).
 * Second cleanup pass, 14 more without effect (`&work->list` written out at the stores, at the helper call or
 * both; `list` set in front of ShenList_Build or once for all arms: 29 to 94 differences).
 */
#if 0
/* Copies `count` wishes into the nodes of a list. */
static inline void ShenList_Fill(ShenNode *head, ShenWish *src, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        ShenList_Find(head, i)->wish = *src++;
    }
}

void Shen_BuildList(ShenWork *work, ShenWishFile *file) {
    s32 a;
    s32 b;
    s32 c;
    s32 r;
    ShenNode *list;

    if (gSaveData->slot[8].flags & 1) {
        a = 40;
        b = 60;
        c = 100;
    } else {
        a = 50;
        b = 100;
        c = 200;
    }
    r = Rand_Range(100);
    if (r >= b && r < c) {
        work->dragon = SHEN_DRAGON_2;
        work->wishMax = 1;
        ShenScene_Init(SHEN_DRAGON_2);
        ShenList_Build(&work->list, 4);
        list = &work->list;
        ShenList_Fill(list, file->list2, 4);
        work->out = list;
    } else if (r >= a && r < b) {
        work->dragon = SHEN_DRAGON_1;
        work->wishMax = 3;
        ShenScene_Init(SHEN_DRAGON_1);
        ShenList_Build(&work->list, 7);
        list = &work->list;
        ShenList_Fill(list, file->list1, 7);
        work->out = list;
    } else {
        work->dragon = SHEN_DRAGON_0;
        work->wishMax = 1;
        ShenScene_Init(SHEN_DRAGON_0);
        ShenList_Build(&work->list, 4);
        list = &work->list;
        ShenList_Fill(list, file->list0, 4);
        work->out = list;
    }
    work->top = list;
}
#endif
INCLUDE_ASM("asm/nonmatchings/sys/late_a", Shen_BuildList);

/* Unpacks the screen's pack file and builds the list movie, the windows and the wish list. */
void Shen_Init(ShenWork *work, s32 section) {
    FlashTexRes *res = NULL;
    s32 i;

    work->pack = PACK_AT(gMenuArc12, section);
    work->res = Sprite_Unpack(work->pack, NULL, NULL);

    Shen_DebugFile("host:data/test/shenron/sr_select_top_tex_PS2_.dbt");
    res = PACK_AT(work->res, 4);
    Res_RelocateOffsets(&res, res, res);
    work->tex[0] = res->tex;
    work->tex[1] = res->tex + FLASH_TEX_SIZE;
    work->tex[2] = res->tex + FLASH_TEX_SIZE * 2;
    work->tex[4] = res->tex + FLASH_TEX_SIZE * 3;
    work->tex[5] = res->tex + FLASH_TEX_SIZE * 4;
    work->tex[6] = res->tex + FLASH_TEX_SIZE * 5;
    work->tex[7] = res->tex + FLASH_TEX_SIZE * 6;
    work->tex[3] = NULL;
    Shen_DebugFile("host:data/test/shenron/shenron_PS2_.fod");
    for (i = 0; i < SHEN_FLASH_COUNT; i++) {
        Flash_Create(&work->flash[i], PACK_AT(work->res, 3), work->tex);
    }

    Shen_DebugFile("host:data/test/shenron/sr_select_top_title_JP_PS2_.dbt");
    res = PACK_AT(work->res, 5);
    Res_RelocateOffsets(&res, res, res);
    Shen_DebugFile("host:data/test/shenron/if_title_line_PS2_.pak");
    IconWin_Init(PACK_AT(work->res, 8), res);

    Shen_DebugFile("host:data/test/shenron/shenron_msg_JP_PS2_.pak");
    work->msg = PACK_AT(work->res, 1);
    Shen_DebugFile("host:data/test/shenron/if_msg_window_PS2_.pak");
    MsgWin_Init4(PACK_AT(work->res, 9), work->msg, 0, work->unk5C);
    Shen_DebugFile("host:data/test/shenron/if_system_window_JP_PS2_.pak");
    Dialog_Init(PACK_AT(work->res, 10), work->msg, 0);
    Shen_DebugFile("host:data/test/shenron/reconfir_JP_PS2_.pak");
    ShenCfm_Init(PACK_AT(work->res, 6));
    Shen_DebugFile("host:data/test/shenron/zitem_parameter_PS2_.dat");
    work->items = PACK_AT(work->res, 7);
    Shen_DebugFile("host:data/test/shenron/get_window_JP_PS2_.pak");
    GetWin_Init(PACK_AT(work->res, 11), 0);

    Shen_DebugFile("host:data/test/shenron/font_BattleTutorial_CC_JP_PS2_.pak");
    work->font = PACK_AT(work->res, 13);
    for (i = 0; i < 5; i++) {
        TextBox_Init(&work->box[i], work->font, 2);
        TextBox_SetRect(&work->box[i], 0, 0x200, 0x6A, 0x11E);
    }

    Shen_DebugFile("host:data/test/shenron/shenron_list_PS2_.dat");
    Shen_BuildList(work, PACK_AT(work->res, 2));
    gSaveData->unlockFlags &= ~0x7F;
}

/* Steps the list movie. */
void Shen_Advance(ShenWork *work) {
    s32 i;

    for (i = 0; i < SHEN_FLASH_COUNT; i++) {
        Flash_Advance(&work->flash[i]);
    }
}

/* Draws the screen; opens the windows once the backdrop is ready. */
void Shen_Draw(ShenWork *work) {
    s32 i;

    if (!(work->flags & SHEN_FLAG_SHOWN)) {
        if (ShenScene_GetState() == 1) {
            work->flags |= SHEN_FLAG_SHOWN | SHEN_FLAG_OPENING;
        }
    } else if (work->flags & SHEN_FLAG_OPENING) {
        IconWin_Open();
        MsgWin_Open();
        work->flags ^= SHEN_FLAG_OPENING;
    }
    Shen_DrawList(work);
    ShenScene_Update();
    for (i = 0; i < SHEN_FLASH_COUNT; i++) {
        Flash_Draw(&work->flash[i]);
    }
    IconWin_Draw();
    GetWin_Draw();
    MsgWin_Draw(0, 0, work->line);
    ShenCfm_Draw();
    Dialog_Draw(0);
}

/* Frees everything Shen_Init made. */
void Shen_Term(ShenWork *work) {
    s32 i;

    for (i = 0; i < SHEN_FLASH_COUNT; i++) {
        Flash_Destroy(&work->flash[i]);
    }
    ShenList_Free(&work->list);
    ShenCfm_Term();
    IconWin_Term();
    Dialog_Term();
    MsgWin_Term();
    ShenScene_Term();
    GetWin_Term();
    if (work->res != NULL) {
        Heap_Free(work->res);
        work->res = NULL;
    }
}

/* Pad 0 confirm, with its sound. */
static inline s32 Shen_PressedOk(void) {
    if (gPad[0].gamePressed & PADG_CROSS) {
        Snd_PlaySe(1, 1);
        return 1;
    }
    return 0;
}

/*
 * The dragon's greeting and farewell. Returns 1 while the list may not be used. The greeting (lines 0..2, or
 * 8..10 for dragon 1) lets the list movie start when the voice ends or confirm is pressed; the farewell (line
 * 7, or 15) closes the windows the same way and, 40 frames later, asks the backdrop to end.
 */
s32 Shen_UpdateTalk(ShenWork *work) {
    if (!(work->flags & SHEN_FLAG_GREETED)) {
        s32 line = Rand_Range(2);

        if (work->dragon != SHEN_DRAGON_1) {
            if (line == 1) {
                line = 2;
            }
        }
        Shen_Say(work, work->dragon == SHEN_DRAGON_1 ? line + 8 : line);
        work->flags |= SHEN_FLAG_GREETED;
    } else if (work->line == (work->dragon == SHEN_DRAGON_1 ? 8 : 0) || work->line == (work->dragon == SHEN_DRAGON_1 ? 9 : 1)
        || work->line == (work->dragon == SHEN_DRAGON_1 ? 10 : 2)) {
        if (Voice_GetStat(0) == 5 || Shen_PressedOk()) {
            Flash_Play(work->flash, 1);
        }
        if (!(work->flags & SHEN_FLAG_LIST_ON)) {
            if (work->flash[0].unk0[2] & 2) {
                Shen_SetPlate(work->flash, work->cursor, 1);
                work->flags |= SHEN_FLAG_LIST_ON;
            }
        }
        return 0;
    } else if (work->line == (work->dragon == SHEN_DRAGON_1 ? 15 : 7) && !(work->flags & SHEN_FLAG_BYE)) {
        if (Voice_GetStat(0) == 5 || Shen_PressedOk()) {
            MsgWin_Close();
            IconWin_Close();
            work->timer = 40;
            work->line = -1;
            work->flags |= SHEN_FLAG_BYE;
        }
    } else if (work->flags & SHEN_FLAG_BYE) {
        work->timer--;
        if (work->timer == -1) {
            ShenScene_SetState(3);
            work->flags ^= SHEN_FLAG_BYE;
        }
    } else {
        return 0;
    }
    return 1;
}

/* One frame of the screen's logic: cursor, confirmation, granting the wish, the reward window. */
void Shen_Update(ShenWork *work) {
    s32 ready;
    ShenWish *wish;

    if (work->flags & SHEN_FLAG_EXIT) {
        return;
    }
    ready = ShenScene_GetState();
    if (ready != 1) {
        return;
    }
    if (gProgress->flags & PROGRESS_FLAG_FREEZE) {
        return;
    }
    if (GetWin_IsAnimating()) {
        return;
    }
    if (Shen_UpdateTalk(work)) {
        return;
    }
    if (!(work->flash[0].unk0[2] & 2)) {
        return;
    }
    switch (work->state) {
    case SHEN_STATE_SELECT:
        if (gPad[0].gameRepeat & PADG_UP) {
            Shen_MoveCursor(work, 0);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            Shen_MoveCursor(work, 1);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            s32 kind = ShenList_StepWish(work->top, work->cursor)->kind;
            s32 id = ShenList_StepWish(work->top, work->cursor)->id - 1;

            work->state = SHEN_STATE_CONFIRM;
            Shen_PlayPlateOk(work);
            ShenCfm_Open(kind, id);
            Snd_PlaySe(1, 1);
        }
        break;
    case SHEN_STATE_CONFIRM:
        if (ShenCfm_Update(&work->answer)) {
            switch (work->answer) {
            case 0: {
                s32 line;

                work->granted++;
                work->state = SHEN_STATE_GET;
                work->flags |= SHEN_FLAG_UNUSED40;
                ShenCfm_Close();
                Shen_SetupGetWin(work);
                GetWin_Open();
                wish = ShenList_StepWish(work->top, work->cursor);
                line = Rand_Range(2);
                if (line == 0) {
                    line = 2;
                }
                Shen_Say(work, work->dragon == SHEN_DRAGON_1 ? line + 11 : line + 3);
                switch (wish->kind) {
                case SHEN_WISH_ITEM:
                    Save_AddItem(wish->id - 1);
                    break;
                case SHEN_WISH_STAGE:
                    gSaveData->stageBits |= 1LL << (wish->id - 1);
                    break;
                case SHEN_WISH_CHARA:
                    SAVE_CHARA_WORD(gSaveData, wish->id - 1) |= SAVE_CHARA_MASK(wish->id - 1);
                    break;
                case SHEN_WISH_MONEY:
                    Save_AddMoney(wish->id);
                    break;
                }
                break;
            }
            case 1:
                work->state = SHEN_STATE_SELECT;
                ShenCfm_Close();
                break;
            }
        }
        break;
    case SHEN_STATE_GET:
        if (SHEN_PRESSED_CROSS_OR_TRIANGLE()) {
            ShenWish *got = ShenList_StepWish(work->top, work->cursor);

            if (got->kind == SHEN_WISH_CHARA && got->id == 0xA0 && !Shen_IsOwned(got->kind, got->id)) {
                SAVE_CHARA_WORD(gSaveData, got->id) |= SAVE_CHARA_MASK(got->id);
                GetWin_Setup(GETWIN_KIND_0, got->id);
                GetWin_Next();
            } else {
                GetWin_Close();
                if (work->wishMax == work->granted) {
                    Shen_Say(work, work->dragon == SHEN_DRAGON_1 ? 15 : 7);
                    Flash_GotoLabel(work->flash, "fl_out", 1);
                    work->state = SHEN_STATE_LEAVE;
                } else {
                    work->state = SHEN_STATE_SELECT;
                }
            }
            Snd_PlaySe(1, 1);
        }
        break;
    }
}

/* Starts the fade-out when the backdrop has ended; returns 1 when the screen is black. */
s32 Shen_UpdateExit(ShenWork *work) {
    if (ShenScene_GetState() == 2 && !(work->flags & SHEN_FLAG_EXIT)) {
        work->flags |= SHEN_FLAG_EXIT;
    }
    if (work->flags & SHEN_FLAG_EXIT) {
        if (!(work->flags & SHEN_FLAG_FADING)) {
            work->flags |= SHEN_FLAG_FADING;
            work->unk398 = 30;
            ColorFade_StartOut(0, 0, 0, 20);
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            if (work->result == 0) {
                Bgm_FadeOutStep();
            }
        } else {
            return 1;
        }
    }
    return 0;
}

/*
 * The screen's frame loop (30 frames per second: Gfx_EndFrame(2)). Per frame: Gfx_BeginFrame, Pad_Update,
 * ColorFade_Update, Snd_Update, Shen_Advance, Shen_Draw, ColorFade_Draw, Gfx_EndFrame, Dma_Flush, then
 * Shen_UpdateExit and, while that returns 0, Shen_Update. Leaves when the fade-out started by the end of the
 * backdrop is complete.
 */
s32 Shen_Run(s32 section) {
    ShenWork work;

    memset(&work, 0, sizeof(ShenWork));
    Shen_Init(&work, section);
    ColorFade_StartIn(0, 0, 0, 20);
    for (;;) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        Shen_Advance(&work);
        Shen_Draw(&work);
        ColorFade_Draw();
        Gfx_EndFrame(2);
        Dma_Flush();
        File_Stub264D90();
        if (Shen_UpdateExit(&work)) {
            break;
        }
        Shen_Update(&work);
    }
    Shen_Term(&work);
    Dma_ResetBuffers();
    return work.result;
}
