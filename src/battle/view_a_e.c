#ifdef PORT
#define GRID_CHARACTER(id) PortGame_IsCharacter(id)
#else
#define GRID_CHARACTER(id) ((id) < CHRGRID_ID_RANDOM)
#endif
#include "common.h"
#include "battle/view_a.h"
#include "sys/heap.h"
#include "sys/rand.h"
#include "sys/save.h"

/*
 * Progress_Init, the menu helpers and the head of the text box module, 0x25DE68..0x2600B0. See battle/view_a.h.
 *
 *   Progress_*     the block of state shared by the menus and the battle (gProgress)
 *   FlashAnim_*    frame animation of one movie clip: blinking eyes, a talking mouth, a sprite sheet, a
 *                  scrolling texture. They do nothing while PROGRESS_FLAG_FREEZE is set or the clip is missing.
 *   Num_*          numbers drawn with one movie clip per digit
 *   ChrGrid_*      the character select grid (7 columns) and its cursor
 *   StgGrid_*      the stage select grid (6 columns) and its cursor
 *   BgmList_*      the music list
 *
 * Nearly all callers are in the menu overlay; Num_ToDigits and Num_CountDigits are also used by sys/loading.c.
 *   TextBox_*      0x25FE00..0x2600B0: the first functions of the text box module (a text file plus the style
 *                  its lines are drawn in), which continues after 0x2600B0
 *
 * One object: the powers of ten of Num_ToDigits (0x2F3250) and the jump table of TextBox_Init (0x2F3270) lie in
 * one 16-byte aligned read-only block (linked as two files the table landed at 0x2F3248), and the object goes on
 * past 0x2600B0. The TextBox part was written as view_a_f.c and merged in when the files were linked.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 sprintf(char *dst, const char *fmt, ...);

extern void Flash_FindLabel(Flash *flash, char *parent, char *name, FlashRef *out);
extern void Flash_ClipSetFlags(Flash *flash, FlashRef *ref, s32 prop, s32 value);
extern void Flash_ClipSetOffset(Flash *flash, FlashRef *ref, s32 a, s32 b);
extern void Flash_ClipSetTex(Flash *flash, FlashRef *ref, s32 frame);
extern void Flash_ClipSetUv(Flash *flash, FlashRef *ref, FlashUv *uv);

/* voice module, after 0x2600B0 (not decompiled) */
extern void LipSync_Update(void);
extern s32 LipSync_IsOpen(void); /* non-zero while a voice line plays */

/* Defined here: this object's .sdata (0x2FF10C). */
ViewProgress *gProgress = NULL;

/* Allocates the progress block and the three buffers of the loading screen. */
void Progress_Init(void) {
    gProgress = Heap_Alloc(sizeof(ViewProgress), 0x20, 0, HEAP_ANY);
    memset(gProgress, 0, sizeof(ViewProgress));
    gProgress->unk4 = 0x1C1;
    gProgress->loadPack = Heap_Alloc(0x3000, 0x40, 0, HEAP_ANY);
    gProgress->loadRes = Heap_Alloc(0x6800, 0x20, 0, HEAP_ANY);
    gProgress->loadSprites = Heap_Alloc(0x380, 0x20, 0, HEAP_ANY);
    gProgress->mode = 1;
    gProgress->flags |= PROGRESS_FLAG_40;
    Progress_ClearSession();
    gProgress->unk24 = -1;
}

/* Clears the parts of the progress block that do not survive a return to the title (also called by Save_*). */
void Progress_ClearSession(void) {
    s32 i;

    if (gProgress != NULL) {
        memset(gProgress->unk30, 0, sizeof(gProgress->unk30));
        memset(gProgress->unk7C, 0, sizeof(gProgress->unk7C));
        memset(gProgress->unk440, 0, 0x1F4);
        memset(gProgress->unk634, 0, sizeof(gProgress->unk634));
        memset(gProgress->unk68C, 0, sizeof(gProgress->unk68C));
        gProgress->unk7D0 = 0;
        memset(gProgress->unk7D4, 0, sizeof(gProgress->unk7D4));
        for (i = 0; i < 5; i++) {
            gProgress->unk440[i].unk0 = 0;
            gProgress->unk530[i].unk0 = 1;
        }
    }
}

/* Eye blink: the clip is hidden except for a short blink (frames `frame`, `frame + 1`, `frame`); the next
 * blink comes after 136..300 calls, decided by Rand_Range(12) >= 8 on each call past 136. */
void FlashAnim_Blink(Flash *flash, FlashRef *ref, s32 *timer, s32 frame) {
    if (gProgress->flags & PROGRESS_FLAG_FREEZE) {
        return;
    }
    if (ref->id < 0) {
        return;
    }
    if (*timer < 36) {
        Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 0);
    } else if (*timer < 40) {
        Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetTex(flash, ref, frame);
    } else if (*timer < 44) {
        Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetTex(flash, ref, frame + 1);
    } else if (*timer < 48) {
        Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetTex(flash, ref, frame);
    } else {
        Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 0);
    }
    (*timer)++;
    if (*timer >= 136) {
        if ((s32)Rand_Range(12) >= 8) {
            *timer = 0;
        }
    }
    if (*timer > 300) {
        *timer = 0;
    }
}

