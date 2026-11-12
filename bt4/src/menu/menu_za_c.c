#include "common.h"
#include "menu/menu_za.h"

/*
 * Menu overlay DBZP.BIN, 0x3AEAB0..0x3AF290: PassChk, what the password entry screen needs to judge a decoded
 * password: the tables of the password pack (section 19 of the screen's pack), the converter from the previous
 * game's 32-character password and the two validity checks. A whole object; `.rodata` 0x3BE240..0x3BE3A8 (the
 * five development paths only). Its work pointer gPassChk (0x2FF290) is an uninitialised global that the linker
 * placed in the MAIN executable's `.sbss`.
 */

#define PASSCHK_HOST "host:data/ps2/test/main/DC/PWD_INPUT/"

#define PASSCHK_OLD_ITEM_NUM 0x109  /* items of the previous game */
#define PASSCHK_NONE 999

/* A section of the pack; `host` is the development file it was built from (unused: see DcPass_Init). */
static inline void *PassChk_Section(u32 *pack, s32 n, const char *host) {
    return MPACK_AT(pack, n);
}

/* Takes the five tables of the password pack. */
void PassChk_Init(u32 *pack) {
    gPassChk = Heap_Alloc(sizeof(PassChk), 0x20, 0, 2);
    memset(gPassChk, 0, sizeof(PassChk));
    gPassChk->oldChara = PassChk_Section(pack, 1, PASSCHK_HOST "password_chara_ID_Neo_PS2_.dat");
    gPassChk->oldItem = PassChk_Section(pack, 2, PASSCHK_HOST "password_chara_item_Neo_PS2_.dat");
    gPassChk->preset = PassChk_Section(pack, 3, PASSCHK_HOST "password_Item_toZs3_PS2_.dat");
    gPassChk->chara = PassChk_Section(pack, 5, PASSCHK_HOST "progress_chara_param_neo_PS2_.dat");
    gPassChk->item = PassChk_Section(pack, 4, PASSCHK_HOST "zitem_parameter_neo_PS2_.dat");
}

/* Frees the table pointers. */
void PassChk_Term(void) {
    if (gPassChk != NULL) {
        Heap_Free(gPassChk);
        gPassChk = NULL;
    }
}

/*
 * Turns a password of the previous game into this game's record. The old items are not carried over one by
 * one: their levels are summed into one of six classes, their attack and defence totals pick one of three
 * types (defence, attack, balanced), and the character gets the seven preset items of that class and type.
 */
ZaChrPass PassChk_ConvertOld(ZaOldPass *old) {
    ZaChrPass out;
    ZaChrEntry *charaTbl = (ZaChrEntry *)MPACK_AT(gCommonRes->data[2], 1);
    u32 level = 0;
    u32 defense = 0;
    u32 attack = 0;
    s32 chara;
    s32 cls;
    s32 type;
    s32 (*row)[7];
    s32 i;

    memset(&out, 0, sizeof(out));
    chara = gPassChk->oldChara[old->charId];
    out.extraSlots = 7 - charaTbl[chara].slots;
    for (i = 0; i < 7; i++) {
        if ((u32)(old->item[i] - 1) < PASSCHK_OLD_ITEM_NUM) {
            PassOldItem *item = &gPassChk->oldItem[old->item[i] - 1];

            level += item->level;
            defense += item->defense;
            attack += item->attack;
        }
    }
    if (level == 0) {
        cls = 0;
    } else if (level >= 1 && level <= 7) {
        cls = 1;
    } else if (level >= 8 && level <= 14) {
        cls = 2;
    } else if (level >= 15 && level <= 21) {
        cls = 3;
    } else if (level >= 22 && level <= 28) {
        cls = 4;
    } else if (level >= 29 && level <= 35) {
        cls = 5;
    } else {
        cls = 0;
    }
    row = gPassChk->preset[cls];
    if (attack < defense) {
        type = 0;
    } else if (defense < attack) {
        type = 1;
    } else if (defense == attack) {
        type = 2;
    } else {
        type = 0;
    }
    out.charId = chara;
    for (i = 0; i < 7; i++) {
        if (row[type][i] != PASSCHK_NONE) {
            out.item[i] = row[type][i] + 1;
        } else {
            out.item[i] = 0;
        }
    }
    return out;
}

