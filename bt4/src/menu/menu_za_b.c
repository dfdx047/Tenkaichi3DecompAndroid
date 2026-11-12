#include "common.h"
#include "menu/menu_za.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/*
 * Menu overlay DBZP.BIN, 0x3AE648..0x3AEAB0: PassWin, the window that shows the password of a saved custom
 * character (opened from the character list of the Data Center, progress mode 55). A whole object: its `.data`
 * is gPassWin (0x3BC9B8) and the key table gPassKeys (0x3BC9C0); its `.rodata` is 0x3BE170..0x3BE240.
 *
 * The password itself is made by the main executable (ChrPass_Encode leaves the 34 characters in a static
 * buffer, ChrPass_GetText returns it); this file only draws it, one picture per character.
 */

/* The pictures of the password characters: four rows of fourteen (lower-case letters are four rows further). */
/* Overlay .data: the window's work (0x3BC9B8) and the key table (0x3BC9C0). */
PassWin *gPassWin = NULL;
char gPassKeys[4][14] = {"ABCDEFGHIJKLMN", "OPQRSTUVWXYZ", "#$%&@!?-+*()", "0123456789"};

/* Bit 1 of the C library's character class table: a lower-case letter. */
extern u8 _ctype_[];
#define PASSWIN_TOUPPER(c) __extension__({ s32 x_ = (c); (_ctype_ + 1)[x_] & 2 ? x_ - 'a' + 'A' : x_; })

#define PASSWIN_HOST "wii/test/main/DC/password/"

/* A section of the window's pack; `host` is the development file it was built from (unused: see DcPass_Init). */
static inline u8 *PassWin_Section(u32 *pack, s32 n, const char *host) {
    return MPACK_AT(pack, n);
}

/* Texture rectangle of one cell of a sheet. */
static inline void PassWin_SetCell(MFlashUv *uv, s32 col, s32 row, s32 w, s32 h) {
    uv->x0 = col * w;
    uv->x1 = uv->x0 + w;
    uv->y0 = row * h;
    uv->y1 = uv->y0 + h;
}

/* Sets the texture rectangle of a clip found by name. */
static inline void PassWin_SetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Builds the window from its pack (movie, three texture lists). */
void PassWin_Init(u32 *pack) {
    MTexRes *res = NULL;

    gPassWin = Heap_Alloc(sizeof(PassWin), 0x20, 0, 2);
    memset(gPassWin, 0, sizeof(PassWin));
    res = (MTexRes *)PassWin_Section(pack, 2, PASSWIN_HOST "PWD_password_Wii_.dbt");
    Res_RelocateOffsets(&res, res, res);
    gPassWin->tex[0] = MTEX(res, 0);
    gPassWin->tex[1] = MTEX(res, 1);
    gPassWin->tex[3] = MTEX(res, 2);
    res = (MTexRes *)PassWin_Section(pack, 3, PASSWIN_HOST "PWD_password_text_JP_Wii_.dbt");
    Res_RelocateOffsets(&res, res, res);
    gPassWin->tex[2] = MTEX(res, 0);
    Flash_Create(&gPassWin->flash[0], PassWin_Section(pack, 1, PASSWIN_HOST "password_Wii_.fod"), gPassWin->tex);
    Flash_Play(&gPassWin->flash[0], 1);
}

/* Frees the window. */
void PassWin_Term(void) {
    s32 i;

    for (i = 0; i < 1; i++) {
        Flash_Destroy(&gPassWin->flash[i]);
    }
    if (gPassWin != NULL) {
        Heap_Free(gPassWin);
        gPassWin = NULL;
    }
}

/* Finds a password character in the key table, ignoring case; leaves *col and *row alone when it is not there. */
void PassWin_FindKey(char c, s32 *col, s32 *row) {
    s32 i;
    s32 j;

    if (c >= 'a' && c <= 'z') {
        c = PASSWIN_TOUPPER(c);
    }
    for (j = 0; j < 4; j++) {
        for (i = 0; i < 14; i++) {
            if (c == gPassKeys[j][i]) {
                *col = i;
                *row = j;
                return;
            }
        }
    }
}

/* Advances and draws the window: sets the picture of each of the 34 password characters. */
void PassWin_Draw(void) {
#ifdef PORT
    Port_MenuPage(40); /* widescreen menus: this screen's page */
#endif
    MFlashUv uv;
    char name[0x100];
    s32 col;
    s32 row;
    char *text;
    s32 i;

    i = 0;
    text = ChrPass_GetText();
    Flash_Advance(&gPassWin->flash[0]);
    while (text[i] != '\0') {
        char c = text[i];

        PassWin_FindKey(c, &col, &row);
        if (c >= 'a' && c <= 'z') {
            PassWin_SetCell(&uv, col, row + 4, 0x20, 0x28);
        } else {
            PassWin_SetCell(&uv, col, row, 0x20, 0x28);
        }
        if (i <= 0x10) {
            i++;
            sprintf(name, "mc_input_pass_%d_%02d", 1, i);
        } else {
            sprintf(name, "mc_input_pass_%d_%02d", 2, i - 0x10);
            i++;
        }
        PassWin_SetUv(&gPassWin->flash[0], NULL, name, &uv);
        if (i > 0x22) {
            break;
        }
    }
    Flash_Draw(&gPassWin->flash[0]);
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Encodes the password of character `chara` with the eight items `items` and opens the window. */
void PassWin_Open(u16 *items, s32 chara) {
    ZaChrPass pass;
    s32 i;

    for (i = 0; i < 8; i++) {
        pass.item[i] = items[i];
    }
    pass.charId = chara;
    ChrPass_Encode(&pass);
    Flash_GotoLabel(&gPassWin->flash[0], "fl_password_in", 1);
}

/* The same with the number of extra item slots. */
void PassWin_OpenEx(u16 *items, s32 chara, s32 extraSlots) {
    ZaChrPass pass;
    s32 i;

    for (i = 0; i < 8; i++) {
        pass.item[i] = items[i];
    }
    pass.charId = chara;
    pass.extraSlots = extraSlots;
    ChrPass_Encode(&pass);
    Flash_GotoLabel(&gPassWin->flash[0], "fl_password_in", 1);
}

/* Closes the window. */
void PassWin_Close(void) {
    Flash_GotoLabel(&gPassWin->flash[0], "fl_password_out", 1);
}