/* Shows the clip at frame + 1. */
void FlashAnim_ShowNext(Flash *flash, FlashRef *ref, s32 frame) {
    if (ref->id >= 0) {
        Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetTex(flash, ref, frame + 1);
    }
}

/* Mouth movement: while a voice line plays, every sixth call picks one of three mouth shapes with
 * Rand_Range(3); otherwise the clip rests at frame + 1. */
void FlashAnim_Talk(Flash *flash, FlashRef *ref, s32 *timer, s32 frame) {
    if (gProgress->flags & PROGRESS_FLAG_FREEZE) {
        return;
    }
    if (ref->id < 0) {
        return;
    }
    LipSync_Update();
    if (LipSync_IsOpen() != 0) {
        (*timer)++;
        if (*timer % 6 == 0) {
            switch ((s32)Rand_Range(3)) {
            case 0:
                Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 1);
                Flash_ClipSetTex(flash, ref, frame);
                break;
            case 1:
                Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 1);
                Flash_ClipSetTex(flash, ref, frame + 1);
                break;
            case 2:
                Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 0);
                Flash_ClipSetTex(flash, ref, frame);
                break;
            }
        }
    } else {
        Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetTex(flash, ref, frame + 1);
    }
}

/* Shows the clip at frame + 1 (the same code as FlashAnim_ShowNext). */
void FlashAnim_ShowNext2(Flash *flash, FlashRef *ref, s32 frame) {
    if (ref->id >= 0) {
        Flash_ClipSetFlags(flash, ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetTex(flash, ref, frame + 1);
    }
}

/* Sprite-sheet animation: every `period` calls the frame advances (cols * rows frames, looping) and the
 * clip's texture rectangle moves to that cell. uv->x1 / y1 are the cell size here. */
void FlashAnim_Sheet(Flash *flash, FlashRef *ref, s32 *timer, s32 *frame, FlashUv *uv, s32 cols, s32 rows, s32 period) {
    FlashUv cell;
    s32 count = cols * rows;
    s32 row;
    s32 col;

    if (gProgress->flags & PROGRESS_FLAG_FREEZE) {
        return;
    }
    if (ref->id < 0) {
        return;
    }
    (*timer)++;
    if (*timer % period == 0) {
        *timer = 0;
        (*frame)++;
        if (*frame >= count) {
            *frame = 0;
        }
    }
    if (*frame != 0) {
        col = *frame % cols;
        row = *frame / cols;
    } else {
        row = 0;
        col = 0;
    }
    cell.x0 = uv->x0 + uv->x1 * col;
    cell.y0 = uv->y0 + uv->y1 * row;
    cell.x1 = uv->x1 + uv->x1 * col;
    cell.y1 = uv->y1 + uv->y1 * row;
    cell.unk10 = 0;
    Flash_ClipSetUv(flash, ref, &cell);
}

/* Scrolling texture: the rectangle is shifted by (*x, *y), which then advance by (dx, dy) and wrap at the
 * rectangle's x1 / y1. Either pointer may be NULL. */
void FlashAnim_Scroll(Flash *flash, FlashRef *ref, FlashUv *uv, f32 *x, f32 *y, f32 dx, f32 dy) {
    FlashUv cell;

    if (gProgress->flags & PROGRESS_FLAG_FREEZE) {
        return;
    }
    if (ref->id < 0) {
        return;
    }
    cell = *uv;
    if (x != NULL) {
        cell.x0 += (s32)*x;
        cell.x1 += (s32)*x;
        *x += dx;
        if (dx < 0.0f) {
            if (*x < 0.0f) {
                *x += (f32)uv->x1;
            }
        } else if ((f32)uv->x1 <= *x) {
            *x -= (f32)uv->x1;
        }
    }
    if (y != NULL) {
        cell.y0 += (s32)*y;
        cell.y1 += (s32)*y;
        *y += dy;
        if (dy < 0.0f) {
            if (*y < 0.0f) {
                *y += (f32)uv->y1;
            }
        } else if ((f32)uv->y1 <= *y) {
            *y -= (f32)uv->y1;
        }
    }
    Flash_ClipSetFlags(flash, ref, FLASH_PROP_UV, 1);
    Flash_ClipSetUv(flash, ref, &cell);
}

/* Writes the `count` decimal digits of |value|, most significant first, then 0xFF. Leading zeros become 10
 * (blank), or 0 when zeroPad is set. */
void Num_ToDigits(u8 *digits, s32 value, s32 count, s32 zeroPad) {
    s32 pow10[8] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000 };
    s32 started = 0;
    s32 i;
    s32 d;

    if (value == 0) {
        for (i = 0; i < count - 1; i++) {
            if (zeroPad != 0) {
                *digits = 0;
            } else {
                *digits = 10;
            }
            digits++;
        }
        digits[0] = 0;
        digits[1] = 0xFF;
    } else {
        i = count - 1;
        value = value < 0 ? -value : value;
        for (; i > 0; i--) {
            d = value / pow10[i];
            if (d == 0 && started) {
                *digits = 0;
                digits++;
            } else if (d != 0) {
                *digits = d;
                digits++;
                value -= d * pow10[i];
                started = 1;
            } else {
                if (zeroPad != 0) {
                    *digits = 0;
                    digits++;
                } else {
                    *digits = 10;
                    digits++;
                }
            }
        }
        digits[0] = value;
        digits[1] = 0xFF;
    }
}

