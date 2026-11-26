/* PC only: Z Warriors, the custom warrior editor (src/menu/port_zw.c), and the warriors it edits (port/src/plat_mods.c,
   the Custom Character System: a mod folder with a warrior.txt, made into a fighter of the mod grid). */
#ifndef PORT_ZW_H
#define PORT_ZW_H

#define ZW_COLOURS 48 /* colours a warrior keeps (plat_mods.c WARRIOR_COLOURS: the same) */
#define ZW_EDITS 32   /* pieces' edits a warrior keeps */
#define ZW_PATCH (-2000) /* an edit whose node is this: the holes patched (WP_PATCH of plat_warrior_parts.inc) */
#define ZW_DUP (-2002)   /* and this: a piece copied (WP_DUP): its piece word the copy's number k (the copy is piece
                            ZW_DUP_FIRST + k of its source's node), x and y stretch the source's node and piece */
#define ZW_DUP_FIRST 200
#define ZW_SHADE (-2001) /* and this: the hair laid over the hair shaded as (its stretch x) 1 its own, 2 the skin, 3 the outfit;
                            none: as the base hair (WP_SHADE) */

/* A warrior as warrior.txt says it (the same layout in plat_mods.c): fighters by their ids, -1 = not given (the moves
   fighter's own; the body's for render / hud). */
