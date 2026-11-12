#include "common.h"
#include "battle/view_b.h"
#include "sys/common.h"
#include "sys/save.h"

/*
 * ChrTbl / ItemSet, 0x260D20..0x2614B0: readers of the two tables of common file 4 (gCommonRes->data[2]): the
 * character entries (costume count, cost, level and experience) and the equippable items (what a set of items
 * adds up to, which characters may equip an item). The menu overlay calls nearly all of them; the battle calls
 * ItemSet_GetStats when a member is set up (BtlMember_ApplyItems). See battle/view_b.h.
 */

extern void *memset(void *dst, s32 c, u32 n);

/* A section of common file 4. */
#define CHRTBL_FILE ((ChrTblFile *)gCommonRes->data[2])
#define CHRTBL_SECTION(file, ofs) ((u8 *)(file) + (((file)->ofs >> 2) << 2))

/* Returns the character's number of costumes, and sets *costume to 0 if it is not one of them. */
s32 ChrTbl_WrapCostume(s32 chara, s32 *costume) {
    ChrTblFile *file = CHRTBL_FILE;
    ChrTblEntry *table = (ChrTblEntry *)CHRTBL_SECTION(file, charaOffset);

    if (costume != NULL && !(*costume < table[chara].costumes)) {
        *costume = 0;
    }
    return table[chara].costumes;
}

/* The character's cost. */
s32 ChrTbl_GetCost(s32 chara) {
    ChrTblFile *file = CHRTBL_FILE;
    ChrTblEntry *table = (ChrTblEntry *)CHRTBL_SECTION(file, charaOffset);

    return table[chara].cost;
}

/* The character's level: the table's base plus the saved level of a customisation slot or of a record. */
s32 ChrTbl_GetLevel(s32 chara, s32 slot, s32 fromRec) {
    s32 level;
    ChrTblFile *file;
    ChrTblEntry *table;

    if (fromRec != 0) {
        level = gSaveData->rec[slot].level;
    } else {
        level = gSaveData->custom[slot].level;
    }
    file = CHRTBL_FILE;
    table = (ChrTblEntry *)CHRTBL_SECTION(file, charaOffset);
    return table[chara].baseLevel + level;
}

/* The experience value of the level the customisation slot has reached. */
s32 ChrTbl_GetExp(s32 chara, s32 slot) {
    ChrTblFile *file = CHRTBL_FILE;
    ChrTblEntry *table = (ChrTblEntry *)CHRTBL_SECTION(file, charaOffset);

    return table[chara].exp[gSaveData->custom[slot].level];
}

/* The last experience value of the character's list. */
s32 ChrTbl_GetMaxExp(s32 chara) {
    ChrTblFile *file = CHRTBL_FILE;
    ChrTblEntry *table = (ChrTblEntry *)CHRTBL_SECTION(file, charaOffset);
    s32 i;
    s32 max = 0;

    for (i = 0; i < CHRTBL_EXP_COUNT; i++) {
        if (table[chara].exp[i] != 0) {
            max = table[chara].exp[i];
        } else {
            break;
        }
    }
    return max;
}

/* Adds up the slots the items of a set take. The items that do not fit in `capacity` are removed (from the
   first one that does not fit to the end). Returns the slots used; *used gets the number of items kept plus
   the slots left over. */
s32 ItemSet_Fit(u16 *ids, ItemTblEntry *table, s32 capacity, s32 *used) {
    s32 total = 0;
    s32 i;
    s32 j;
    s32 sum;
    u16 *p;

    if (used != NULL) {
        *used = 0;
    }
    for (i = 0; i < ITEMSET_USABLE; i++) {
        p = ids + i;
        if (*p != 0) {
            sum = total + table[*p - 1].slots;
            if (!(capacity < sum)) {
                total = sum;
                if (used != NULL) {
                    *used = i + 1;
                }
            } else {
                for (j = i; j < ITEMSET_USABLE; j++) {
                    ids[j] = 0;
                }
                break;
            }
        }
    }
    if (used != NULL) {
        *used += capacity - total;
    }
    return total;
}

/* What the menu shows for a set: out[0] = slots of the type 1 items (at most 7), out[1..4] = the four stat
   changes, each limited to +-20, divided by 5 and limited again to +-3. */