/* Shows a digit string made by Num_ToDigits: one clip per digit (refs[i]), texture cell cells[digit]; a blank
 * hides the clip. (a2, a3) go to Flash_ClipSetOffset for every clip. */
void Num_DrawDigits(Flash *flash, FlashRef *refs, s32 a2, s32 a3, FlashUv *cells, u8 *digits) {
    s32 i;

    for (i = 0; *digits != 0xFF; i++) {
        if (*digits != 10) {
            Flash_ClipSetUv(flash, &refs[i], &cells[*digits]);
            Flash_ClipSetFlags(flash, &refs[i], FLASH_PROP_VISIBLE, 1);
        } else {
            Flash_ClipSetFlags(flash, &refs[i], FLASH_PROP_VISIBLE, 0);
        }
        Flash_ClipSetOffset(flash, &refs[i], a2, a3);
        digits++;
    }
}

/* base to the power exp, for exp >= 1 (exp <= 1 gives base). */
s32 Num_Pow(s32 base, s32 exp) {
    s32 result = base;
    s32 i;

    for (i = 1; i < exp; i++) {
        result *= base;
    }
    return result;
}

/* Draws |value| with the clips named fmt % (first + n), n = 0 for the last digit. The digit sheet has four
 * w * h cells per row; cell 10 is the blank. count 0 = as many digits as the value has. mode 0 hides leading
 * zeros, 1 shows them, 2 shows the blank cell in every clip. */
void Num_Draw(Flash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode) {
    FlashRef ref;
    char name[0x40];
    FlashUv uv;
    s32 started = 0;
    s32 i;
    s32 d;
    s32 digit;

    if (count == 0) {
        count = Num_CountDigits(value);
    }
    i = count - 1;
    value = value < 0 ? -value : value;
    for (; i >= 0; i--) {
        sprintf(name, fmt, i + first);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (mode != 2) {
            if (i != 0) {
                d = value / Num_Pow(10, i);
            } else {
                d = value;
            }
            if (d == 0 && started) {
                digit = 0;
            } else if (d != 0) {
                if (i != 0) {
                    value -= d * Num_Pow(10, i);
                } else {
                    value -= d;
                }
                digit = d;
                started = 1;
            } else if (mode != 0 || i == 0) {
                digit = 0;
            } else {
                Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 0);
                continue;
            }
        } else {
            digit = 10;
        }
        uv.y0 = h * (digit / 4);
        uv.y1 = h * (digit / 4) + h;
        uv.x0 = w * (digit % 4);
        uv.x1 = w * (digit % 4) + w;
        Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
}

/* Num_Draw for digit clips inside another clip: the clips are fmt % n inside `parent`, or, with parentFmt
 * set, the clip `fmt` inside the parents named parent % n. */
void Num_DrawChild(Flash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode,
                   s32 parentFmt) {
    FlashRef ref;
    char name[0x40];
    FlashUv uv;
    s32 started = 0;
    s32 i;
    s32 d;
    s32 digit;

    if (count == 0) {
        count = Num_CountDigits(value);
    }
    i = count - 1;
    value = value < 0 ? -value : value;
    for (; i >= 0; i--) {
        if (parentFmt != 0) {
            sprintf(name, parent, i + first);
            Flash_FindLabel(flash, name, fmt, &ref);
        } else {
            sprintf(name, fmt, i + first);
            Flash_FindLabel(flash, parent, name, &ref);
        }
        if (mode != 2) {
            if (i != 0) {
                d = value / Num_Pow(10, i);
            } else {
                d = value;
            }
            if (d == 0 && started) {
                digit = 0;
            } else if (d != 0) {
                if (i != 0) {
                    value -= d * Num_Pow(10, i);
                } else {
                    value -= d;
                }
                digit = d;
                started = 1;
            } else if (mode != 0 || i == 0) {
                digit = 0;
            } else {
                Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 0);
                continue;
            }
        } else {
            digit = 10;
        }
        uv.y0 = h * (digit / 4);
        uv.y1 = h * (digit / 4) + h;
        uv.x0 = w * (digit % 4);
        uv.x1 = w * (digit % 4) + w;
        Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
}