typedef struct PortWarrior {
    char folder[64];
    char name[40], title[40];
    s32 body, moves, effects, voice, style, finishers, render, hud;
    s32 skill[2], skillFrom[2]; /* Blast 1 slot s: whose, and which of that fighter's two (0 / 1) */
    s32 tech[3], techFrom[3];   /* Blast 2, Blast 2, Ultimate: whose, and which of that fighter's three (0..2) */
    s32 mod;                    /* its place in the mod library, -1 when its files are not made */
    s32 problem;                /* 1: making its files went wrong (Port_WarriorError says why) */
    s32 colours;                /* colour regions given a new colour: the region's key (Port_WarriorRegions), 0xBBGGRR */
    s32 colourKey[ZW_COLOURS];
    u32 colourRgb[ZW_COLOURS];
    s32 part[12];               /* hair, head, face, chest, waist, upper arms, forearms, hands, hips, shins, feet,
                                   accessories: whose model part (a part swap), -1 the body's, -2 none (taken away) */
    s32 size[8];                /* head, hair, hands, feet (size), arms, torso, legs (build): percent, <= 0 as they are */
    s32 layer[12];              /* another fighter's part layered over each slot, -1 none */
    s32 colourPart[ZW_COLOURS]; /* each colour's parts (1 << slot), 0 all: those parts get colours of their own */
    s32 stance;                 /* whose standing motions (the menus' pose, the battle idles), -1 the moves fighter's */
    s32 techFx[3], techFxFrom[3]; /* each technique's effects from another's (of the same effect type), -1 its own */
    s32 edits;                  /* the pieces' edits: {node (-1 - slot: a whole part), piece (-1 all), hidden,
                                   stretch x y z (percent), move x y z (hundredths), turn x y z (degrees)} */
    s32 edit[ZW_EDITS][12];
    char moveName[5][32];       /* Blast 1 x2, Blast 2 x2, Ultimate: a name of its own, "" the technique's */
    s32 techColour[3];          /* Blast 2 x2, Ultimate: its effects' colour, 0x1000000 | 0xBBGGRR, 0 their own */
    s32 auraColour;             /* the aura's colour, 0x1000000 | 0xBBGGRR, 0 its own */
    s32 partCostume[12], layerCostume[12], bodyCostume; /* always that costume (1..4) of the part's, the layer's, the
                                                          body's fighter; 0 the warrior's own costume's */
    s32 aura;                   /* whose aura (its kind: the flames, the light on the body, the ki blasts' trails), -1
                                   the moves fighter's; the ki blasts' looks are the effects fighter's */
#define ZW_ANIM_CATS 6
    s32 anim[ZW_ANIM_CATS];     /* whose animations of each kind: moving & flying, charging, melee, ki blasts, grabs &
                                   throws, guard & dodge (plat_mods.c kWarriorAnimCat); -1 the moves fighter's. What
                                   the moves do (combos, damage) stays the moves fighter's */
    s32 skillFx[2], skillFxFrom[2]; /* each Blast 1 skill's look from another Blast 1 skill (whose, which 0 / 1), -1 its
                                       own: only how it looks, it does what it did */
    char look[4][64];           /* other warriors' looks (their folders) worn as: [0] the damaged look of every costume,
                                   [1..3] costumes 2..4 (their damaged versions theirs); "" the warrior's own; "#own"
                                   (costumes 2..4) its own costume, kept in costumeN/warrior.txt of its folder */
    s32 damage;                 /* the damaged look of every costume: 0 the fighters' own damaged textures, 1 scuffed,
                                   2 battle-torn (wear painted over the clean textures), 3 stays clean */
    s32 xlayer[4], xslot[4], xcos[4]; /* more layers: each another fighter's part (slot xslot: 0 hair .. 11 accessories)
                                         over the look, -1 none; xcos its costume (1..4), 0 the warrior's own */
#define ZW_POSE_BONES 16
    s32 pose[ZW_POSE_BONES][3]; /* the stance maker: each bone (plat_mods.c kPoseNode) turned x, y, z degrees on top of the
                                   standing motions (they still breathe) */
    s32 shadeFrom;              /* whose shading and glow each part takes (a Super Saiyan's on a base form), -1 its own */
    s32 dynShade;               /* the Super Saiyan lighting (the model's flag 0x1000000: its colour mode, its light table):
                                   0 as its shading fighter has it (else the body), 1 on, 2 off */
    char formTo[4][64];         /* its transformations: each the warrior (folder) it turns into, "" none */
    s32 formCost[4], formKind[4]; /* each one's ki bars (blast stocks), and kind: 0 a power up, 1 back to base */
    s32 formAura[4];            /* each form's aura colour (0x1000000 | 0xBBGGRR; 0 as that warrior has it): saved into it */
    s32 camFrom[4], camSlot[4]; /* the cameras: Blast 2 (1), Blast 2 (2), the Ultimate's opening (in), the Ultimate's
                                   beam / hit cinematic (loop): whose technique's (fighter, its technique 0..2), -1 the
                                   technique's own */
    s32 labSet[5];              /* the Technique Lab: each technique's (Blast 1 x2, Blast 2 x2, Ultimate) stats set by hand */
    s32 lab[5][16];             /* ... their values (LAB_ of plat_mods.c) */
    s32 texShade[16][2];        /* shading by texture: {its picture hash (24 bits), 0x1000000 | 0xBBGGRR} */
    s32 skillColour[2];         /* a Blast 1 skill's effects' colour (0x1000000 | 0xBBGGRR), 0 their own */
    s32 anim2[3];               /* whose intro, victory and taunt motions; -1 the moves fighter's */
    s32 bodyScale;              /* the whole warrior's size in percent (50..250), 0 as it is */
    s32 glow[5];                /* each technique's body glow while it is done (0x1000000 | 0xBBGGRR), 0 none */
} PortWarrior;

/* A colour region (Port_WarriorRegions): its key, its colour, its size and what it is, the parts that draw it. */
#define ZW_REG_FACE 0x10000   /* the face's own (eyebrows, eye details) */
#define ZW_REG_SKIN 0x20000
#define ZW_REG_HAIR 0x40000
#define ZW_REG_UNSEEN 0x80000 /* drawn nowhere */
#define ZW_REG_BROWS 0x100000 /* the face's own, above the eyes */
#define ZW_REG_EYES 0x200000  /* the face's own, on the eyes */

extern s32 Port_WarriorCount(void);
extern void Port_WarriorGet(s32 i, PortWarrior *out);
extern void Port_WarriorError(s32 i, char *out, s32 size);
extern s32 Port_WarriorSave(s32 i, PortWarrior *in); /* i -1: a new one; its index afterwards, -1 not written */
extern s32 Port_WarriorPreview(PortWarrior *in); /* the warrior being edited made for the preview: its mod, -1 none */
/* The next Port_WarriorSave also writes costumes 2..4 of cos[4] (the bits of mask: 1 << k), and takes away the others */
extern void Port_WarriorCostumesNext(PortWarrior *cos, s32 mask);
/* Drawings on the textures (paint/ of the warrior's folder): the editor's begin with the warrior opened ("" a new one);
   one texture's colour numbers (and its 16 colours, or NULL: those it has) kept; one back as it was; a picture file put
   on one (w x h, fitted, its own 16 colours: into idx / pal too) */
