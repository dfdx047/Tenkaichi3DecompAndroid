/* PC only: what the game's code uses of the mod library (port/src/plat_mods.c; the layouts there are the same).
   Pointers passed to these functions are the game's own (4 bytes); the library takes them as 32-bit values. */
#ifndef PORT_MODS_H
#define PORT_MODS_H

#define PORT_REF_MOD 0x1000 /* a transformation target that is a mod: + its index */

typedef struct PortModInfo {
    char name[64];
    char target[64];  /* the fighter it replaces, or for a new one its template */
    char white[40];
    char yellow[40];
    s32 chara;        /* replaced fighter / template */
    s32 on;
    s32 kinds;        /* bit 0..3 costumes, 4..7 damaged, 8 animations, 9 effects, 10 portrait */
    s32 costumes;
    s32 large;
    s32 raw;
    s32 conflict;
    s32 files;
    s32 isNew;        /* a fighter of its own on the mod grid */
    s32 skin;         /* its place there, -1 = not on it */
    s32 chip[3];      /* the square of its portrait that is its face: x, y, size (0 = the usual one) */
    s32 named;
    s32 art[4];       /* a portrait made in the manager: set, x, y, zoom in percent */
    s32 rival;        /* another mod that is on and wants the same files of the same fighter, -1 none */
    s32 wins;         /* (on, with a rival) its files are the ones used */
    s32 problem;  /* what is wrong with its files: 1 a pak that is not one, 2 too large, 3 empty, 4 bad portrait, 5 several fighters */
    s32 priority; /* (on) its place in the order mods win in, 1 = first; 0 off */
    s32 onCount;  /* how many mods are on */
    s32 charas;   /* how many fighters it has files of (a folder looked at: more than one is a batch) */
    s32 artH;     /* the portrait's height in percent (art[3] its width) */
    s32 charaList[8]; /* the fighters it has files of (a batch: each one) */
} PortModInfo;

/* A fighter's transformations and fusions (the same layout in port/src/plat_mods.c): every field of its parameter
   block the game reads for them. */
typedef struct PortTrans {
    s32 set;          /* the transformations are set in the manager (0: the fighter's own, as its file has them) */
    s32 target[4];    /* -1, a character id, or PORT_REF_MOD + mod index */
    s32 cost[4];      /* blast stocks */
    s32 next;         /* the slot plain R3 takes, -1 none */
    s32 type[4];      /* 0 power up, 1 back to base, 2 great ape, 3 absorption, 4 machine merge */
    s32 effect[4];    /* 0..11 (3: needs the moon) */
    s32 assist[4];    /* the other fighter of its scene, -1 none */
    s32 fresh[4];     /* 1: the new form starts with its clothes whole */
    s32 fuseSet;      /* the fusions are set in the manager */
    s32 fuse[3];      /* the result, a character id, -1 none */
    s32 fuseCost[3];
    s32 fuseType[3];  /* 1 fusion dance, 2 potara */
    s32 fuseLead[3];  /* the partner its scene shows */
    s32 partner[3][4]; /* whom it fuses with, -1 none */
    s32 adLow;
    s32 aura[4];      /* (not used: kept for the layout) */
    s32 fuseAura[3];  /* (not used) */
    s32 fuseAnim[3];  /* the scene's motions: 0 by the kind, 1 Goku & Vegeta's dance, 2 Goten & Trunks', 3 SSJ4, 4 Potara,
                         5 the fighters' own */
} PortTrans;

typedef struct PortDirEntry {
    char name[96];
    s32 isDir;
    s32 isMod;
    s32 chara;
    s32 kinds;
} PortDirEntry;