/* Number of decimal digits of |value| (1 for 0). */
s32 Num_CountDigits(s32 value) {
    s32 digits = 1;

    value = __builtin_abs(value);
    while (value >= Num_Pow(10, digits)) {
        digits++;
    }
    return digits;
}

/* Num_Draw driven by a NumStyle: the clip naming follows the flags, and with NUM_FLAG_SIGN the clip in front
 * of the first digit shows cell 10 when the value is not zero. */
/* Two source forms decide the registers here (both found from a decomp-permuter candidate that used
 * `digit++; digit--;` in the loop):
 * - the count is an if / else with the call first (`if (style->count == 0) count = f(); else count = style->count;`):
 *   the field is then loaded into v0 and copied to s0 at the join, as in the original; `count = style->count;
 *   if (count == 0) ...` and the conditional expression load it straight into s0.
 * - the blank test comes first (`if (flags & NUM_FLAG_BLANK) { digit = 10; } else { ... }`). The code is the
 *   same either way, but the ORDER of the sets of `digit` in the insn chain differs, and local-alloc doubles a
 *   register's live length once for every constant set without a REG_EQUAL note at the head of that chain:
 *   with the negated test the chain starts `digit = 0` (top), `digit = 0` (first arm), which is two doublings
 *   (556 = 4 x 139) and puts digit / flash / name in s7 / s5 / s6; with the blank arm first the second set is
 *   `digit = 10` carrying a note (280 = 2 x 140), the original's s5 / s6 / s7. */
void Num_DrawEx(Flash *flash, NumStyle *style, char *a, char *b, s32 value) {
    FlashRef ref;
    char name[0x40];
    FlashUv uv;
    s32 digit = 0;
    s32 sign = 0;
    s32 started = 0;
    s32 signPos = 0;
    s32 count;
    s32 i;
    s32 d;
    s32 flags;

    if (style->count == 0) {
        count = Num_CountDigits(value);
    } else {
        count = style->count;
    }
    if (style->flags & NUM_FLAG_SIGN) {
        if (value <= 0) {
            if (value < 0) {
                sign = -1;
            } else {
                sign = 0;
            }
        } else {
            sign = 1;
        }
        signPos = Num_CountDigits(value);
    }
    i = count - 1;
    value = value < 0 ? -value : value;
    for (; i >= 0; i--) {
        if (b == NULL) {
            sprintf(name, a, i + style->first);
            Flash_FindLabel(flash, NULL, name, &ref);
        } else if (style->flags & NUM_FLAG_PARENT_FMT) {
            sprintf(name, a, i + style->first);
            Flash_FindLabel(flash, name, b, &ref);
        } else if (style->flags & NUM_FLAG_CHILD_FMT) {
            sprintf(name, b, i + style->first);
            Flash_FindLabel(flash, a, name, &ref);
        }
        flags = style->flags;
        if (flags & NUM_FLAG_BLANK) {
            digit = 10;
        } else {
            if (i != 0) {
                d = value / Num_Pow(10, i);
            } else {
                d = value;
            }
            if (d == 0 && started) {
                digit = 0;
            } else if (d != 0) {
                if (i != 0) {
                    value -= d * Num_Pow(10, i);
                } else {
                    value -= d;
                }
                digit = d;
                started = 1;
            } else if ((flags & NUM_FLAG_ZEROS) || i == 0) {
                digit = 0;
            } else {
                if ((flags & NUM_FLAG_SIGN) && signPos == i && sign != 0) {
                    if (sign > 0) {
                        digit = 10;
                        goto draw;
                    }
                    if (sign < 0) {
                        digit = 10;
                    }
                    goto draw;
                }
                Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 0);
                continue;
            }
        }
    draw:
        uv.y0 = style->h * (digit / 4);
        uv.y1 = style->h * (digit / 4) + style->h;
        uv.x0 = style->w * (digit % 4);
        uv.x1 = style->w * (digit % 4) + style->w;
        Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 1);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
}

/* Non-zero if the cursor may rest on a cell: a character, the custom cell or the random cell. */
s32 ChrGrid_IsSelectable(ChrGridCell *cells, s32 index) {
    s32 ok = 1;
#ifdef PORT
    if (PortGame_IsBT4()) return PortGame_IsCharacter(cells[index].id) ||
        cells[index].id==CHRGRID_ID_RANDOM || cells[index].id==CHRGRID_ID_CUSTOM;
#endif

    if (cells[index].id >= CHRGRID_ID_LOCKED) {
        ok = cells[index].id == CHRGRID_ID_CUSTOM;
    }
    return ok;
}