/* Whether a decoded password of the previous game is acceptable: a known character, allowed items only. */
s32 PassChk_IsOldValid(ZaOldPass *old) {
    u32 chara = old->charId;
    s32 i;

    if (chara >= 0xCA) {
        return 0;
    }
    if (chara == 0x82 || chara == 0x84) {
        return 0;
    }
    if (chara == 0xC8) {
        chara = 0x82;
    } else if (chara == 0xCA) {
        chara = 0x84;
    }
    if (gPassChk->chara[chara].valid == 0) {
        return 0;
    }
    if ((u32)old->unk20 >= 3) {
        return 0;
    }
    for (i = 0; i < 7; i++) {
        if (old->item[i] != 0) {
            u32 item = old->item[i] - 1;

            if (item >= 0x15F) {
                return 0;
            }
            if (gPassChk->item[item].flags & 1) {
                return 0;
            }
            if (!(gPassChk->item[item].flags & 0x20)) {
                return 0;
            }
            if (item == 0xA2) {
                if (chara >= 0x24 && chara <= 0x26 || chara == 0x53 || chara == 0x76 || chara == 0x77) {
                    return 0;
                }
            }
            if (gPassChk->item[item].group != 0) {
                if (!(gPassChk->chara[chara].mask & (1 << (gPassChk->item[item].group - 1)))) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

/*
 * Whether a decoded password of this game is acceptable: a character id below 161, item ids below 350 with no
 * gap in the list, no item twice, no two items of the same type and kind, a type 2 item in the eighth place
 * only, no item the character's class refuses, and a total slot cost within the character's slots plus the
 * extra slots. An item of type 3 sets the extra slots to "up to seven" first (written back into *pass).
 */
s32 PassChk_IsValid(ZaChrPass *pass) {
    s32 fill = 0;
    s32 cost = 0;
    ZaItemEntry *itemTbl = (ZaItemEntry *)MPACK_AT(gCommonRes->data[2], 2);
    ZaChrEntry *charaTbl = (ZaChrEntry *)MPACK_AT(gCommonRes->data[2], 1);
    u32 chara = pass->charId;
    s32 extra;
    s32 i;
    s32 j;

    if (chara > 0xA0) {
        return 0;
    }
    extra = pass->extraSlots;
    if (charaTbl[chara].slots + extra > 7) {
        return 0;
    }
    for (i = 0; i < 8; i++) {
        if (pass->item[i] != 0) {
            u32 item = pass->item[i] - 1;

            if (itemTbl[item].type == 3) {
                fill = 1;
            }
            if (item >= 0x15E) {
                return 0;
            }
            for (j = 0; j < 7; j++) {
                u32 other = pass->item[j] - 1;

                if (j != i && item == other) {
                    return 0;
                }
            }
            for (j = i + 1; j < 7; j++) {
                u32 other = pass->item[j] - 1;

                if (other < 0x15E && j != i) {
                    if (itemTbl[item].type == itemTbl[other].type) {
                        if (itemTbl[item].kind == itemTbl[other].kind) {
                            return 0;
                        }
                    }
                }
            }
            if (i == 7) {
                if (itemTbl[item].type != 2) {
                    return 0;
                }
            } else if (itemTbl[item].type == 2) {
                return 0;
            }
            if (itemTbl[item].flags & 5) {
                return 0;
            }
            if ((itemTbl[item].flags & 0x18) != 0x18) {
                if ((charaTbl[chara].flags & 1) && (itemTbl[item].flags & 0x10)) {
                    return 0;
                }
                if ((charaTbl[chara].flags & 2) && (itemTbl[item].flags & 8)) {
                    return 0;
                }
            }
            cost += itemTbl[item].slots;
        } else if (i < 6 && pass->item[i + 1] != 0) {
            return 0;
        }
    }
    if (fill) {
        extra = pass->extraSlots = 7 - charaTbl[chara].slots;
    }
    if (extra + charaTbl[chara].slots < cost) {
        return 0;
    }
    return 1;
}