extern s32 Port_ModCount(void);
extern void Port_ModGetInfo(s32 i, PortModInfo *out);
extern s32 Port_ModToggle(s32 i); /* the first mod it switched off (same fighter, same files), -1 none */
extern void Port_ModPreview(s32 i); /* -1 none, -2 the folder the browser looked at */
extern void Port_ModRescan(void);
extern s32 Port_ModRemove(s32 i);
extern void Port_ModSet(s32 i, s32 field, s32 value); /* 0 new, 1 template, 2 target, 3..5 chip x / y / size */
extern void Port_ModSetText(s32 i, s32 which, char *str); /* 0 white, 1 yellow */
extern s32 Port_ModChip(s32 i, void *out, s32 max);
extern s32 Port_ModMoves(s32 i, void *out, s32 max);   /* the names of its blasts: lines of 40 bytes; how many */
extern s32 Port_ModImport(char *path);
extern s32 Port_ModPortrait(s32 i, void *out, s32 max);
extern void PortMods_PatchSelect(void *names, void *forms, u32 *chips); /* src/menu/port_modmgr2.c */
extern s32 Port_ModImportAll(char *path); /* every character mod in a folder (or the folder itself); how many */
extern s32 Port_ModImportInfo(s32 *skipped); /* after an import: the first mod added (-1 none, -2 the library's own folder); how many were in it already */
extern s32 Port_ModImportStart(char *path, s32 all); /* adds on a thread of its own: 0 one runs already */
extern s32 Port_ModImportPoll(s32 *progress); /* files done, of how many, what (0 looks 1 unpacks 2 copies); 0 none 1 runs 2 just done */
extern s32 Port_ModImportResult(void);         /* how many the last import added */
extern s32 Port_ModsChanged(void);             /* 1: the mods folder changed from outside since it was read */
extern void Port_ModAbout(s32 i, char *out);   /* author (32 bytes), version (16), description (200) */
extern void Port_ModPriority(s32 i, s32 dir);  /* -1: wins over one more, +1: one less */
extern void Port_ModShowFolder(s32 i);         /* the mod's folder (-1 the mods folder) in the PC's file explorer */
extern s32 Port_RemovedList(void);             /* the mods in mods_removed: how many */
extern void Port_RemovedName(s32 i, char *out); /* 64 bytes */
extern s32 Port_RemovedRestore(s32 i);         /* back into the library: its index there, -1 not done */
extern void Port_PickPicture(void);      /* the open dialog: the picture chosen comes as a dropped one (Port_DropTake) */
extern s32 Port_DropTake(char *path);     /* 1: a folder or file was dropped on the window, its path (700 bytes) */
extern s32 Port_ModListName(s32 list, s32 i, char *out); /* names of <data>/modmgr lists: 1 moves, 2 items, 3 maps, 4 songs; 48 bytes */
extern s32 Port_ModTestStage(void);       /* the stage of the manager's test fight */
extern s32 Port_ModIsWarrior(s32 i);        /* 1: the mod is a warrior of Z Warriors */
extern s32 PortModGrid_Pick(s32 padNo, s32 *chara, s32 *code);
extern u16 *PortMods_SkinName(s32 code, s32 which, s32 side, void *table); /* a picked new fighter's name, NULL = the template's */
extern void Port_CharaName(s32 chara, char *out, s32 size);
extern s32 Port_ReadFile(char *path, void *buf, s32 max);
extern s32 Port_SkinCount(void);
extern s32 Port_SkinMod(s32 skin);
extern s32 Port_CostumeOf(s32 code);
extern s32 Port_SkinFile(s32 chara, s32 code, s32 kind, s32 variant);
extern s32 Port_CharaOfFile(s32 id);
extern void Port_RosterMove(s32 skin, s32 dir);
extern void Port_TransGet(s32 who, PortTrans *out);
extern void Port_TransSet(s32 who, PortTrans *in);
extern void Port_TransApplyFile(s32 modelFile, void *param);
extern s32 Port_DirOpen(char *path);
extern void Port_DirKind(s32 kind);    /* what is listed besides folders: 0 a mod's files, 1 pictures */
extern s32 Port_ModArtBase(s32 i, void *out, s32 max);   /* the portrait's picture whole in the frame, as an unpacked portrait file */
extern s32 Port_ModArtSet(s32 i, s32 x, s32 y, s32 zoom); /* makes the portrait: the picture there, that large */
extern s32 Port_ModArtSet2(s32 i, s32 x, s32 y, s32 zoom, s32 zoomY); /* that wide and that high */
extern void Port_ModArtReset(s32 i);
extern s32 Port_ModArtFit(s32 i);                    /* its picture filling the frame */
extern s32 Port_ModArtSource(s32 i, char *path);       /* a picture file to make it from */
extern s32 Port_DirFilter(char *text); /* keeps the entries with `text` in their name; returns the count */
extern void Port_EscQuit(s32 on);      /* 0: Esc does not close the program, the screen takes it (Port_EscTake) */
extern s32 Port_EscTake(void);         /* 1 once per press of Esc while that is so */
extern void Port_TextMode(s32 on);     /* the PC keyboard is typed on: its keys are text, not pad buttons */
extern s32 Port_TextRead(void);        /* the next character typed (8 backspace, 13 enter, 27 escape), 0 = none */
extern void Port_DirGet(s32 i, PortDirEntry *out);
extern s32 Port_DirLook(char *path, PortModInfo *out);

/* A costume number as the game's own tables know it: a new fighter's costume code (8 + skin * 4 + costume) carries
   which skin it is; everything that is not the file loader sees the plain costume. */
#define PORT_COSTUME(c) ((c) >= 8 ? (((c) - 8) & 3) : (c))
#define PORT_SKIN_CODE(skin, costume) (8 + (skin) * 4 + (costume))

#endif