/* Moves the cursor right in its row to the next selectable cell, wrapping. 0 if there is no other one. */
s32 ChrGrid_MoveRight(ChrGridCell *cells, s32 *col, s32 row) {
    s32 start = *col;
    s32 ok = 1;
    s32 id;

    for (;;) {
        (*col)++;
        if (*col >= CHRGRID_COLS) {
            *col = 0;
        }
        if (start == *col) {
            ok = 0;
            break;
        }
        id = cells[row * CHRGRID_COLS + *col].id;
        if (id < CHRGRID_ID_LOCKED
#ifdef PORT
            || PortGame_IsCharacter(id)
#endif
        ) {
            break;
        }
        if (id == CHRGRID_ID_CUSTOM) {
            goto end;
        }
    }
end:
    return ok;
}

/* Moves the cursor left in its row to the next selectable cell, wrapping. 0 if there is no other one. */
s32 ChrGrid_MoveLeft(ChrGridCell *cells, s32 *col, s32 row) {
    s32 start = *col;
    s32 ok = 1;
    s32 id;

    for (;;) {
        (*col)--;
        if (*col < 0) {
            *col = CHRGRID_COLS - 1;
        }
        if (start == *col) {
            ok = 0;
            break;
        }
        id = cells[row * CHRGRID_COLS + *col].id;
        if (id < CHRGRID_ID_LOCKED
#ifdef PORT
            || PortGame_IsCharacter(id)
#endif
        ) {
            break;
        }
        if (id == CHRGRID_ID_CUSTOM) {
            goto end;
        }
    }
end:
    return ok;
}

/* Moves the cursor down (wrapping) to a row with a selectable cell, sliding right in the row if needed. */
void ChrGrid_MoveDown(ChrGridCell *cells, s32 *col, s32 *row, s32 rows) {
    s32 id;

    do {
        (*row)++;
        if (*row > rows - 1) {
            *row = 0;
        }
        id = cells[*row * CHRGRID_COLS + *col].id;
        if (id < CHRGRID_ID_LOCKED
#ifdef PORT
            || PortGame_IsCharacter(id)
#endif
        ) {
            return;
        }
        if (id == CHRGRID_ID_CUSTOM) {
            return;
        }
    } while (ChrGrid_MoveRight(cells, col, *row) == 0);
}

/* Moves the cursor up (wrapping) to a row with a selectable cell, sliding right in the row if needed. */
void ChrGrid_MoveUp(ChrGridCell *cells, s32 *col, s32 *row, s32 rows) {
    s32 id;

    do {
        (*row)--;
        if (*row < 0) {
            *row = rows - 1;
        }
        id = cells[*row * CHRGRID_COLS + *col].id;
        if (id < CHRGRID_ID_LOCKED
#ifdef PORT
            || PortGame_IsCharacter(id)
#endif
        ) {
            return;
        }
        if (id == CHRGRID_ID_CUSTOM) {
            return;
        }
    } while (ChrGrid_MoveRight(cells, col, *row) == 0);
}

/* Steps to the next form of a cell that is a character (id <= 0xA0), wrapping in the seven slots. */
void ChrGrid_NextForm(s32 *forms, s32 *index) {
    s32 start = *index;

    do {
        (*index)++;
        if (*index >= CHRGRID_FORM_MAX) {
            *index = 0;
        }
        if (start == *index) {
            break;
        }
    } while (!GRID_CHARACTER(forms[*index]));
}

/* Steps to the previous form of a cell that is a character, wrapping in the seven slots. */
void ChrGrid_PrevForm(s32 *forms, s32 *index) {
    s32 start = *index;

    do {
        (*index)--;
        if (*index < 0) {
            *index = CHRGRID_FORM_MAX - 1;
        }
        if (start == *index) {
            break;
        }
    } while (!GRID_CHARACTER(forms[*index]));
}

/* Puts the cursor on a character if it is on a marker cell: right in the row first, then down.
 * Declared with a result that it never sets: the call of ChrGrid_MoveDown is not a tail call. */
s32 ChrGrid_FixCursor(ChrGridCell *cells, s32 *col, s32 *row, s32 rows) {
    if (!GRID_CHARACTER(cells[*row * CHRGRID_COLS + *col].id)) {
        if (ChrGrid_MoveRight(cells, col, *row) == 0) {
            ChrGrid_MoveDown(cells, col, row, rows);
        }
    }
}

/* Non-zero if the stage cell is an unlocked stage. */
s32 StgGrid_IsSelectable(s32 *ids, s32 index) {
    return ids[index] < STGGRID_ID_LOCKED;
}

/* Moves the stage cursor right to the next unlocked stage; it stops in column 6 (the column past the grid). */
void StgGrid_MoveRight(s32 *ids, s32 *col, s32 row) {
    do {
        (*col)++;
        if (*col >= 7) {
            *col = 0;
        } else if (*col == STGGRID_COLS) {
            break;
        }
    } while (ids[row * STGGRID_COLS + *col] >= STGGRID_ID_LOCKED);
}