extern void Port_PaintBegin(char *folder);
/* The mouse on the picture shown: out[0], out[1] where (0..10000 across / down), out[2] buttons (1 left, 2 right, 4
   middle), out[3] the wheel's turns since asked, out[4] 1 once it has moved at all */
extern void Port_MouseState(s32 *out);
extern s32 Port_AxisKey(void);     /* the stance maker's axis kept to: 0 none, 1 x, 2 y, 3 z (the keys X, Y, Z) */
extern void Port_AxisKeySet(s32 k);
extern s32 Port_PaintSave(s32 key, u8 *idx, s32 w, s32 h, u32 *pal);
extern void Port_PaintClear(s32 key);
/* A picture put on a texture as a sticker: read (the texture idx w x h and its colours kept under it; the picture's
   width | height << 16, 0 none), shown at a place (hundredths of texture pixels: middle, width; quarter turns) into idx,
   done */
extern s32 Port_DecalLoad(char *path, u8 *idx, s32 w, s32 h, u32 *pal);
extern void Port_DecalShow(s32 cx, s32 cy, s32 size, s32 turn, u8 *idx);
extern s32 Port_LayerCount(s32 key);                                                 /* a texture's picture layers */
extern s32 Port_LayerEdit(s32 key, s32 i, u8 *idx, s32 w, s32 h, u32 *pal, s32 *out); /* layer i taken up to be placed again */
extern s32 Port_LayerKeep(s32 key, s32 i, char *path, s32 x, s32 y, s32 w, s32 turn); /* the placed picture kept as a layer */
extern s32 Port_LayerRemove(s32 key, s32 i, u8 *idx, s32 w, s32 h, u32 *pal);        /* layer i taken off */
extern void Port_DecalEnd(void);
extern s32 Port_PaintUndo(s32 op, u8 *idx, s32 w, s32 h); /* 0 keep before a change, 1 undo, 2 redo, 3 forget; 1 done */
extern s32 Port_PaintImport(s32 key, char *path, s32 w, s32 h, u8 *idx, u32 *pal);
/* A picture of colour numbers (one byte a pixel, w x h) with 16 colours (RGBA, 0x80 opaque) as TBO_PICTURE: 1 done */
extern s32 Port_PictureIndexed(u8 *idx, s32 w, s32 h, u32 *pal);
/* Costume k (1..3) of the warrior in folder `folder` into *out: 1 there is one of its own */
extern s32 Port_WarriorCostumeRead(char *folder, s32 k, PortWarrior *out);
extern s32 Port_WarriorCopy(s32 i);
extern s32 Port_WarriorRemove(s32 i);
extern void Port_FighterMove(s32 chara, s32 slot, char *out, s32 size); /* slot 0..4: Blast 1 x2, Blast 2 x2, Ultimate */
extern s32 Port_WarriorRegions(PortWarrior *w, s32 *out, s32 max); /* {key, 0xBBGGRR, size | ZW_REG_*, parts} each */
extern s32 Port_WarriorZoneMask(PortWarrior *w, s32 key, s32 own, u8 *mask); /* region (key, own parts): mask[palette * 16 + colour]; palettes */
extern s32 Port_WarriorPaintLive(PortWarrior *w, void *tex); /* its colours painted into the model on screen's textures */
extern s32 Port_WarriorPaintFollow(void *expr); /* an expression texture file following that paint */
extern s32 Port_WarriorPreviewPaint(PortWarrior *w); /* only the colours changed: the preview painted again (0: make it again) */
extern s32 Port_WarriorTextures(PortWarrior *w, s32 *out, s32 max); /* {key, colour, w | h << 12 | kind << 24, parts} each */
extern s32 Port_WarriorTexturePicture(PortWarrior *w, s32 key, u8 *idx, u32 *pal); /* w | h << 16, 0 none */
#define ZW_TINT 0x40000000 /* a colour key of the eyedropper: picture << 5 | colour (16: all of the texture) */
extern void Port_ZwViewLog(s32 key, s32 chara, s32 mod); /* (the log: which model the screen shows) */
extern s32 Port_WarriorHudPhoto(s32 mod, s32 start, s32 from, s32 to); /* its gauge faces from a photo (corners x | y << 16) */
extern s32 Port_WarriorPortraitPhoto(s32 mod, s32 step, s32 from, s32 to); /* its render (kind 1) / small portrait (2) photo */
extern s32 Port_WarriorHasChipPhoto(s32 mod);
extern s32 Port_WarriorHasHudPhoto(s32 mod);