void ItemSet_GetBonus(u16 *ids, ItemTblEntry *table, s32 *out) {
    s32 i;
    s32 k;
    s32 idx;
    s32 div;

    memset(out, 0, 0x14);
    for (i = 0; i < ITEMSET_COUNT; i++) {
        if (ids[i] != 0) {
            idx = ids[i] - 1;
            if (table[idx].type == ITEMTBL_TYPE_SLOTS) {
                out[0] += table[idx].slots;
            }
            for (k = 0; k < ITEMSET_STAT_COUNT; k++) {
                out[1 + k] += table[idx].stat[k];
            }
        }
    }
    for (k = 0; k < 5; k++) {
        if (k == 0) {
            if (out[0] >= 8) {
                out[0] = 7;
            }
        } else if (out[k] > 20) {
            out[k] = 20;
        } else if (out[k] < -20) {
            out[k] = -20;
        }
    }
    div = 5;
    for (k = 0; k < ITEMSET_STAT_COUNT; k++) {
        out[1 + k] /= div;
        if (out[1 + k] > 3) {
            out[1 + k] = 3;
        } else if (out[1 + k] < -3) {
            out[1 + k] = -3;
        }
    }
}

/* What a member's items add up to in a battle: the four stat changes, the ability bits, and the AI type
   (from an AI item, else the character's own; 0 for a negative character id). */
void ItemSet_GetStats(u16 *ids, s32 *stats, s32 *ability, s32 chara) {
    ChrTblFile *file = CHRTBL_FILE;
    ItemTblEntry *items = (ItemTblEntry *)CHRTBL_SECTION(file, itemOffset);
    ChrTblEntry *charas = (ChrTblEntry *)CHRTBL_SECTION(file, charaOffset);
    s32 i;
    s32 k;
    s32 m;
    s32 id;
    s32 idx;

    memset(stats, 0, 0x14);
    memset(ability, 0, 0x10);
    for (i = 0; i < ITEMSET_COUNT; i++) {
        if (ids[i] != 0) {
            id = ids[i];
            idx = id - 1;
            if (items[idx].type == ITEMTBL_TYPE_AI) {
                stats[4] = id - ITEMTBL_AI_FIRST;
            } else {
                for (k = 0; k < ITEMSET_STAT_COUNT; k++) {
                    stats[k] += items[idx].stat[k];
                    for (m = 0; m < 4; m++) {
                        ability[m] |= items[idx].ability[m];
                    }
                }
            }
        }
    }
    if (stats[4] == 0) {
        if (chara < 0) {
            stats[4] = 0;
        } else {
            stats[4] = charas[chara].aiType;
        }
    }
}

/* The same for a list of item ids; without an AI item the AI type stays 0. */
void ItemSet_GetStatsList(s32 *ids, s32 count, s32 *stats, s32 *ability) {
    ChrTblFile *file = CHRTBL_FILE;
    ItemTblEntry *items = (ItemTblEntry *)CHRTBL_SECTION(file, itemOffset);
    s32 i;
    s32 k;
    s32 m;
    s32 id;
    s32 idx;

    memset(stats, 0, 0x14);
    memset(ability, 0, 0x10);
    for (i = 0; i < count; i++) {
        if (ids[i] != 0) {
            id = ids[i];
            idx = id - 1;
            if (items[idx].type == ITEMTBL_TYPE_AI) {
                stats[4] = id - ITEMTBL_AI_FIRST;
            } else {
                for (k = 0; k < ITEMSET_STAT_COUNT; k++) {
                    stats[k] += items[idx].stat[k];
                    for (m = 0; m < 4; m++) {
                        ability[m] |= items[idx].ability[m];
                    }
                }
            }
        }
    }
    if (stats[4] == 0) {
        stats[4] = 0;
    }
}

/* Whether the character may equip the item (index, not id). */
s32 ItemTbl_CanEquip(s32 item, s32 chara, ItemTblEntry *table) {
    u32 flags = table[item].flags;
    ChrTblFile *file;
    ChrTblEntry *charas;

    if (flags & (ITEMTBL_FLAG_NONE_A | ITEMTBL_FLAG_NONE_B)) {
        return 0;
    }
    file = CHRTBL_FILE;
    charas = (ChrTblEntry *)CHRTBL_SECTION(file, charaOffset);
    if (charas[chara].flags & CHRTBL_FLAG_0) {
        if (flags & ITEMTBL_FLAG_8) {
            return 1;
        }
        return 0;
    }
    if (flags & ITEMTBL_FLAG_10) {
        return 1;
    }
    return 0;
}

/* 2 for an item without flag 8, else whether it has flag 0x10. */
s32 ItemTbl_GetClass(s32 item, ItemTblEntry *table) {
    u32 flags = table[item].flags;

    if (flags & ITEMTBL_FLAG_8) {
        if (flags & ITEMTBL_FLAG_10) {
            return 1;
        }
        return 0;
    }
    return 2;
}