/* Moves the stage cursor left to the next unlocked stage; from column 0 it goes to column 6. */
void StgGrid_MoveLeft(s32 *ids, s32 *col, s32 row) {
    for (;;) {
        (*col)--;
        if (*col < 0) {
            *col = STGGRID_COLS;
            return;
        }
        if (ids[row * STGGRID_COLS + *col] < STGGRID_ID_LOCKED) {
            return;
        }
    }
}

/* Moves the stage cursor right inside the six columns, wrapping. 0 if the row has no other unlocked stage. */
s32 StgGrid_MoveRightWrap(s32 *ids, s32 *col, s32 row) {
    s32 start = *col;
    s32 ok = 1;

    do {
        (*col)++;
        if (*col >= STGGRID_COLS) {
            *col = 0;
        }
        if (start == *col) {
            ok = 0;
            break;
        }
    } while (ids[row * STGGRID_COLS + *col] >= STGGRID_ID_LOCKED);
    return ok;
}

/* Moves the stage cursor down (wrapping) to a row with an unlocked stage. */
void StgGrid_MoveDown(s32 *ids, s32 *col, s32 *row, s32 rows) {
    /* the first exit is a break and the second a return: only the first is moved to the loop's end */
    for (;;) {
        (*row)++;
        if (*row > rows - 1) {
            *row = 0;
        }
        if (ids[*row * STGGRID_COLS + *col] < STGGRID_ID_LOCKED) {
            break;
        }
        if (StgGrid_MoveRightWrap(ids, col, *row) != 0) {
            return;
        }
    }
}

/* Moves the stage cursor up (wrapping) to a row with an unlocked stage. */
void StgGrid_MoveUp(s32 *ids, s32 *col, s32 *row, s32 rows) {
    /* the first exit is a break and the second a return: only the first is moved to the loop's end */
    for (;;) {
        (*row)--;
        if (*row < 0) {
            *row = rows - 1;
        }
        if (ids[*row * STGGRID_COLS + *col] < STGGRID_ID_LOCKED) {
            break;
        }
        if (StgGrid_MoveRightWrap(ids, col, *row) != 0) {
            return;
        }
    }
}

#ifdef PORT
/* BT4's extra roster does not fit the original save's three unlock words.
   Its roster is available from the disc; never index past the BT3 save. */
#define CHARA_UNLOCKED(id) (PortGame_IsBT4() ? PortGame_IsCharacter(id) : ((s32)((SAVE_CHARA_WORD(gSaveData, id) >> ((id) % 64)) & 1)))
#else
#define CHARA_UNLOCKED(id) ((s32)((SAVE_CHARA_WORD(gSaveData, id) >> ((id) % 64)) & 1))
#endif

/* Build flags of ChrGrid_Build. */
#define CHRGRID_ALL 1        /* no unlock test (never set here) */
#define CHRGRID_NO_CUSTOM 2  /* the custom cell (0xA3) becomes a filler, and the custom list is not built */
#define CHRGRID_NO_RANDOM 4  /* the random cell (0xA1) becomes a filler */
#define CHRGRID_NO_FORMS 8   /* a cell with several unlocked forms shows only the first */

/* Builds the character grid of the current menu screen (gProgress->mode) from the master list: locked
 * characters become CHRGRID_ID_LOCKED, a cell keeps only its unlocked forms, and the list is padded to a
 * multiple of seven with fillers. Then lists the saved custom characters (gSaveData->rec) in `custom`. */