/* The Community Workshop (plat_mods.c, a local catalogue for now): an entry as Port_WsGet gives it. */
typedef struct PortWsEntry {
    char id[16];
    char name[40], title[40];
    char creator[32];
    char bio[160];
    char tags[64];
    char uploaded[16];
    s32 downloads, likes, version, liked, owned;
    s32 body, render;
    s32 built[8], builtN;
    char line[4][6][56]; /* OVERVIEW, LOOK, MOVES, FORMS */
    s32 mine; /* one of the player's own uploads */
} PortWsEntry;
extern s32 Port_WsScan(s32 sort);               /* 0 newest, 1 most downloaded, 2 featured, 3 my uploads: the count */
extern s32 Port_WsGet(s32 i, PortWsEntry *out);
extern s32 Port_WsUpload(char *folder, char *bio, char *tags);
extern s32 Port_WsDownload(s32 i);
extern s32 Port_WsLike(s32 i);
extern s32 Port_WsMine(s32 i);
extern s32 Port_WarriorLabBase(PortWarrior *w, s32 t, s32 *out); /* technique t's own numbers (the Lab's units), out[15] fx */
extern s32 Port_WarriorLabLike(s32 fx, s32 k);
extern s32 Port_WarriorTexShade(s32 key, s32 *out); /* a texture's shading ramps on the preview: {count, 4 colours each} */                    /* "behaves like" choices: fighter * 4 + technique, -1 */
/* Mod fighters (a Mod Manager mod, another warrior) as sources: fighter ids ZW_MODREF + n (plat_mods.c's table) */
#define ZW_MODREF 10000
extern s32 Port_ModRefCount(s32 skip);       /* how many (mod `skip` left out) */
extern s32 Port_ModRefAt(s32 j, s32 skip);   /* the j-th one's fighter id */
extern s32 Port_ModRefMod(s32 who);          /* its mod index, -1 */
extern s32 Port_ModRefChara(s32 who);        /* the roster fighter it stands on */
extern s32 Port_WsDelete(s32 i); /* the player's own upload taken down */
extern s32 Port_WarriorPieces(PortWarrior *w, s32 *out, s32 max); /* {node, piece, slot, vertices} each */
extern s32 Port_WarriorShapeLive(PortWarrior *w, void *mdl, s32 blinkNode, s32 blinkPiece); /* its edits made on the model on screen */
extern s32 Port_FighterMoveFx(s32 chara, s32 slot); /* a technique's effect type (slot 2..4), -1 none */
extern s32 Port_WarriorShadeScan(PortWarrior *w); /* reads every part's shading textures for Port_WarriorShades */
extern s32 Port_WarriorShades(s32 e, s32 *out); /* the preview's shading textures of entry e (2 * slot a part, + 1 its layer):
                                                 {what it stands for's count, its own count, 8 x 4 colours each}; e -1: each entry's own count */
extern s32 Port_WarriorShare(s32 i);   /* warrior i's code onto the clipboard (to give a friend) */
extern void Port_WarriorImportAsk(void); /* the clipboard read for a warrior's code ... */
extern s32 Port_WarriorImportPoll(void); /* ... and that warrior made: its index, -1 no code, -2 not read yet */
extern s32 Port_WarriorDraft(PortWarrior *w, s32 op); /* the unsaved warrior: 0 write, 1 read back (1 read), 2 remove */
extern void Port_SuiteVersion(char *out, s32 size);   /* "r<release> - <suite version>" */
extern s32 Port_MakeReport(char *folder);             /* a bug report zip in reports/ (the warrior's folder, "" none) */

/* The screen: run from the Duel mode (gPortDuelRole 3, menu_g_b.c) like the Mod Manager. */
s32 PortZw_Run(void);

#endif