#if 0
/* Not matching, 210 of 347 instructions. The mode tests at the top match. In the loop the original keeps every copy of a cell (three of them) and every `id = CHRGRID_ID_EMPTY` store as separate code where this version shares them, and some temporaries sit in other registers (j and j * 4 in t7 / t5; the pointer to the output cell in a0). The original also addresses the saved characters as `gSaveData + 8 + (0x2D50 + i * 0x1C)`. */
void ChrGrid_Build(s32 *outCount, ChrGridCell *out, s32 *inCount, ChrGridCell *in, s32 *customCount,
                   ChrGridCell *custom) {
    s32 flags = 0;
    s32 mode;
    s32 i;
    s32 j;
    s32 pad;

    if (gProgress->mode >= 0x26 && gProgress->mode < 0x2A) {
        if (gProgress->mode == 0x28) {
            if (gProgress->unk624 == 2) {
                flags |= CHRGRID_NO_RANDOM;
            }
        }
    }
    mode = gProgress->mode;
    if (mode >= 0x21 && mode < 0x24) {
        flags |= CHRGRID_NO_CUSTOM;
    }
    if (mode >= 0xD && mode < 0x1F) {
        flags |= CHRGRID_NO_CUSTOM;
        flags |= CHRGRID_NO_RANDOM;
        if (mode == 0x15) {
            flags |= CHRGRID_NO_FORMS;
        }
    }
    *outCount = 0;
    if (mode >= 0x30 && mode < 0x33) {
        flags |= CHRGRID_NO_CUSTOM;
        flags |= CHRGRID_NO_RANDOM;
        flags |= CHRGRID_NO_FORMS;
    }

    for (i = 0; i < *inCount; i++) {
        if (in[i].formCount != 0) {
            out[*outCount].formCount = 0;
            for (j = 0; j < in[i].formCount; j++) {
                if ((flags & CHRGRID_ALL) || CHARA_UNLOCKED(in[i].form[j])) {
                    out[*outCount].form[out[*outCount].formCount] = in[i].form[j];
                    out[*outCount].formCount++;
                }
            }
            if (out[*outCount].formCount >= 2) {
                if (flags & CHRGRID_NO_FORMS) {
                    out[*outCount].formCount = 0;
                    out[*outCount].id = out[*outCount].form[0];
                    (*outCount)++;
                    continue;
                }
                out[*outCount].id = out[*outCount].form[0];
            } else if (out[*outCount].formCount == 1) {
                out[*outCount].formCount = 0;
                out[*outCount].id = out[*outCount].form[0];
                (*outCount)++;
                continue;
            } else {
                out[*outCount].id = CHRGRID_ID_LOCKED;
            }
        } else {
            switch (in[i].id) {
            case CHRGRID_ID_CUSTOM:
                if (flags & CHRGRID_NO_CUSTOM) {
                    out[*outCount].id = CHRGRID_ID_EMPTY;
                } else {
                    out[*outCount] = in[i];
                }
                break;
            case CHRGRID_ID_RANDOM:
                if (flags & CHRGRID_NO_RANDOM) {
                    out[*outCount].id = CHRGRID_ID_EMPTY;
                } else {
                    out[*outCount] = in[i];
                }
                break;
            default:
                if ((flags & CHRGRID_ALL) || CHARA_UNLOCKED(in[i].id)) {
                    out[*outCount] = in[i];
                } else {
                    out[*outCount].id = CHRGRID_ID_LOCKED;
                }
                break;
            }
        }
        (*outCount)++;
    }

    pad = (*outCount + (CHRGRID_COLS - 1)) / CHRGRID_COLS * CHRGRID_COLS;
    if (pad != 0) {
        pad -= *outCount;
        for (i = 0; i < pad; i++) {
            out[*outCount].id = CHRGRID_ID_EMPTY;
            out[*outCount].formCount = 0;
            (*outCount)++;
        }
    }

    if (!(flags & CHRGRID_NO_CUSTOM) && custom != NULL) {
        *customCount = 0;
        for (i = 0; i < SAVE_REC_COUNT; i++) {
            custom[i].id = CHRGRID_ID_LOCKED;
            custom[i].formCount = 0;
            if (gSaveData->rec[i].chara >= 0) {
                for (j = 0; j < *inCount; j++) {
                    if (gSaveData->rec[i].chara == in[j].id) {
                        custom[i] = out[j];
                        custom[i].id = gSaveData->rec[i].chara;
                        custom[i].form[0] = gSaveData->rec[i].chara;
                        (*customCount)++;
                        break;
                    }
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/view_a_e", ChrGrid_Build);
#endif

/* Marks the locked stages of a stage list: a stage whose bit in gSaveData->stageBits is clear becomes
 * STGGRID_ID_LOCKED. In mode 0x28, stages 4 and 0x1B become fillers even when unlocked. */
void StgGrid_ApplyUnlocks(s32 *count, s32 *ids) {
    s32 flags = 0;
    s32 i;
#ifdef PORT
    if (PortGame_IsBT4()) {
        for (i=0;i<*count;i++) if ((u32)ids[i]>=99) ids[i]=STGGRID_ID_EMPTY;
        return;
    }
#endif

    if (gProgress->mode == 0x28) {
        flags = 4;
    }
    for (i = 0; i < *count; i++) {
        s32 id = ids[i];

        switch (id) {
        case 0x23:
            if (flags & 2) {
                ids[i] = STGGRID_ID_EMPTY;
            }
            break;
        case 4:
        case 0x1B:
            if (!(flags & 1)) {
                if (!(s32)((gSaveData->stageBits >> id) & 1LL)) {
                    ids[i] = STGGRID_ID_LOCKED;
                } else if (flags & 4) {
                    ids[i] = STGGRID_ID_EMPTY;
                }
            }
            break;
        default:
            if (!(flags & 1)) {
                if (!(s32)((gSaveData->stageBits >> id) & 1LL)) {
                    ids[i] = STGGRID_ID_LOCKED;
                }
            }
            break;
        }
    }
}

/* Marks the locked entries of the music list: an entry whose bit in gSaveData->bgmBits is clear becomes
 * BGMLIST_ID_LOCKED. In mode 0x3E the random entry (0x18) is removed as well. */
/* The list pointer itself walks (`ids++` in the loop header): with `ids[i]` the code is the same but `i` and
 * the hoisted constant 0x19 exchange registers (t0 / t1, 9 of 40 instructions), because the strength-reduced
 * index leaves `i` a longer live range than the constant's allocation priority allows. (decomp-permuter matched
 * the indexed form with a self-assignment `gProgress->mode = gProgress->mode;` in the last arm: three
 * instructions that exist only until reload.) */
void BgmList_ApplyUnlocks(s32 *count, s32 *ids) {
    s32 flags = 0;
    s32 i;
#ifdef PORT
    if (PortGame_IsBT4()) return;
#endif

    if (gProgress->mode == 0x3E) {
        flags = 2;
    }
    for (i = 0; i < *count; i++, ids++) {
        if (*ids == BGMLIST_ID_RANDOM) {
            if (flags & 2) {
                *ids = BGMLIST_ID_LOCKED;
            }
        } else if (!(flags & 1)) {
            if (!(gSaveData->bgmBits & (1 << *ids))) {
                *ids = BGMLIST_ID_LOCKED;
            }
        }
    }
}

/*
 * TextBox, 0x25FE00..0x2600B0.
 */

/* text box module, after 0x2600B0 (not decompiled) */
extern void TextBox_SetMaxWidth(TextBox *box, s32 a);
extern void TextBox_SetLineOffsets(TextBox *box, s32 a, s32 b, s32 c, s32 d, s32 e);

/* Clears a text box, binds it to a text file and applies one of seven style presets. */
void TextBox_Init(TextBox *box, void *text, u32 preset) {
    memset(box, 0, sizeof(TextBox));
    box->text = text;
    switch (preset) {
    case 1:
        TextBox_SetUnk50(box, 2);
        TextBox_SetUnkC(box, 0x100, 0);
        TextBox_SetMaxWidth(box, 0xE1);
        break;
    case 2:
        TextBox_SetUnk50(box, 0);
        TextBox_SetUnkC(box, 0, 0);
        TextBox_SetMaxWidth(box, 0xE1);
        break;
    case 3:
        TextBox_SetUnk50(box, 2);
        TextBox_SetUnkC(box, 0x100, 0);
        TextBox_SetColor(box, 0xFFFF0080);
        TextBox_SetMaxWidth(box, 0xD4);
        break;
    case 4:
        TextBox_SetUnk50(box, 0);
        TextBox_SetUnkC(box, 0, 0);
        TextBox_SetColor(box, 0xFFFF0080);
        TextBox_SetMaxWidth(box, 0xD4);
        break;
    case 5:
        TextBox_SetUnk50(box, 0);
        TextBox_SetUnk80(box, 1);
        TextBox_SetUnkC(box, 0, 0);
        TextBox_SetLineOffsets(box, 0x20, 0x14, 0xA, 0, 0);
        break;
    case 6:
        TextBox_SetUnk50(box, 0);
        TextBox_SetMaxWidth(box, 0x160);
        break;
#ifdef PORT
    case 7:
        if(PortGame_IsBT4()) {
            TextBox_SetUnk50(box,1);
            TextBox_SetUnkC(box,0x18,0x28);
            TextBox_SetMaxWidth(box,0xD0);
        }
        break;
#endif
    case 0:
        break;
    }
}

void TextBox_SetUnk50(TextBox *box, s32 value) {
    box->unk50 = value;
}

void TextBox_SetUnk80(TextBox *box, s32 value) {
    box->unk80 = value;
}

void TextBox_SetUnkC(TextBox *box, s32 a, s32 b) {
    box->unkC = a;
    box->unk10 = b;
}

/* Gives the box four values (a rectangle, by the look of it) and marks them valid. */
void TextBox_SetRect(TextBox *box, s32 a, s32 b, s32 c, s32 d) {
    box->rect[3] = d;
    box->rect[0] = a;
    box->rect[1] = c;
    box->rect[2] = b;
    box->flags |= TEXTBOX_FLAG_RECT;
}

/* Sets the text colour from 0xRRGGBBAA. */
void TextBox_SetColor(TextBox *box, u32 rgba) {
    box->flags |= TEXTBOX_FLAG_COLOR;
    box->color[0] = rgba >> 24;
    box->color[1] = rgba >> 16;
    box->color[2] = rgba >> 8;
    box->color[3] = rgba;
}

/* Sets the second colour from 0xRRGGBBAA. */
void TextBox_SetColor2(TextBox *box, u32 rgba) {
    box->flags |= TEXTBOX_FLAG_COLOR2;
    box->color2[0] = rgba >> 24;
    box->color2[1] = rgba >> 16;
    box->color2[2] = rgba >> 8;
    box->color2[3] = rgba;
}
