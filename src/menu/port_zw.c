#include "common.h"
#include "menu/menu_a.h"
#include "sys/pad.h"

#ifdef PORT
/*
 * PC only: Z Warriors (main menu item 13), the editor of the Custom Character System's warriors. A warrior is a mod
 * folder with a warrior.txt that names a fighter for each part of it (port/src/plat_mods.c makes the mod's files from
 * them and it fights as a new fighter of the mod grid); this screen lists them, edits them and writes them back.
 *
 * The look is its own (zwarriors_design/mockups/chosen: Other World, Grand Kai's planet, violet and gold, King Kai in
 * the talk box), its pictures tbo/ui/zw_*.png (port/build/ml/zw_art.py); the drawing helpers are the Mod Manager's
 * kit (port_mmkit.c: places in the mockups' pixels, 1376 x 768, lines of letters, the pad's buttons, round faces) and
 * its model viewer (Mm_View*). Screens: the library (S1); the editor in two tabs, WARRIOR (name, look, colours,
 * portraits) and MOVES (moveset, Blast 1 / 2, Ultimate, finishers), with the warrior as it is being edited on the
 * platform (Port_WarriorPreview makes it a moment after each change, no saving needed); the picker of fighters and
 * their techniques (S10); the colours (S6); the name keyboard (S13); the small portrait's square; and pop-up questions.
 * Everything moves: panels slide in, the cursor glides, the chosen row pulses, King Kai and the platform bob.
 */
#include "menu/port_modui.h"
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "menu/port_zw.h"
#include "sys/math3d.h"

extern void Job_Run(void);
extern void *Sprite_Unpack(void *src, void *dst, s32 *size);
extern void Mm_PromptReset(void);
extern f32 sinf(f32 x);
extern f32 Mm_Ease(f32 t);

#define ZW_MAX 64
#define ZW_ART_FILE 0x16800 /* (the Mod Manager's portrait buffers: port_modmgr2.c) */
#define ZW_ART_RES 0x20800
#define ZW_PREVIEW_REST 20  /* frames after a change before the preview is made */
#define ZW_PAINT_REST 8     /* frames after a colour is changed before the preview is painted with it */
#define ZW_REGS 40
#define ZW_PIECES 640
#define ZW_RECENT 8
#define PK_COLS 9 /* (the picker's columns) */
#define CO_Y 158.0f /* (the colours screen's first line) */
#define CO_PITCH 42.0f

enum { ZW_LIBRARY, ZW_EDIT, ZW_PICK, ZW_NAME, ZW_CROP, ZW_COLOUR, ZW_WORK, ZW_PHOTO, ZW_FXCOL, ZW_SHADEPK, ZW_TEXTURE, ZW_POSE,
       ZW_WSHOP, ZW_WSDETAIL, ZW_WSUPLOAD, ZW_LAB };
#define ZW_UNDO 12          /* the changes kept to step back through */
#define ZW_DRAFT_EVERY 600  /* frames between two writes of the unsaved warrior */
enum { ASK_NONE, ASK_REMOVE, ASK_LEAVE, ASK_COSON, ASK_COSOFF, ASK_WSDEL };

/* The editor's rows, in two tabs: a section heading, or a part of the warrior. */
enum {
    ROW_SECTION,
    ROW_NAME, ROW_TITLE,
    ROW_BODY, ROW_VOICE,
    ROW_PART, ROW_PART_END = ROW_PART + 11, /* a model part's slot (plat_warrior_parts.inc): hair .. accessories */
    ROW_WHOLEHEAD, ROW_OUTFIT,              /* hair, head and face at once; chest down to the accessories at once */
    ROW_SIZE, ROW_SIZE_END = ROW_SIZE + 6,  /* head, hair, hands, feet (size), arms, torso, legs (build): percent */
    ROW_SURPRISE, ROW_RESETPARTS, ROW_WORKSHOP, ROW_STANCE,
    ROW_FX0, ROW_FX1, ROW_FX2,              /* a technique's effects (from another technique of the same type) */
    ROW_COLOURS,
    ROW_RENDER, ROW_CHIP, ROW_HUD,
    ROW_MOVES, ROW_EFFECTS, ROW_STYLE,
    ROW_SKILL0, ROW_SKILL1,
    ROW_TECH0, ROW_TECH1, ROW_TECH2,
    ROW_FINISHERS,
    ROW_SHADING, /* each part's shading: whose shades, colours of your own */
    ROW_AURA,    /* whose aura (its kind), apart from the ki blasts' looks (ROW_EFFECTS) */
    ROW_ANIM, ROW_ANIM_END = ROW_ANIM + ZW_ANIM_CATS + 3 - 1, /* whose animations of each kind (then the intro, victory, taunt) */
    ROW_SFX0, ROW_SFX1, /* a Blast 1 skill's look (from another Blast 1 skill) */
    ROW_LOOK, ROW_LOOK_END = ROW_LOOK + 3, /* another warrior's look worn as the damaged look, costumes 2..4 */
    ROW_COSTUMES, /* costumes on (four outfits of its own) or off (one) */
    ROW_TEXTURES, /* drawing on the textures, a picture put on one: a screen of its own */
    ROW_POSE,     /* the stance maker: the bones of the stance turned, a screen of its own */
    ROW_SHADEFROM, /* whose shading and glow every part takes (a transformed fighter's) */
    ROW_DYNSHADE,  /* the Super Saiyan lighting: as the shading fighter, on, off */
    ROW_FORM, ROW_FORM_END = ROW_FORM + 3,         /* a transformation: the warrior it turns into */
    ROW_FORMCOST, ROW_FORMCOST_END = ROW_FORMCOST + 3, /* its ki bars */
    ROW_FORMKIND, ROW_FORMKIND_END = ROW_FORMKIND + 3, /* power up, back to base */
    ROW_FORMAURA, ROW_FORMAURA_END = ROW_FORMAURA + 3, /* its aura's colour (the form's warrior's) */
    ROW_FORMKEY, ROW_FORMKEY_END = ROW_FORMKEY + 3,    /* (shown only) the buttons that transform into it */
    ROW_XL, ROW_XL_END = ROW_XL + 3, /* more layers: another fighter's part over the look (several hairs, a scouter) */
    ROW_COSEDIT,  /* the costume being edited */
    ROW_DAMAGE,   /* the damaged look: the fighters' own, wear painted on, clean */
    ROW_CAM, ROW_CAM_END = ROW_CAM + 3, /* the cameras: Blast 2 (1), (2), the Ultimate's in, its loop (another technique's) */
    ROW_PREVIEW, /* the portraits for real: the character select, a test fight (its HUD) */
    ROW_LAB, ROW_LAB_END = ROW_LAB + 4, /* the Technique Lab of Blast 1 (1), (2), Blast 2 (1), (2), the Ultimate */
    ROW_LABLIKE, /* (the picker of Fine-tune's "Copy rules from": any Blast 2 or Ultimate but a rush) */
    ROW_BODYSCALE, /* the whole warrior's size */
    ROW_GLOW, ROW_GLOW_END = ROW_GLOW + 4, /* each technique's body glow (Kaio-ken like) */
};
/* A warrior's motions of kind k: 0..5 its kinds, 6..8 the intro, victory and taunt (kept apart at the struct's end). */
static s32 *Zw_AnimOf(PortWarrior *w, s32 k) {
    return k < ZW_ANIM_CATS ? &w->anim[k] : &w->anim2[k - ZW_ANIM_CATS];
}

typedef struct ZwRow {
    s32 kind;
    char *label;
} ZwRow;
static const ZwRow kRowsWarrior[] = {
    {ROW_SECTION, "WARRIOR"}, {ROW_NAME, "Name"}, {ROW_TITLE, "Title"},
    {ROW_SECTION, "LOOK"}, {ROW_BODY, "Body"}, {ROW_STANCE, "Stance"}, {ROW_POSE, "Custom pose"}, {ROW_VOICE, "Voice"},
    {ROW_SECTION, "PORTRAITS"}, {ROW_RENDER, "Portrait"}, {ROW_CHIP, "Small portrait"}, {ROW_HUD, "Health bar face"}, {ROW_PREVIEW, "Preview"},
    {ROW_SECTION, "COSTUMES"}, {ROW_COSTUMES, "Costumes"}, {ROW_COSEDIT, "Edit costume"},
    {ROW_SECTION, "BATTLE DAMAGE"}, {ROW_DAMAGE, "Damaged look"},
};
static const ZwRow kRowsMoves[] = {
    {ROW_SECTION, "FIGHTING"}, {ROW_MOVES, "Melee moves"}, {ROW_FINISHERS, "Finishers"}, {ROW_STYLE, "Stats"},
    {ROW_EFFECTS, "Ki blasts"}, {ROW_AURA, "Aura"},
    {ROW_SECTION, "ANIMATIONS"}, {ROW_ANIM + 0, "Moving & flying"}, {ROW_ANIM + 1, "Charging"},
    {ROW_ANIM + 3, "Firing ki blasts"}, {ROW_ANIM + 4, "Grabs & throws"}, {ROW_ANIM + 5, "Guard & dodge"},
    {ROW_ANIM + 6, "Intro"}, {ROW_ANIM + 7, "Victory"}, {ROW_ANIM + 8, "Taunt"},
    {ROW_SECTION, "BLAST 1"}, {ROW_SKILL0, "Blast 1 (1)"}, {ROW_SFX0, "  Look"}, {ROW_LAB + 0, "  Fine-tune"}, {ROW_GLOW + 0, "  Body glow"},
    {ROW_SKILL1, "Blast 1 (2)"}, {ROW_SFX1, "  Look"}, {ROW_LAB + 1, "  Fine-tune"}, {ROW_GLOW + 1, "  Body glow"},
    {ROW_SECTION, "BLAST 2"}, {ROW_TECH0, "Blast 2 (1)"}, {ROW_FX0, "  Effects"}, {ROW_CAM + 0, "  Camera"}, {ROW_LAB + 2, "  Fine-tune"},
    {ROW_GLOW + 2, "  Body glow"},
    {ROW_TECH1, "Blast 2 (2)"}, {ROW_FX1, "  Effects"}, {ROW_CAM + 1, "  Camera"}, {ROW_LAB + 3, "  Fine-tune"}, {ROW_GLOW + 3, "  Body glow"},
    {ROW_SECTION, "ULTIMATE"}, {ROW_TECH2, "Ultimate"}, {ROW_FX2, "  Effects"}, {ROW_CAM + 2, "  Opening camera"},
    {ROW_CAM + 3, "  Finish camera"}, {ROW_LAB + 4, "  Fine-tune"}, {ROW_GLOW + 4, "  Body glow"},
};
static const ZwRow kRowsForms[] = {
    {ROW_SECTION, "TRANSFORMATIONS"},
    {ROW_FORM + 0, "Form 1"}, {ROW_FORMKEY + 0, "  Input"}, {ROW_FORMCOST + 0, "  Ki cost"}, {ROW_FORMKIND + 0, "  Type"}, {ROW_FORMAURA + 0, "  Aura colour"},
    {ROW_FORM + 1, "Form 2"}, {ROW_FORMKEY + 1, "  Input"}, {ROW_FORMCOST + 1, "  Ki cost"}, {ROW_FORMKIND + 1, "  Type"}, {ROW_FORMAURA + 1, "  Aura colour"},
    {ROW_FORM + 2, "Form 3"}, {ROW_FORMKEY + 2, "  Input"}, {ROW_FORMCOST + 2, "  Ki cost"}, {ROW_FORMKIND + 2, "  Type"}, {ROW_FORMAURA + 2, "  Aura colour"},
    {ROW_FORM + 3, "Form 4"}, {ROW_FORMKEY + 3, "  Input"}, {ROW_FORMCOST + 3, "  Ki cost"}, {ROW_FORMKIND + 3, "  Type"}, {ROW_FORMAURA + 3, "  Aura colour"},
};
static const ZwRow kRowsParts[] = {
    {ROW_SECTION, "TOOLS"}, {ROW_WORKSHOP, "Sculpt"}, {ROW_COLOURS, "Colours"}, {ROW_SHADING, "Shading"}, {ROW_TEXTURES, "Paint textures"}, {ROW_SHADEFROM, "Borrowed shading"}, {ROW_DYNSHADE, "Super Saiyan glow"}, {ROW_SURPRISE, "Randomize"}, {ROW_WHOLEHEAD, "Whole head"}, {ROW_OUTFIT, "Whole outfit"},
    {ROW_RESETPARTS, "Reset all parts"},
    {ROW_SECTION, "EXTRA LAYERS"}, {ROW_XL + 0, "Extra layer 1"}, {ROW_XL + 1, "Extra layer 2"}, {ROW_XL + 2, "Extra layer 3"},
    {ROW_XL + 3, "Extra layer 4"},
    {ROW_SECTION, "BUILD"}, {ROW_SIZE + 5, "Torso build"}, {ROW_SIZE + 4, "Arm build"}, {ROW_SIZE + 6, "Leg build"},
    {ROW_SECTION, "HEAD"}, {ROW_PART + 0, "Hair"}, {ROW_PART + 1, "Head"}, {ROW_PART + 2, "Face"}, {ROW_SIZE + 0, "Head size"},
    {ROW_SIZE + 1, "Hair size"},
    {ROW_SECTION, "UPPER BODY"}, {ROW_PART + 3, "Chest"}, {ROW_PART + 4, "Waist"}, {ROW_PART + 5, "Upper arms"},
    {ROW_PART + 6, "Forearms"}, {ROW_PART + 7, "Hands"}, {ROW_SIZE + 2, "Hand size"},
    {ROW_SECTION, "LOWER BODY"}, {ROW_PART + 8, "Hips & thighs"}, {ROW_PART + 9, "Shins"}, {ROW_PART + 10, "Feet"},
    {ROW_SIZE + 3, "Foot size"},
    {ROW_SECTION, "EXTRAS"}, {ROW_PART + 11, "Accessories"},
};
#define ROWS_W ((s32)(sizeof(kRowsWarrior) / sizeof(kRowsWarrior[0])))
#define ROWS_P ((s32)(sizeof(kRowsParts) / sizeof(kRowsParts[0])))
#define IS_PART(k) ((k) >= ROW_PART && (k) <= ROW_PART_END)
#define IS_SIZE(k) ((k) >= ROW_SIZE && (k) <= ROW_SIZE_END)
#define IS_LOOKPART(k) (IS_PART(k) || (k) == ROW_WHOLEHEAD || (k) == ROW_OUTFIT)
#define ROWS_M ((s32)(sizeof(kRowsMoves) / sizeof(kRowsMoves[0])))

typedef struct Zw {
    s32 mode, back;
    s32 frames, modeT, rest;
    s32 leaving;
    /* the library */
    s32 count;
    PortWarrior list[ZW_MAX];
    s32 cur, top;            /* 0 = "Create a New Warrior", 1.. the warriors */
    f32 curY;                /* (the cursor's bar, gliding to its row) */
    /* a pop-up question */
    s32 ask, askCur, askT;
    /* the editor */
    s32 at;                  /* the warrior edited: its index, -1 a new one */
    PortWarrior w;
    s32 dirty;
    s32 tab, row[4], rowTop[4], tabT; /* tabs: WARRIOR, PARTS, MOVES, FORMS */
    f32 rowY;
    s32 saving;              /* frames until the save runs (one frame shows "saving") */
    s32 viewGen;             /* (each new preview or save reads the model again) */
    s32 previewDue;          /* frames until the preview is made, -1 none due */
    s32 previewMod;          /* the preview's mod, -1 none */
    s32 previewing;          /* a frame that shows "making" before the preview runs */
    char note[96];
    s32 noteT;
    /* the picker */
    const ZwRow *pickRow;
    s32 pickCur, pickTop, pickSlot, pickStage; /* the face under the cursor; the technique; 0 faces 1 techniques */
    s32 pickLayer;                             /* 1: a part's layer is being chosen */
    s32 pickCos;                               /* the costume of the fighter being tried on: 1..4, 0 the warrior's own */
    s32 pickRest, tried;                       /* frames the cursor has rested; 1 once something was tried on */
    /* the keyboard */
    s32 nameWhich;
    char text[160];
    s32 keyCur, keyPage;
    /* the colours: the regions ({key, 0xBBGGRR, size | ZW_REG_*, parts | own parts << 16}), the list (headings < 0),
       the picker (hue, strength, lightness; its row, swatch, "only on" choice; the warrior before it), the colours
       used lately, what the model on the screen was last painted as */
    s32 reg[ZW_REGS * 4];
    u32 regWas[ZW_REGS];       /* each texture's main colour as the fighters have it (before any painting) */
    s32 regs, coList[ZW_REGS + 4], coRows, coCur, coTop, colourStage;
    f32 zoneY;
    PortWarrior pkBack;
    s32 pkRow, pkSwatch, pkOnly;
    f32 pkH, pkS, pkV;
    s32 pkC; /* a whole texture's contrast in percent (100: as it is) */
    u32 recent[ZW_RECENT];
    s32 recents;
    s32 paintKey;
    void *paintTex;
    /* the workshop: the pieces ({node, piece, slot, vertices}), the list (headings, whole parts, pieces), the cursor,
       the sliders; what the model on the screen was last shaped as */
    s32 piece[ZW_PIECES * 4];
    s32 pieces, wsList[ZW_PIECES + 30], wsRows, wsCur, wsTop, wsStage, wsRow;
    s32 shapeKey, shapeDirty, shapeEdited;
    void *shapeMdl;
    /* the viewer's camera: the model's turn, how far up or down it is seen from (-1 the feet, 1 the head), the zoom;
       the T-pose */
    f32 camYaw, camV, camZoom, camShownV, camShownZoom;
    s32 tpose;
    /* the hovered colour zone, lit on the model */
    s32 glowKey, glowOwn, glowView, glowOn, glowState, glowPals;
    void *glowTex;
    u8 glowMask[64 * 16];
    u32 glowSave[64 * 16];
    /* the small portrait */
    s32 crop[3];
    u8 *artFile, *artRes;
    MmTex *artTex;
    char artPending[700]; /* a picture for the render, used once the warrior is saved */
    /* the HUD face photo: the camera on the face, the frames since the shutter (0 none) */
    f32 phYaw, phZoom, phFocus, phTilt, phPan;
    s32 phShot;
    s32 phKind;  /* the photo: 0 the HUD face, 1 the render (cut out), 2 the small portrait */
    s32 phStep;  /* (the render's two photos: 2 the one on black, 4 the one on white) */
    s32 phWhite; /* (the background is white: the render's second photo) */
    s32 phAt;    /* (the frame the second photo was asked for after) */
    s32 exitTo;  /* left for: 1 a test fight, 2 the character select (then back here) */
    /* the Community Workshop */
    PortWsEntry *wk;           /* the catalogue as scanned (WK_MAX) */
    s32 wkN, wkSort, wkCur, wkTop, wkTab, wkDl, wkDlOk;
    s32 wkUpAt, wkUpCur, wkUpTags; /* the upload: the warrior, the row, the tags chosen (bits) */
    char wkBio[160];
    MmRes *wkChip[64];         /* each entry's small portrait from its own photo (NULL: its face) */
    MmRes *refChip[64];        /* a mod fighter's chip (Port_ModChip), by its place in the table */
    s32 labT, labRow, labTop, labHold; /* the Technique Lab: the technique (0..4), the stat row, the list's top, held frames */
    s32 labBase[16];           /* the technique's own numbers (Port_WarriorLabBase) */
    u8 refChipTried[64];
    s32 pkMods;                /* the picker's mod fighters after the roster (0: this row takes none) */
    s32 pkRef[64];             /* (their fighter ids) */
    s32 wkPrevMod;             /* the entry built for the stage (mods/_zw_wspreview), -1 none */
    char wkPrevId[16];         /* (which entry that is) */
    s32 phPose;  /* the render's pose: 0 the stance, else an entry of kPhPose (a motion of its animation file) */
    u8 *phMotion; /* (that motion, unpacked) */
    s32 phHold;  /* (that moment held still) */
    s32 paintDue; /* frames until the preview is painted again with the colours (-1 none): Port_WarriorPreviewPaint */
    /* the eyedropper: the texture of row picRow as a picture (its colours' numbers), its colours now and before */
    u8 *pic;
    u32 picPal[32];
    s32 picW, picH, picX, picY, picRow, picFrom, picHold, picGen;
    s32 picDraw, picBrush, picSize, picImport, picDirty; /* (drawing on it: on, the colour, the brush's size 0..2, a picture
                                                             file asked for, a stroke not kept yet) */
    s32 paintWait;
    /* undo: the warrior as it was after each change (hist[histPos] now; hist[ZW_UNDO] last frame's), frames since the
       last change */
    PortWarrior *hist;
    s32 histN, histPos, histQuiet;
    s32 lockMask;   /* parts (1 << slot) and builds (1 << (16 + size)) "Surprise me!" leaves as they are */
    s32 fxTarget;   /* the colour being picked: techniques 0..2, 3 the aura */
    s32 fxBack;     /* (its colour before) */
    s32 importWait; /* (frames left for the clipboard's warrior code to arrive) */
    s32 shRow;      /* the shading screen: its row (what is shaded) */
    s32 shFocus, shSet; /* (1: on its settings, shSet the one) */
    s32 shByTex, shTexRow, shTexTop; /* the shading screen by texture: on, the texture row (of coList), the list's top */
    s32 shTexZ;     /* (the texture whose shadow colour is being picked: fxTarget 5) */
    s32 shE, shT;   /* (the shade being coloured by hand: its entry, its texture) */
    s32 shInfo[66]; /* (Port_WarriorShades) */
    s32 draftT;     /* frames until the unsaved warrior is written again */
    char version[48];
    /* costumes: each one's look (cos[k]: only its look's fields count), the one being edited (its look is in w), the
       ones of its own (1 << k; costume 1 always) */
    PortWarrior *cos;
    s32 cosCur, cosHave;
} Zw;
static Zw *sZw;

/* ---------------------------------------------------------------------------------------------- helpers */
static f32 Zw_In(s32 t, s32 len) { /* 0..1 eased over `len` frames */
    f32 k = (f32)t / (f32)len;

    return Mm_Ease(k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k);
}

static f32 Zw_Beat(s32 period) { /* -1..1 */
    return sinf((f32)(sZw->frames % period) * 6.2831853f / (f32)period);
}

static char *Zw_Name(s32 chara) {
    static char names[4][48];
    static s32 n;
    char *out = names[n++ & 3];

    if (chara < 0) {
        out[0] = 0;
    } else {
        Port_CharaName(chara, out, 48);
    }
    return out;
}

static void Zw_Note(char *s) {
    Ps_Copy(sZw->note, s, sizeof(sZw->note));
    sZw->noteT = 150;
}

static void Glow_Off(void);
static void Co_Load(void);
static void Co_PaintTick(void);
static void Ws_Load(void);
static void Ws_ShapeTick(void);
static s32 Sh_BlinkNode(void);

static void Zw_SetMode(s32 mode) {
    Glow_Off();
    sZw->glowKey = 0;
    sZw->back = sZw->mode;
    sZw->mode = mode;
    sZw->modeT = 0;
    sZw->rest = 0;
    Port_TextMode(mode == ZW_NAME);
}

static void Zw_Reload(void) {
    s32 i;

    Port_ModRescan();
    sZw->count = Port_WarriorCount();
    if (sZw->count > ZW_MAX) {
        sZw->count = ZW_MAX;
    }
    for (i = 0; i < sZw->count; i++) {
        Port_WarriorGet(i, &sZw->list[i]);
    }
    if (sZw->cur > sZw->count) {
        sZw->cur = sZw->count;
    }
    sZw->previewMod = -1; /* (the library's order changed: the preview is made again when it is needed) */
}

/* A change to the warrior being edited: it is not saved, and the preview is made again a moment later. */
static void Zw_Changed(void) {
    sZw->dirty = 1;
    sZw->previewDue = ZW_PREVIEW_REST;
}

/* The slots a parts row sets: [*first, *first + *n). */
static void Zw_Slots(s32 kind, s32 *first, s32 *n) {
    *first = kind == ROW_WHOLEHEAD ? 0 : kind == ROW_OUTFIT ? 3 : kind - ROW_PART;
    *n = kind == ROW_WHOLEHEAD ? 3 : kind == ROW_OUTFIT ? 9 : 1;
}

/* Whose a parts row is: the one fighter of all its slots (the body when they are its own), -2 mixed. */
static s32 Zw_PartWho(PortWarrior *w, s32 kind) {
    s32 first, n, i;
    s32 who = -3;

    Zw_Slots(kind, &first, &n);
    for (i = first; i < first + n; i++) {
        s32 c = w->part[i] >= 0 ? w->part[i] : w->part[i] == -2 ? -4 : w->body; /* (-4: taken away) */

        if (who != -3 && c != who) {
            return -2;
        }
        who = c;
    }
    return who;
}

/* The fighter a part comes from, as the game makes it: the part's own, else who it falls back to. */
static s32 Zw_Who(PortWarrior *w, s32 kind) {
    switch (kind) {
    case ROW_BODY: return w->body;
    case ROW_STANCE: return w->stance >= 0 ? w->stance : w->body;
    case ROW_FX0: case ROW_FX1: case ROW_FX2: return w->techFx[kind - ROW_FX0] >= 0 ? w->techFx[kind - ROW_FX0] : Zw_Who(w, ROW_TECH0 + kind - ROW_FX0);
    case ROW_CAM + 0: case ROW_CAM + 1: case ROW_CAM + 2: case ROW_CAM + 3:
        return w->camFrom[kind - ROW_CAM] >= 0 ? w->camFrom[kind - ROW_CAM] : Zw_Who(w, ROW_TECH0 + (kind - ROW_CAM < 2 ? kind - ROW_CAM : 2));
    case ROW_VOICE: return w->voice >= 0 ? w->voice : w->moves;
    case ROW_RENDER: return w->render >= 0 ? w->render : w->body;
    case ROW_HUD: return w->hud >= 0 ? w->hud : w->body;
    case ROW_MOVES: return w->moves;
    case ROW_EFFECTS: return w->effects >= 0 ? w->effects : w->moves;
    case ROW_AURA: return w->aura >= 0 ? w->aura : w->moves;
    case ROW_ANIM + 0: case ROW_ANIM + 1: case ROW_ANIM + 2: case ROW_ANIM + 3: case ROW_ANIM + 4: case ROW_ANIM + 5:
    case ROW_ANIM + 6: case ROW_ANIM + 7: case ROW_ANIM + 8:
        return *Zw_AnimOf(w, kind - ROW_ANIM) >= 0 ? *Zw_AnimOf(w, kind - ROW_ANIM) : w->moves;
    case ROW_XL + 0: case ROW_XL + 1: case ROW_XL + 2: case ROW_XL + 3: return w->xlayer[kind - ROW_XL] >= 0 ? w->xlayer[kind - ROW_XL] : -1;
    case ROW_SHADEFROM: return w->shadeFrom >= 0 ? w->shadeFrom : w->body;
    case ROW_SFX0: case ROW_SFX1: return w->skillFx[kind - ROW_SFX0] >= 0 ? w->skillFx[kind - ROW_SFX0] : Zw_Who(w, ROW_SKILL0 + kind - ROW_SFX0);
    case ROW_STYLE: return w->style >= 0 ? w->style : w->moves;
    case ROW_SKILL0: case ROW_SKILL1: return w->skill[kind - ROW_SKILL0] >= 0 ? w->skill[kind - ROW_SKILL0] : w->moves;
    case ROW_TECH0: case ROW_TECH1: case ROW_TECH2: return w->tech[kind - ROW_TECH0] >= 0 ? w->tech[kind - ROW_TECH0] : w->moves;
    case ROW_LABLIKE: return (w->labSet[sZw->labT] >> 12) & 1 ? w->lab[sZw->labT][12] / 4 : w->moves;
    case ROW_FINISHERS: return w->finishers >= 0 ? w->finishers : w->moves;
    }
    if (IS_LOOKPART(kind)) {
        s32 who = Zw_PartWho(w, kind);

        return who >= 0 ? who : -1;
    }
    return -1;
}

static u32 sZwSeed = 12345;

static u32 Zw_Rand(void) {
    sZwSeed = sZwSeed * 1103515245u + 12345u + (u32)sZw->frames;
    return (sZwSeed >> 8) & 0xFFFFFF;
}

static s32 Zw_IsMove(s32 kind) {
    return kind >= ROW_SKILL0 && kind <= ROW_TECH2;
}

/* Whether a part is the moves fighter's own (not chosen apart). */
static s32 Zw_Own(PortWarrior *w, s32 kind) {
    switch (kind) {
    case ROW_SKILL0: case ROW_SKILL1: return w->skill[kind - ROW_SKILL0] < 0;
    case ROW_TECH0: case ROW_TECH1: case ROW_TECH2: return w->tech[kind - ROW_TECH0] < 0;
    case ROW_FINISHERS: return w->finishers < 0;
    case ROW_EFFECTS: return w->effects < 0;
    case ROW_AURA: return w->aura < 0;
    case ROW_ANIM + 0: case ROW_ANIM + 1: case ROW_ANIM + 2: case ROW_ANIM + 3: case ROW_ANIM + 4: case ROW_ANIM + 5:
    case ROW_ANIM + 6: case ROW_ANIM + 7: case ROW_ANIM + 8:
        return *Zw_AnimOf(w, kind - ROW_ANIM) < 0;
    case ROW_SFX0: case ROW_SFX1: return w->skillFx[kind - ROW_SFX0] < 0;
    case ROW_STYLE: return w->style < 0;
    case ROW_VOICE: return w->voice < 0;
    case ROW_STANCE: return w->stance < 0;
    case ROW_FX0: case ROW_FX1: case ROW_FX2: return w->techFx[kind - ROW_FX0] < 0;
    case ROW_CAM + 0: case ROW_CAM + 1: case ROW_CAM + 2: case ROW_CAM + 3: return w->camFrom[kind - ROW_CAM] < 0;
    }
    if (IS_LOOKPART(kind)) {
        s32 first, n, i;

        Zw_Slots(kind, &first, &n);
        for (i = first; i < first + n; i++) {
            if (w->part[i] >= 0) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

/* A move row's technique: 0, 1 the Blast 1 skills, 2..4 the techniques, -1 not a move row. */
static s32 Zw_MoveIndex(s32 kind) {
    return kind == ROW_SKILL0 || kind == ROW_SKILL1 ? kind - ROW_SKILL0 : kind >= ROW_TECH0 && kind <= ROW_TECH2 ? 2 + kind - ROW_TECH0 : -1;
}

/* The technique colours (0xBBGGRR; the first: its own). */
static const u32 kTechColour[11] = {0, 0x2020E8, 0x1C78F8, 0x30E0F8, 0x30D040, 0xE8E040, 0xF06020, 0xE030A0, 0xC060F8, 0xF0F0F0, 0x202020};
static char *kTechColourName[11] = {"own colours", "Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Pink", "White", "Black"};

static void Co_ToHsv(u32 rgb, f32 *h, f32 *s, f32 *v);

static s32 Zw_TechColourIndex(PortWarrior *w, s32 t) {
    s32 i;

    for (i = 1; i < 11; i++) {
        if (w->techColour[t] == (s32)(0x1000000 | kTechColour[i])) {
            return i;
        }
    }
    return 0;
}

/* The name of the technique a move row has (its own name, or its fighter's move of the slot it is taken from). */
static char *Zw_MoveName(PortWarrior *w, s32 kind) {
    static char out[2][48];
    static s32 n;
    char *s = out[n++ & 1];
    s32 slot = -1;

    if (Zw_MoveIndex(kind) >= 0 && w->moveName[Zw_MoveIndex(kind)][0]) {
        Ps_Copy(s, w->moveName[Zw_MoveIndex(kind)], 48);
        return s;
    }

    if (kind == ROW_SKILL0 || kind == ROW_SKILL1) {
        s32 k = kind - ROW_SKILL0;
        slot = w->skill[k] >= 0 ? w->skillFrom[k] : k;
    } else if (kind >= ROW_TECH0 && kind <= ROW_TECH2) {
        s32 k = kind - ROW_TECH0;
        slot = 2 + (w->tech[k] >= 0 ? w->techFrom[k] : k);
    }
    s[0] = 0;
    if (slot >= 0) {
        Port_FighterMove(Zw_Who(w, kind), slot, s, 48);
    }
    if (s[0] == 0) { /* (a copy, not a ternary with a literal: this compiler's casts trip on that here) */
        Ps_Copy(s, "(none)", 48);
    }
    return s;
}

static const ZwRow *Zw_Rows(s32 tab, s32 *count) {
    if (tab == 3) {
        *count = (s32)(sizeof(kRowsForms) / sizeof(kRowsForms[0]));
        return kRowsForms;
    }
    *count = tab == 2 ? ROWS_M : tab == 1 ? ROWS_P : ROWS_W;
    return tab == 2 ? kRowsMoves : tab == 1 ? kRowsParts : kRowsWarrior;
}

/* The editor's sections folded (a bit per section of each tab; kept while the game runs): a folded section shows its
   heading only. Headings take the cursor: X folds or opens them. */
static s32 sZwFold[4];

static s32 Zw_SecOf(const ZwRow *rows, s32 r) { /* (the section row r is in: its number in the tab) */
    s32 i, sec = -1;

    for (i = 0; i <= r; i++) {
        sec += rows[i].kind == ROW_SECTION;
    }
    return sec;
}

static s32 Zw_RowShown(const ZwRow *rows, s32 tab, s32 r) {
    s32 sec;

    if (rows[r].kind == ROW_SECTION) {
        return 1;
    }
    sec = Zw_SecOf(rows, r);
    return sec < 0 || sec > 31 || !((sZwFold[tab] >> sec) & 1);
}

static s32 Zw_VisPos(const ZwRow *rows, s32 tab, s32 r) { /* (its line among the rows shown) */
    s32 i, v = 0;

    for (i = 0; i < r; i++) {
        v += Zw_RowShown(rows, tab, i);
    }
    return v;
}

static const ZwRow *Zw_CurRow(void) {
    s32 n;
    const ZwRow *rows = Zw_Rows(sZw->tab, &n);

    return &rows[sZw->row[sZw->tab]];
}

/* What a row of the editor shows as its value. */
static char *Zw_Value(s32 kind) {
    static char out[80];
    PortWarrior *w = &sZw->w;
    PortModInfo info;

    switch (kind) {
    case ROW_NAME: return w->name[0] ? w->name : "(none)";
    case ROW_TITLE: return w->title[0] ? w->title : "(none)";
    case ROW_COLOURS:
        if (w->colours <= 0) {
            return "Original";
        }
        sprintf(out, "%d painted", w->colours);
        return out;
    case ROW_RENDER:
        if (w->mod >= 0) {
            Port_ModGetInfo(w->mod, &info);
            if (info.art[0]) {
                return "Your picture";
            }
        }
        break;
    case ROW_CHIP:
        if (w->mod < 0) {
            return "(save first)";
        }
        if (Port_WarriorHasChipPhoto(w->mod)) {
            return "Your photo";
        }
        Port_ModGetInfo(w->mod, &info);
        return info.chip[2] > 0 ? "Cropped" : "Automatic";
    case ROW_PREVIEW:
        return w->mod >= 0 ? "Character select / Test fight" : "(save first)";
    case ROW_HUD:
        if (w->mod >= 0 && Port_WarriorHasHudPhoto(w->mod)) {
            return "Your photo";
        }
        break;
    case ROW_SKILL0: case ROW_SKILL1:
        if (w->skillColour[kind - ROW_SKILL0] > 0) {
            sprintf(out, "%s (own colour)", Zw_MoveName(w, kind));
            return out;
        }
        return Zw_MoveName(w, kind);
    case ROW_TECH0: case ROW_TECH1: case ROW_TECH2:
        if (Zw_TechColourIndex(w, kind - ROW_TECH0) > 0) {
            sprintf(out, "%s (%s)", Zw_MoveName(w, kind), kTechColourName[Zw_TechColourIndex(w, kind - ROW_TECH0)]);
            return out;
        }
        return Zw_MoveName(w, kind);
    }
    if (kind == ROW_SURPRISE || kind == ROW_RESETPARTS) {
        return kind == ROW_SURPRISE ? "Random parts" : "";
    }
    if (kind >= ROW_FORM && kind <= ROW_FORM_END) {
        s32 i;
        const char *to = w->formTo[kind - ROW_FORM];

        if (to[0] == 0) {
            return "None";
        }
        for (i = 0; i < sZw->count; i++) {
            if (strcmp(sZw->list[i].folder, to) == 0) {
                sprintf(out, "Turns into %s", sZw->list[i].name[0] ? sZw->list[i].name : sZw->list[i].folder);
                return out;
            }
        }
        Ps_Copy(out, "(a warrior that is gone)", 48);
        return out;
    }
    if (kind >= ROW_FORMCOST && kind <= ROW_FORMCOST_END) {
        if (w->formTo[kind - ROW_FORMCOST][0] == 0) {
            return "-";
        }
        sprintf(out, "%d", (int)w->formCost[kind - ROW_FORMCOST]);
        return out;
    }
    if (kind >= ROW_FORMKEY && kind <= ROW_FORMKEY_END) { /* (the game's: R3 with left / up / right / down; R3 alone when one) */
        static char *dirs[4] = {"R3 + Left", "R3 + Up", "R3 + Right", "R3 + Down"};
        s32 f = kind - ROW_FORMKEY, n = 0, k2;

        if (w->formTo[f][0] == 0) {
            return "-";
        }
        for (k2 = 0; k2 < 4; k2++) {
            n += w->formTo[k2][0] != 0;
        }
        if (n == 1) {
            sprintf(out, "R3 (or %s)", dirs[f]);
            return out;
        }
        Ps_Copy(out, dirs[f], 48);
        return out;
    }
    if (kind >= ROW_FORMAURA && kind <= ROW_FORMAURA_END) {
        s32 f = kind - ROW_FORMAURA, i;

        if (w->formTo[f][0] == 0) {
            return "-";
        }
        for (i = 1; i < 11; i++) {
            if (w->formAura[f] == (s32)(0x1000000 | kTechColour[i])) {
                Ps_Copy(out, kTechColourName[i], 48);
                return out;
            }
        }
        if (w->formAura[f] != 0) {
            return "Your colour";
        }
        return "Default";
    }
    if (kind >= ROW_FORMKIND && kind <= ROW_FORMKIND_END) {
        if (w->formTo[kind - ROW_FORMKIND][0] == 0) {
            return "-";
        }
        return w->formKind[kind - ROW_FORMKIND] == 1 ? "Back to base form" : "Power up";
    }
    if (kind == ROW_DYNSHADE) {
        if (w->dynShade == 1) {
            return "On";
        }
        if (w->dynShade == 2) {
            return "Off";
        }
        return w->shadeFrom >= 0 ? "From borrowed shading" : "Default";
    }
    if (kind == ROW_SHADEFROM) {
        if (w->shadeFrom < 0) {
            return "Default";
        }
        sprintf(out, "From %s", Zw_Name(w->shadeFrom));
        return out;
    }
    if (kind == ROW_POSE) {
        s32 b, n = 0;

        for (b = 0; b < ZW_POSE_BONES; b++) {
            n += w->pose[b][0] != 0 || w->pose[b][1] != 0 || w->pose[b][2] != 0;
        }
        if (n == 0) {
            return "Default pose";
        }
        sprintf(out, "%d bones posed", (int)n);
        return out;
    }
    if (kind == ROW_TEXTURES) {
        return "Draw or add pictures";
    }
    if (kind >= ROW_LAB && kind <= ROW_LAB_END) {
        s32 b, n = 0;

        for (b = 0; b < 16; b++) {
            n += (w->labSet[kind - ROW_LAB] >> b) & 1;
        }
        if (n == 0) {
            return "Default";
        }
        sprintf(out, n == 1 ? "%d stat changed" : "%d stats changed", (int)n);
        return out;
    }
    if (kind == ROW_SHADING) {
        s32 i, n = 0;

        for (i = 0; i < w->edits; i++) {
            n += w->edit[i][0] == ZW_SHADE;
        }
        if (n == 0) {
            return "Default";
        }
        sprintf(out, n == 1 ? "%d custom part" : "%d custom parts", (int)n);
        return out;
    }
    if (kind == ROW_WORKSHOP) {
        if (w->edits > 0) {
            s32 i, patched = 0;

            s32 shade = 0;

            for (i = 0; i < w->edits; i++) {
                patched |= w->edit[i][0] == ZW_PATCH;
                shade += w->edit[i][0] == ZW_SHADE;
            }
            if (w->edits - patched - shade == 0) {
                return patched ? "Holes patched" : "Stretch, hide or move pieces";
            }
            sprintf(out, patched ? "%d pieces edited, patched" : "%d pieces edited", (int)(w->edits - patched - shade));
            return out;
        }
        return "Stretch, hide or move pieces";
    }
    if (kind == ROW_STANCE && w->stance < 0) {
        return "Default";
    }
    if (kind == ROW_COSTUMES) {
        return sZw->cosHave > 1 ? "On (four outfits)" : "Off (one outfit)";
    }
    if (kind >= ROW_XL && kind <= ROW_XL_END) {
        static char *slots[13] = {"Hair", "Head", "Face", "Chest", "Waist", "Upper arms", "Forearms", "Hands", "Hips", "Shins", "Feet", "Accessories",
                                  "Tail"};
        s32 x = kind - ROW_XL;

        if (w->xlayer[x] < 0) {
            return "None";
        }
        sprintf(out, "%s: %s", slots[w->xslot[x] >= 0 && w->xslot[x] < 13 ? w->xslot[x] : 0], Zw_Name(w->xlayer[x]));
        return out;
    }
    if (kind == ROW_COSEDIT) {
        if (sZw->cosHave <= 1) {
            return "Costume 1 (costumes off)";
        }
        sprintf(out, "Costume %d of 4", (int)sZw->cosCur + 1);
        return out;
    }
    if (kind == ROW_DAMAGE) {
        static char *names[4] = {"The fighters' own", "Scuffed", "Battle-torn", "Stays clean"};

        return names[w->damage > 0 && w->damage < 4 ? w->damage : 0];
    }
    if (kind >= ROW_LOOK && kind <= ROW_LOOK_END) { /* (another warrior's look: its name) */
        s32 i;

        if (w->look[kind - ROW_LOOK][0] == 0) {
            return kind == ROW_LOOK ? "Default damage" : "Default costume";
        }
        for (i = 0; i < sZw->count; i++) {
            if (strcmp(sZw->list[i].folder, w->look[kind - ROW_LOOK]) == 0) {
                sprintf(out, "Looks like %s", sZw->list[i].name[0] ? sZw->list[i].name : sZw->list[i].folder);
                return out;
            }
        }
        Ps_Copy(out, "(a warrior that is gone)", 48);
        return out;
    }
    if (kind == ROW_SFX0 || kind == ROW_SFX1) {
        s32 k = kind - ROW_SFX0;

        if (w->skillFx[k] < 0) {
            return "Default";
        }
        Port_FighterMove(w->skillFx[k], w->skillFxFrom[k], out, 48);
        sprintf(out + Ps_Len(out), " (%s)", Zw_Name(w->skillFx[k]));
        return out;
    }
    if ((kind >= ROW_CAM && kind <= ROW_CAM_END)) {
        s32 k = kind - ROW_CAM;

        if (w->camFrom[k] < 0) {
            return "Default";
        }
        Port_FighterMove(w->camFrom[k], 2 + w->camSlot[k], out, 48);
        sprintf(out + Ps_Len(out), " (%s)", Zw_Name(w->camFrom[k]));
        return out;
    }
    if (kind >= ROW_FX0 && kind <= ROW_FX2) {
        s32 k = kind - ROW_FX0;

        if (w->techFx[k] < 0) {
            return "Default";
        }
        Port_FighterMove(w->techFx[k], 2 + w->techFxFrom[k], out, 48);
        sprintf(out + Ps_Len(out), " (%s)", Zw_Name(w->techFx[k]));
        return out;
    }
    if (kind >= ROW_GLOW && kind <= ROW_GLOW_END) {
        s32 g = w->glow[kind - ROW_GLOW];

        if (g <= 0) {
            return "None";
        }
        sprintf(out, "%s  (strength %d%%)", (g & 0xFF) > 0xC0 && ((g >> 8) & 0xFF) < 0x60 && ((g >> 16) & 0xFF) < 0x60 ? "Kaio-ken red" : "Your colour",
                (int)((((g & 0xFF) > ((g >> 8) & 0xFF) ? ((g & 0xFF) > ((g >> 16) & 0xFF) ? (g & 0xFF) : ((g >> 16) & 0xFF))
                                                         : (((g >> 8) & 0xFF) > ((g >> 16) & 0xFF) ? ((g >> 8) & 0xFF) : ((g >> 16) & 0xFF)))) * 100 / 255));
        return out;
    }
    if (kind == ROW_BODYSCALE) {
        sprintf(out, "%d%%%s", (int)(w->bodyScale > 0 ? w->bodyScale : 100), w->bodyScale > 130 ? "  (a giant!)" : "");
        return out;
    }
    if (IS_SIZE(kind)) {
        s32 v = w->size[kind - ROW_SIZE];

        sprintf(out, sZw->lockMask & (1 << (16 + kind - ROW_SIZE)) ? "%d%%  (locked)" : "%d%%", v > 0 ? v : 100);
        return out;
    }
    if (IS_LOOKPART(kind)) {
        s32 first, n;
        char *main = Zw_Own(w, kind) ? "Own" : Zw_PartWho(w, kind) == -2 ? "Mixed" : Zw_PartWho(w, kind) == -4 ? "None" : Zw_Name(Zw_Who(w, kind));

        Zw_Slots(kind, &first, &n);
        if (n == 1 && w->layer[first] >= 0) { /* (a layer on it) */
            sprintf(out, "%s + %s", main, Zw_Name(w->layer[first]));
            return out;
        }
        if (IS_PART(kind) && (sZw->lockMask & (1 << (kind - ROW_PART)))) { /* ("Surprise me!" leaves it) */
            sprintf(out, "%s  (locked)", Zw_Own(w, kind) ? "The body's own" : main);
            return out;
        }
        return Zw_Own(w, kind) ? "The body's own" : main;
    }
    sprintf(out, "%s", Zw_Name(Zw_Who(w, kind)));
    if (kind == ROW_AURA && w->auraColour > 0) { /* (its aura's colour) */
        s32 i;

        for (i = 1; i < 11; i++) {
            if (w->auraColour == (s32)(0x1000000 | kTechColour[i])) {
                sprintf(out + Ps_Len(out), " (%s aura)", kTechColourName[i]);
            }
        }
    }
    return out;
}

/* ---------------------------------------------------------------------------------------------- drawing */
static void Zw_Title(char *s) {
    f32 pop = Zw_In(sZw->modeT, 14);
    f32 spring = 1.0f + 0.12f * sinf(pop * 3.14159f) * (1.0f - pop * 0.6f);

    Mx_Fit(ZW_EMBLEM, 82.0f, 62.0f + 2.0f * Zw_Beat(150), 104.0f * spring, 0x80);
    Mx_Heading(146.0f - 30.0f * (1.0f - pop), 62.0f, s, 54.0f * spring, 760.0f, 0);
}

/* King Kai standing by: his two pictures (cheering, a hand on his chin) in turn, one fading into the other every few
   seconds, bobbing a little; `first` says which one a screen starts on. */
static void Zw_Host(f32 cx, f32 cy, f32 h, s32 first) {
    s32 t = sZw->frames % 480;
    s32 a = first ? ZW_KK_THINK : ZW_KK_THUMB;
    s32 b = first ? ZW_KK_THUMB : ZW_KK_THINK;
    f32 k;
    f32 in = Zw_In(sZw->modeT - 6, 16);

    if (t >= 240) {
        s32 c = a;
        a = b;
        b = c;
        t -= 240;
    }
    cy += 5.0f * Zw_Beat(110) + 40.0f * (1.0f - in);
    k = t < 210 ? 0.0f : (f32)(t - 210) / 30.0f;
    if (k < 1.0f) {
        Mx_Fit(a, cx, cy, h, (s32)((1.0f - k) * 128.0f * in));
    }
    if (k > 0.0f) {
        Mx_Fit(b, cx, cy, h, (s32)(k * 128.0f * in));
    }
}

/* King Kai's talk box at the foot: his portrait, his line, the pad's buttons for what can be done. */
typedef struct ZwPrompt {
    s32 kind;
    char *label;
} ZwPrompt;
static ZwPrompt sPrompt[8];
static s32 sPromptCount;

static void Zw_Prompt(s32 kind, char *label) {
    if (sPromptCount < 8) {
        sPrompt[sPromptCount].kind = kind;
        sPrompt[sPromptCount].label = label;
        sPromptCount++;
    }
}

static void Zw_Talk(char *line) {
    f32 x = 1320.0f;
    s32 i;

    Mx_Box(ZW_DIALOG, 30.0f, 640.0f, 1346.0f, 752.0f, 26.0f, 0x80);
    Mx_Fit(ZW_PORTRAIT, 92.0f, 690.0f + 2.0f * Zw_Beat(90), 86.0f, 0x80);
    if (sZw->noteT > 0) {
        line = sZw->note;
    }
    { /* (a line wider than the box: in two or three smaller lines - one that long is not drawn at all) */
        static char last[200];
        static char wrapped[3][64];
        static s32 lines;

        if (line != NULL && strcmp(line, last) != 0) {
            Ps_Copy(last, line, sizeof(last));
            /* (a line too long to measure measures 0: its length says it then) */
            lines = Mx_TextW(line, 26.0f) > 1150.0f || Ps_Len(line) > 84 ? Mx_Wrap(line, 22.0f, 1150.0f, wrapped, 2) : 0;
            if (lines >= 2 && (Mx_TextW(line, 22.0f) > 2200.0f || Ps_Len(line) > 150)) {
                lines = Mx_Wrap(line, 19.0f, 1150.0f, wrapped, 3);
            }
        }
        if (lines >= 3) {
            Mx_Say(150.0f, 662.0f, wrapped[0], 19.0f, 1150.0f, MX_WHITE, 0);
            Mx_Say(150.0f, 686.0f, wrapped[1], 19.0f, 1150.0f, MX_WHITE, 0);
            Mx_Say(150.0f, 710.0f, wrapped[2], 19.0f, 760.0f, MX_WHITE, 0);
        } else if (lines == 2) {
            Mx_Say(150.0f, 666.0f, wrapped[0], 22.0f, 1150.0f, MX_WHITE, 0);
            Mx_Say(150.0f, 696.0f, wrapped[1], 22.0f, 1150.0f, MX_WHITE, 0);
        } else {
            Mx_Say(150.0f, 678.0f, line, 26.0f, 1150.0f, MX_WHITE, 0);
        }
    }
    for (i = sPromptCount - 1; i >= 0; i--) { /* (from the right) */
        f32 w = Mx_TextW(sPrompt[i].label, 18.0f);

        x -= w;
        Mx_Text(x, 728.0f, sPrompt[i].label, 18.0f, 0.0f, MX_LAV, 0);
        x -= Mx_ButtonW(sPrompt[i].kind, 24.0f) + 6.0f;
        Mx_Button(sPrompt[i].kind, x, 728.0f, 24.0f);
        x -= 22.0f;
    }
}

/* A round face in its gold ring (lit: the glowing one, pulsing). */
/* A mod fighter's chip (NULL: none; its fighter's face then). */
static MmTex *Zw_RefChip(s32 chara) {
    s32 r = chara - ZW_MODREF;

    if (r < 0 || r >= 64) {
        return NULL;
    }
    if (!sZw->refChipTried[r]) {
        s32 mod = Port_ModRefMod(chara);
        MmRes *res;

        sZw->refChipTried[r] = 1;
        res = mod >= 0 ? Heap_Alloc(0x1600, 0x40, 0, 2) : NULL;
        if (res != NULL && Port_ModChip(mod, res, 0x1600) > 0) {
            Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
            sZw->refChip[r] = res;
        } else if (res != NULL) {
            Heap_Free(res);
        }
    }
    return sZw->refChip[r] != NULL ? sZw->refChip[r]->tex : NULL;
}

static s32 sZwChipTag; /* (the picker's faces: a mod fighter's is tagged) */
static s32 Pk_Side(void);

static void Zw_Chip(s32 chara, f32 cx, f32 cy, f32 size, s32 lit) {
    f32 s = lit ? size * (1.08f + 0.04f * Zw_Beat(40)) : size;

    if (chara >= ZW_MODREF) {
        MmTex *t = Zw_RefChip(chara);

        if (t != NULL) {
            Mx_TexRound(t, cx, cy, s * 0.8f, 0x80);
        } else {
            Mx_FaceRound(Port_ModRefChara(chara), cx, cy, s * 0.8f, 0x80);
        }
    } else if (chara >= 0) {
        Mx_FaceRound(chara, cx, cy, s * 0.8f, 0x80);
    }
    Mx_Fit(lit ? ZW_RING_SEL : ZW_RING, cx, cy, s * (lit ? 1.12f : 1.0f), 0x80);
    if (chara >= ZW_MODREF && sZwChipTag) { /* (a mod fighter in the picker: tagged) */
        Mx_Bar(ZW_PILL, cx - s * 0.32f, cy + s * 0.24f, cx + s * 0.32f, cy + s * 0.5f, 0x80);
        Mx_Text(cx, cy + s * 0.37f, "MOD", s * 0.2f, s * 0.6f, MX_DARK, 1);
    }
}

/* A round colour in its gold ring. */
static void Zw_Swatch(u32 rgb, f32 cx, f32 cy, f32 size, s32 lit) {
    u32 c = Mx_Rgba((s32)(rgb & 0xFF), (s32)((rgb >> 8) & 0xFF), (s32)((rgb >> 16) & 0xFF), 0x80);
    f32 s = lit ? size * (1.08f + 0.04f * Zw_Beat(40)) : size;

    Mx_Disc(cx, cy, s * 0.8f, c, Ps_Mix(c, Mx_Rgba(0, 0, 0, 0x80), 0.25f));
    Mx_Fit(lit ? ZW_RING_SEL : ZW_RING, cx, cy, s * (lit ? 1.12f : 1.0f), 0x80);
}

/* The chosen row's gold bar, pulsing a little. */
static void Zw_SelBar(f32 x0, f32 y0, f32 x1, f32 y1) {
    s32 g = 0x78 + (s32)(8.0f * Zw_Beat(50));

    Mx_BarTint(ZW_BAR_SEL, x0, y0, x1, y1, (u32)g | ((u32)g << 8) | ((u32)g << 16), 0x80);
}

/* The model on the stone platform (bobbing): `chara` with the files of mod `mod` (-1 its own), read again when `key`
   changes; nothing while `chara` < 0. A spinning halo says it is being made. */
static void Zw_Stage(s32 key, s32 chara, s32 mod, f32 cx, f32 feet, f32 height, s32 making) {
    f32 bob = 3.0f * Zw_Beat(140);
    f32 zoom = sZw->camShownZoom;
    f32 focus = 0.5f + sZw->camShownV * 0.42f; /* (the height on the body the camera looks at, 0 the feet) */
    f32 mid = feet - height * 0.5f;
    f32 at = mid + height * zoom * focus; /* the feet, the focus staying in the stage's middle */

    Mx_Fit(ZW_PLATFORM, cx, at + 14.0f * zoom + bob, height * 0.36f * zoom, (s32)(128.0f * Zw_In(sZw->modeT, 10)));
    if (chara < 0) {
        Mm_ViewWant(-1, 0, 0, -1, 1);
    } else {
        static s32 logged = -1;

        if (key != logged) {
            logged = key;
            Port_ZwViewLog(key, chara, mod);
        }
        Mm_ViewWant(key, chara, 0, mod, sZw->rest > 4 || (key & 0xF0000) == 0x10000); /* (the preview made again: read at once,
                                                                                       whatever is being pressed) */
        Mm_ViewCamera(sZw->camYaw, sZw->camShownV * 0.45f, sZw->tpose);
        { /* (a giant, or a warrior made bigger, at a fighter's size here: Port_WarriorViewScale) */
            extern f32 gMmViewScale;
            extern s32 Port_WarriorViewScale(s32 mod);

            gMmViewScale = mod >= 0 ? (f32)Port_WarriorViewScale(mod) / 100.0f : 1.0f;
            Mm_ViewDraw(cx, at + bob, height * zoom);
            gMmViewScale = 1.0f;
        }
    }
    if (chara >= 0 && sZw->ask == ASK_NONE && sZw->mode != ZW_WSHOP && sZw->mode != ZW_WSDETAIL && sZw->mode != ZW_WSUPLOAD &&
        sZw->mode != ZW_LAB && (sZw->mode != ZW_PICK || Pk_Side())) { /* how to look at it
                                                                                                                         (not on the Workshop's pages) */
        s32 top = sZw->mode == ZW_LIBRARY || sZw->mode == ZW_EDIT || sZw->mode == ZW_PICK || sZw->mode == ZW_SHADEPK;
        f32 x = sZw->mode == ZW_EDIT ? cx - 330.0f : cx - 250.0f;
        f32 y = top ? 128.0f : feet + 52.0f;
        static const s32 kinds[4] = {16, 10, 17, 13};
        static char *labels[4] = {"Turn / look", "Zoom", "Reset", "T-pose"};
        s32 i;

        for (i = 0; i < 4; i++) {
            Mx_Button(kinds[i], x, y, 20.0f);
            x += Mx_ButtonW(kinds[i], 20.0f) + 5.0f;
            Mx_Text(x, y, i == 3 && sZw->tpose ? "Stance" : labels[i], 16.0f, 0.0f, MX_LAV, 0);
            x += Mx_TextW(i == 3 && sZw->tpose ? "Stance" : labels[i], 16.0f) + 16.0f;
        }
    }
    if (making || (chara >= 0 && Mm_ViewKey() != key)) {
        f32 a = (f32)(sZw->frames % 40) / 40.0f;

        Mx_Fit(ZW_HALO, cx, feet - height * 0.5f + 10.0f * sinf(a * 6.2831853f), 60.0f, 0x60);
        Mx_Text(cx, feet - height * 0.5f + 46.0f, making ? "MAKING..." : "LOADING...", 18.0f, 0.0f, MX_GOLD, 1);
    }
}

/* The warrior being edited on the platform: the preview of how it is now when there is one, else the saved warrior,
   else (a moment after a change, while the preview is made) its body as the disc has it. */
static void Zw_EditStage(f32 cx, f32 feet, f32 height) {
    PortModInfo info;
    PortWarrior *w = &sZw->w;

    if (sZw->previewMod >= 0) {
        Port_ModGetInfo(sZw->previewMod, &info);
        Zw_Stage(0x10000 + (sZw->viewGen & 0xFFF), info.chara, sZw->previewMod, cx, feet, height, sZw->previewing);
    } else if (sZw->at >= 0 && sZw->list[sZw->at].mod >= 0) { /* (saved; or, changed, the saved one while its preview is made) */
        Port_ModGetInfo(sZw->list[sZw->at].mod, &info);
        Zw_Stage(0x20000 + sZw->list[sZw->at].mod * 64 + (sZw->viewGen & 63), info.chara, sZw->list[sZw->at].mod, cx, feet, height,
                 sZw->dirty && (sZw->previewDue >= 0 || sZw->previewing));
    } else {
        Zw_Stage(0x30000 + w->body, w->body, -1, cx, feet, height, sZw->previewDue >= 0 || sZw->previewing);
    }
}

/* Makes the preview when a change has rested (one frame says so first: the making takes a moment). */
/* The viewer's camera by hand (the library, the editor, the colours): the right stick turns the model and moves the
   camera up and down it, L2 / R2 zoom, R3 puts it back, SELECT the T-pose. It eases to where it is put. */
static void Zw_CameraInput(s32 live) {
    Pad *pad = &gPad[0];
    f32 x = pad->right[0];
    f32 y = pad->right[1];

    if (live) {
        if (pad->gameHeld & PADG_RSTICK_LEFT) {
            x = -1.0f;
        } else if (pad->gameHeld & PADG_RSTICK_RIGHT) {
            x = 1.0f;
        }
        if (pad->gameHeld & PADG_RSTICK_UP) {
            y = -1.0f;
        } else if (pad->gameHeld & PADG_RSTICK_DOWN) {
            y = 1.0f;
        }
        if (x * x > 0.04f) {
            sZw->camYaw += x * 0.06f;
        }
        if (y * y > 0.04f) {
            sZw->camV -= y * 0.03f;
            sZw->camV = sZw->camV < -1.0f ? -1.0f : sZw->camV > 1.0f ? 1.0f : sZw->camV;
        }
        if (pad->gameHeld & PADG_R2) {
            sZw->camZoom *= 1.03f;
        }
        if (pad->gameHeld & PADG_L2) {
            sZw->camZoom /= 1.03f;
        }
        sZw->camZoom = sZw->camZoom < 1.0f ? 1.0f : sZw->camZoom > 3.2f ? 3.2f : sZw->camZoom;
        if (pad->gamePressed & PADG_R3) {
            sZw->camYaw = 3.59f;
            sZw->camV = 0.0f;
            sZw->camZoom = 1.0f;
            Snd_PlaySe(1, 2);
        }
        if (pad->gamePressed & PADG_SELECT) {
            sZw->tpose ^= 1;
            Snd_PlaySe(1, 1);
        }
    }
    sZw->camShownV += (sZw->camV - sZw->camShownV) * 0.25f;
    sZw->camShownZoom += (sZw->camZoom - sZw->camShownZoom) * 0.25f;
}

/* The viewer still reading the last preview made (its key not on the screen yet): the preview's files are not to be
   written again under it -- a model file read in pieces, half of one making and half of the next, has every part on
   another part's texture. Two seconds at most, so a viewer showing something else does not hold it up for ever. */
static s32 Zw_ViewBusy(s32 *wait) {
    if (sZw->previewMod >= 0 && Mm_ViewKey() != 0x10000 + (sZw->viewGen & 0xFFF) && *wait < 120) {
        (*wait)++;
        return 1;
    }
    *wait = 0;
    return 0;
}

static void Zw_PreviewTick(void) {
    static s32 makeWait;

    if (sZw->paintDue > 0) {
        sZw->paintDue--;
    } else if (sZw->paintDue == 0 && !sZw->previewing && sZw->previewMod >= 0 && Mm_ViewKey() != 0x10000 + (sZw->viewGen & 0xFFF) &&
               ++sZw->picHold < 1000 && (sZw->paintWait = sZw->paintWait + 1) < 120) {
        /* (the last painting still being read: this one waits for it, two seconds at most, so they do not keep stopping
           each other) */
    } else if (sZw->paintDue == 0 && !sZw->previewing) {
        sZw->paintWait = 0; /* the colours: painted into the preview, else it is made again */
        sZw->paintDue = -1;
        if (sZw->previewMod >= 0 && sZw->previewDue < 0 && Port_WarriorPreviewPaint(&sZw->w)) {
            sZw->viewGen++;
            sZw->glowOn = 0; /* (another model on the screen soon: the glow takes its palettes again) */
            sZw->glowState = -1;
        } else if (sZw->previewDue < 0) {
            sZw->previewDue = 0;
        }
    }
    if (sZw->previewing) {
        if (Zw_ViewBusy(&makeWait)) {
            return; /* (made once the last one is read) */
        }
        sZw->previewing = 0;
        sZw->previewMod = Port_WarriorPreview(&sZw->w);
        sZw->viewGen++;
        {
            extern s32 Port_WarriorRampShort(void);

            if (Port_WarriorRampShort() > 0) {
                Zw_Note("Too many fighters' shading (16 at most): some parts borrow another's and look off. Fewer hair layers fix it.");
            }
        }
        return;
    }
    if (sZw->previewDue > 0) {
        sZw->previewDue--;
    } else if (sZw->previewDue == 0) {
        sZw->previewDue = -1;
        sZw->previewing = 1;
    }
}

/* ---- the hovered colour zone, lit on the model ----
 * The model on screen uploads its palettes from its texture file every frame, so a zone is shown by changing its
 * colours there, and putting them back after: blinking in a bright colour while the zone is hovered, the hovered
 * swatch's colour (with the zone's shading) while one is being chosen. Two looks at a time at most: the texture
 * upscaler caches every look by its content (port/src/gs/gs_upscale.c). The palettes are counted as the warrior maker
 * counts them (Port_WarriorZoneMask): the model must be a made warrior (the preview, the saved one) or the plain body
 * when no part is swapped. */
typedef struct ZwTexEntry { /* (a texture file as the game has it loaded: include/sys/gfxm_b_b.h TexEntry / TexFile) */
    u32 pixOfs, clutOfs;
    s32 pixSize, clutSize, tbpStep, cbpStep;
    u32 pixBlt, clutBlt;
    s32 tbpOfs, cbpOfs;
    u8 unk28[8];
    u64 tex0;
    void *pix, *clut;
} ZwTexEntry;
typedef struct ZwTexFile {
    u32 count, entOfs;
    u8 unk08[8];
    ZwTexEntry *ent;
} ZwTexFile;

static u8 *Glow_Palette(ZwTexFile *tf, s32 want) {
    s32 i;
    s32 p = 0;

    for (i = 0; i < (s32)tf->count && p < 64; i++) {
        ZwTexEntry *e = &tf->ent[i];

        if (e->clutSize == 0 || ((e->tex0 >> 20) & 0x3F) != 0x14 || e->clutOfs == 0) {
            continue;
        }
        if (p++ == want) {
            return (u8 *)e->clut + 0x60;
        }
    }
    return NULL;
}

static void Glow_Off(void) {
    ZwTexFile *tf;
    s32 p;
    s32 k;

    if (sZw == NULL || !sZw->glowOn) {
        return;
    }
    tf = (ZwTexFile *)Mm_ViewTex();
    if ((void *)tf == sZw->glowTex && Mm_ViewKey() == sZw->glowView) {
        for (p = 0; p < sZw->glowPals; p++) {
            u8 *pal = Glow_Palette(tf, p);

            for (k = 0; pal != NULL && k < 16; k++) {
                if (sZw->glowMask[p * 16 + k]) {
                    memcpy(pal + k * 4, &sZw->glowSave[p * 16 + k], 4);
                }
            }
        }
    }
    sZw->glowOn = 0;
    sZw->glowState = -1;
}

/* Lights zone `key` (0 none): mode 1 blinking, 2 in colour `rgb` (0xBBGGRR) shaded as the zone is shaded now (`now`). */
static void Glow_Update(s32 key, s32 own, s32 mode) {
    ZwTexFile *tf = (ZwTexFile *)Mm_ViewTex();
    s32 view = Mm_ViewKey();
    s32 parts = 0;
    s32 state;
    s32 p;
    s32 k;

    for (p = 0; p < 12; p++) {
        parts |= sZw->w.part[p] >= 0 || sZw->w.layer[p] >= 0;
    }
    for (p = 0; p < 8; p++) {
        parts |= sZw->w.size[p] > 0 && sZw->w.size[p] != 100;
    }
    for (p = 0; p < sZw->w.colours; p++) {
        parts |= sZw->w.colourPart[p] != 0;
    }
    if (sZw->glowOn && ((void *)tf != sZw->glowTex || view != sZw->glowView)) { /* (read again: nothing to put back) */
        sZw->glowOn = 0;
        sZw->glowState = -1;
    }
    if (key != sZw->glowKey || own != sZw->glowOwn) {
        Glow_Off();
        sZw->glowKey = key;
        sZw->glowOwn = own;
        sZw->glowPals = key != 0 ? Port_WarriorZoneMask(&sZw->w, key, own, sZw->glowMask) : 0;
    }
    if (key == 0 || mode == 0 || tf == NULL || sZw->glowPals == 0 || (view >= 0x30000 && parts)) {
        Glow_Off();
        return;
    }
    if (!sZw->glowOn) {
        for (p = 0; p < sZw->glowPals; p++) {
            u8 *pal = Glow_Palette(tf, p);

            for (k = 0; pal != NULL && k < 16; k++) {
                memcpy(&sZw->glowSave[p * 16 + k], pal + k * 4, 4);
            }
        }
        sZw->glowOn = 1;
        sZw->glowTex = (void *)tf;
        sZw->glowView = view;
        sZw->glowState = -1;
    }
    state = (sZw->frames / 18) & 1;
    if (state == sZw->glowState) {
        return;
    }
    sZw->glowState = state;
    for (p = 0; p < sZw->glowPals; p++) {
        u8 *pal = Glow_Palette(tf, p);

        for (k = 0; pal != NULL && k < 16; k++) {
            u32 c = sZw->glowSave[p * 16 + k];
            u32 out = c;
            s32 j;

            if (!sZw->glowMask[p * 16 + k]) {
                continue;
            }
            if (mode == 1 && state) { /* a bright pink, three quarters over the colour (its shading shows through) */
                static const s32 hl[3] = {0xFF, 0x40, 0xD0};

                out = c & 0xFF000000;
                for (j = 0; j < 3; j++) {
                    out |= (u32)(((s32)((c >> (j * 8)) & 0xFF) + hl[j] * 3) / 4) << (j * 8);
                }
            }
            memcpy(pal + k * 4, &out, 4);
        }
    }
}

/* ---------------------------------------------------------------------------------------------- pop-up questions */
static char *Ask_Text(void) {
    if (sZw->ask == ASK_WSDEL) {
        return "Take this warrior down from the Workshop?";
    }
    return sZw->ask == ASK_REMOVE ? "Send this warrior away?" : sZw->ask == ASK_COSON ? "Start editing Costume 2?"
           : sZw->ask == ASK_COSOFF ? "Turn costumes off?" : "You have changes that are not saved!";
}

static char *Ask_Sub(void) {
    if (sZw->ask == ASK_WSDEL) {
        return "It leaves the Workshop. Your own warrior and the ones others got stay.";
    }
    return sZw->ask == ASK_REMOVE ? "Its folder goes to mods_removed: nothing is deleted."
           : sZw->ask == ASK_COSON ? "What you have now is kept as Costume 1. Costume 2 starts as a copy of it."
           : sZw->ask == ASK_COSOFF ? "Costumes 2 to 4 go when you save. Costume 1 stays." : "Keep them before you go?";
}

static s32 Ask_Count(void) {
    return sZw->ask == ASK_LEAVE ? 3 : 2;
}

static char *Ask_Button(s32 k) {
    static char *remove[2] = {"Remove", "Keep it"};
    static char *leave[3] = {"Save", "Don't save", "Keep editing"};
    static char *cosOn[2] = {"Yes, start it", "Not now"};
    static char *cosOff[2] = {"Turn them off", "Keep them"};
    static char *wsDel[2] = {"Take it down", "Keep it up"};

    if (sZw->ask == ASK_WSDEL) {
        return wsDel[k];
    }
    return sZw->ask == ASK_REMOVE ? remove[k] : sZw->ask == ASK_COSON ? cosOn[k] : sZw->ask == ASK_COSOFF ? cosOff[k] : leave[k];
}

static void Ask_Open(s32 ask) {
    sZw->ask = ask;
    sZw->askCur = ask == ASK_REMOVE || ask == ASK_COSOFF || ask == ASK_WSDEL ? 1 : 0;
    sZw->askT = 0;
    Snd_PlaySe(1, 0);
}

static void Ask_Draw(void) {
    f32 pop = Zw_In(sZw->askT, 10);
    f32 s = 0.85f + 0.15f * pop;
    f32 cx = 688.0f;
    f32 cy = 360.0f;
    f32 hw = 330.0f * s;
    f32 hh = 140.0f * s;
    s32 n = Ask_Count();
    s32 k;

    Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(0, 0, 24, (s32)(0x48 * pop)), Mx_Rgba(0, 0, 24, (s32)(0x48 * pop)));
    Mx_Box(ZW_WINDOW, cx - hw, cy - hh, cx + hw, cy + hh, 26.0f, (s32)(128.0f * pop));
    Mx_Fit(ZW_HALO, cx, cy - hh + 6.0f + 3.0f * Zw_Beat(60), 54.0f, (s32)(128.0f * pop));
    Mx_TextA(cx, cy - 58.0f * s, Ask_Text(), 28.0f * s, hw * 1.8f, MX_GOLD, 1, (s32)(128.0f * pop));
    Mx_TextA(cx, cy - 14.0f * s, Ask_Sub(), 19.0f * s, hw * 1.8f, MX_WHITE, 1, (s32)(128.0f * pop));
    for (k = 0; k < n; k++) {
        f32 bw = n == 3 ? 190.0f : 230.0f;
        f32 bx = cx + ((f32)k - (f32)(n - 1) * 0.5f) * (bw + 18.0f) * s;
        s32 lit = k == sZw->askCur;
        f32 grow = lit ? 1.0f + 0.03f * Zw_Beat(40) : 1.0f;

        Mx_Bar(lit ? ZW_PILL : ZW_PILL_STEEL, bx - bw * 0.5f * s * grow, cy + 50.0f * s, bx + bw * 0.5f * s * grow, cy + 96.0f * s, (s32)(128.0f * pop));
        Mx_TextA(bx, cy + 73.0f * s, Ask_Button(k), 20.0f * s, bw * s - 20.0f, lit ? MX_DARK : MX_WHITE, 1, (s32)(128.0f * pop));
    }
}

static void Zw_Edit(s32 at);
static void Ed_Save(void);
static void Zw_CosSwitch(s32 to);
static void Zw_LookCopy(PortWarrior *to, const PortWarrior *from);
static void Zw_HistStart(void);
static void Wk_Load(void);

/* The question's answer: k (-1 cancelled). */
static void Ask_Answer(s32 k) {
    s32 ask = sZw->ask;

    sZw->ask = ASK_NONE;
    if (ask == ASK_REMOVE && k == 0) {
        if (Port_WarriorRemove(sZw->cur - 1)) {
            Zw_Note("Off to the Other Other World with you!");
        }
        Zw_Reload();
        Snd_PlaySe(1, 1);
    } else if (ask == ASK_LEAVE && k == 0) {
        sZw->saving = 2;
        sZw->leaving = 2; /* (back to the library once saved) */
        Snd_PlaySe(1, 1);
    } else if (ask == ASK_COSON && k == 0 && sZw->cos != NULL) { /* what there is now: costume 1; costume 2 from it */
        sZw->cos[0] = sZw->w;
        sZw->cosHave = 1;
        Zw_CosSwitch(1);
        Zw_Note("Costume 2! Change anything on its look: parts, colours, edits. Left and right on Editing for the others.");
        Snd_PlaySe(1, 1);
    } else if (ask == ASK_COSOFF && k == 0 && sZw->cos != NULL) { /* back to costume 1 alone */
        Zw_CosSwitch(0);
        sZw->cosHave = 1;
        Zw_Changed();
        Zw_Note("One outfit again. Save to make it so.");
        Snd_PlaySe(1, 1);
    } else if (ask == ASK_WSDEL && k == 0) {
        if (Port_WsDelete(sZw->wkCur)) {
            Zw_Note("Taken down from the Workshop.");
        }
        Wk_Load();
        Zw_SetMode(ZW_WSHOP);
        Snd_PlaySe(1, 1);
    } else if (ask == ASK_LEAVE && k == 1) {
        Port_WarriorDraft(&sZw->w, 2); /* (not kept, as asked) */
        Zw_SetMode(ZW_LIBRARY);
        Snd_PlaySe(1, 2);
    } else {
        Snd_PlaySe(1, 2);
    }
}

static void Ask_Update(void) {
    Pad *pad = &gPad[0];
    s32 n = Ask_Count();

    sZw->askT++;
    if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        sZw->askCur = (sZw->askCur + n - 1) % n;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        sZw->askCur = (sZw->askCur + 1) % n;
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & PADG_CROSS) {
        Ask_Answer(sZw->askCur);
    } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
        Ask_Answer(-1);
    }
}

/* ---------------------------------------------------------------------------------------------- the Workshop
 * The Community Workshop inside Z Warriors (mockups zwarriors_design/workshop, WWE 2K's Community Creations as the
 * model): a grid of everyone's warriors (sorted by L1 / R1, paged by L2 / R2) with the one under the cursor on the
 * stage, its details page (OVERVIEW, LOOK, MOVES, FORMS, its bio), getting one (a pop-up while its recipe comes and the
 * warrior is built from the player's own game), and putting one's own up (a bio, tags). The catalogue is plat_mods.c's
 * (a local folder for now; a server later). */
#define WK_MAX 64
#define WK_COLS 5
#define WK_ROWS 3
#define WK_X 40.0f
#define WK_Y 178.0f
#define WK_W 162.0f
#define WK_H 146.0f
#define WK_GAP 8.0f
static char *kWkSort[4] = {"MOST RECENT", "MOST DOWNLOADED", "FEATURED", "MY UPLOADS"};
static char *kWkTab[4] = {"OVERVIEW", "LOOK", "MOVES", "FORMS"};
static char *kWkTag[6] = {"Saiyan", "Fan form", "Fusion", "Xeno", "What-if", "Original"};

static void Wk_Open(void);
static void Wk_UploadOpen(s32 at);
static s32 Lib_Face(PortWarrior *v);

/* MY WARRIORS / WORKSHOP under the title (which: 0 the library's, 1 the Workshop's). */
static void Wk_Tabs(s32 which) {
    Mx_Bar(which == 0 ? ZW_PILL : ZW_PILL_STEEL, 880.0f, 26.0f, 1070.0f, 60.0f, 0x80);
    Mx_Text(975.0f, 43.0f, "MY WARRIORS", 17.0f, 170.0f, which == 0 ? MX_DARK : MX_WHITE, 1);
    Mx_Bar(which == 1 ? ZW_PILL : ZW_PILL_STEEL, 1080.0f, 26.0f, 1270.0f, 60.0f, 0x80);
    Mx_Text(1175.0f, 43.0f, "WORKSHOP", 17.0f, 170.0f, which == 1 ? MX_DARK : MX_WHITE, 1);
    if (which == 0) {
        Mx_Button(8, 1280.0f, 43.0f, 22.0f);
    }
}

extern s32 Port_WsPreview(s32 i);
extern s32 Port_WsChip(s32 i, void *out, s32 max);

static void Wk_FreeChips(void) {
    s32 i;

    for (i = 0; i < WK_MAX; i++) {
        if (sZw->wkChip[i] != NULL) {
            Heap_Free(sZw->wkChip[i]);
            sZw->wkChip[i] = NULL;
        }
    }
}

static void Wk_Load(void) {
    s32 i, n;

    if (sZw->wk == NULL) {
        sZw->wk = Heap_Alloc(sizeof(PortWsEntry) * WK_MAX, 0x20, 0, 2);
    }
    if (sZw->wk == NULL) {
        sZw->wkN = 0;
        return;
    }
    n = Port_WsScan(sZw->wkSort);
    sZw->wkN = n > WK_MAX ? WK_MAX : n;
    Wk_FreeChips();
    sZw->wkPrevMod = -1; /* (the library may have moved: the stage's warrior is made again) */
    sZw->wkPrevId[0] = 0;
    sZw->rest = 0;
    for (i = 0; i < sZw->wkN; i++) {
        MmRes *res;

        Port_WsGet(i, &sZw->wk[i]);
        res = Heap_Alloc(0x1600, 0x40, 0, 2);
        if (res != NULL && Port_WsChip(i, res, 0x1600) > 0) { /* (its own small portrait) */
            Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
            sZw->wkChip[i] = res;
        } else if (res != NULL) {
            Heap_Free(res);
        }
    }
    if (sZw->wkCur >= sZw->wkN) {
        sZw->wkCur = sZw->wkN > 0 ? sZw->wkN - 1 : 0;
    }
}

/* The entry on the stage as it is built (its preview, made once the cursor has rested on it), "LOADING" meanwhile. */
static void Wk_Stage(PortWsEntry *e, f32 cx, f32 feet, f32 height) {
    if (sZw->wkPrevMod >= 0 && strcmp(sZw->wkPrevId, e->id) == 0) {
        PortModInfo info;

        Port_ModGetInfo(sZw->wkPrevMod, &info);
        Zw_Stage(0x50000 + sZw->wkPrevMod * 64 + (sZw->viewGen & 63), info.chara, sZw->wkPrevMod, cx, feet, height, 0);
    } else {
        Zw_Stage(-1, -1, -1, cx, feet, height, 0);
        Mx_Text(cx, feet - height * 0.5f, "LOADING...", 24.0f, 0.0f, MX_GOLD, 1);
    }
}

/* Builds the entry under the cursor for the stage once the cursor has rested on it a moment. */
static void Wk_PreviewTick(void) {
    PortWsEntry *e;

    if (sZw->wkN <= 0 || sZw->rest != 14) {
        return;
    }
    e = &sZw->wk[sZw->wkCur];
    if (sZw->wkPrevMod >= 0 && strcmp(sZw->wkPrevId, e->id) == 0) {
        return;
    }
    sZw->wkPrevMod = Port_WsPreview(sZw->wkCur);
    Ps_Copy(sZw->wkPrevId, e->id, sizeof(sZw->wkPrevId));
    sZw->viewGen++;
}

static void Wk_Open(void) {
    sZw->wkPrevMod = -1;
    sZw->wkPrevId[0] = 0;
    sZw->wkSort = 0;
    sZw->wkCur = 0;
    sZw->wkTop = 0;
    sZw->wkDl = 0;
    Wk_Load();
    Zw_SetMode(ZW_WSHOP);
}

/* Bio text in lines of about `per` letters (at a space). */
static void Wk_Wrap(char *s, f32 x, f32 y, f32 pitch, s32 per, s32 maxLines, f32 size) {
    char line[80];
    s32 n = 0, i;

    while (*s && n < maxLines) {
        s32 len = Ps_Len(s), cut = len;

        if (len > per) {
            cut = per;
            while (cut > per / 2 && s[cut] != ' ') {
                cut--;
            }
        }
        for (i = 0; i < cut && i < 79; i++) {
            line[i] = s[i];
        }
        line[i] = 0;
        Mx_Text(x, y + (f32)n * pitch, line, size, 0.0f, MX_WHITE, 0);
        s += cut;
        while (*s == ' ') {
            s++;
        }
        n++;
    }
}

/* The download pop-up, over whichever screen: its recipe, its pictures, built from the player's own game, ready. */
static void Wk_DlDraw(void) {
    PortWsEntry *e = &sZw->wk[sZw->wkCur];
    static char *steps[4] = {"Recipe received", "Drawings and photos", "Building from your game...", "Ready to fight"};
    f32 t = (f32)sZw->wkDl / 90.0f;
    char pct[16];
    s32 k, at = sZw->wkDl < 20 ? 1 : sZw->wkDl < 40 ? 2 : sZw->wkDl < 80 ? 3 : 4;

    Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(0, 0, 0, 0x50), Mx_Rgba(0, 0, 0, 0x50));
    Mx_Box(ZW_WINDOW, 300.0f, 200.0f, 1076.0f, 560.0f, 24.0f, 0x80);
    Mx_Frame(300.0f, 200.0f, 1076.0f, 560.0f, 3.0f, Mx_Rgba(250, 205, 90, 0x80));
    Mx_Heading(688.0f, 236.0f, "DOWNLOADING", 32.0f, 600.0f, 1);
    Mx_Face(e->render, 330.0f, 270.0f, 530.0f, 470.0f, 0x80);
    Mx_Rect(330.0f, 470.0f, 530.0f, 520.0f, Mx_Rgba(16, 20, 60, 0x78), Mx_Rgba(10, 12, 40, 0x78));
    Mx_Text(430.0f, 486.0f, e->name, 17.0f, 190.0f, MX_WHITE, 1);
    Mx_Text(430.0f, 506.0f, e->creator, 13.0f, 190.0f, MX_GOLD, 1);
    Mx_Bar(ZW_PILL_STEEL, 560.0f, 280.0f, 1040.0f, 318.0f, 0x80);
    Mx_Bar(ZW_PILL, 560.0f, 280.0f, 560.0f + 480.0f * (t > 1.0f ? 1.0f : t), 318.0f, 0x80);
    sprintf(pct, "%d%%", (s32)(100.0f * (t > 1.0f ? 1.0f : t)));
    Mx_Text(800.0f, 299.0f, pct, 20.0f, 0.0f, MX_DARK, 1);
    for (k = 0; k < 4; k++) {
        f32 y = 352.0f + (f32)k * 36.0f;

        Mx_Text(580.0f, y, k < at ? "+" : k == at ? "o" : "-", 22.0f, 0.0f, k < at ? MX_GOLD : MX_LAV, 0);
        Mx_Text(614.0f, y, steps[k], 19.0f, 420.0f, k <= at ? MX_WHITE : MX_LAV, 0);
    }
    Mx_Text(688.0f, 536.0f, "Built from your own copy of the game: only the recipe is downloaded.", 14.0f, 700.0f, MX_LAV, 1);
}

/* The download's steps, a frame at a time (the recipe is copied at once, the warrior built by the rescan). */
static void Wk_DlStep(void) {
    if (sZw->wkDl == 1) {
        sZw->wkDlOk = Port_WsDownload(sZw->wkCur);
    } else if (sZw->wkDl == 60) {
        Zw_Reload(); /* (the library sees the new warrior: it is built from the player's disc) */
    } else if (sZw->wkDl >= 90) {
        sZw->wkDl = 0;
        Zw_Note(sZw->wkDlOk ? "Here he is! He's in your warriors now, ready to fight." : "Hmm, that one could not be brought here.");
        Snd_PlaySe(1, sZw->wkDlOk ? 1 : 2);
        Wk_Load();
        return;
    }
    sZw->wkDl++;
}

static void Wk_Draw(void) {
    PortWsEntry *e = sZw->wkN > 0 ? &sZw->wk[sZw->wkCur] : NULL;
    s32 i, pages = (sZw->wkN + WK_COLS * WK_ROWS - 1) / (WK_COLS * WK_ROWS);
    char line[96];

    Zw_Host(1290.0f, 300.0f, 220.0f, 0);
    Zw_Title("WORKSHOP");
    Mx_Text(170.0f, 108.0f, "COMMUNITY WARRIORS / DOWNLOADS", 15.0f, 0.0f, MX_LAV, 0);
    for (i = 0; i < 4; i++) {
        f32 x0 = 44.0f + (f32)i * 186.0f;

        Mx_Bar(i == sZw->wkSort ? ZW_PILL : ZW_PILL_STEEL, x0, 124.0f, x0 + 178.0f, 156.0f, 0x80);
        Mx_Text(x0 + 89.0f, 140.0f, kWkSort[i], 14.0f, 166.0f, i == sZw->wkSort ? MX_DARK : MX_WHITE, 1);
    }
    sprintf(line, "L2  PAGE %d / %d  R2", sZw->wkTop / (WK_COLS * WK_ROWS) + 1, pages > 0 ? pages : 1);
    Mx_Text(880.0f, 108.0f, line, 15.0f, 0.0f, MX_GOLD, 2);
    Mx_Box(ZW_WINDOW, WK_X - 10.0f, WK_Y - 10.0f, WK_X + WK_COLS * (WK_W + WK_GAP) + 2.0f, WK_Y + WK_ROWS * (WK_H + WK_GAP) + 2.0f, 18.0f, 0x80);
    for (i = 0; i < WK_COLS * WK_ROWS && sZw->wkTop + i < sZw->wkN; i++) {
        s32 at = sZw->wkTop + i;
        PortWsEntry *t = &sZw->wk[at];
        f32 x0 = WK_X + (f32)(i % WK_COLS) * (WK_W + WK_GAP), y0 = WK_Y + (f32)(i / WK_COLS) * (WK_H + WK_GAP);
        f32 in = Zw_In(sZw->modeT - i, 10);

        Mx_Rect(x0, y0, x0 + WK_W, y0 + WK_H - 36.0f, Mx_Rgba(0x1C, 0x2A, 0x5C, 0x80), Mx_Rgba(0x0A, 0x12, 0x30, 0x80));
        if (at < WK_MAX && sZw->wkChip[at] != NULL) {
            Mx_Tex(sZw->wkChip[at]->tex, x0, y0, x0 + WK_W, y0 + WK_H - 36.0f, 0, 10, 64, 54, (s32)(128.0f * in), 0x80);
        } else {
            Mx_Face(t->render, x0, y0, x0 + WK_W, y0 + WK_H - 36.0f, (s32)(128.0f * in));
        }
        Mx_Rect(x0, y0 + WK_H - 36.0f, x0 + WK_W, y0 + WK_H, at == sZw->wkCur ? Mx_Rgba(200, 150, 40, 0x80) : Mx_Rgba(20, 26, 70, 0x80),
                at == sZw->wkCur ? Mx_Rgba(150, 100, 20, 0x80) : Mx_Rgba(10, 14, 44, 0x80));
        Mx_Text(x0 + WK_W * 0.5f, y0 + WK_H - 25.0f, t->name, 14.0f, WK_W - 10.0f, MX_WHITE, 1);
        Mx_Text(x0 + WK_W * 0.5f, y0 + WK_H - 9.0f, t->creator, 11.0f, WK_W - 10.0f, MX_GOLD, 1);
        if (t->owned) {
            Mx_Bar(ZW_PILL, x0 + WK_W - 60.0f, y0 + 4.0f, x0 + WK_W - 4.0f, y0 + 24.0f, 0x80);
            Mx_Text(x0 + WK_W - 32.0f, y0 + 14.0f, "GOT", 11.0f, 50.0f, MX_DARK, 1);
        }
        if (at == sZw->wkCur) {
            Mx_Frame(x0 - 3.0f, y0 - 3.0f, x0 + WK_W + 3.0f, y0 + WK_H + 3.0f, 4.0f, Mx_Rgba(255, 210, 80, 0x60 + (s32)(32.0f * Zw_Beat(40))));
        }
    }
    if (sZw->wkN == 0) {
        Mx_Text(460.0f, 380.0f, "Nothing here yet!", 28.0f, 0.0f, MX_GOLD, 1);
        Mx_Text(460.0f, 420.0f, "Upload a warrior from MY WARRIORS (Start).", 18.0f, 0.0f, MX_WHITE, 1);
    }
    if (e != NULL) { /* the one under the cursor */
        Wk_Stage(e, 1080.0f, 450.0f, 300.0f);
        Mx_Box(ZW_WINDOW, 920.0f, 470.0f, 1346.0f, 628.0f, 18.0f, 0x80);
        sprintf(line, "Created by %s", e->creator);
        Mx_Text(940.0f, 494.0f, line, 18.0f, 390.0f, MX_GOLD, 0);
        sprintf(line, "Downloads %d    Likes %d", e->downloads, e->likes);
        Mx_Text(940.0f, 526.0f, line, 17.0f, 390.0f, MX_WHITE, 0);
        Ps_Copy(line, e->line[0][3], sizeof(line));
        Mx_Text(940.0f, 556.0f, line, 17.0f, 390.0f, MX_LAV, 0);
        Mx_Bar(e->owned ? ZW_PILL_STEEL : ZW_PILL, 980.0f, 580.0f, 1290.0f, 616.0f, 0x80);
        Mx_Text(1135.0f, 598.0f, e->owned ? "IN YOUR WARRIORS" : "DOWNLOAD", 18.0f, 290.0f, e->owned ? MX_WHITE : MX_DARK, 1);
    }
    if (sZw->wkDl > 0) {
        Wk_DlDraw();
        Zw_Prompt(2, "Wait");
        Zw_Talk("Here he comes! Hold on to your antennae!");
        return;
    }
    Zw_Prompt(0, "Details");
    Zw_Prompt(1, "Download");
    if (sZw->wkN > 0 && sZw->wk[sZw->wkCur].mine) {
        Zw_Prompt(4, "Take down");
    }
    Zw_Prompt(14, "Sort");
    Zw_Prompt(10, "Page");
    Zw_Prompt(2, "Back");
    Zw_Talk("Pick a warrior from any world, heh heh!");
}

static void Wk_Move(s32 *cur, s32 n, Pad *pad) {
    s32 c = *cur;

    if (n <= 0) {
        return;
    }
    if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        c = c + 1 < n ? c + 1 : c;
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        c = c > 0 ? c - 1 : c;
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        c = c + WK_COLS < n ? c + WK_COLS : c;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        c = c >= WK_COLS ? c - WK_COLS : c;
    } else if (pad->gamePressed & PADG_R2) {
        c = c + WK_COLS * WK_ROWS < n ? c + WK_COLS * WK_ROWS : n - 1;
    } else if (pad->gamePressed & PADG_L2) {
        c = c >= WK_COLS * WK_ROWS ? c - WK_COLS * WK_ROWS : 0;
    }
    if (c != *cur) {
        *cur = c;
        sZw->rest = 0;
        Snd_PlaySe(2, 0);
    }
}

static void Wk_Update(s32 live) {
    Pad *pad = &gPad[0];

    Wk_PreviewTick();
    if (sZw->wkDl > 0) {
        Wk_DlStep();
        return;
    }
    if (!live) {
        return;
    }
    Wk_Move(&sZw->wkCur, sZw->wkN, pad);
    sZw->wkTop = sZw->wkCur / (WK_COLS * WK_ROWS) * (WK_COLS * WK_ROWS);
    if (pad->gamePressed & (PADG_L1 | PADG_R1)) {
        sZw->wkSort = (sZw->wkSort + ((pad->gamePressed & PADG_R1) ? 1 : 3)) % 4;
        sZw->wkCur = 0;
        Wk_Load();
        Snd_PlaySe(1, 0);
    } else if ((pad->gamePressed & PADG_CROSS) && sZw->wkN > 0) {
        sZw->wkTab = 0;
        Zw_SetMode(ZW_WSDETAIL);
        Snd_PlaySe(1, 1);
    } else if ((pad->gamePressed & PADG_SQUARE) && sZw->wkN > 0) {
        if (sZw->wk[sZw->wkCur].owned) {
            Zw_Note("Already in your warriors! Look in MY WARRIORS.");
            Snd_PlaySe(1, 2);
        } else {
            sZw->wkDl = 1;
            Snd_PlaySe(1, 1);
        }
    } else if ((pad->gamePressed & PADG_CIRCLE) && sZw->wkN > 0 && sZw->wk[sZw->wkCur].mine) {
        Ask_Open(ASK_WSDEL);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Zw_Reload();
        Zw_SetMode(ZW_LIBRARY);
        Snd_PlaySe(1, 2);
    }
}

static void Wk_DetailDraw(void) {
    PortWsEntry *e = &sZw->wk[sZw->wkCur];
    char line[160];
    s32 i;

    Zw_Title("WORKSHOP");
    sprintf(line, "COMMUNITY WARRIORS / %s", e->name);
    Mx_Text(170.0f, 108.0f, line, 15.0f, 0.0f, MX_LAV, 0);
    for (i = 0; i < 4; i++) {
        f32 x0 = 44.0f + (f32)i * 160.0f;

        Mx_Bar(i == sZw->wkTab ? ZW_PILL : ZW_PILL_STEEL, x0, 124.0f, x0 + 152.0f, 156.0f, 0x80);
        Mx_Text(x0 + 76.0f, 140.0f, kWkTab[i], 15.0f, 140.0f, i == sZw->wkTab ? MX_DARK : MX_WHITE, 1);
    }
    /* the portrait and its name */
    Mx_Box(ZW_WINDOW, 34.0f, 170.0f, 300.0f, 560.0f, 18.0f, 0x80);
    Mx_Face(e->render, 48.0f, 184.0f, 286.0f, 470.0f, 0x80);
    Mx_Rect(48.0f, 476.0f, 286.0f, 546.0f, Mx_Rgba(20, 26, 70, 0x80), Mx_Rgba(10, 14, 44, 0x80));
    Mx_Text(167.0f, 498.0f, e->name, 24.0f, 230.0f, MX_WHITE, 1);
    Mx_Text(167.0f, 528.0f, e->title, 15.0f, 230.0f, MX_GOLD, 1);
    /* the creator, the counts */
    Mx_Box(ZW_WINDOW, 312.0f, 170.0f, 880.0f, 244.0f, 18.0f, 0x80);
    sprintf(line, "Created by %s", e->creator);
    Mx_Text(330.0f, 194.0f, line, 20.0f, 300.0f, MX_GOLD, 0);
    sprintf(line, "Downloads %d    Likes %d%s    Uploaded %s    Version %d", e->downloads, e->likes, e->liked ? " (you)" : "", e->uploaded, e->version);
    Mx_Text(330.0f, 224.0f, line, 15.0f, 540.0f, MX_WHITE, 0);
    /* the bio */
    Mx_Box(ZW_WINDOW, 312.0f, 252.0f, 880.0f, 360.0f, 18.0f, 0x80);
    Mx_Text(330.0f, 272.0f, "BIO", 18.0f, 0.0f, MX_GOLD, 0);
    if (e->bio[0]) {
        Wk_Wrap(e->bio, 330.0f, 298.0f, 22.0f, 64, 2, 16.0f);
    } else {
        Mx_Text(330.0f, 298.0f, "(no bio)", 16.0f, 0.0f, MX_LAV, 0);
    }
    if (e->tags[0]) {
        sprintf(line, "Tags: %s", e->tags);
        Mx_Text(330.0f, 344.0f, line, 14.0f, 530.0f, MX_LAV, 0);
    }
    /* what it has: this tab's lines */
    Mx_Box(ZW_WINDOW, 312.0f, 368.0f, 880.0f, 548.0f, 18.0f, 0x80);
    if (sZw->wkTab == 0) { /* (no table entry and literal in one ?: in this build) */
        Ps_Copy(line, "CONTAINS", 16);
    } else {
        Ps_Copy(line, kWkTab[sZw->wkTab], 16);
    }
    Mx_Text(330.0f, 388.0f, line, 18.0f, 0.0f, MX_GOLD, 0);
    for (i = 0; i < 6; i++) {
        Ps_Copy(line, e->line[sZw->wkTab][i], 60);
        if (line[0]) {
            Mx_Text(340.0f + (f32)(i / 3) * 270.0f, 420.0f + (f32)(i % 3) * 38.0f, line, 15.0f, 260.0f, MX_WHITE, 0);
        }
    }
    /* built from */
    Mx_Box(ZW_WINDOW, 312.0f, 556.0f, 880.0f, 626.0f, 18.0f, 0x80);
    Mx_Text(330.0f, 591.0f, "BUILT FROM", 16.0f, 0.0f, MX_GOLD, 0);
    for (i = 0; i < e->builtN && i < 8; i++) {
        Zw_Chip(e->built[i], 480.0f + (f32)i * 52.0f, 591.0f, 46.0f, 0);
    }
    /* the warrior, what to do */
    Wk_Stage(e, 1100.0f, 470.0f, 320.0f);
    Mx_Bar(e->owned ? ZW_PILL_STEEL : ZW_PILL, 960.0f, 520.0f, 1240.0f, 556.0f, 0x80);
    Mx_Text(1100.0f, 538.0f, e->owned ? "IN YOUR WARRIORS" : "DOWNLOAD", 18.0f, 260.0f, e->owned ? MX_WHITE : MX_DARK, 1);
    Mx_Bar(e->liked ? ZW_PILL : ZW_PILL_STEEL, 960.0f, 566.0f, 1240.0f, 600.0f, 0x80);
    Mx_Text(1100.0f, 583.0f, e->liked ? "LIKED" : "LIKE", 17.0f, 260.0f, e->liked ? MX_DARK : MX_WHITE, 1);
    if (sZw->wkDl > 0) {
        Wk_DlDraw();
        Zw_Prompt(2, "Wait");
        Zw_Talk("Here he comes! Hold on to your antennae!");
        return;
    }
    Zw_Prompt(0, "Download");
    Zw_Prompt(4, e->liked ? "Unlike" : "Like");
    if (e->mine) {
        Zw_Prompt(1, "Take down");
    }
    Zw_Prompt(14, "Tab");
    Zw_Prompt(2, "Back");
    Zw_Talk(e->downloads > 0 ? "Ooh, a popular one! Read up before you take him home." : "A fresh one! Be the first to try him out.");
}

static void Wk_DetailUpdate(s32 live) {
    Pad *pad = &gPad[0];

    Wk_PreviewTick();
    if (sZw->wkDl > 0) {
        Wk_DlStep();
        return;
    }
    if (!live) {
        return;
    }
    if (pad->gamePressed & (PADG_L1 | PADG_R1)) {
        sZw->wkTab = (sZw->wkTab + ((pad->gamePressed & PADG_R1) ? 1 : 3)) % 4;
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_CROSS) {
        if (sZw->wk[sZw->wkCur].owned) {
            Zw_Note("Already in your warriors! Look in MY WARRIORS.");
            Snd_PlaySe(1, 2);
        } else {
            sZw->wkDl = 1;
            Snd_PlaySe(1, 1);
        }
    } else if ((pad->gamePressed & PADG_SQUARE) && sZw->wk[sZw->wkCur].mine) {
        Ask_Open(ASK_WSDEL);
    } else if (pad->gamePressed & PADG_CIRCLE) {
        Port_WsLike(sZw->wkCur);
        Port_WsGet(sZw->wkCur, &sZw->wk[sZw->wkCur]);
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Zw_SetMode(ZW_WSHOP);
        Snd_PlaySe(1, 2);
    }
}

/* ---- putting one's own warrior up ---- */
static void Wk_UploadOpen(s32 at) {
    if (at < 0 || at >= sZw->count) {
        return;
    }
    sZw->wkUpAt = at;
    sZw->wkUpCur = 0;
    sZw->wkUpTags = 0;
    sZw->wkBio[0] = 0;
    Zw_SetMode(ZW_WSUPLOAD);
}

static void Wk_UploadDraw(void) {
    PortWarrior *w = &sZw->list[sZw->wkUpAt];
    char line[96];
    s32 i;

    Zw_Title("UPLOAD");
    Mx_Text(170.0f, 108.0f, "COMMUNITY WARRIORS / UPLOAD", 15.0f, 0.0f, MX_LAV, 0);
    Mx_Box(ZW_WINDOW, 34.0f, 140.0f, 300.0f, 530.0f, 18.0f, 0x80);
    Mx_Face(Lib_Face(w), 48.0f, 154.0f, 286.0f, 440.0f, 0x80);
    Mx_Rect(48.0f, 446.0f, 286.0f, 516.0f, Mx_Rgba(20, 26, 70, 0x80), Mx_Rgba(10, 14, 44, 0x80));
    Mx_Text(167.0f, 468.0f, w->name, 24.0f, 230.0f, MX_WHITE, 1);
    Mx_Text(167.0f, 498.0f, w->title, 15.0f, 230.0f, MX_GOLD, 1);
    Mx_Box(ZW_WINDOW, 312.0f, 140.0f, 900.0f, 470.0f, 18.0f, 0x80);
    Mx_Text(330.0f, 164.0f, "DETAILS", 20.0f, 0.0f, MX_GOLD, 0);
    sprintf(line, "Name: %s", w->name);
    Mx_Text(340.0f, 200.0f, line, 18.0f, 540.0f, MX_WHITE, 0);
    sprintf(line, "Title: %s", w->title);
    Mx_Text(340.0f, 232.0f, line, 18.0f, 540.0f, MX_WHITE, 0);
    if (sZw->wkUpCur == 0) {
        Zw_SelBar(330.0f, 252.0f, 890.0f, 330.0f);
    }
    Mx_Text(340.0f, 270.0f, "Bio:", 18.0f, 0.0f, sZw->wkUpCur == 0 ? MX_DARK : MX_GOLD, 0);
    if (sZw->wkBio[0]) {
        Wk_Wrap(sZw->wkBio, 400.0f, 272.0f, 24.0f, 54, 2, 15.0f);
    } else {
        Mx_Text(400.0f, 272.0f, "(Cross to write one)", 15.0f, 0.0f, sZw->wkUpCur == 0 ? MX_DARK : MX_LAV, 0);
    }
    Mx_Text(340.0f, 360.0f, "Tags:", 18.0f, 0.0f, MX_GOLD, 0);
    for (i = 0; i < 6; i++) {
        f32 x0 = 410.0f + (f32)(i % 3) * 160.0f, y0 = 344.0f + (f32)(i / 3) * 44.0f;
        s32 on = (sZw->wkUpTags >> i) & 1;

        Mx_Bar(on ? ZW_PILL : ZW_PILL_STEEL, x0, y0, x0 + 150.0f, y0 + 34.0f, 0x80);
        Mx_Text(x0 + 75.0f, y0 + 17.0f, kWkTag[i], 15.0f, 140.0f, on ? MX_DARK : MX_WHITE, 1);
        if (sZw->wkUpCur == 1 + i) {
            Mx_Frame(x0 - 3.0f, y0 - 3.0f, x0 + 153.0f, y0 + 37.0f, 3.0f, Mx_Rgba(255, 210, 80, 0x80));
        }
    }
    Mx_Box(ZW_WINDOW, 312.0f, 480.0f, 900.0f, 626.0f, 18.0f, 0x80);
    Mx_Text(330.0f, 502.0f, "READY CHECK", 18.0f, 0.0f, MX_GOLD, 0);
    Mx_Text(340.0f, 534.0f, w->problem ? "- It does not build yet: fix it first" : "+ Builds without errors", 16.0f, 540.0f, w->problem ? MX_LAV : MX_WHITE, 0);
    Mx_Text(340.0f, 562.0f, "+ Drawings, photos and costumes included", 16.0f, 540.0f, MX_WHITE, 0);
    Mx_Text(340.0f, 594.0f, "Only your recipe is uploaded, never the game's files.", 14.0f, 540.0f, MX_LAV, 0);
    if (w->mod >= 0) {
        PortModInfo info;

        Port_ModGetInfo(w->mod, &info);
        Zw_Stage(0x20000 + w->mod * 64 + (sZw->viewGen & 63), info.chara, w->mod, 1110.0f, 470.0f, 320.0f, 0);
    }
    Mx_Bar(sZw->wkUpCur == 7 ? ZW_PILL : ZW_PILL_STEEL, 960.0f, 530.0f, 1260.0f, 574.0f, 0x80);
    Mx_Text(1110.0f, 552.0f, "UPLOAD", 22.0f, 280.0f, sZw->wkUpCur == 7 ? MX_DARK : MX_WHITE, 1);
    Zw_Prompt(0, sZw->wkUpCur == 0 ? "Write" : sZw->wkUpCur == 7 ? "Upload" : "Tag");
    Zw_Prompt(7, "Upload");
    Zw_Prompt(2, "Cancel");
    Zw_Talk("Show the whole Other World what you made!");
}

static void Wk_UploadGo(void) {
    PortWarrior *w = &sZw->list[sZw->wkUpAt];
    char tags[64];
    s32 i;

    if (w->problem || w->mod < 0) {
        Zw_Note("It has to build first! Fix it in the editor, then upload.");
        Snd_PlaySe(1, 2);
        return;
    }
    tags[0] = 0;
    for (i = 0; i < 6; i++) {
        if ((sZw->wkUpTags >> i) & 1) {
            if (tags[0]) {
                sprintf(tags + Ps_Len(tags), ", ");
            }
            sprintf(tags + Ps_Len(tags), "%s", kWkTag[i]);
        }
    }
    if (Port_WsUpload(w->folder, sZw->wkBio, tags)) {
        Snd_PlaySe(1, 1);
        Wk_Open();
        Zw_Note("It's up! The whole Other World can get it now.");
    } else {
        Zw_Note("Hmm, the upload did not work. Try again?");
        Snd_PlaySe(1, 2);
    }
}

static void Wk_UploadUpdate(s32 live) {
    Pad *pad = &gPad[0];

    if (!live) {
        return;
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sZw->wkUpCur = sZw->wkUpCur == 0 ? 1 : sZw->wkUpCur <= 3 ? sZw->wkUpCur + 3 : 7;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sZw->wkUpCur = sZw->wkUpCur == 7 ? 4 : sZw->wkUpCur >= 4 ? sZw->wkUpCur - 3 : 0;
        Snd_PlaySe(1, 0);
    } else if ((pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) && sZw->wkUpCur >= 1 && sZw->wkUpCur < 7) {
        sZw->wkUpCur = sZw->wkUpCur % 3 == 0 ? sZw->wkUpCur : sZw->wkUpCur + 1;
        Snd_PlaySe(1, 0);
    } else if ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) && sZw->wkUpCur >= 1 && sZw->wkUpCur < 7) {
        sZw->wkUpCur = sZw->wkUpCur % 3 == 1 ? sZw->wkUpCur : sZw->wkUpCur - 1;
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & PADG_CROSS) {
        if (sZw->wkUpCur == 0) { /* the bio: the keyboard */
            sZw->nameWhich = 9;
            Ps_Copy(sZw->text, sZw->wkBio, sizeof(sZw->text));
            sZw->keyCur = 0;
            Zw_SetMode(ZW_NAME);
            Snd_PlaySe(1, 1);
        } else if (sZw->wkUpCur == 7) {
            Wk_UploadGo();
        } else {
            sZw->wkUpTags ^= 1 << (sZw->wkUpCur - 1);
            Snd_PlaySe(1, 0);
        }
    } else if (pad->gamePressed & PADG_START) {
        Wk_UploadGo();
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Zw_SetMode(ZW_LIBRARY);
        Snd_PlaySe(1, 2);
    }
}

/* ---------------------------------------------------------------------------------------------- touch */
static u32 sTapQueue[64]; /* presses still to give, one every third frame */
static s32 sTapLen, sTapAt2, sTapWait;

static void Zw_Queue(u32 bit, s32 times) {
    s32 i;

    for (i = 0; i < times && sTapLen < 64; i++) {
        sTapQueue[sTapLen++] = bit;
    }
}

static s32 Zw_Same(f32 a0, f32 a1, f32 b0, f32 b1) { /* do two spans share more than half of the smaller */
    f32 lo = a0 > b0 ? a0 : b0;
    f32 hi = a1 < b1 ? a1 : b1;
    f32 small = a1 - a0 < b1 - b0 ? a1 - a0 : b1 - b0;

    return hi - lo > small * 0.5f;
}

/* A finger on (x, y) of the mockup: the cursor's item is pressed (its ends: left / right, for a value); another item has the cursor walked to it (pressed with a second touch). */
static void Zw_TapAt(f32 x, f32 y) {
    s32 i, k, hit = -1, cur = -1, steps;
    f32 best = 1e9f;
    MxTapRect *h, *c;

    for (i = 0; i < gMxTapCount; i++) { /* the smallest thing under the finger */
        MxTapRect *r = &gMxTap[i];
        f32 area = (r->x1 - r->x0) * (r->y1 - r->y0);

        if (x >= r->x0 && x <= r->x1 && y >= r->y0 - 4.0f && y <= r->y1 + 4.0f && area <= best) {
            best = area;
            hit = i;
        }
    }
    if (hit < 0) {
        return;
    }
    h = &gMxTap[hit];
    if (sZw->mode == ZW_EDIT && sZw->ask == ASK_NONE && h->y0 >= 110.0f && h->y1 <= 170.0f && h->x1 - h->x0 < 130.0f) { /* a tab */
        k = (s32)((h->x0 - 30.0f - 16.0f + 20.0f) / 120.0f); /* (Ed_Draw: ED_X0 + 16 + k * 120) */
        if (k >= 0 && k < 4) {
            Zw_Queue(PADG_R1, (k - sZw->tab + 4) % 4);
        }
        return;
    }
    for (i = 0; i < gMxTapCount; i++) { /* the cursor's item, nearest the one touched */
        if (gMxTap[i].kind == 2 && (cur < 0 || Zw_Same(gMxTap[i].x0, gMxTap[i].x1, h->x0, h->x1))) {
            cur = i;
        }
    }
    for (i = 0; cur < 0 && i < gMxTapCount; i++) {
        if (gMxTap[i].kind == 3) {
            cur = i;
        }
    }
    if (cur < 0) {
        Zw_Queue(PADG_CROSS, 1);
        return;
    }
    c = &gMxTap[cur];
    if (Zw_Same(c->x0, c->x1, x - 1.0f, x + 1.0f) && Zw_Same(c->y0, c->y1, y - 1.0f, y + 1.0f)) {
        f32 w = c->x1 - c->x0;

        if (w > 200.0f && x < c->x0 + w * 0.2f) {
            Zw_Queue(PADG_LEFT, 1);
        } else if (w > 200.0f && x > c->x1 - w * 0.2f) {
            Zw_Queue(PADG_RIGHT, 1);
        } else {
            Zw_Queue(PADG_CROSS, 1);
        }
        return;
    }
    /* walk the cursor: first the rows (the things of its column between it and the one touched), then along the row */
    if (!Zw_Same(c->y0, c->y1, h->y0, h->y1)) {
        f32 from = (c->y0 + c->y1) * 0.5f, to = (h->y0 + h->y1) * 0.5f;
        f32 seen[64];
        s32 n = 0;

        for (i = 0; i < gMxTapCount; i++) {
            f32 mid = (gMxTap[i].y0 + gMxTap[i].y1) * 0.5f;

            if (!Zw_Same(gMxTap[i].x0, gMxTap[i].x1, h->x0, h->x1) || !(to > from ? (mid > from + 8.0f && mid <= to + 8.0f) : (mid < from - 8.0f && mid >= to - 8.0f))) {
                continue;
            }
            for (k = 0; k < n && !(mid > seen[k] - 12.0f && mid < seen[k] + 12.0f); k++) {
            }
            if (k == n && n < 64) {
                seen[n++] = mid;
            }
        }
        Zw_Queue(to > from ? PADG_DOWN : PADG_UP, n);
    }
    if (!Zw_Same(c->x0, c->x1, h->x0, h->x1) && (c->x1 - c->x0) < 260.0f) {
        steps = (s32)(((h->x0 + h->x1) - (c->x0 + c->x1)) * 0.5f / (c->x1 - c->x0 + 6.0f));
        Zw_Queue(steps > 0 ? PADG_RIGHT : PADG_LEFT, steps > 0 ? steps : -steps);
    }
}

static void Zw_Touch(void) {
    extern s32 Port_MenuTap(u32 out); /* port/src/gs/gs_input.c: the picture's 512 x 448 */
    static s32 at[2];

    if (sTapAt2 < sTapLen) {
        if (sTapWait > 0) {
            sTapWait--;
        } else {
            gPad[0].gamePressed |= sTapQueue[sTapAt2];
            gPad[0].gameRepeat |= sTapQueue[sTapAt2];
            sTapAt2++;
            sTapWait = 2;
        }
        return;
    }
    sTapLen = sTapAt2 = 0;
    if ((sZw->mode != ZW_LIBRARY || sZw->ask != ASK_NONE) && Port_MenuTap((u32)at)) { /* (the home screen has its own) */
        Zw_TapAt((f32)at[0] / Mx_X(1.0f), (f32)at[1] / Mx_Y(1.0f));
    }
}

/* ---------------------------------------------------------------------------------------------- the library (S1) */
#define LIB_X 34.0f
#define LIB_Y 168.0f /* the first row's middle */
#define LIB_PITCH 104.0f
#define LIB_ROWS 5
#define LIB_ROW_W 596.0f
#define LIB_ROW_H 94.0f
#define LIB_BTN_Y0 668.0f /* the buttons at the foot */
#define LIB_BTN_Y1 742.0f
#define LIB_BTN_X1 963.0f

static s32 Lib_Face(PortWarrior *v) {
    return v->render >= 0 ? v->render : v->body;
}

/* A prompt's pad button (Zw_Prompt's kinds, as Lib_Update reads them). */
static u32 Lib_Button(s32 kind) {
    switch (kind) {
    case 0:
        return PADG_CROSS;
    case 1:
        return PADG_SQUARE;
    case 2:
        return PADG_TRIANGLE;
    case 4:
        return PADG_CIRCLE;
    case 8:
        return PADG_R1;
    case 9:
        return PADG_L1;
    case 13:
        return PADG_SELECT;
    }
    return 0;
}

/* The home screen (Android): the Kaioshins' world, the list on the left, the warrior on the stage, Shin on the right,
   everything pressed by touch (Lib_Touch). The foot's buttons are the screen's prompts. */
static void Lib_Draw(void) {
    PortWarrior *w = sZw->cur > 0 ? &sZw->list[sZw->cur - 1] : NULL;
    s32 i;
    s32 n;
    f32 bw;
    char *say;
    char why[160];

    Mx_Pic(ZW_SHIN, 1044.0f, 132.0f, 1252.0f, 736.0f, 0x80);
    Zw_Title("Z WARRIORS");
    for (i = 0; i < LIB_ROWS && sZw->top + i <= sZw->count; i++) {
        s32 at = sZw->top + i;
        s32 sel = at == sZw->cur;
        f32 y = LIB_Y + (f32)i * LIB_PITCH;
        char *name;

        if (sel) {
            Zw_SelBar(LIB_X, y - LIB_ROW_H * 0.5f, LIB_X + LIB_ROW_W, y + LIB_ROW_H * 0.5f);
        } else {
            Mx_Bar(ZW_BAR, LIB_X, y - LIB_ROW_H * 0.5f, LIB_X + LIB_ROW_W, y + LIB_ROW_H * 0.5f, 0x80);
        }
        if (at == 0) {
            Mx_Text(LIB_X + 40.0f, y, "+", 48.0f, 0.0f, sel ? MX_DARK : MX_GOLD, 0);
            name = "Create a New Warrior";
        } else {
            PortWarrior *v = &sZw->list[at - 1];

            Zw_Chip(Lib_Face(v), LIB_X + 56.0f, y, 68.0f, sel);
            name = v->name[0] ? v->name : v->folder;
        }
        Mx_Text(LIB_X + 104.0f, y, name, 32.0f, LIB_ROW_W - 130.0f, sel ? MX_DARK : MX_WHITE, 0);
    }
    if (sZw->top > 0) {
        Mx_Fit(ZW_UP, LIB_X + LIB_ROW_W + 30.0f, LIB_Y, 40.0f, 0x80);
    }
    if (sZw->top + LIB_ROWS <= sZw->count) {
        Mx_Fit(ZW_DOWN, LIB_X + LIB_ROW_W + 30.0f, LIB_Y + LIB_PITCH * (LIB_ROWS - 1), 40.0f, 0x80);
    }
    if (w != NULL) {
        PortModInfo info;

        if (w->mod >= 0) {
            Port_ModGetInfo(w->mod, &info);
            Zw_Stage(0x20000 + w->mod * 64 + (sZw->viewGen & 63), info.chara, w->mod, 835.0f, 590.0f, 420.0f, 0);
        } else {
            Zw_Stage(0x30000 + w->body, w->body, -1, 835.0f, 590.0f, 420.0f, 0);
        }
    } else {
        Zw_Stage(-1, -1, -1, 835.0f, 590.0f, 420.0f, 0);
        Mx_Text(835.0f, 330.0f, "A NEW WARRIOR", 32.0f, 0.0f, MX_GOLD, 1);
    }
    Zw_Prompt(0, sZw->cur == 0 ? "Create" : "Edit");
    if (sZw->cur > 0) {
        Zw_Prompt(1, "Copy");
        Zw_Prompt(4, "Remove");
        Zw_Prompt(9, "Share");
    }
    Zw_Prompt(8, "Paste a code");
    Zw_Prompt(13, "Bug report");
    Zw_Prompt(2, "Back");
    n = sPromptCount;
    bw = (LIB_BTN_X1 - LIB_X - 8.0f * (f32)(n - 1)) / (f32)n;
    for (i = 0; i < n; i++) {
        f32 x = LIB_X + (bw + 8.0f) * (f32)i;

        Mx_Bar(i == 0 ? ZW_BAR_SEL : ZW_BAR, x, LIB_BTN_Y0, x + bw, LIB_BTN_Y1, 0x80);
        Mx_Text(x + bw * 0.5f, (LIB_BTN_Y0 + LIB_BTN_Y1) * 0.5f, sPrompt[i].label, 22.0f, bw - 16.0f, i == 0 ? MX_DARK : MX_WHITE, 1);
    }
    /* what Shin says */
    if (sZw->noteT > 0) {
        say = sZw->note;
    } else if (w != NULL && w->problem) {
        Port_WarriorError(sZw->cur - 1, why, sizeof(why));
        say = why[0] ? why : "This warrior could not be made. Check its parts!";
    } else {
        say = sZw->count == 0 ? "Welcome! Shall we make a warrior?" : w != NULL ? "A fine warrior you have there." : "A new warrior? Let us begin.";
    }
    Mx_Box(ZW_DIALOG, 940.0f, 24.0f, 1356.0f, 124.0f, 24.0f, 0x80);
    Mx_Say(1148.0f, 56.0f, say, 21.0f, 380.0f, MX_WHITE, 1);
}

/* A finger on a row chooses it (on the chosen one: presses it); on a button of the foot, presses it. */
static void Lib_Touch(Pad *pad) {
    extern s32 Port_MenuTap(u32 out); /* port/src/gs/gs_input.c: the picture's 512 x 448 */
    static s32 at[2];
    f32 x, y;
    s32 i;

    if (!Port_MenuTap((u32)at)) {
        return;
    }
    x = (f32)at[0] / Mx_X(1.0f);
    y = (f32)at[1] / Mx_Y(1.0f);
    if (y >= LIB_BTN_Y0 - 6.0f && sPromptCount > 0) {
        f32 bw = (LIB_BTN_X1 - LIB_X - 8.0f * (f32)(sPromptCount - 1)) / (f32)sPromptCount;

        for (i = 0; i < sPromptCount; i++) {
            f32 x0 = LIB_X + (bw + 8.0f) * (f32)i;

            if (x >= x0 && x <= x0 + bw) {
                pad->gamePressed |= Lib_Button(sPrompt[i].kind);
            }
        }
        return;
    }
    if (x >= LIB_X && x <= LIB_X + LIB_ROW_W + 60.0f) {
        for (i = 0; i < LIB_ROWS && sZw->top + i <= sZw->count; i++) {
            f32 mid = LIB_Y + (f32)i * LIB_PITCH;

            if (y >= mid - LIB_PITCH * 0.5f && y < mid + LIB_PITCH * 0.5f) {
                if (sZw->top + i == sZw->cur) {
                    pad->gamePressed |= PADG_CROSS;
                } else {
                    sZw->cur = sZw->top + i;
                    sZw->rest = 0;
                    Snd_PlaySe(1, 0);
                }
            }
        }
        if (y < LIB_Y - LIB_PITCH * 0.5f && sZw->cur > 0) { /* above the first row: up the list */
            sZw->cur--;
            sZw->rest = 0;
        }
    }
}

static void Zw_Edit(s32 at) {
    sZw->at = at;
    if (at >= 0) {
        sZw->w = sZw->list[at];
    } else { /* a new one starts as Goku (End) through and through */
        memset(&sZw->w, 0xFF, sizeof(sZw->w));
        memset(sZw->w.folder, 0, sizeof(sZw->w.folder));
        Ps_Copy(sZw->w.name, "New Warrior", sizeof(sZw->w.name));
        Ps_Copy(sZw->w.title, "Z Warrior", sizeof(sZw->w.title));
        sZw->w.body = 3;
        sZw->w.moves = 3;
        sZw->w.mod = -1;
        sZw->w.problem = 0;
        sZw->w.colours = 0;
        sZw->w.edits = 0;
        memset(sZw->w.moveName, 0, sizeof(sZw->w.moveName));
        memset(sZw->w.techColour, 0, sizeof(sZw->w.techColour));
        sZw->w.auraColour = 0;
        memset(sZw->w.partCostume, 0, sizeof(sZw->w.partCostume));
        memset(sZw->w.layerCostume, 0, sizeof(sZw->w.layerCostume));
        sZw->w.bodyCostume = 0;
        memset(sZw->w.look, 0, sizeof(sZw->w.look)); /* (the later fields: none, nothing, its own) */
        sZw->w.damage = 0;
        memset(sZw->w.xcos, 0, sizeof(sZw->w.xcos));
        memset(sZw->w.pose, 0, sizeof(sZw->w.pose));
        sZw->w.dynShade = 0;
        memset(sZw->w.formTo, 0, sizeof(sZw->w.formTo));
        memset(sZw->w.formCost, 0, sizeof(sZw->w.formCost));
        memset(sZw->w.formKind, 0, sizeof(sZw->w.formKind));
        memset(sZw->w.formAura, 0, sizeof(sZw->w.formAura));
        memset(sZw->w.labSet, 0, sizeof(sZw->w.labSet)); /* (the Technique Lab, shading by texture: none) */
        memset(sZw->w.lab, 0, sizeof(sZw->w.lab));
        memset(sZw->w.texShade, 0, sizeof(sZw->w.texShade));
        memset(sZw->w.skillColour, 0, sizeof(sZw->w.skillColour));
        sZw->w.bodyScale = 0;
        memset(sZw->w.glow, 0, sizeof(sZw->w.glow));
        memset(sZw->w.camSlot, 0, sizeof(sZw->w.camSlot));
    }
    sZw->dirty = at < 0;
    if (Port_WarriorDraft(&sZw->w, 1)) { /* (unsaved changes from last time: a crash, a quit) */
        sZw->dirty = 1;
        Zw_Note("Ah, your unsaved changes from last time! Picked up right where you left off.");
    }
    if (sZw->cos == NULL) {
        sZw->cos = Heap_Alloc(sizeof(PortWarrior) * 4, 0x20, 0, 2);
    }
    Port_PaintBegin(at >= 0 ? sZw->w.folder : "");
    sZw->picDraw = 0;
    sZw->cosCur = 0;
    sZw->cosHave = 1;
    if (sZw->cos != NULL) { /* its costumes of its own (costumeN/warrior.txt of its folder) */
        s32 k;

        for (k = 0; k < 4; k++) {
            sZw->cos[k] = sZw->w;
            if (k > 0 && at >= 0 && strcmp(sZw->w.look[k], "#own") == 0 && Port_WarriorCostumeRead(sZw->w.folder, k, &sZw->cos[k])) {
                sZw->cosHave |= 1 << k;
            }
        }
    }
    Zw_HistStart();
    sZw->draftT = ZW_DRAFT_EVERY;
    sZw->tab = 0;
    sZw->row[0] = 1;
    sZw->row[1] = 1;
    sZw->row[2] = 1;
    sZw->row[3] = 1;
    {
        s32 t, n2;

        for (t = 0; t < 4; t++) { /* (the first row of an open section, the cursor's) */
            const ZwRow *rows = Zw_Rows(t, &n2);

            while (sZw->row[t] < n2 - 1 && (rows[sZw->row[t]].kind == ROW_SECTION || !Zw_RowShown(rows, t, sZw->row[t]))) {
                sZw->row[t]++;
            }
            if (!Zw_RowShown(rows, t, sZw->row[t])) {
                sZw->row[t] = 0;
            }
        }
    }
    sZw->rowTop[0] = 0;
    sZw->rowTop[1] = 0;
    sZw->rowTop[2] = 0;
    sZw->previewMod = -1;
    sZw->previewDue = at < 0 ? 0 : -1;
    Zw_SetMode(ZW_EDIT);
}

static void Lib_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 was = sZw->cur;

    if (!live) {
        return;
    }
    if (sZw->importWait <= 0) {
        Lib_Touch(pad);
    }
    if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sZw->cur = sZw->cur > 0 ? sZw->cur - 1 : sZw->count;
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sZw->cur = sZw->cur < sZw->count ? sZw->cur + 1 : 0;
    }
    if (sZw->cur != was) {
        Snd_PlaySe(1, 0);
        sZw->rest = 0;
    }
    if (sZw->cur < sZw->top) {
        sZw->top = sZw->cur;
    }
    if (sZw->cur >= sZw->top + LIB_ROWS) {
        sZw->top = sZw->cur - LIB_ROWS + 1;
    }
    if (sZw->importWait > 0) { /* the clipboard's warrior code, read on the window's thread */
        s32 n = Port_WarriorImportPoll();

        if (n != -2) {
            sZw->importWait = 0;
            Zw_Reload();
            if (n >= 0) {
                sZw->cur = n + 1;
                Zw_Note("A warrior from a friend! Built from your own game, ready to fight.");
            } else {
                Zw_Note("No warrior code on the clipboard. Copy the whole line your friend sent (it starts with ZW1:).");
            }
        } else if (--sZw->importWait == 0) {
            Zw_Note("The clipboard didn't answer. Try again!");
        }
        return;
    }
    if ((pad->gamePressed & PADG_L1) && sZw->cur > 0) { /* its code onto the clipboard, for a friend */
        Zw_Note(Port_WarriorShare(sZw->cur - 1) ? "Code copied! Send it to a friend: they paste it here with R1."
                                                 : "That warrior's code couldn't be made.");
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_R1) { /* a warrior from the clipboard's code */
        Port_WarriorImportAsk();
        sZw->importWait = 30;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_CROSS) {
        Snd_PlaySe(1, 1);
        Zw_Edit(sZw->cur - 1);
    } else if ((pad->gamePressed & PADG_SQUARE) && sZw->cur > 0) {
        s32 n = Port_WarriorCopy(sZw->cur - 1);

        Zw_Reload();
        if (n >= 0) {
            sZw->cur = n + 1;
            Zw_Note("Two of them! Now that's a training partner.");
        }
        Snd_PlaySe(1, 1);
    } else if ((pad->gamePressed & PADG_CIRCLE) && sZw->cur > 0) {
        Ask_Open(ASK_REMOVE);
    } else if (pad->gamePressed & PADG_SELECT) { /* a bug report: the log, the version, this warrior's file */
        pad->gamePressed &= ~PADG_SELECT;
        Zw_Note(Port_MakeReport(sZw->cur > 0 ? sZw->list[sZw->cur - 1].folder : "") ? "A report, all zipped up in the reports folder! Send it with what happened."
                                                                                   : "Hmm, the report could not be made.");
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        sZw->leaving = 1;
        ColorFade_StartOut(0, 0, 0, 0x14);
        Snd_PlaySe(1, 2);
    }
}

/* ---------------------------------------------------------------------------------------------- the editor (S9) */
#define ED_X0 30.0f
#define ED_X1 650.0f
#define ED_Y 196.0f
#define ED_PITCH 36.0f
#define ED_LINES 11

static char *Ed_Line(s32 kind) {
    switch (kind) {
    case ROW_NAME: return "What do we call this fine fellow?";
    case ROW_TITLE: return "The second line on the select screen. Make it catchy!";
    case ROW_BODY: return "Pick the body your warrior fights in!";
    case ROW_WHOLEHEAD: return "Hair, head and face from one fighter, all at once!";
    case ROW_OUTFIT: return "Everything from the neck down, accessories too. A whole new outfit!";
    case ROW_PART + 0: return "Whose hair? Spikes, bangs, the lot! Circle layers another on top.";
    case ROW_PART + 1: return "Whose head and ears? The skull under the hair.";
    case ROW_PART + 2: return "Whose face? The eyes and expressions come along!";
    case ROW_PART + 3: return "Whose chest and shoulders? Armour, a gi, a bare chest...";
    case ROW_PART + 4: return "Whose waist? The belt or sash.";
    case ROW_PART + 5: return "Whose upper arms? Sleeves or muscles!";
    case ROW_PART + 6: return "Whose forearms? Wristbands, gloves' cuffs...";
    case ROW_PART + 7: return "Whose hands? Gloves, claws, fingers and all!";
    case ROW_PART + 8: return "Whose hips and thighs? Trousers, armour skirts...";
    case ROW_PART + 9: return "Whose shins? The bottom of the trousers or the boots' tops.";
    case ROW_PART + 10: return "Whose feet? A warrior needs good boots!";
    case ROW_PART + 11: return "Whose accessories? Capes, swords, halos, tails of cloth!";
    case ROW_SIZE + 0: return "Left and right: a bigger or smaller head!";
    case ROW_SIZE + 1: return "Left and right: wilder hair, or tamer!";
    case ROW_SIZE + 2: return "Left and right: the hands' size. Big fists hit hard!";
    case ROW_SIZE + 3: return "Left and right: the feet's size.";
    case ROW_SIZE + 4: return "Left and right: skinny arms or huge ones!";
    case ROW_SIZE + 5: return "Left and right: a broader chest, or a leaner one.";
    case ROW_SIZE + 6: return "Left and right: the legs' build. Never skip leg day!";
    case ROW_SURPRISE: return "A warrior made of random parts. Feeling lucky?";
    case ROW_RESETPARTS: return "Resets every part, size and layer to the body's default.";
    case ROW_VOICE: return "Every warrior needs a battle cry. Whose shall it be?";
    case ROW_COLOURS: return "A splash of colour! Paint any part of your warrior.";
    case ROW_FORM + 0: case ROW_FORM + 1: case ROW_FORM + 2: case ROW_FORM + 3:
        return "Transforms into another warrior you made. Left and right to choose. In battle, use the normal transform input.";
    case ROW_FORMCOST + 0: case ROW_FORMCOST + 1: case ROW_FORMCOST + 2: case ROW_FORMCOST + 3:
        return "How many ki bars the transformation costs.";
    case ROW_FORMKEY + 0: case ROW_FORMKEY + 1: case ROW_FORMKEY + 2: case ROW_FORMKEY + 3:
        return "In battle: build up the ki it costs, then press these buttons.";
    case ROW_FORMAURA + 0: case ROW_FORMAURA + 1: case ROW_FORMAURA + 2: case ROW_FORMAURA + 3:
        return "The aura colour when it transforms. Left and right to change it.";
    case ROW_FORMKIND + 0: case ROW_FORMKIND + 1: case ROW_FORMKIND + 2: case ROW_FORMKIND + 3:
        return "Power up, or return to base form. Give the transformed warrior a return form to change back.";
    case ROW_DYNSHADE: return "The glow transformed fighters have, like Super Saiyans. Left and right to change it.";
    case ROW_SHADEFROM: return "Borrow another fighter's shading and glow for every part. Try a Super Saiyan's on a base form!";
    case ROW_POSE: return "Pose your warrior's stance: turn the arms, legs, head... It still breathes!";
    case ROW_TEXTURES: return "Draw on your warrior's textures, or put a picture of your own on one! A mouse works too.";
    case ROW_SHADING: return "Shadow colours by part or by texture (L1 / R1 to switch on that screen).";
    case ROW_RENDER: return "The big portrait: Circle for a photo of your warrior, or drop a picture on the window!";
    case ROW_CHIP: return "The little face on the grids: crop it from the render, or Circle for a photo!";
    case ROW_PREVIEW: return "No guessing! Cross: the real character select. Circle: a test fight, health bar and all.";
    case ROW_HUD: return "The face on the health bar in battle. Or take a photo of your warrior's face!";
    case ROW_MOVES: return "Whose punches, kicks and rushes? The techniques below too, unless you swap them.";
    case ROW_EFFECTS: return "Whose ki blasts and charge? Each technique keeps its own effects.";
    case ROW_XL + 0: case ROW_XL + 1: case ROW_XL + 2: case ROW_XL + 3:
        return "Add another fighter's part on top, like a second hairstyle or a scouter. Cross: choose the fighter. Left and right: choose the part.";
    case ROW_COSTUMES: return "Four outfits of your own! Turn it on, then pick a costume below and change its look.";
    case ROW_COSEDIT: return sZw->cosHave > 1 ? "The costume you are changing now. Left and right for another; Square starts it again from Costume 1."
                                             : "Turn Costumes on first: then each costume gets its own look!";
    case ROW_DAMAGE: return "How your warrior looks beaten up, in every costume: the fighters' own, or wear painted on!";
    case ROW_LOOK + 0: return "Your warrior beaten up: the look of another warrior you made. Make one with torn clothes!";
    case ROW_LOOK + 1: case ROW_LOOK + 2: case ROW_LOOK + 3:
        return "Another outfit: wear the look of another warrior you made. Left and right to choose.";
    case ROW_SFX0: case ROW_SFX1: return "Borrow another Blast 1's look. Only the look changes, not what it does.";
    case ROW_ANIM + 0: return "Whose way of moving? Walking, dashing, jumping, flying up and down.";
    case ROW_ANIM + 1: return "How your warrior charges ki and powers up!";
    case ROW_ANIM + 2: return "Whose punches and kicks look? The combos and their damage stay the same.";
    case ROW_ANIM + 3: return "Whose way of throwing ki blasts? Only the motion, not the blasts themselves.";
    case ROW_ANIM + 4: return "Whose grabs and throws?";
    case ROW_ANIM + 5: return "Whose guard, dodges and vanishes?";
    case ROW_AURA: return "Whose aura? Its flames, the light on your warrior and the ki trails. Left and right or Select: its colour!";
    case ROW_STYLE: return "Whose health, stats and ki rates should your warrior have?";
    case ROW_STANCE: return "How your warrior stands: the pose and the battle idle. Its own, or anyone's!";
    case ROW_WORKSHOP: return "Sculpt every piece of your warrior: stretch it, move it or hide it.";
    case ROW_FX0: case ROW_FX1: case ROW_FX2: return "The look of this technique, from another of the same kind. A purple Kamehameha, anyone?";
    case ROW_CAM + 0: case ROW_CAM + 1: return "The camera for the start and charge, borrowed from any technique. Rushes keep their own.";
    case ROW_CAM + 2: return "The camera for the Ultimate's opening pose and charge, borrowed from any technique.";
    case ROW_CAM + 3: return "The camera once the Ultimate fires: the beam, the hit, the rush. Borrowed from any technique.";
    case ROW_FINISHERS: return "How the rush combos end. Pow!";
    case ROW_SECTION: return "Fold a section away, or open it again. Less scrolling!";
    case ROW_LABLIKE: return "Copy how another technique behaves: any Blast 2 or Ultimate, except rushes.";
    case ROW_LAB + 0: case ROW_LAB + 1: case ROW_LAB + 2: case ROW_LAB + 3: case ROW_LAB + 4:
        return "Fine-tune this technique: damage, hits, size, speed, unblockable and more.";
    }
    return "Whose technique should your warrior learn?";
}

/* What the highlighted part is, in the window under the model (most of all for the moves). */
static void Ed_Info(const ZwRow *row, f32 in) {
    PortWarrior *w = &sZw->w;
    s32 kind = row->kind;
    s32 a = (s32)(128.0f * in);
    char line[96];

    if (kind == ROW_NAME || kind == ROW_TITLE || kind == ROW_CHIP || kind == ROW_COLOURS || kind == ROW_RENDER || IS_SIZE(kind) ||
        Zw_Who(w, kind) < 0) {
        return;
    }
    Mx_Box(ZW_WINDOW, 700.0f, 572.0f - 20.0f * (1.0f - in), 1110.0f, 630.0f - 20.0f * (1.0f - in), 14.0f, a);
    Zw_Chip(Zw_Who(w, kind), 736.0f, 601.0f - 20.0f * (1.0f - in), 44.0f, 0);
    if (Zw_IsMove(kind)) {
        sprintf(line, "%s", Zw_MoveName(w, kind));
        Mx_TextA(768.0f, 590.0f - 20.0f * (1.0f - in), line, 19.0f, 330.0f, MX_GOLD, 0, a);
        sprintf(line, Zw_Own(w, kind) ? "Default (%s)" : "From %s", Zw_Name(Zw_Who(w, kind)));
        Mx_TextA(768.0f, 614.0f - 20.0f * (1.0f - in), line, 16.0f, 330.0f, MX_WHITE, 0, a);
    } else {
        Mx_TextA(768.0f, 590.0f - 20.0f * (1.0f - in), Zw_Name(Zw_Who(w, kind)), 19.0f, 330.0f, MX_GOLD, 0, a);
        Mx_TextA(768.0f, 614.0f - 20.0f * (1.0f - in),
                 Zw_Own(w, kind) ? "Default" : row->label, 16.0f, 330.0f,
                 MX_WHITE, 0, a);
    }
}

static s32 sZwEdBehind; /* (the editor drawn behind the picker) */

static void Ed_Draw(void) {
    PortWarrior *w = &sZw->w;
    s32 n;
    const ZwRow *rows = Zw_Rows(sZw->tab, &n);
    s32 cur = sZw->row[sZw->tab];
    s32 top = sZw->rowTop[sZw->tab];
    f32 slide = 1.0f - Zw_In(sZw->modeT, 12);
    f32 tabIn = Zw_In(sZw->tabT, 10);
    f32 ox = -60.0f * slide - 24.0f * (1.0f - tabIn);
    f32 want = ED_Y + (f32)(Zw_VisPos(rows, sZw->tab, cur) - top) * ED_PITCH;
    s32 i, vis = 0, shown = Zw_VisPos(rows, sZw->tab, n);
    s32 k;

    /* the warrior as it is being edited (first: the list goes over it; King Kai behind it) */
    Zw_Host(1250.0f, 380.0f, 250.0f, 1);
    Zw_EditStage(940.0f, 530.0f, 390.0f);
    Co_PaintTick(); /* (the colours, painted straight into it; the pieces' edits made on it) */
    Ws_ShapeTick();
    Zw_Title(w->name[0] ? w->name : "EDIT WARRIOR");
    /* the four tabs */
    for (k = 0; k < 4; k++) {
        static char *names[4] = {"WARRIOR", "PARTS", "MOVES", "FORMS"};
        f32 x0 = ED_X0 + 16.0f + (f32)k * 120.0f + ox;
        s32 lit = k == sZw->tab;

        Mx_Bar(lit ? ZW_PILL : ZW_PILL_STEEL, x0, 120.0f, x0 + 114.0f, 160.0f, 0x80);
        Mx_Text(x0 + 57.0f, 140.0f, names[k], 17.0f, 100.0f, lit ? MX_DARK : MX_WHITE, 1);
    }
    Mx_Button(9, ED_X0 + 500.0f + ox, 140.0f, 22.0f);
    Mx_Button(8, ED_X0 + 500.0f + ox + Mx_ButtonW(9, 22.0f) + 4.0f, 140.0f, 22.0f);
    Mx_Box(ZW_WINDOW, ED_X0 + ox, ED_Y - 30.0f, ED_X1 + ox, ED_Y + ED_PITCH * ED_LINES - 4.0f, 22.0f, (s32)(128.0f * (1.0f - slide)));
    sZw->rowY += (want - sZw->rowY) * 0.4f;
    for (k = 0; k < n; k++) {
        s32 r = k;
        const ZwRow *row = &rows[r];
        f32 y;
        s32 sel = r == cur;
        s32 a;

        if (!Zw_RowShown(rows, sZw->tab, r)) {
            continue;
        }
        i = vis++ - top;
        if (i < 0) {
            continue;
        }
        if (i >= ED_LINES) {
            break;
        }
        y = ED_Y + (f32)i * ED_PITCH;
        a = (s32)(128.0f * Zw_In(sZw->modeT - i, 10) * tabIn);
        if (row->kind == ROW_SECTION) { /* (a heading: folded "+", open "-"; lit under the cursor) */
            s32 sec = Zw_SecOf(rows, r);
            s32 folded = sec >= 0 && sec < 32 && ((sZwFold[sZw->tab] >> sec) & 1);

            if (sel) {
                Zw_SelBar(ED_X0 + 12.0f + ox, sZw->rowY - 17.0f, ED_X1 - 12.0f + ox, sZw->rowY + 17.0f);
            }
            Mx_Bar(ZW_SECTION, ED_X0 + 20.0f + ox, y - 14.0f, ED_X0 + 230.0f + ox, y + 14.0f, a);
            Mx_TextA(ED_X0 + 36.0f + ox, y, row->label, 18.0f, 160.0f, MX_DARK, 0, a);
            Mx_TextA(ED_X0 + 214.0f + ox, y, folded ? "+" : "-", 22.0f, 20.0f, MX_DARK, 1, a);
            if (folded) {
                s32 q, hid = 0;
                char more[24];

                for (q = r + 1; q < n && rows[q].kind != ROW_SECTION; q++) {
                    hid++;
                }
                sprintf(more, "%d hidden", (int)hid);
                Mx_TextA(ED_X0 + 250.0f + ox, y, more, 16.0f, 160.0f, sel ? MX_DARK : MX_LAV, 0, a);
            }
            continue;
        }
        if (sel) {
            Zw_SelBar(ED_X0 + 12.0f + ox, sZw->rowY - 17.0f, ED_X1 - 12.0f + ox, sZw->rowY + 17.0f);
        }
        Mx_TextA(ED_X0 + 40.0f + ox, y, row->label, 20.0f, 160.0f, sel ? MX_DARK : MX_LAV, 0, a);
        Mx_TextA(ED_X0 + 214.0f + ox, y, Zw_Value(row->kind), 20.0f, 330.0f, sel ? MX_DARK : MX_WHITE, 0, a);
        if (IS_SIZE(row->kind)) { /* a slider: 50% .. 180% */
            s32 v = w->size[row->kind - ROW_SIZE] > 0 ? w->size[row->kind - ROW_SIZE] : 100;
            f32 x0 = ED_X1 - 170.0f + ox;
            f32 k2 = (f32)(v - 50) / 130.0f;

            Mx_Rect(x0, y - 3.0f, x0 + 140.0f, y + 3.0f, Mx_Rgba(0x30, 0x20, 0x50, 0x80), Mx_Rgba(0x30, 0x20, 0x50, 0x80));
            Mx_Rect(x0, y - 3.0f, x0 + 140.0f * k2, y + 3.0f, Mx_Rgba(0xF0, 0xC0, 0x40, 0x80), Mx_Rgba(0xC0, 0x80, 0x20, 0x80));
            Mx_Rect(x0 + 140.0f * (50.0f / 130.0f) - 1.0f, y - 7.0f, x0 + 140.0f * (50.0f / 130.0f) + 1.0f, y + 7.0f, Mx_Rgba(0xFF, 0xFF, 0xFF, 0x60),
                    Mx_Rgba(0xFF, 0xFF, 0xFF, 0x60));
            Zw_Swatch(0x40C8F0, x0 + 140.0f * k2, y, sel ? 22.0f : 18.0f, sel);
        } else if (row->kind >= ROW_BODY && row->kind != ROW_CHIP && row->kind != ROW_COLOURS && Zw_Who(w, row->kind) >= 0) {
            Zw_Chip(Zw_Who(w, row->kind), ED_X1 - 42.0f + ox, y, 32.0f, sel);
        } else if (row->kind == ROW_COLOURS && w->colours > 0) {
            s32 c;
            for (c = 0; c < w->colours && c < 4; c++) {
                Zw_Swatch(w->colourRgb[c], ED_X1 - 42.0f - 26.0f * (f32)c + ox, y, 24.0f, 0);
            }
        }
    }
    if (top > 0) {
        Mx_Fit(ZW_UP, ED_X1 - 70.0f + ox, ED_Y - 40.0f + 3.0f * Zw_Beat(36), 26.0f, 0x80);
    }
    if (top + ED_LINES < shown) {
        Mx_Fit(ZW_DOWN, ED_X1 - 70.0f + ox, ED_Y + ED_PITCH * ED_LINES + 6.0f - 3.0f * Zw_Beat(36), 26.0f, 0x80);
    }
    if (!sZwEdBehind) {
        Ed_Info(&rows[cur], Zw_In(sZw->rest, 10));
    }
    if (sZw->dirty) {
        Mx_Bar(ZW_PILL, 1150.0f, 110.0f, 1340.0f, 146.0f, 0x60 + (s32)(0x20 * Zw_Beat(60)));
        Mx_Text(1245.0f, 128.0f, "NOT SAVED", 17.0f, 170.0f, MX_DARK, 1);
    }
    if (sZw->cosHave > 1) { /* (the costume being changed, always in sight) */
        char tag[32];

        sprintf(tag, "EDITING COSTUME %d", (int)sZw->cosCur + 1);
        Mx_Text(1340.0f, 186.0f, tag, 17.0f, 0.0f, MX_GOLD, 2);
    }
    if (rows[cur].kind == ROW_SECTION) {
        s32 sec = Zw_SecOf(rows, cur);

        Zw_Prompt(0, sec >= 0 && sec < 32 && ((sZwFold[sZw->tab] >> sec) & 1) ? "Open section" : "Fold section");
    } else if (IS_SIZE(rows[cur].kind)) {
        Zw_Prompt(3, "Size");
    } else {
        if (rows[cur].kind == ROW_PREVIEW) {
            Zw_Prompt(0, "Character select");
        } else {
            Zw_Prompt(0, rows[cur].kind == ROW_CHIP ? "Crop" : rows[cur].kind == ROW_COLOURS ? "Paint" : Zw_IsMove(rows[cur].kind) ? "Pick technique" : "Change");
        }
    }
    if (Zw_IsMove(rows[cur].kind) || rows[cur].kind == ROW_RENDER || rows[cur].kind == ROW_HUD || rows[cur].kind == ROW_VOICE ||
        IS_LOOKPART(rows[cur].kind) || IS_SIZE(rows[cur].kind) || rows[cur].kind == ROW_STANCE ||
        (rows[cur].kind >= ROW_FX0 && rows[cur].kind <= ROW_FX2) || (rows[cur].kind >= ROW_CAM && rows[cur].kind <= ROW_CAM_END) || rows[cur].kind == ROW_SFX0 || rows[cur].kind == ROW_SFX1 ||
        (rows[cur].kind >= ROW_XL && rows[cur].kind <= ROW_XL_END) || rows[cur].kind == ROW_SHADEFROM || rows[cur].kind == ROW_DYNSHADE || (rows[cur].kind >= ROW_FORM && rows[cur].kind <= ROW_FORM_END) || rows[cur].kind == ROW_DAMAGE || (rows[cur].kind == ROW_COSEDIT && sZw->cosCur > 0) || rows[cur].kind == ROW_EFFECTS || rows[cur].kind == ROW_AURA || (rows[cur].kind >= ROW_ANIM && rows[cur].kind <= ROW_ANIM_END) || rows[cur].kind == ROW_STYLE || rows[cur].kind == ROW_FINISHERS ||
        rows[cur].kind == ROW_COLOURS) {
        Zw_Prompt(1, "Default");
    }
    if ((rows[cur].kind >= ROW_TECH0 && rows[cur].kind <= ROW_TECH2) || rows[cur].kind == ROW_AURA || rows[cur].kind == ROW_SKILL0 ||
        rows[cur].kind == ROW_SKILL1) {
        Zw_Prompt(13, rows[cur].kind == ROW_AURA ? "Aura colour" : "Colour");
    } else if (IS_PART(rows[cur].kind) || IS_SIZE(rows[cur].kind)) {
        Zw_Prompt(13, "Lock");
    }
    if (sZw->histN > 1) { /* (undo / redo, top right) */
        Mx_Text(1340.0f, 160.0f, sZw->histPos > 0 && sZw->histPos < sZw->histN - 1 ? "L3 undo   R3 redo" : sZw->histPos > 0 ? "L3 undo" : "R3 redo",
                15.0f, 0.0f, MX_LAV, 2);
    }
    if (IS_PART(rows[cur].kind)) {
        Zw_Prompt(4, "Layer");
    } else if (Zw_IsMove(rows[cur].kind)) {
        Zw_Prompt(4, "Rename");
    } else if (rows[cur].kind == ROW_RENDER || rows[cur].kind == ROW_CHIP) {
        Zw_Prompt(4, "Photo of the model");
    } else if (rows[cur].kind == ROW_PREVIEW) {
        Zw_Prompt(4, "Test fight");
    } else if (rows[cur].kind == ROW_HUD) {
        Zw_Prompt(4, "Photo of the model");
    }
    Zw_Prompt(14, "Tab");
    Zw_Prompt(7, "Save");
    Zw_Prompt(2, "Back");
    if (sZw->saving) {
        if (!sZwEdBehind) {
            Zw_Talk("Building your warrior... hold still!");
        }
    } else {
        if (!sZwEdBehind) {
            Zw_Talk(Ed_Line(rows[cur].kind));
        }
    }
}

static void Zw_Pick(const ZwRow *row);
static void Ph_Open(s32 kind);
static void Zw_LeaveFor(s32 to);
static void Tx_Open(void);
static void Po_Open(void);
static void Lb_Open(s32 t);

/* Picture file `path` as the saved warrior's render. */
static void Zw_UseArt(char *path) {
    if (Port_ModArtSource(sZw->w.mod, path) && Port_ModArtFit(sZw->w.mod)) {
        Zw_Note("Ooh, a portrait of your own! Very artistic.");
        sZw->viewGen++;
    } else {
        Zw_Note("That picture could not be used. Try a PNG or JPG.");
    }
}

/* The fields of a warrior that are its look (what a costume of its own has): body, parts, layers, sizes, colours, edits. */
static void Zw_LookCopy(PortWarrior *to, const PortWarrior *from) {
    to->body = from->body;
    to->bodyCostume = from->bodyCostume;
    memcpy(to->part, from->part, sizeof(to->part));
    memcpy(to->partCostume, from->partCostume, sizeof(to->partCostume));
    memcpy(to->layer, from->layer, sizeof(to->layer));
    memcpy(to->layerCostume, from->layerCostume, sizeof(to->layerCostume));
    memcpy(to->size, from->size, sizeof(to->size));
    to->colours = from->colours;
    memcpy(to->colourKey, from->colourKey, sizeof(to->colourKey));
    memcpy(to->colourRgb, from->colourRgb, sizeof(to->colourRgb));
    memcpy(to->colourPart, from->colourPart, sizeof(to->colourPart));
    to->edits = from->edits;
    memcpy(to->edit, from->edit, sizeof(to->edit));
    memcpy(to->xlayer, from->xlayer, sizeof(to->xlayer));
    memcpy(to->xslot, from->xslot, sizeof(to->xslot));
    memcpy(to->xcos, from->xcos, sizeof(to->xcos));
}

/* Undo starts again from the warrior as it is now. */
static void Zw_HistStart(void) {
    if (sZw->hist == NULL) {
        sZw->hist = Heap_Alloc(sizeof(PortWarrior) * (ZW_UNDO + 1), 0x20, 0, 2);
    }
    if (sZw->hist != NULL) {
        sZw->hist[0] = sZw->w;
        sZw->hist[ZW_UNDO] = sZw->w;
    }
    sZw->histN = 1;
    sZw->histPos = 0;
    sZw->histQuiet = 0;
}

/* Costume `to` edited from now on: the one being edited kept, `to` from costume 1 if it has no look of its own yet. */
static void Zw_CosSwitch(s32 to) {
    if (sZw->cos == NULL || to < 0 || to > 3) {
        return;
    }
    Zw_LookCopy(&sZw->cos[sZw->cosCur], &sZw->w);
    if (!((sZw->cosHave >> to) & 1)) {
        Zw_LookCopy(&sZw->cos[to], &sZw->cos[0]);
        sZw->cosHave |= 1 << to;
    }
    Zw_LookCopy(&sZw->w, &sZw->cos[to]);
    sZw->cosCur = to;
    Zw_HistStart(); /* (undo stays within one costume) */
    Zw_Changed();
}

static void Ed_Save(void) {
    s32 n, k;
    static PortWarrior out;

    out = sZw->w;
    if (sZw->cos != NULL) {
        Zw_LookCopy(&sZw->cos[sZw->cosCur], &sZw->w);
        Zw_LookCopy(&out, &sZw->cos[0]);
        out.look[0][0] = 0; /* (a linked damaged look of before: the Battle damage row now) */
        for (k = 1; k < 4; k++) {
            if (!((sZw->cosHave >> k) & 1) && sZw->w.look[k][0] != 0 && strcmp(sZw->w.look[k], "#own") != 0) {
                continue; /* (another warrior's look worn as this costume, as warrior.txt can say: kept) */
            }
            Ps_Copy(out.look[k], (sZw->cosHave >> k) & 1 ? "#own" : "", 64);
        }
        Port_WarriorCostumesNext(sZw->cos, sZw->cosHave);
    }
    n = Port_WarriorSave(sZw->at, &out);
    Zw_Reload();
    if (n >= 0) {
        sZw->at = n;
        sZw->w = sZw->list[n];
        if (sZw->cos != NULL) { /* (the costume being edited stays on the screen) */
            Zw_LookCopy(&sZw->w, &sZw->cos[sZw->cosCur]);
        }
        sZw->cur = n + 1;
        sZw->dirty = 0;
        sZw->previewDue = -1;
        sZw->viewGen++;
        Port_WarriorDraft(&sZw->w, 2);
        Zw_Note(sZw->w.problem ? "Hmm, something went wrong. Look at the library for why." : "Saved! Your warrior is ready to fight.");
        if (sZw->artPending[0] != 0 && sZw->w.mod >= 0) {
            Zw_UseArt(sZw->artPending);
        }
        sZw->artPending[0] = 0;
    } else {
        sZw->artPending[0] = 0;
        Zw_Note("It could not be written. Is the mods folder read-only?");
    }
}

static void Ed_Default(s32 kind) {
    PortWarrior *w = &sZw->w;

    switch (kind) {
    case ROW_VOICE: w->voice = -1; break;
    case ROW_RENDER:
        w->render = -1;
        if (w->mod >= 0) {
            Port_ModArtReset(w->mod); /* (and no picture of one's own) */
        }
        break;
    case ROW_HUD:
        w->hud = -1;
        if (w->mod >= 0) {
            Port_WarriorHudPhoto(w->mod, -1, 0, 0); /* (and no photo of its own) */
        }
        break;
    case ROW_CHIP:
        if (w->mod >= 0) {
            Port_WarriorPortraitPhoto(w->mod, 9 | 2 << 8, 0, 0); /* (no photo of its own: cropped from the render again) */
        }
        break;
    case ROW_STANCE: w->stance = -1; break;
    case ROW_FX0: case ROW_FX1: case ROW_FX2: w->techFx[kind - ROW_FX0] = -1; break;
    case ROW_CAM + 0: case ROW_CAM + 1: case ROW_CAM + 2: case ROW_CAM + 3: w->camFrom[kind - ROW_CAM] = -1; break;
    case ROW_LAB + 0: case ROW_LAB + 1: case ROW_LAB + 2: case ROW_LAB + 3: case ROW_LAB + 4: w->labSet[kind - ROW_LAB] = 0; break;
    case ROW_SFX0: case ROW_SFX1: w->skillFx[kind - ROW_SFX0] = -1; break;
    case ROW_LOOK + 0: case ROW_LOOK + 1: case ROW_LOOK + 2: case ROW_LOOK + 3: w->look[kind - ROW_LOOK][0] = 0; break;
    case ROW_DAMAGE: w->damage = 0; break;
    case ROW_SHADEFROM: w->shadeFrom = -1; break;
    case ROW_DYNSHADE: w->dynShade = 0; break;
    case ROW_FORM + 0: case ROW_FORM + 1: case ROW_FORM + 2: case ROW_FORM + 3: w->formTo[kind - ROW_FORM][0] = 0; break;
    case ROW_XL + 0: case ROW_XL + 1: case ROW_XL + 2: case ROW_XL + 3: w->xlayer[kind - ROW_XL] = -1; break;
    case ROW_COSEDIT:
        if (sZw->cosCur > 0 && sZw->cos != NULL) { /* (this costume again from costume 1) */
            Zw_LookCopy(w, &sZw->cos[0]);
            Zw_Note("This costume starts again from Costume 1's look.");
        } else {
            Zw_Note("Costume 1 is the warrior's own look: change it with the parts!");
            return;
        }
        break;
    case ROW_EFFECTS: w->effects = -1; break;
    case ROW_ANIM + 0: case ROW_ANIM + 1: case ROW_ANIM + 2: case ROW_ANIM + 3: case ROW_ANIM + 4: case ROW_ANIM + 5:
    case ROW_ANIM + 6: case ROW_ANIM + 7: case ROW_ANIM + 8:
        *Zw_AnimOf(w, kind - ROW_ANIM) = -1;
        break;
    case ROW_AURA:
        w->aura = -1;
        w->auraColour = 0;
        break;
    case ROW_STYLE: w->style = -1; break;
    case ROW_FINISHERS: w->finishers = -1; break;
    case ROW_COLOURS: w->colours = 0; break;
    case ROW_SKILL0: case ROW_SKILL1:
        w->skill[kind - ROW_SKILL0] = -1;
        w->moveName[kind - ROW_SKILL0][0] = 0;
        break;
    case ROW_TECH0: case ROW_TECH1: case ROW_TECH2:
        w->tech[kind - ROW_TECH0] = -1;
        w->moveName[2 + kind - ROW_TECH0][0] = 0;
        w->techColour[kind - ROW_TECH0] = 0;
        break;
    default:
        if (IS_LOOKPART(kind)) {
            s32 first, n, i;

            Zw_Slots(kind, &first, &n);
            for (i = first; i < first + n; i++) {
                w->part[i] = -1;
                w->layer[i] = -1;
            }
        } else if (IS_SIZE(kind)) {
            w->size[kind - ROW_SIZE] = 0;
        } else if (kind == ROW_BODYSCALE) {
            w->bodyScale = 0;
        } else {
            return;
        }
        break;
    }
    Zw_Changed();
    Zw_Note("Back to the way it was!");
    Snd_PlaySe(1, 2);
}

static void Zw_ArtLoad(s32 mod);

/* Saving: a frame that says so, then the save; leaving 2: back to the library afterwards. */
static s32 Zw_SaveTick(void) {
    if (!sZw->saving) {
        return 0;
    }
    if (--sZw->saving == 0) {
        Ed_Save();
        if (sZw->leaving == 2) {
            sZw->leaving = 0;
            Zw_SetMode(ZW_LIBRARY);
        }
    }
    return 1;
}

static void Ed_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 n;
    const ZwRow *rows = Zw_Rows(sZw->tab, &n);
    s32 *cur = &sZw->row[sZw->tab];
    s32 *top = &sZw->rowTop[sZw->tab];
    s32 was = *cur;
    s32 kind = rows[*cur].kind;
    char path[700];

    if (Zw_SaveTick() || !live) {
        return;
    }
    if (Port_DropTake(path)) { /* a picture dropped on the window or chosen: the warrior's own render */
        if (sZw->w.mod < 0 || sZw->dirty) { /* (saved first, then used: Ed_Save) */
            Ps_Copy(sZw->artPending, path, sizeof(sZw->artPending));
            sZw->saving = 2;
            Zw_Note("Saving your warrior first, then your picture goes on!");
        } else {
            Zw_UseArt(path);
        }
    }
    if (pad->gamePressed & (PADG_L1 | PADG_R1)) {
        sZw->tab = (sZw->tab + ((pad->gamePressed & PADG_R1) ? 1 : 3)) % 4;
        sZw->tabT = 0;
        sZw->rowY = ED_Y + (f32)(Zw_VisPos(Zw_Rows(sZw->tab, &n), sZw->tab, sZw->row[sZw->tab]) - sZw->rowTop[sZw->tab]) * ED_PITCH;
        Snd_PlaySe(1, 0);
        return;
    }
    if (pad->gameRepeat & (PADG_UP | PADG_UP2)) { /* (over the rows shown, headings too) */
        do {
            *cur = *cur > 0 ? *cur - 1 : n - 1;
        } while (!Zw_RowShown(rows, sZw->tab, *cur));
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        do {
            *cur = *cur < n - 1 ? *cur + 1 : 0;
        } while (!Zw_RowShown(rows, sZw->tab, *cur));
    }
    if (*cur != was) {
        Snd_PlaySe(1, 0);
        sZw->rest = 0;
    }
    {
        s32 v = Zw_VisPos(rows, sZw->tab, *cur);

        if (v - 1 < *top) { /* (its section heading stays in sight) */
            *top = v - 1 < 0 ? 0 : v - 1;
        }
        if (v >= *top + ED_LINES) {
            *top = v - ED_LINES + 1;
        }
    }
    if (rows[*cur].kind == ROW_SECTION) { /* a heading: X folds or opens its section */
        if (pad->gamePressed & (PADG_CROSS | PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) {
            s32 sec = Zw_SecOf(rows, *cur);

            if (sec >= 0 && sec < 32) {
                sZwFold[sZw->tab] ^= 1 << sec;
                Snd_PlaySe(1, (sZwFold[sZw->tab] >> sec) & 1 ? 2 : 1);
            }
            {
                s32 shown = Zw_VisPos(rows, sZw->tab, n);

                *top = *top > shown - ED_LINES ? (shown - ED_LINES < 0 ? 0 : shown - ED_LINES) : *top;
            }
        } else if (pad->gamePressed & PADG_START) { /* (saving and leaving work on a heading too) */
            sZw->saving = 2;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_TRIANGLE) {
            if (sZw->dirty) {
                Ask_Open(ASK_LEAVE);
            } else {
                Zw_SetMode(ZW_LIBRARY);
                Snd_PlaySe(1, 2);
            }
        }
        return;
    }
    if (kind >= ROW_GLOW && kind <= ROW_GLOW_END && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) {
        static const u32 kGlow[8] = {0, 0x1E1EF0, 0xF07828, 0x28C8F0, 0xF03CB4, 0x3CF050, 0xF0F0F0, 0x2828A0}; /* (none, Kaio-ken red,
                                                                                                   blue, gold, purple, green, white, a dim red) */
        s32 *g = &sZw->w.glow[kind - ROW_GLOW], k;

        for (k = 0; k < 8 && (u32)(*g > 0 ? (*g & 0xFFFFFF) : 0) != kGlow[k]; k++) {
        }
        k = k == 8 ? 0 : k;
        k = (k + ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 7 : 1)) % 8;
        *g = kGlow[k] != 0 ? (s32)(0x1000000 | kGlow[k]) : 0;
        Zw_Changed();
        Snd_PlaySe(1, 0);
    } else if (kind >= ROW_GLOW && kind <= ROW_GLOW_END && (pad->gamePressed & PADG_CROSS)) { /* any colour (its brightness: how strong) */
        s32 c = sZw->w.glow[kind - ROW_GLOW];

        sZw->fxTarget = 9 + kind - ROW_GLOW;
        sZw->fxBack = c;
        Co_ToHsv(c > 0 ? (u32)c & 0xFFFFFF : 0x1E1EF0, &sZw->pkH, &sZw->pkS, &sZw->pkV);
        sZw->pkRow = 0;
        sZw->pkSwatch = 0;
        Zw_SetMode(ZW_FXCOL);
        Snd_PlaySe(1, 1);
        return;
    }
    if (kind == ROW_BODYSCALE && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) { /* the whole size, 5% a step */
        s32 now = sZw->w.bodyScale > 0 ? sZw->w.bodyScale : 100;

        now += (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? -5 : 5;
        now = now < 50 ? 50 : now > 250 ? 250 : now;
        sZw->w.bodyScale = now == 100 ? 0 : now;
        Zw_Changed();
        Snd_PlaySe(1, 0);
    }
    if (IS_SIZE(kind) && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) { /* a size, 5% a step */
        s32 *v = &sZw->w.size[kind - ROW_SIZE];
        s32 now = *v > 0 ? *v : 100;

        now += (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? -5 : 5;
        now = now < 50 ? 50 : now > 180 ? 180 : now;
        if (now != (*v > 0 ? *v : 100)) {
            *v = now;
            Zw_Changed();
            Snd_PlaySe(1, 0);
        }
    }
    if ((pad->gamePressed & PADG_SELECT) && (IS_PART(kind) || IS_SIZE(kind))) { /* locked: "Surprise me!" leaves it */
        s32 bit = IS_PART(kind) ? 1 << (kind - ROW_PART) : 1 << (16 + kind - ROW_SIZE);

        sZw->lockMask ^= bit;
        Zw_Note(sZw->lockMask & bit ? "Locked! Surprises leave this one alone." : "Unlocked. Anything goes!");
        pad->gamePressed &= ~PADG_SELECT; /* (not the T-pose) */
        Snd_PlaySe(1, 1);
    } else if ((pad->gamePressed & PADG_SELECT) && ((kind >= ROW_TECH0 && kind <= ROW_TECH2) || kind == ROW_AURA || kind == ROW_SKILL0 ||
                                                    kind == ROW_SKILL1)) { /* any colour */
        s32 c;

        sZw->fxTarget = kind == ROW_AURA ? 3 : kind == ROW_SKILL0 || kind == ROW_SKILL1 ? 6 + kind - ROW_SKILL0 : kind - ROW_TECH0;
        c = sZw->fxTarget >= 6 ? sZw->w.skillColour[sZw->fxTarget - 6] : sZw->fxTarget == 3 ? sZw->w.auraColour : sZw->w.techColour[sZw->fxTarget];
        sZw->fxBack = c;
        Co_ToHsv(c > 0 ? (u32)c & 0xFFFFFF : 0x30C8F0, &sZw->pkH, &sZw->pkS, &sZw->pkV);
        sZw->pkRow = 0;
        sZw->pkSwatch = 0;
        pad->gamePressed &= ~PADG_SELECT;
        Zw_SetMode(ZW_FXCOL);
        Snd_PlaySe(1, 1);
        return;
    }
    if (kind >= ROW_XL && kind <= ROW_XL_END && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) {
        s32 x = kind - ROW_XL, step = (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 12 : 1;

        if (sZw->w.xlayer[x] < 0) {
            Zw_Note("Choose whose first: Cross!");
            Snd_PlaySe(1, 2);
        } else {
            sZw->w.xslot[x] = ((sZw->w.xslot[x] < 0 ? 0 : sZw->w.xslot[x]) + step) % 13;
            Zw_Changed();
            Snd_PlaySe(1, 0);
        }
    }
    if (kind >= ROW_FORM && kind <= ROW_FORM_END &&
        ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) || (pad->gamePressed & PADG_CROSS))) { /* the warriors round */
        char *to = sZw->w.formTo[kind - ROW_FORM];
        s32 at = -1, i, step = (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? -1 : 1, n = sZw->count + 1, tries;

        for (i = 0; i < sZw->count; i++) {
            at = strcmp(sZw->list[i].folder, to) == 0 ? i : at;
        }
        for (tries = 0, i = at; tries < n; tries++) {
            i = (i + 1 + step + n) % n - 1;
            if (i < 0 || (sZw->list[i].folder[0] != '_' && strcmp(sZw->list[i].folder, sZw->w.folder) != 0)) {
                break;
            }
        }
        if (i < 0) {
            to[0] = 0;
        } else {
            Ps_Copy(to, sZw->list[i].folder, 64);
            if (sZw->w.formCost[kind - ROW_FORM] <= 0) {
                sZw->w.formCost[kind - ROW_FORM] = 1;
            }
        }
        pad->gamePressed &= ~PADG_CROSS;
        sZw->dirty = 1;
        Zw_Note(sZw->count <= 1 ? "Make another warrior first: that one is the form it turns into!" : "Save to make it so. Give that warrior a way back too!");
        Snd_PlaySe(1, 0);
    }
    if (kind >= ROW_FORMCOST && kind <= ROW_FORMCOST_END && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) {
        s32 *c = &sZw->w.formCost[kind - ROW_FORMCOST];

        *c = (*c + ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 7 : 1)) % 8;
        sZw->dirty = 1;
        Snd_PlaySe(1, 0);
    }
    if (kind >= ROW_FORMAURA && kind <= ROW_FORMAURA_END && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) {
        s32 f = kind - ROW_FORMAURA, c, i;

        for (c = 0, i = 1; i < 11; i++) {
            c = sZw->w.formAura[f] == (s32)(0x1000000 | kTechColour[i]) ? i : c;
        }
        c = (c + ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 10 : 1)) % 11;
        sZw->w.formAura[f] = c > 0 ? (s32)(0x1000000 | kTechColour[c]) : 0;
        sZw->dirty = 1;
        Snd_PlaySe(1, 0);
    }
    if (kind >= ROW_FORMKIND && kind <= ROW_FORMKIND_END &&
        ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) || (pad->gamePressed & PADG_CROSS))) {
        pad->gamePressed &= ~PADG_CROSS;
        sZw->w.formKind[kind - ROW_FORMKIND] ^= 1;
        sZw->dirty = 1;
        Snd_PlaySe(1, 0);
    }
    if (kind == ROW_DYNSHADE && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2) || (pad->gamePressed & PADG_CROSS))) {
        pad->gamePressed &= ~PADG_CROSS;
        sZw->w.dynShade = (sZw->w.dynShade + ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 2 : 1)) % 3;
        Zw_Changed();
        Snd_PlaySe(1, 0);
    }
    if (kind == ROW_COSTUMES && (pad->gamePressed & (PADG_CROSS | PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) {
        pad->gamePressed &= ~PADG_CROSS;
        Ask_Open(sZw->cosHave > 1 ? ASK_COSOFF : ASK_COSON);
        return;
    }
    if (kind == ROW_COSEDIT && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2) || (pad->gamePressed & PADG_CROSS))) {
        pad->gamePressed &= ~PADG_CROSS;
        if (sZw->cosHave <= 1) {
            Ask_Open(ASK_COSON);
            return;
        }
        Zw_CosSwitch((sZw->cosCur + ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 3 : 1)) % 4);
        Snd_PlaySe(1, 0);
    }
    if (kind == ROW_DAMAGE && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2) || (pad->gamePressed & PADG_CROSS))) {
        static char *notes[4] = {"Beaten up the way the fighters are on the disc.", "A few scuffs and dirt: still standing proud!",
                                 "Torn and battered: a real fight happened here!", "Not a scratch, ever. Show-off!"};
        s32 d = sZw->w.damage > 0 && sZw->w.damage < 4 ? sZw->w.damage : 0;

        pad->gamePressed &= ~PADG_CROSS;
        d = (d + ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 3 : 1)) % 4;
        sZw->w.damage = d;
        sZw->dirty = 1;
        Zw_Note(notes[d]);
        Snd_PlaySe(1, 0);
    }
    if (kind >= ROW_LOOK && kind <= ROW_LOOK_END &&
        ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) || (pad->gamePressed & PADG_CROSS))) { /* another warrior's look, round */
        char *look = sZw->w.look[kind - ROW_LOOK];
        s32 at = -1, i, step = (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? -1 : 1, n = sZw->count + 1, tries;

        for (i = 0; i < sZw->count; i++) {
            at = strcmp(sZw->list[i].folder, look) == 0 ? i : at;
        }
        for (tries = 0, i = at; tries < n; tries++) { /* (the next one that is another warrior; -1: its own) */
            i = (i + 1 + step + n) % n - 1;
            if (i < 0 || (sZw->list[i].folder[0] != '_' && strcmp(sZw->list[i].folder, sZw->w.folder) != 0)) {
                break;
            }
        }
        if (i < 0) {
            look[0] = 0;
        } else {
            Ps_Copy(look, sZw->list[i].folder, 64);
        }
        pad->gamePressed &= ~PADG_CROSS;
        sZw->dirty = 1;
        Zw_Note(sZw->count <= 1 ? "Make another warrior first: its look is what this costume wears!" : "Saved with the warrior: that costume wears this look.");
        Snd_PlaySe(1, 0);
    }
    if (kind == ROW_AURA && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) { /* the aura's colour */
        s32 c, i;
        char line[80];

        for (c = 0, i = 1; i < 11; i++) {
            c = sZw->w.auraColour == (s32)(0x1000000 | kTechColour[i]) ? i : c;
        }
        c = (c + ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 10 : 1)) % 11;
        sZw->w.auraColour = c > 0 ? (s32)(0x1000000 | kTechColour[c]) : 0;
        sZw->dirty = 1;
        sprintf(line, c > 0 ? "A %s aura! Power up in a fight to see it." : "The aura in its %s.", kTechColourName[c]);
        Zw_Note(line);
        Snd_PlaySe(1, 0);
    }
    if (kind >= ROW_TECH0 && kind <= ROW_TECH2 && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) { /* its colour */
        s32 t = kind - ROW_TECH0, c = Zw_TechColourIndex(&sZw->w, t);
        char line[80];

        c = (c + ((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 10 : 1)) % 11;
        sZw->w.techColour[t] = c > 0 ? (s32)(0x1000000 | kTechColour[c]) : 0;
        sZw->dirty = 1; /* (the effects, not the model: no new preview) */
        sprintf(line, c > 0 ? "%s, in %s! Fire it in a fight to see it." : "%s, in its %s.", Zw_MoveName(&sZw->w, kind), kTechColourName[c]);
        Zw_Note(line);
        Snd_PlaySe(1, 0);
    }
    if ((pad->gamePressed & PADG_CROSS) && IS_SIZE(kind)) {
        /* (a size has no picker) */
    } else if ((pad->gamePressed & PADG_CROSS) && kind >= ROW_LAB && kind <= ROW_LAB_END) { /* the Technique Lab */
        Snd_PlaySe(1, 1);
        Lb_Open(kind - ROW_LAB);
    } else if ((pad->gamePressed & PADG_CROSS) && kind == ROW_POSE) { /* the stance maker, a screen of its own */
        Snd_PlaySe(1, 1);
        Po_Open();
    } else if ((pad->gamePressed & PADG_CROSS) && kind == ROW_TEXTURES) { /* drawing on the textures, a screen of its own */
        Snd_PlaySe(1, 1);
        Tx_Open();
    } else if ((pad->gamePressed & PADG_CROSS) && kind == ROW_SHADING) { /* each part's shading, a screen of its own */
        Snd_PlaySe(1, 1);
        Port_WarriorShadeScan(&sZw->w);
        sZw->shRow = 0;
        sZw->shFocus = 0;
        Zw_SetMode(ZW_SHADEPK);
    } else if ((pad->gamePressed & PADG_CROSS) && kind == ROW_WORKSHOP) {
        Snd_PlaySe(1, 1);
        sZw->wsStage = 0;
        sZw->wsCur = 0;
        sZw->wsTop = 0;
        Ws_Load();
        sZw->zoneY = CO_Y;
        Zw_SetMode(ZW_WORK);
    } else if ((pad->gamePressed & PADG_CROSS) && (kind == ROW_SURPRISE || kind == ROW_RESETPARTS)) {
        PortWarrior *w = &sZw->w;
        s32 i;

        for (i = 0; i < 12; i++) { /* (the locked ones stay) */
            if (!(sZw->lockMask & (1 << i))) {
                w->part[i] = -1;
                w->layer[i] = -1;
            }
        }
        for (i = 0; i < 8; i++) {
            if (!(sZw->lockMask & (1 << (16 + i)))) {
                w->size[i] = 0;
            }
        }
        if (kind == ROW_SURPRISE) { /* (each part someone's or the body's own; the builds a little off) */
            for (i = 0; i < 12; i++) {
                s32 r = (s32)(Zw_Rand() % 100);

                if (!(sZw->lockMask & (1 << i)) && (i == 11 ? r < 25 : r < 70)) {
                    w->part[i] = (s32)(Zw_Rand() % MM_CHARA_COUNT);
                }
            }
            for (i = 4; i < 7; i++) {
                if (!(sZw->lockMask & (1 << (16 + i)))) {
                    w->size[i] = 85 + (s32)(Zw_Rand() % 7) * 5;
                }
            }
            Zw_Note("Ta-daa! Heh heh, a one of a kind warrior!");
        } else {
            Zw_Note("Back to the body it came with!");
        }
        Zw_Changed();
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_CROSS) {
        Snd_PlaySe(1, 1);
        if (kind == ROW_NAME || kind == ROW_TITLE) {
            sZw->nameWhich = kind == ROW_TITLE;
            Ps_Copy(sZw->text, kind == ROW_TITLE ? sZw->w.title : sZw->w.name, sizeof(sZw->text));
            sZw->keyCur = 0;
            Zw_SetMode(ZW_NAME);
        } else if (kind == ROW_COLOURS) {
            sZw->coCur = 0;
            sZw->coTop = 0;
            Co_Load();
            sZw->zoneY = CO_Y;
            sZw->colourStage = 0;
            Zw_SetMode(ZW_COLOUR);
        } else if (kind == ROW_PREVIEW) {
            Zw_LeaveFor(2);
        } else if (kind == ROW_CHIP) {
            if (sZw->w.mod < 0 || sZw->dirty) {
                Zw_Note("Save the warrior first, then crop its face.");
            } else {
                PortModInfo info;

                Port_ModGetInfo(sZw->w.mod, &info);
                sZw->crop[0] = info.chip[2] > 0 ? info.chip[0] : 64;
                sZw->crop[1] = info.chip[2] > 0 ? info.chip[1] : 48;
                sZw->crop[2] = info.chip[2] > 0 ? info.chip[2] : 128;
                Zw_ArtLoad(sZw->w.mod);
                Zw_SetMode(ZW_CROP);
            }
        } else {
            Zw_Pick(&rows[*cur]);
        }
    } else if ((pad->gamePressed & PADG_CIRCLE) && IS_PART(kind)) { /* a layer over the part */
        Snd_PlaySe(1, 1);
        Zw_Pick(&rows[*cur]);
        sZw->pickLayer = 1;
        sZw->pickCos = Pk_CosOf(&sZw->w, kind, 1);
        if (sZw->w.layer[kind - ROW_PART] >= 0) {
            s32 j;

            sZw->pickCur = sZw->w.layer[kind - ROW_PART] < MM_CHARA_COUNT ? sZw->w.layer[kind - ROW_PART] : 0;
            for (j = 0; j < sZw->pkMods; j++) {
                if (sZw->pkRef[j] == sZw->w.layer[kind - ROW_PART]) {
                    sZw->pickCur = MM_CHARA_COUNT + j;
                }
            }
            sZw->pickTop = sZw->pickCur / PK_COLS - 2 < 0 ? 0 : sZw->pickCur / PK_COLS - 2;
        }
    } else if ((pad->gamePressed & PADG_CIRCLE) && Zw_IsMove(kind)) { /* a name of its own */
        sZw->nameWhich = 2 + Zw_MoveIndex(kind);
        Ps_Copy(sZw->text, Zw_MoveName(&sZw->w, kind), sizeof(sZw->text));
        sZw->keyCur = 0;
        Zw_SetMode(ZW_NAME);
        Snd_PlaySe(1, 1);
    } else if ((pad->gamePressed & PADG_CIRCLE) && kind == ROW_RENDER) { /* the render photographed off the model */
        Snd_PlaySe(1, 1);
        Ph_Open(1);
    } else if ((pad->gamePressed & PADG_CIRCLE) && kind == ROW_CHIP) { /* the small portrait photographed off the model */
        Snd_PlaySe(1, 1);
        Ph_Open(2);
    } else if ((pad->gamePressed & PADG_CIRCLE) && kind == ROW_PREVIEW) { /* a test fight: its HUD face for real */
        Zw_LeaveFor(1);
    } else if ((pad->gamePressed & PADG_CIRCLE) && kind == ROW_HUD) { /* the HUD face photographed off the model */
        Snd_PlaySe(1, 1);
        Ph_Open(0);
    } else if (pad->gamePressed & PADG_SQUARE) {
        Ed_Default(kind);
    } else if (pad->gamePressed & PADG_START) {
        sZw->saving = 2;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        if (sZw->dirty) {
            Ask_Open(ASK_LEAVE);
        } else {
            Zw_SetMode(ZW_LIBRARY);
            Snd_PlaySe(1, 2);
        }
    }
}

/* ---------------------------------------------------------------------------------------------- the picker (S10) */
#define PK_COLS 9
#define PK_ROWS 6
#define PK_X 226.0f
#define PK_Y 214.0f
#define PK_CELL 66.0f

/* How many techniques a row takes from a fighter: 2 (Blast 1), 3 (Blast 2, Ultimate), 0 (a whole fighter). */
static s32 Pk_Slots(s32 kind) {
    return kind == ROW_SKILL0 || kind == ROW_SKILL1 || kind == ROW_SFX0 || kind == ROW_SFX1 ? 2
           : (kind >= ROW_TECH0 && kind <= ROW_TECH2) || (kind >= ROW_FX0 && kind <= ROW_FX2) || (kind >= ROW_CAM && kind <= ROW_CAM_END) ||
                     kind == ROW_LABLIKE ? 3 : 0;
}

/* For an effects row: the effect type its technique has (the effects must come from a technique of that type). */
static s32 Pk_FxType(s32 kind) {
    PortWarrior *w = &sZw->w;
    s32 k = kind - ROW_FX0;
    s32 who = Zw_Who(w, ROW_TECH0 + k);
    s32 from = w->tech[k] >= 0 ? w->techFrom[k] : k;

    return Port_FighterMoveFx(who, 2 + from);
}

/* The picker on the side (the model left in sight, trying the fighter under the cursor on): the look's rows. */
static s32 Pk_Side(void) {
    s32 kind = sZw->pickRow->kind;

    return kind == ROW_BODY || kind == ROW_STANCE || IS_LOOKPART(kind) || (kind >= ROW_XL && kind <= ROW_XL_END) || kind == ROW_SHADEFROM;
}

/* The costume the body or a look part is taken in (PortWarrior's partCostume ...): 1..4, 0 the warrior's own. */
static s32 Pk_CosOf(const PortWarrior *w, s32 kind, s32 layer) {
    s32 first, n;

    if (kind == ROW_BODY) {
        return w->bodyCostume;
    }
    if (kind >= ROW_XL && kind <= ROW_XL_END) {
        return w->xcos[kind - ROW_XL] > 0 ? w->xcos[kind - ROW_XL] : 0;
    }
    if (!IS_LOOKPART(kind)) {
        return 0;
    }
    Zw_Slots(kind, &first, &n);
    return layer ? w->layerCostume[first] : w->partCostume[first];
}

/* "None" is a choice: the hair, the accessories, a layer. */
static s32 Pk_None(void) {
    s32 kind = sZw->pickRow->kind;

    return sZw->pickLayer || kind == ROW_PART + 0 || kind == ROW_PART + 11 || (kind >= ROW_XL && kind <= ROW_XL_END) || kind == ROW_SHADEFROM;
}

/* The picker's faces: the roster, then the mod fighters (rows that make the warrior count as a fighter take none). */
static s32 Pk_Count(void) {
    return MM_CHARA_COUNT + sZw->pkMods;
}

static s32 Pk_Chara(s32 i) {
    return i >= MM_CHARA_COUNT && i < Pk_Count() ? sZw->pkRef[i - MM_CHARA_COUNT] : i;
}

static void Pk_Mods(s32 kind) {
    s32 n = 0, j;

    if (kind != ROW_MOVES && kind != ROW_STYLE && kind != ROW_VOICE && kind != ROW_LABLIKE) {
        n = Port_ModRefCount(sZw->w.mod);
        n = n > 64 ? 64 : n;
    }
    sZw->pkMods = 0;
    for (j = 0; j < n; j++) {
        s32 id = Port_ModRefAt(j, sZw->w.mod);

        if (id >= ZW_MODREF) {
            sZw->pkRef[sZw->pkMods++] = id;
        }
    }
}

static void Zw_Pick(const ZwRow *row) {
    s32 kind = row->kind;
    s32 who = Zw_Who(&sZw->w, kind);
    s32 j;

    sZw->pickRow = row;
    sZw->pickRest = 0;
    sZw->tried = 0;
    Pk_Mods(kind);
    sZw->pickCur = who >= 0 && who < MM_CHARA_COUNT ? who : 0;
    for (j = 0; j < sZw->pkMods; j++) {
        if (sZw->pkRef[j] == who) {
            sZw->pickCur = MM_CHARA_COUNT + j;
        }
    }
    sZw->pickTop = sZw->pickCur / PK_COLS - 2 < 0 ? 0 : sZw->pickCur / PK_COLS - 2;
    sZw->pickStage = 0;
    sZw->pickSlot = 0;
    sZw->pickLayer = 0;
    sZw->pickCos = Pk_CosOf(&sZw->w, kind, 0);
    if (kind == ROW_SKILL0 || kind == ROW_SKILL1) {
        s32 k = kind - ROW_SKILL0;
        sZw->pickSlot = sZw->w.skill[k] >= 0 ? sZw->w.skillFrom[k] : k;
    } else if (kind >= ROW_TECH0 && kind <= ROW_TECH2) {
        s32 k = kind - ROW_TECH0;
        sZw->pickSlot = sZw->w.tech[k] >= 0 ? sZw->w.techFrom[k] : k;
    } else if (kind >= ROW_FX0 && kind <= ROW_FX2) {
        s32 k = kind - ROW_FX0;
        sZw->pickSlot = sZw->w.techFx[k] >= 0 ? sZw->w.techFxFrom[k] : k;
    } else if ((kind >= ROW_CAM && kind <= ROW_CAM_END)) {
        s32 k = kind - ROW_CAM;
        sZw->pickSlot = sZw->w.camFrom[k] >= 0 ? sZw->w.camSlot[k] : k < 2 ? k : 2;
    } else if (kind == ROW_SFX0 || kind == ROW_SFX1) {
        s32 k = kind - ROW_SFX0;
        sZw->pickSlot = sZw->w.skillFx[k] >= 0 ? sZw->w.skillFxFrom[k] : k;
    } else if (kind == ROW_LABLIKE) {
        sZw->pickSlot = (sZw->w.labSet[sZw->labT] >> 12) & 1 ? sZw->w.lab[sZw->labT][12] % 4 : 0;
    }
    Zw_SetMode(ZW_PICK);
}

static void Pk_Draw(void) {
    s32 kind = sZw->pickRow->kind;
    s32 slots = Pk_Slots(kind);
    f32 pop = Zw_In(sZw->modeT, 10);
    f32 s = 0.9f + 0.1f * pop;
    s32 a = (s32)(128.0f * pop);
    s32 i;
    s32 side = Pk_Side();
    f32 px = side ? 84.0f : PK_X; /* (the grid's first column) */
    char line[96];

    if (side) { /* on the left: the warrior stays in sight, wearing the fighter under the cursor a moment later */
        Mx_Rect(24.0f, 90.0f, 690.0f, 634.0f, Mx_Rgba(28, 18, 70, (s32)(0x70 * pop)), Mx_Rgba(16, 10, 44, (s32)(0x70 * pop)));
        Mx_Box(ZW_WINDOW, 30.0f - 40.0f * (1.0f - pop), 96.0f, 680.0f - 40.0f * (1.0f - pop), 626.0f, 26.0f, a);
        Mx_Heading(60.0f, 136.0f, sZw->pickLayer ? "LAYER ON TOP" : "TRY IT ON", 34.0f * s, 400.0f, 0);
        sprintf(line, "%s", sZw->pickRow->label);
        Mx_Bar(ZW_PILL, 440.0f, 118.0f, 660.0f, 154.0f, a);
        Mx_TextA(550.0f, 136.0f, line, 18.0f, 200.0f, MX_DARK, 1, a);
        if (Pk_None()) {
            s32 lit = sZw->pickCur == -2;

            Mx_Bar(lit ? ZW_PILL : ZW_PILL_STEEL, 60.0f, 158.0f, 260.0f, 186.0f, a);
            Mx_TextA(160.0f, 172.0f, "None", 17.0f, 180.0f, lit ? MX_DARK : MX_WHITE, 1, a);
        }
        Mx_TextA(355.0f, 586.0f, sZw->pickCur == -2 ? "None: taken away" : Zw_Name(Pk_Chara(sZw->pickCur)), 22.0f, 600.0f, MX_GOLD, 1, a);
        if ((kind == ROW_BODY || IS_LOOKPART(kind)) && sZw->pickCur >= 0) { /* (which of its costumes) */
            if (sZw->pickCos > 0) {
                sprintf(line, "Costume %d", (int)sZw->pickCos);
            } else {
                Ps_Copy(line, "Costume: the warrior's own", sizeof(line));
            }
            Mx_TextA(355.0f, 612.0f, line, 16.0f, 600.0f, MX_LAV, 1, a);
        }
        if (sZw->pickRest < 24 && sZw->pickRest > 2) {
            Mx_Text(940.0f, 300.0f, "TRYING IT ON...", 18.0f, 0.0f, MX_GOLD, 1);
        }
    } else {
        Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(0, 0, 20, (s32)(0x50 * pop)), Mx_Rgba(0, 0, 20, (s32)(0x50 * pop)));
        Mx_Box(ZW_WINDOW, 688.0f - 518.0f * s, 361.0f - 265.0f * s, 688.0f + 518.0f * s, 361.0f + 265.0f * s, 26.0f, a);
        Mx_Heading(200.0f, 136.0f, slots ? "LEARN A TECHNIQUE" : sZw->pickLayer ? "LAYER ON TOP" : "CHOOSE A FIGHTER", 40.0f * s, 560.0f, 0);
        sprintf(line, "for %s", sZw->pickRow->label);
        Mx_Bar(ZW_PILL, 790.0f, 118.0f, 1060.0f, 154.0f, a);
        Mx_TextA(925.0f, 136.0f, line, 18.0f, 250.0f, MX_DARK, 1, a);
    }
    for (i = 0; i < PK_COLS * PK_ROWS; i++) {
        s32 c = (sZw->pickTop + i / PK_COLS) * PK_COLS + i % PK_COLS;
        f32 cx = px + (f32)(i % PK_COLS) * PK_CELL;
        f32 cy = PK_Y + (f32)(i / PK_COLS) * PK_CELL;

        if (c >= Pk_Count()) {
            break;
        }
        if (Zw_In(sZw->modeT - i / 3, 8) > 0.0f) {
            sZwChipTag = 1;
            Zw_Chip(Pk_Chara(c), cx, cy, 58.0f * Zw_In(sZw->modeT - i / 3, 8), c == sZw->pickCur && sZw->pickStage == 0);
            sZwChipTag = 0;
        }
    }
    if (sZw->pickTop > 0) {
        Mx_Fit(ZW_UP, px + PK_CELL * 4.0f, PK_Y - 44.0f + 3.0f * Zw_Beat(36), 26.0f, a);
    }
    if ((sZw->pickTop + PK_ROWS) * PK_COLS < Pk_Count()) {
        Mx_Fit(ZW_DOWN, px + PK_CELL * 4.0f, PK_Y + PK_CELL * PK_ROWS - 22.0f - 3.0f * Zw_Beat(36), 26.0f, a);
    }
    if (side) {
        Zw_Prompt(15, "Fighter");
        Zw_Prompt(0, "Wear it");
        Zw_Prompt(2, "Cancel");
        Zw_Talk(Ed_Line(kind));
        return;
    }
    /* the fighter under the cursor: its name, and its techniques to take */
    Mx_TextA(1010.0f, 210.0f, Zw_Name(Pk_Chara(sZw->pickCur)), 26.0f, 330.0f, MX_GOLD, 1, a);
    Zw_Chip(Pk_Chara(sZw->pickCur), 1010.0f, 292.0f, 112.0f, 0);
    for (i = 0; i < slots; i++) {
        char name[48];
        f32 y = 392.0f + (f32)i * 56.0f;
        s32 lit = sZw->pickStage == 1 && sZw->pickSlot == i;
        f32 grow = lit ? 6.0f + 3.0f * Zw_Beat(40) : 0.0f;

        Port_FighterMove(Pk_Chara(sZw->pickCur), (slots == 2 ? 0 : 2) + i, name, sizeof(name));
        if (kind == ROW_LABLIKE && (Port_FighterMoveFx(Pk_Chara(sZw->pickCur), 2 + i) == 2 || Port_FighterMoveFx(Pk_Chara(sZw->pickCur), 2 + i) == 7)) {
            sprintf(name + Ps_Len(name), " - rush");
        } else if (kind >= ROW_FX0 && kind <= ROW_FX2 && Port_FighterMoveFx(Pk_Chara(sZw->pickCur), 2 + i) != Pk_FxType(kind)) { /* (another kind: it does not fit) */
            sprintf(name + Ps_Len(name), " - won't fit");
        }
        Mx_Bar(lit ? ZW_PILL : ZW_PILL_STEEL, 850.0f - grow, y - 22.0f, 1170.0f + grow, y + 22.0f, sZw->pickStage == 1 || lit ? a : a * 3 / 4);
        Mx_TextA(1010.0f, y, name[0] ? name : "(none)", 19.0f, 290.0f, lit ? MX_DARK : MX_WHITE, 1, a);
    }
    if (slots == 3) {
        Mx_TextA(1010.0f, 572.0f, "the third is its Ultimate", 16.0f, 300.0f, MX_LAV, 1, a);
    }
    if (slots && sZw->pickStage == 0) {
        Mx_TextA(1010.0f, 600.0f, "Cross: choose from its techniques", 16.0f, 320.0f, MX_GOLD, 1, a);
    }
    Zw_Prompt(15, sZw->pickStage ? "Technique" : "Fighter");
    if ((kind == ROW_BODY || IS_LOOKPART(kind)) && sZw->pickCur >= 0) {
        Zw_Prompt(14, "Costume");
    }
    Zw_Prompt(0, slots && sZw->pickStage == 0 ? "Its techniques" : "Take it");
    Zw_Prompt(2, sZw->pickStage ? "Other fighter" : "Cancel");
    Zw_Talk(slots ? (sZw->pickStage ? "Which one? The third one is the big finisher!" : "Whose technique should your warrior learn?") : Ed_Line(kind));
}

static void Pk_TakeInto(PortWarrior *w, s32 quiet);

static void Pk_Take(void) {
    Pk_TakeInto(&sZw->w, 0);
}

static void Pk_TakeInto(PortWarrior *w, s32 quiet) {
    s32 kind = sZw->pickRow->kind;
    s32 c = sZw->pickCur == -2 ? -2 : Pk_Chara(sZw->pickCur);
    char line[96];

    switch (kind) {
    case ROW_BODY:
        w->body = c;
        w->bodyCostume = sZw->pickCos;
        break;
    case ROW_VOICE: w->voice = c; break;
    case ROW_RENDER: w->render = c; break;
    case ROW_HUD: w->hud = c; break;
    case ROW_STANCE: w->stance = c == w->body ? -1 : c; break;
    case ROW_SFX0: case ROW_SFX1:
        w->skillFx[kind - ROW_SFX0] = c;
        w->skillFxFrom[kind - ROW_SFX0] = sZw->pickSlot;
        break;
    case ROW_FX0: case ROW_FX1: case ROW_FX2:
        w->techFx[kind - ROW_FX0] = c;
        w->techFxFrom[kind - ROW_FX0] = sZw->pickSlot;
        break;
    case ROW_CAM + 0: case ROW_CAM + 1: case ROW_CAM + 2: case ROW_CAM + 3:
        w->camFrom[kind - ROW_CAM] = c;
        w->camSlot[kind - ROW_CAM] = sZw->pickSlot;
        break;
    case ROW_PART + 0: case ROW_PART + 1: case ROW_PART + 2: case ROW_PART + 3: case ROW_PART + 4: case ROW_PART + 5:
    case ROW_PART + 6: case ROW_PART + 7: case ROW_PART + 8: case ROW_PART + 9: case ROW_PART + 10: case ROW_PART + 11:
    case ROW_WHOLEHEAD: case ROW_OUTFIT: {
        s32 first, n, i;

        Zw_Slots(kind, &first, &n);
        for (i = first; i < first + n; i++) {
            if (sZw->pickLayer) {
                w->layer[i] = c == -2 ? -1 : c;
                w->layerCostume[i] = c >= 0 ? sZw->pickCos : 0;
            } else {
                w->part[i] = c == w->body && sZw->pickCos == 0 ? -1 : c; /* (-2: none; the body's own in another costume: named) */
                w->partCostume[i] = c >= 0 ? sZw->pickCos : 0;
            }
        }
        if (!quiet) {
            Zw_Note(c == -2 ? "Gone! Clean as a whistle." : sZw->pickLayer ? "Layered on top! Mix and match, heh heh." : "Looking sharp!");
        }
        break;
    }
    case ROW_XL + 0: case ROW_XL + 1: case ROW_XL + 2: case ROW_XL + 3: {
        s32 x = kind - ROW_XL;

        w->xlayer[x] = c == -2 ? -1 : c;
        w->xcos[x] = c >= 0 ? sZw->pickCos : 0;
        w->xslot[x] = w->xslot[x] >= 0 && w->xslot[x] < 13 ? w->xslot[x] : 0;
        if (!quiet) {
            Zw_Note(c == -2 ? "Layer gone." : "On top! Left and right on its row for which part: hair, head, accessories, a tail...");
        }
        break;
    }
    case ROW_SHADEFROM:
        w->shadeFrom = c == -2 || c == w->body ? -1 : c;
        if (!quiet) {
            Zw_Note(w->shadeFrom < 0 ? "Its own shading again." : "Shaded like that fighter! Give the hair its colour and it is a real transformation.");
        }
        break;
    case ROW_MOVES: w->moves = c; break;
    case ROW_EFFECTS: w->effects = c; break;
    case ROW_AURA: w->aura = c; break;
    case ROW_ANIM + 0: case ROW_ANIM + 1: case ROW_ANIM + 2: case ROW_ANIM + 3: case ROW_ANIM + 4: case ROW_ANIM + 5:
    case ROW_ANIM + 6: case ROW_ANIM + 7: case ROW_ANIM + 8:
        *Zw_AnimOf(w, kind - ROW_ANIM) = c;
        break;
    case ROW_STYLE: w->style = c; break;
    case ROW_FINISHERS: w->finishers = c; break;
    case ROW_SKILL0: case ROW_SKILL1:
        w->skill[kind - ROW_SKILL0] = c;
        w->skillFrom[kind - ROW_SKILL0] = sZw->pickSlot;
        w->labSet[kind - ROW_SKILL0] = 0; /* (another technique: the Lab starts from its numbers) */
        w->moveName[kind - ROW_SKILL0][0] = 0; /* (another technique: its own name) */
        break;
    case ROW_LABLIKE: /* (Fine-tune: its rules from that technique) */
        w->lab[sZw->labT][12] = c * 4 + sZw->pickSlot;
        w->labSet[sZw->labT] |= 1 << 12;
        sZw->dirty = 1;
        return;
    case ROW_TECH0: case ROW_TECH1: case ROW_TECH2:
        w->tech[kind - ROW_TECH0] = c;
        w->techFrom[kind - ROW_TECH0] = sZw->pickSlot;
        w->labSet[2 + kind - ROW_TECH0] = 0;
        w->moveName[2 + kind - ROW_TECH0][0] = 0;
        break;
    }
    if (quiet) {
        return;
    }
    if (kind == ROW_BODY && w->colours > 0) { /* (another body has other colour regions) */
        w->colours = 0;
        Zw_Note("A new body! Its colours start fresh.");
    } else if (Zw_IsMove(kind)) {
        sprintf(line, "%s! Good choice!", Zw_MoveName(w, kind));
        Zw_Note(line);
    }
    Zw_Changed();
}

static void Pk_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 slots = Pk_Slots(sZw->pickRow->kind);
    s32 was = sZw->pickCur * 8 + sZw->pickSlot;

    if (!live) {
        return;
    }
    if (sZw->pickStage == 0) {
        s32 c = sZw->pickCur;

        if (c == -2) { /* (on "None", above the grid) */
            if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
                c = sZw->pickTop * PK_COLS;
            }
        } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
            c = c > 0 ? c - 1 : Pk_Count() - 1;
        } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
            c = c < Pk_Count() - 1 ? c + 1 : 0;
        } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            c = c >= PK_COLS ? c - PK_COLS : Pk_Side() && Pk_None() ? -2 : c;
        } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            c = c + PK_COLS < Pk_Count() ? c + PK_COLS : c;
        }
        if (c != sZw->pickCur) {
            sZw->pickRest = 0;
        }
        sZw->pickCur = c;
        if (c >= 0 && c / PK_COLS < sZw->pickTop) {
            sZw->pickTop = c / PK_COLS;
        }
        if (c >= 0 && c / PK_COLS >= sZw->pickTop + PK_ROWS) {
            sZw->pickTop = c / PK_COLS - PK_ROWS + 1;
        }
        if ((pad->gamePressed & (PADG_L1 | PADG_R1)) && (sZw->pickRow->kind == ROW_BODY || IS_LOOKPART(sZw->pickRow->kind)) && c >= 0) {
            sZw->pickCos = (sZw->pickCos + ((pad->gamePressed & PADG_R1) ? 1 : 4)) % 5; /* (its own, costume 1 .. 4, round) */
            sZw->pickRest = 0; /* (tried on again) */
            Snd_PlaySe(1, 0);
        }
        static s32 tryWait;

        if (Pk_Side() && sZw->pickRest == 23 && Zw_ViewBusy(&tryWait)) {
            /* (the last one tried still being read: this one waits for it) */
        } else if (Pk_Side() && ++sZw->pickRest == 24) { /* trying it on: the warrior made with it, for the screen only */
            PortWarrior tmp = sZw->w;

            Pk_TakeInto(&tmp, 1);
            sZw->previewMod = Port_WarriorPreview(&tmp);
            sZw->viewGen++;
            sZw->tried = 1;
        }
    } else {
        if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            sZw->pickSlot = sZw->pickSlot > 0 ? sZw->pickSlot - 1 : slots - 1;
        } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            sZw->pickSlot = sZw->pickSlot < slots - 1 ? sZw->pickSlot + 1 : 0;
        }
    }
    if (sZw->pickCur * 8 + sZw->pickSlot != was) {
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & PADG_CROSS) {
        Snd_PlaySe(1, 1);
        if (slots && sZw->pickStage == 0) {
            sZw->pickStage = 1;
            if (sZw->pickSlot >= slots) {
                sZw->pickSlot = 0;
            }
        } else if (sZw->pickRow->kind == ROW_LABLIKE && (Port_FighterMoveFx(Pk_Chara(sZw->pickCur), 2 + sZw->pickSlot) == 2 ||
                                                         Port_FighterMoveFx(Pk_Chara(sZw->pickCur), 2 + sZw->pickSlot) == 7)) {
            Zw_Note("Rushes can't lend their rules. Pick a beam, a ball or a barrage!");
            Snd_PlaySe(1, 2);
        } else if (sZw->pickRow->kind >= ROW_FX0 && sZw->pickRow->kind <= ROW_FX2 &&
                   Port_FighterMoveFx(Pk_Chara(sZw->pickCur), 2 + sZw->pickSlot) != Pk_FxType(sZw->pickRow->kind)) {
            Zw_Note("That one is another kind of technique: its effects won't fit!");
            Snd_PlaySe(1, 2);
        } else {
            s32 back = sZw->pickRow->kind == ROW_LABLIKE ? ZW_LAB : ZW_EDIT;

            Pk_Take();
            Zw_SetMode(back);
        }
    } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
        Snd_PlaySe(1, 2);
        if (sZw->pickStage == 1) {
            sZw->pickStage = 0;
        } else {
            if (sZw->tried) { /* (back to the warrior as it is) */
                sZw->previewDue = 0;
            }
            Zw_SetMode(sZw->pickRow->kind == ROW_LABLIKE ? ZW_LAB : ZW_EDIT);
        }
    }
}

/* ---------------------------------------------------------------------------------------------- the Technique Lab
 * A technique's numbers tuned by hand (plat_mods.c warrior_lab_apply; LAB_ there): its power, cost, hits, shots, size,
 * speed, homing, range, knock-back, charge boost, recoil, whether it can be blocked, and whose rules it follows (another
 * technique's flags and hit reactions: a piercing beam, an exploding ball, an unblockable bomb). Each stat as its
 * technique has it until it is changed (Port_WarriorLabBase); the readout weighs the result against the original. */
enum { LB_DAMAGE, LB_COST, LB_HITS, LB_SHOTS, LB_SIZE, LB_SPEED, LB_HOMING, LB_TIME, LB_PUSH, LB_CHARGE, LB_RECOIL, LB_UNBLOCK, LB_LIKE,
       LB_HEAL, LB_KI };
enum { LB_ATTACK = LB_SHOTS, LB_DEFENSE = LB_SIZE, LB_KIBOOST = LB_SPEED, LB_BLAST = LB_HOMING }; /* (a Blast 1's boosts: levels + 100) */
typedef struct LbStat {
    s32 id;
    char *label;
    s32 lo, hi, step; /* (a skill's cost: stocks 0..7) */
    char *say;        /* King Kai on it */
} LbStat;
static const LbStat kLbSuper[] = {
    {LB_DAMAGE, "Damage", 0, 40000, 250, "How much damage it does. 10,000 is a full health bar."},
    {LB_COST, "Ki cost", 0, 100000, 10000, "The ki it takes. 20,000 is one bar of ki."},
    {LB_HITS, "Hits", 1, 30, 1, "How many times it hits per shot: a beam's multi-hit. The power is spread over them."},
    {LB_SHOTS, "Shots", 1, 40, 1, "How many shots it fires: a barrage's count. Best on barrage techniques!"},
    {LB_SIZE, "Size", 10, 1000, 10, "How big the blast is: a needle-thin beam or a planet-sized bomb!"},
    {LB_SPEED, "Speed", 0, 2000, 25, "How fast it flies."},
    {LB_HOMING, "Homing", 0, 720, 10, "How hard it turns after its target. 0 flies straight."},
    {LB_TIME, "Range", 10, 800, 10, "How long it flies before it fades: its range."},
    {LB_PUSH, "Knock-back", 0, 3000, 50, "How far it sends the foe flying."},
    {LB_CHARGE, "Charge boost", 100, 400, 10, "Its power when fully charged: x1.50 is half as strong again."},
    {LB_RECOIL, "Recoil", 0, 20000, 250, "Damage to your own warrior for firing it. Power has a price!"},
    {LB_UNBLOCK, "Unblockable", 0, 1, 1, "Unblockable: guarding does nothing, only a super guard saves the foe!"},
    {LB_LIKE, "Copy rules from", -1, 99, 1, "Copy how any technique behaves: piercing like Special Beam Cannon, exploding like a Death Ball... Cross to choose."},
};
static const LbStat kLbSkill[] = {
    {LB_COST, "Cost", 0, 7, 1, "How many Blast stocks it uses."},
    {LB_TIME, "Duration", 10, 6000, 50, "How long the boost or the stun lasts."},
    {LB_ATTACK, "Attack boost", 90, 110, 1, "Stronger punches, kicks and ki blasts while it lasts."},
    {LB_DEFENSE, "Defense boost", 90, 110, 1, "Takes less damage while it lasts."},
    {LB_KIBOOST, "Ki & speed boost", 90, 110, 1, "Faster ki charging and faster movement while it lasts."},
    {LB_BLAST, "Blast boost", 90, 110, 1, "Stronger Blast 2 attacks and Ultimates while it lasts."},
    {LB_HEAL, "Heal", -100, 100, 5, "Health restored, in percent. Below zero, it costs health."},
    {LB_KI, "Ki restored", -100000, 100000, 10000, "Ki restored. 20,000 is one bar."},
    {LB_DAMAGE, "Damage", 0, 40000, 250, "Damage, for skills that hit. 10,000 is a full health bar."},
    {LB_UNBLOCK, "Max Power", 0, 1, 1, "Yes: using it also puts your warrior into Max Power mode!"},
};
#define LB_ROWS 13
#define LB_X 40.0f
#define LB_Y 196.0f
#define LB_PITCH 33.0f

static const LbStat *Lb_Stats(s32 *n) {
    if (sZw->labT >= 2) {
        *n = (s32)(sizeof(kLbSuper) / sizeof(kLbSuper[0]));
        return kLbSuper;
    }
    *n = (s32)(sizeof(kLbSkill) / sizeof(kLbSkill[0]));
    return kLbSkill;
}

static s32 Lb_Set(s32 id) {
    return (sZw->w.labSet[sZw->labT] >> id) & 1;
}

static s32 Lb_Value(s32 id) {
    return Lb_Set(id) ? sZw->w.lab[sZw->labT][id] : sZw->labBase[id];
}

static void Lb_Open(s32 t) {
    sZw->labT = t;
    sZw->labRow = 0;
    sZw->labTop = 0;
    sZw->labHold = 0;
    if (!Port_WarriorLabBase(&sZw->w, t, sZw->labBase)) {
        Zw_Note("That technique's numbers could not be read.");
        return;
    }
    Zw_SetMode(ZW_LAB);
}

/* The "behaves like" choice now: its place among the choices (-1 its own). */
static s32 Lb_LikeIndex(void) {
    s32 k, v;

    if (!Lb_Set(LB_LIKE)) {
        return -1;
    }
    for (k = 0; (v = Port_WarriorLabLike(sZw->labBase[15], k)) >= 0; k++) {
        if (v == sZw->w.lab[sZw->labT][LB_LIKE]) {
            return k;
        }
    }
    return -1;
}

/* A stat's value as shown. */
static void Lb_Text(s32 id, s32 v, char *out) {
    if (sZw->labT < 2 && (id == LB_ATTACK || id == LB_DEFENSE || id == LB_KIBOOST || id == LB_BLAST)) { /* (a Blast 1's boost: levels) */
        sprintf(out, v == 100 ? "None" : v > 100 ? "+%d" : "%d", (int)(v - 100));
        return;
    }
    switch (id) {
    case LB_DAMAGE: case LB_RECOIL: sprintf(out, "%d", (int)v); break;
    case LB_COST:
        if (sZw->labT >= 2) {
            sprintf(out, "%d.%d bars", (int)(v / 20000), (int)(v % 20000 / 2000));
        } else {
            sprintf(out, v == 1 ? "%d stock" : "%d stocks", (int)v);
        }
        break;
    case LB_SIZE: case LB_CHARGE: sprintf(out, "x%d.%02d", (int)(v / 100), (int)(v % 100)); break;
    case LB_SPEED: sprintf(out, "%d km/h", (int)(v * 10)); break;
    case LB_HOMING: sprintf(out, v == 0 ? "Straight" : "%d deg/s", (int)v); break;
    case LB_TIME: sprintf(out, "%d.%d s", (int)(v / 100), (int)(v % 100 / 10)); break;
    case LB_PUSH: sprintf(out, "%d", (int)(v / 10)); break;
    case LB_UNBLOCK: Ps_Copy(out, v ? "Yes" : "No", 16); break;
    case LB_HEAL: sprintf(out, "%d%%", (int)v); break;
    case LB_KI: sprintf(out, "%s%d.%d bars", v < 0 ? "-" : "", (int)((v < 0 ? -v : v) / 20000), (int)((v < 0 ? -v : v) % 20000 / 2000)); break;
    case LB_LIKE:
        if (v < 0) {
            Ps_Copy(out, "Default", 32);
        } else {
            Port_FighterMove(v / 4, 2 + v % 4, out, 40);
        }
        break;
    default: sprintf(out, "%d", (int)v); break;
    }
}

static void Lb_Draw(void) {
    s32 n, i;
    const LbStat *st = Lb_Stats(&n);
    f32 in = Zw_In(sZw->modeT, 12);
    s32 a = (s32)(128.0f * in);
    char line[96], val[48];
    s32 kind = sZw->labT >= 2 ? ROW_TECH0 + sZw->labT - 2 : ROW_SKILL0 + sZw->labT;
    s32 dmg = Lb_Value(LB_DAMAGE), hits = Lb_Value(LB_HITS) * Lb_Value(LB_SHOTS), cost = Lb_Value(LB_COST);
    s32 dmg0 = sZw->labBase[LB_DAMAGE], hits0 = sZw->labBase[LB_HITS] * sZw->labBase[LB_SHOTS], cost0 = sZw->labBase[LB_COST];

    Zw_Title("FINE-TUNE");
    sprintf(line, "%s  -  %s", Zw_MoveName(&sZw->w, kind), sZw->labT >= 2 ? (sZw->labT == 4 ? "Ultimate" : "Blast 2") : "Blast 1");
    Mx_Text(170.0f, 108.0f, line, 18.0f, 900.0f, MX_GOLD, 0);
    /* the bench: a window ruled like graph paper, the stats on it */
    Mx_Box(ZW_WINDOW, 24.0f, 140.0f, 736.0f, 632.0f, 22.0f, a);
    for (i = 0; i < 14; i++) { /* (the graph paper) */
        f32 x = 60.0f + (f32)i * 48.0f;

        Mx_Rect(x, 156.0f, x + 1.0f, 616.0f, Mx_Rgba(150, 170, 255, 0x10), Mx_Rgba(150, 170, 255, 0x10));
    }
    {
        f32 scan = 160.0f + (f32)(sZw->frames % 180) * 2.5f; /* (a scan line going down the bench) */

        Mx_Rect(36.0f, scan, 724.0f, scan + 3.0f, Mx_Rgba(120, 255, 200, 0x18), Mx_Rgba(120, 255, 200, 0x04));
    }
    Mx_Text(60.0f, 170.0f, "STAT", 14.0f, 0.0f, MX_LAV, 0);
    Mx_Text(330.0f, 170.0f, "TICK: ORIGINAL", 14.0f, 0.0f, MX_LAV, 0);
    Mx_Text(706.0f, 170.0f, "VALUE", 14.0f, 0.0f, MX_LAV, 2);
    for (i = 0; i < LB_ROWS && sZw->labTop + i < n; i++) {
        s32 r = sZw->labTop + i;
        const LbStat *s1 = &st[r];
        f32 y = LB_Y + (f32)i * LB_PITCH;
        s32 sel = r == sZw->labRow;
        s32 v = Lb_Value(s1->id), b = sZw->labBase[s1->id], changed = Lb_Set(s1->id);
        f32 x0 = 300.0f, x1 = 560.0f;

        if (sel) {
            Zw_SelBar(36.0f, y - 15.0f, 724.0f, y + 15.0f);
        }
        if (changed) {
            Mx_Disc(48.0f, y, 10.0f, Mx_Rgba(120, 255, 170, a), Mx_Rgba(40, 200, 110, a));
        }
        Mx_Text(60.0f, y, s1->label, 18.0f, 230.0f, sel ? MX_DARK : MX_WHITE, 0);
        if (s1->id == LB_LIKE) {
            Lb_Text(LB_LIKE, Lb_Set(LB_LIKE) ? sZw->w.lab[sZw->labT][LB_LIKE] : -1, val);
            Mx_Text(706.0f, y, val, 17.0f, 400.0f, sel ? MX_DARK : MX_GOLD, 2);
            continue;
        }
        if (s1->id == LB_UNBLOCK) {
            Lb_Text(LB_UNBLOCK, v, val);
            Mx_Text(706.0f, y, val, 18.0f, 200.0f, sel ? MX_DARK : (v ? MX_GOLD : MX_WHITE), 2);
            continue;
        }
        { /* the gauge: the original as a tick, now as the fill */
            f32 span = (f32)(s1->hi - s1->lo);
            f32 fv = span > 0.0f ? (f32)(v - s1->lo) / span : 0.0f;
            f32 fb = span > 0.0f ? (f32)(b - s1->lo) / span : 0.0f;
            u32 top = v > b ? Mx_Rgba(255, 190, 70, a) : v < b ? Mx_Rgba(110, 170, 255, a) : Mx_Rgba(200, 200, 230, a);

            fv = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
            fb = fb < 0.0f ? 0.0f : fb > 1.0f ? 1.0f : fb;
            Mx_Rect(x0, y - 6.0f, x1, y + 6.0f, Mx_Rgba(10, 8, 40, a * 3 / 4), Mx_Rgba(10, 8, 40, a * 3 / 4));
            Mx_Rect(x0, y - 6.0f, x0 + (x1 - x0) * fv, y + 6.0f, top, Ps_Mix(top, Mx_Rgba(0, 0, 0, a), 0.35f) | ((u32)a << 24));
            Mx_Rect(x0 + (x1 - x0) * fb - 1.5f, y - 10.0f, x0 + (x1 - x0) * fb + 1.5f, y + 10.0f, Mx_Rgba(255, 255, 255, a), Mx_Rgba(255, 255, 255, a));
        }
        Lb_Text(s1->id, v, val);
        Mx_Text(706.0f, y, val, 18.0f, 136.0f, sel ? MX_DARK : changed ? MX_GOLD : MX_WHITE, 2);
    }
    if (sZw->labTop > 0) {
        Mx_Fit(ZW_UP, 380.0f, 182.0f + 3.0f * Zw_Beat(36), 22.0f, a);
    }
    if (sZw->labTop + LB_ROWS < n) {
        Mx_Fit(ZW_DOWN, 380.0f, 622.0f - 3.0f * Zw_Beat(36), 22.0f, a);
    }
    /* the warrior, the readout */
    Zw_EditStage(1040.0f, 400.0f, 260.0f);
    Mx_Box(ZW_WINDOW, 760.0f, 430.0f, 1346.0f, 632.0f, 18.0f, a);
    Mx_Text(780.0f, 452.0f, "READOUT", 16.0f, 0.0f, MX_GOLD, 0);
    if (sZw->labT < 2) { /* a Blast 1: what it gives */
        s32 boost = Lb_Value(LB_ATTACK) + Lb_Value(LB_DEFENSE) + Lb_Value(LB_KIBOOST) + Lb_Value(LB_BLAST) - 400;
        s32 t = Lb_Value(LB_TIME);

        sprintf(line, "Lasts  %d.%d s", (int)(t / 100), (int)(t % 100 / 10));
        Mx_Text(780.0f, 484.0f, line, 17.0f, 270.0f, MX_WHITE, 0);
        sprintf(line, cost == 1 ? "Cost  %d stock" : "Cost  %d stocks", (int)cost);
        Mx_Text(1060.0f, 484.0f, line, 17.0f, 270.0f, MX_WHITE, 0);
        sprintf(line, boost >= 0 ? "Total boost  +%d levels" : "Total boost  %d levels", (int)boost);
        Mx_Text(780.0f, 520.0f, line, 17.0f, 540.0f, boost > 8 ? MX_GOLD : MX_WHITE, 0);
        sprintf(line, "Heal  %d%%    Ki  %d.%d bars", (int)Lb_Value(LB_HEAL), (int)(Lb_Value(LB_KI) / 20000), (int)((Lb_Value(LB_KI) < 0 ? -Lb_Value(LB_KI) : Lb_Value(LB_KI)) % 20000 / 2000));
        Mx_Text(780.0f, 556.0f, line, 17.0f, 540.0f, MX_WHITE, 0);
        Mx_Text(780.0f, 608.0f, boost > 8 ? "Very strong! Your opponents will not be happy." : "Boosts last for the duration.", 15.0f, 540.0f,
                boost > 8 ? MX_GOLD : MX_LAV, 0);
        goto lb_after;
    }
    sprintf(line, "Total hits  %d", (int)(hits < 1 ? 1 : hits));
    Mx_Text(780.0f, 484.0f, line, 17.0f, 270.0f, MX_WHITE, 0);
    sprintf(line, "Per hit  %d", (int)(dmg / (hits < 1 ? 1 : hits)));
    Mx_Text(1060.0f, 484.0f, line, 17.0f, 270.0f, MX_WHITE, 0);
    if (sZw->labT >= 2) {
        sprintf(line, "Ki per 1,000 damage  %d", (int)(dmg > 0 ? (s32)((f32)cost * 1000.0f / (f32)dmg) : 0));
        Mx_Text(780.0f, 514.0f, line, 17.0f, 540.0f, MX_WHITE, 0);
    }
    { /* power against its cost, against the original's */
        f32 eff = cost > 0 ? (f32)dmg / (f32)cost : (f32)dmg / 10000.0f;
        f32 eff0 = cost0 > 0 ? (f32)dmg0 / (f32)cost0 : (f32)dmg0 / 10000.0f;
        f32 r = eff0 > 0.0f ? eff / eff0 : 1.0f;
        s32 pct = (s32)(r * 100.0f + 0.5f);
        f32 w = r > 3.0f ? 1.0f : r / 3.0f;

        sprintf(line, "Damage for its cost: %d%% of the original", (int)pct);
        Mx_Text(780.0f, 546.0f, line, 17.0f, 540.0f, pct > 150 ? MX_GOLD : MX_WHITE, 0);
        Mx_Rect(780.0f, 568.0f, 1326.0f, 582.0f, Mx_Rgba(10, 8, 40, 0x60), Mx_Rgba(10, 8, 40, 0x60));
        Mx_Rect(780.0f, 568.0f, 780.0f + 546.0f * w, 582.0f, pct > 200 ? Mx_Rgba(255, 90, 70, 0x80) : Mx_Rgba(120, 255, 170, 0x80),
                pct > 200 ? Mx_Rgba(180, 40, 30, 0x80) : Mx_Rgba(40, 180, 100, 0x80));
        Mx_Rect(780.0f + 546.0f / 3.0f - 1.0f, 562.0f, 780.0f + 546.0f / 3.0f + 1.0f, 588.0f, Mx_Rgba(255, 255, 255, 0x80), Mx_Rgba(255, 255, 255, 0x80));
        Mx_Text(780.0f, 608.0f, pct > 200 ? "Overpowered! Your opponents will not be happy." : "Balanced, like the game's own techniques.", 15.0f, 540.0f,
                pct > 200 ? MX_GOLD : MX_LAV, 0);
    }
lb_after:
    if (sZw->dirty) {
        Mx_Bar(ZW_PILL, 1150.0f, 110.0f, 1340.0f, 146.0f, 0x80);
        Mx_Text(1245.0f, 128.0f, "NOT SAVED", 17.0f, 170.0f, MX_DARK, 1);
    }
    if (sZw->labT >= 2 && st[sZw->labRow].id == LB_LIKE) {
        Zw_Prompt(0, "Choose");
    } else {
        Zw_Prompt(15, "Adjust");
    }
    Zw_Prompt(1, "Reset");
    Zw_Prompt(13, "Reset all");
    Zw_Prompt(4, "Test fight");
    Zw_Prompt(7, "Save");
    Zw_Prompt(2, "Back");
    if (sZw->saving) {
        Zw_Talk("Saving your warrior... hold still!");
    } else {
        Zw_Talk(st[sZw->labRow].say);
    }
}

static void Lb_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 n, v, id, step, was;
    const LbStat *st = Lb_Stats(&n);
    PortWarrior *w = &sZw->w;

    if (Zw_SaveTick() || !live) {
        return;
    }
    if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sZw->labRow = sZw->labRow > 0 ? sZw->labRow - 1 : n - 1;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sZw->labRow = sZw->labRow < n - 1 ? sZw->labRow + 1 : 0;
        Snd_PlaySe(1, 0);
    }
    if (sZw->labRow < sZw->labTop) {
        sZw->labTop = sZw->labRow;
    }
    if (sZw->labRow >= sZw->labTop + LB_ROWS) {
        sZw->labTop = sZw->labRow - LB_ROWS + 1;
    }
    id = st[sZw->labRow].id;
    sZw->labHold = pad->gameHeld & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2) ? sZw->labHold + 1 : 0;
    if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) {
        s32 dir = pad->gameRepeat & (PADG_LEFT | PADG_LEFT2) ? -1 : 1;

        if (id == LB_LIKE) {
            (void)dir; /* (Cross opens the picker) */
        } else {
            was = Lb_Value(id);
            step = st[sZw->labRow].step * (sZw->labHold > 90 ? 20 : sZw->labHold > 30 ? 5 : 1);
            v = was + dir * step;
            v = v < st[sZw->labRow].lo ? st[sZw->labRow].lo : v > st[sZw->labRow].hi ? st[sZw->labRow].hi : v;
            if (v != was) {
                w->lab[sZw->labT][id] = v;
                if (v == sZw->labBase[id]) { /* (back where it was: as it is) */
                    w->labSet[sZw->labT] &= ~(1 << id);
                } else {
                    w->labSet[sZw->labT] |= 1 << id;
                }
                sZw->dirty = 1;
                Snd_PlaySe(1, 0);
            }
        }
    }
    if ((pad->gamePressed & PADG_CROSS) && id == LB_LIKE) { /* any Blast 2 or Ultimate of the roster, but a rush */
        static const ZwRow kLikeRow = {ROW_LABLIKE, "Copy rules from"};

        Snd_PlaySe(1, 1);
        Zw_Pick(&kLikeRow);
        return;
    }
    if (pad->gamePressed & PADG_SQUARE) {
        w->labSet[sZw->labT] &= ~(1 << id);
        sZw->dirty = 1;
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & PADG_SELECT) {
        w->labSet[sZw->labT] = 0;
        sZw->dirty = 1;
        Zw_Note("All stats reset to default.");
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & PADG_START) {
        sZw->saving = 2;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_CIRCLE) {
        Zw_LeaveFor(1); /* (a test fight: saved first) */
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Zw_SetMode(ZW_EDIT);
        Snd_PlaySe(1, 2);
    }
}

/* ---------------------------------------------------------------------------------------------- the name (S13) */
#define KB_COLS 10
static char *kKeys[2] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-!?", "abcdefghijklmnopqrstuvwxyz0123456789'&()"};
#define KB_KEYS 40
#define KB_X 268.0f
#define KB_Y 316.0f
#define KB_PITCH 66.0f

static void Kb_Draw(void) {
    s32 i;
    char shown[170];

    if (sZw->nameWhich == 9) { /* (the Workshop bio) */
        Zw_Title("WORKSHOP BIO");
    } else if (sZw->nameWhich >= 2) { /* (a technique's name) */
        Zw_Title("TECHNIQUE NAME");
    } else {
        Zw_Title(sZw->nameWhich ? "WARRIOR TITLE" : "WARRIOR NAME");
        Mx_Bar(sZw->nameWhich ? ZW_PILL_STEEL : ZW_PILL, 470.0f, 128.0f, 600.0f, 158.0f, 0x80);
        Mx_Text(535.0f, 143.0f, "NAME", 16.0f, 120.0f, sZw->nameWhich ? MX_WHITE : MX_DARK, 1);
        Mx_Bar(sZw->nameWhich ? ZW_PILL : ZW_PILL_STEEL, 610.0f, 128.0f, 740.0f, 158.0f, 0x80);
        Mx_Text(675.0f, 143.0f, "TITLE", 16.0f, 120.0f, sZw->nameWhich ? MX_DARK : MX_WHITE, 1);
    }
    Mx_Bar(ZW_FIELD, 300.0f, 172.0f, 920.0f, 250.0f, 0x80);
    sprintf(shown, "%s%s", sZw->text, (sZw->frames / 20) & 1 ? "_" : " ");
    Mx_Text(330.0f, 211.0f, shown, sZw->nameWhich == 9 ? 20.0f : 34.0f, 560.0f, MX_WHITE, 0);
    for (i = 0; i < KB_KEYS + 3; i++) {
        s32 lit = i == sZw->keyCur;
        f32 in = Zw_In(sZw->modeT - (i % KB_COLS) - i / KB_COLS, 8);

        if (i < KB_KEYS) {
            f32 cx = KB_X + (f32)(i % KB_COLS) * KB_PITCH;
            f32 cy = KB_Y + (f32)(i / KB_COLS) * KB_PITCH;
            f32 size = (lit ? 62.0f + 3.0f * Zw_Beat(40) : 58.0f) * in;
            char s[2];

            s[0] = kKeys[sZw->keyPage][i];
            s[1] = 0;
            if (size > 1.0f) {
                Mx_Fit(lit ? ZW_KEY_SEL : ZW_KEY, cx, cy, size, 0x80);
                Mx_TextA(cx, cy, s, 24.0f * in, 0.0f, MX_DARK, 1, (s32)(128.0f * in));
            }
        } else {
            static char *labels[3] = {"SPACE", "DELETE", "OK"};
            f32 x0 = KB_X - 30.0f + (f32)(i - KB_KEYS) * 220.0f;
            f32 cy = KB_Y + KB_PITCH * 4.0f;

            Mx_Bar(lit ? ZW_PILL : ZW_PILL_STEEL, x0, cy - 24.0f, x0 + 200.0f, cy + 24.0f, (s32)(128.0f * in));
            Mx_TextA(x0 + 100.0f, cy, labels[i - KB_KEYS], 19.0f, 180.0f, lit ? MX_DARK : MX_WHITE, 1, (s32)(128.0f * in));
        }
    }
    Zw_Host(1180.0f, 400.0f, 380.0f, 1);
    Zw_Prompt(0, "Type");
    Zw_Prompt(1, "Delete");
    Zw_Prompt(6, "abc");
    if (sZw->nameWhich < 2) {
        Zw_Prompt(14, "Name / title");
    }
    Zw_Prompt(7, "OK");
    Zw_Prompt(2, "Cancel");
    Zw_Talk(sZw->nameWhich >= 2 ? "Name your technique! Shout it loud! (Leave it empty for its own name.)"
                                : "Every great warrior needs a great name! Heh heh. (Your keyboard works too.)");
}

static void Kb_Add(s32 c) {
    s32 n = Ps_Len(sZw->text);

    if (n < (sZw->nameWhich == 9 ? 150 : 30)) {
        sZw->text[n] = (char)c;
        sZw->text[n + 1] = 0;
    }
}

static void Kb_Keep(void) {
    if (sZw->nameWhich == 9) { /* (the Workshop bio) */
        Ps_Copy(sZw->wkBio, sZw->text, sizeof(sZw->wkBio));
        return;
    }
    if (sZw->nameWhich >= 2) { /* (a technique's: as it was named, or its own when emptied) */
        char *own;
        s32 i = 0;

        sZw->w.moveName[sZw->nameWhich - 2][0] = 0;
        own = Zw_MoveName(&sZw->w, sZw->nameWhich == 2 ? ROW_SKILL0 : sZw->nameWhich == 3 ? ROW_SKILL1 : ROW_TECH0 + sZw->nameWhich - 4);
        while (sZw->text[i] && sZw->text[i] == own[i]) {
            i++;
        }
        if (sZw->text[0] && sZw->text[i] != own[i]) {
            Ps_Copy(sZw->w.moveName[sZw->nameWhich - 2], sZw->text, sizeof(sZw->w.moveName[0]));
        }
    } else if (sZw->nameWhich) {
        Ps_Copy(sZw->w.title, sZw->text, sizeof(sZw->w.title));
    } else {
        Ps_Copy(sZw->w.name, sZw->text, sizeof(sZw->w.name));
    }
    sZw->dirty = 1; /* (a name does not change the model: no new preview) */
}

static void Kb_Back(void) { /* (where the keyboard was opened from) */
    Zw_SetMode(sZw->nameWhich == 9 ? ZW_WSUPLOAD : ZW_EDIT);
}

static void Kb_Done(void) {
    Kb_Keep();
    Kb_Back();
    Snd_PlaySe(1, 1);
}

static void Kb_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 k = sZw->keyCur;
    s32 c;

    if (!live) {
        return;
    }
    while ((c = Port_TextRead()) != 0) { /* the PC's keyboard */
        if (c == 8) {
            s32 n = Ps_Len(sZw->text);
            if (n > 0) {
                sZw->text[n - 1] = 0;
            }
        } else if (c == 13) {
            Kb_Done();
            return;
        } else if (c == 27) {
            Kb_Back();
            return;
        } else if (c >= 32 && c < 127) {
            Kb_Add(c);
        }
    }
    if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        k = k >= KB_KEYS ? (k > KB_KEYS ? k - 1 : KB_KEYS + 2) : (k % KB_COLS > 0 ? k - 1 : k + KB_COLS - 1);
    } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        k = k >= KB_KEYS ? (k < KB_KEYS + 2 ? k + 1 : KB_KEYS) : (k % KB_COLS < KB_COLS - 1 ? k + 1 : k - KB_COLS + 1);
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        k = k >= KB_KEYS ? KB_KEYS - KB_COLS + (k - KB_KEYS) * 3 : (k >= KB_COLS ? k - KB_COLS : KB_KEYS);
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        k = k >= KB_KEYS ? (k - KB_KEYS) * 3 : (k + KB_COLS < KB_KEYS ? k + KB_COLS : KB_KEYS + (k % KB_COLS) / 4);
    }
    if (k != sZw->keyCur) {
        sZw->keyCur = k;
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & PADG_CROSS) {
        if (k < KB_KEYS) {
            Kb_Add(kKeys[sZw->keyPage][k]);
        } else if (k == KB_KEYS) {
            Kb_Add(' ');
        } else if (k == KB_KEYS + 1) {
            s32 n = Ps_Len(sZw->text);
            if (n > 0) {
                sZw->text[n - 1] = 0;
            }
        } else {
            Kb_Done();
            return;
        }
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_SQUARE) {
        s32 n = Ps_Len(sZw->text);
        if (n > 0) {
            sZw->text[n - 1] = 0;
        }
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & PADG_L2) {
        sZw->keyPage ^= 1;
        Snd_PlaySe(1, 0);
    } else if ((pad->gamePressed & (PADG_L1 | PADG_R1)) && sZw->nameWhich < 2) { /* the other line, keeping this one */
        Kb_Keep();
        sZw->nameWhich ^= 1;
        Ps_Copy(sZw->text, sZw->nameWhich ? sZw->w.title : sZw->w.name, sizeof(sZw->text));
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_START) {
        Kb_Done();
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Kb_Back();
        Snd_PlaySe(1, 2);
    }
}

/* ---------------------------------------------------------------------------------------------- the colours (S6)
 * The warrior's colour regions (Port_WarriorRegions: each one material of its look, named by the parts that draw it:
 * "Top & Trousers", "Boots", the skin, the hair, the face's own), each given a colour from a picker: hue, strength,
 * lightness, swatches, the colours used lately, and "only on" one of its parts (that part gets its own copy of the
 * colours: a region of its own after the warrior is made again). Every change is painted straight into the model on
 * the screen (Port_WarriorPaintLive); saving keeps it (plat_mods.c, warrior_recolour). */
static const u32 kSwatch[30] = { /* 0xBBGGRR */
    0x2020D0, 0x281890, 0x1C64E8, 0x28B8F0, 0x40E8F8, 0x30D890, 0x309830, 0x808020, 0xD0C020, 0xF0A050,
    0xD05028, 0x602010, 0x901848, 0x983070, 0xA030C8, 0xB078F0, 0xF0F0F0, 0xB8B8B8, 0x787878, 0x404040,
    0x181818, 0x204070, 0x6898C8, 0xA8C8F0, 0x80A8E0, 0x405888, 0x181868, 0x307070, 0xB0B040, 0xE0A8C0,
};
#define CO_ROWS 11
enum { CP_HUE, CP_SAT, CP_VAL, CP_SW1, CP_SW2, CP_RECENT, CP_ONLY, CP_ROWS };

static char *kCoPart[13] = {"Hair", "Head", "Face", "Top", "Belt", "Sleeves", "Forearms", "Hands", "Trousers", "Shins", "Boots", "Extras", "Other"};

/* A colour's name, from how it looks (0xBBGGRR). */
static void Co_Describe(u32 rgb, char *out) {
    static char *hues[12] = {"Red", "Orange", "Gold", "Yellow", "Lime", "Green", "Teal", "Cyan", "Sky blue", "Blue", "Purple", "Pink"};
    static const s32 edge[12] = {12, 36, 50, 66, 90, 150, 175, 195, 215, 250, 290, 340}; /* (each hue's upper bound) */
    s32 r = (s32)(rgb & 0xFF), g = (s32)((rgb >> 8) & 0xFF), b = (s32)((rgb >> 16) & 0xFF);
    s32 mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    s32 mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    f32 v = (f32)mx / 255.0f;
    f32 s = mx > 0 ? (f32)(mx - mn) / (f32)mx : 0.0f;
    f32 h = 0.0f;
    s32 k;

    if (mx > mn) {
        f32 d = (f32)(mx - mn);
        h = mx == r ? (f32)(g - b) / d : mx == g ? 2.0f + (f32)(b - r) / d : 4.0f + (f32)(r - g) / d;
        h *= 60.0f;
        if (h < 0.0f) {
            h += 360.0f;
        }
    }
    if (v < 0.16f || (s < 0.25f && v < 0.24f)) {
        Ps_Copy(out, "Black", 40);
        return;
    }
    if (s < 0.12f) {
        Ps_Copy(out, v > 0.85f ? "White" : v > 0.5f ? "Light grey" : "Dark grey", 40);
        return;
    }
    for (k = 0; k < 11 && h >= (f32)edge[k]; k++) {
    }
    if (h >= 340.0f) {
        k = 0;
    }
    sprintf(out, "%s%s", v < 0.42f ? "Dark " : s < 0.38f ? "Pale " : "", hues[k]);
}

/* Region k's name: what it is, or the parts drawing it and its colour ("Top & Trousers - Orange"). */
static char *Co_RegionName(s32 k) {
    static char out[64];
    char col[40];
    s32 *r = &sZw->reg[k * 4];
    s32 parts = r[3] & 0x1FFF, own = (r[3] >> 16) & 0x1FFF, names = 0, s;

    if (r[0] & ZW_TINT) { /* a texture ("Face", "Top & Trousers"), or one of its colours */
        s32 kind = k == ZW_REGS - 1 ? 0 : (r[2] >> 24) & 0xFF, entry = r[0] & 31;

        out[0] = 0;
        if (kind == 1) {
            Ps_Copy(out, "Face", 64);
        } else if (kind == 2) {
            Ps_Copy(out, "Hair", 64);
        } else {
            s32 first = -1, second = -1, more = 0;

            for (s = 0; s < 13; s++) { /* ("Top & Sleeves"; more than two: "Top, Belt & more") */
                if (!(parts & (1 << s))) {
                    continue;
                }
                if (first < 0) {
                    first = s;
                } else if (second < 0) {
                    second = s;
                } else {
                    more = 1;
                }
            }
            if (first >= 0) {
                sprintf(out, "%s", kCoPart[first]);
                names = 1;
            }
            if (second >= 0) {
                sprintf(out + Ps_Len(out), more ? ", %s & more" : " & %s", kCoPart[second]);
            }
            if (names == 0) {
                Ps_Copy(out, "Details", 64);
            }
        }
        if (k != ZW_REGS - 1) { /* (and its main colour as the fighter has it: two textures of the face, of the hair, told
                                   apart; the name stays when it is painted) */
            Co_Describe(k < sZw->regs ? sZw->regWas[k] : (u32)r[1], col);
            sprintf(out + Ps_Len(out), " - %s", col);
        } else {
            if (entry == 16) {
                sprintf(out, "Whole texture");
            } else {
                Co_Describe((u32)r[1], col);
                sprintf(out, "Colour %d - %s", (int)entry + 1, col);
            }
        }
        return out;
    }

    Co_Describe((u32)r[1], col);
    if (r[2] & ZW_REG_SKIN) {
        return "Skin";
    }
    if ((r[2] & ZW_REG_HAIR) && own == 0) {
        sprintf(out, "Hair - %s", col);
        return out;
    }
    if (r[2] & ZW_REG_BROWS) {
        sprintf(out, "Eyebrows - %s", col);
        return out;
    }
    if (r[2] & ZW_REG_EYES) {
        sprintf(out, "Eyes - %s", col);
        return out;
    }
    if (r[2] & ZW_REG_FACE) {
        sprintf(out, "Face - %s", col);
        return out;
    }
    out[0] = 0;
    {
        s32 first = -1, second = -1, more = 0;

        for (s = 0; s < 13; s++) { /* (two parts named: "Top & Sleeves"; more: "Top, Belt & more") */
            if (!((own != 0 ? own : parts) & (1 << s))) {
                continue;
            }
            if (first < 0) {
                first = s;
            } else if (second < 0) {
                second = s;
            } else {
                more = 1;
            }
        }
        if (first >= 0) {
            sprintf(out, "%s", kCoPart[first]);
            names = 1;
        }
        if (second >= 0) {
            sprintf(out + Ps_Len(out), more ? ", %s & more" : " & %s", kCoPart[second]);
            names = 2;
        }
    }
    if (names == 0) {
        Ps_Copy(out, "Details", 64);
    }
    sprintf(out + Ps_Len(out), " - %s%s", col, own != 0 ? " (own)" : "");
    return out;
}

/* The colour line region k has (its index in the warrior's colours, -1 none), and its colour now. */
static u32 Co_Now(s32 k, s32 *at) {
    s32 i;
    s32 own = (sZw->reg[k * 4 + 3] >> 16) & 0x1FFF;

    for (i = 0; i < sZw->w.colours; i++) {
        if (sZw->w.colourKey[i] == sZw->reg[k * 4] && sZw->w.colourPart[i] == own) {
            *at = i;
            return sZw->w.colourRgb[i];
        }
    }
    *at = -1;
    return (u32)sZw->reg[k * 4 + 1];
}

/* ---- colours as hue, strength, lightness (0..1) ---- */
static void Co_ToHsv(u32 rgb, f32 *h, f32 *s, f32 *v) {
    f32 r = (f32)(rgb & 0xFF) / 255.0f, g = (f32)((rgb >> 8) & 0xFF) / 255.0f, b = (f32)((rgb >> 16) & 0xFF) / 255.0f;
    f32 mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn;

    *v = mx;
    *s = mx > 0.0f ? d / mx : 0.0f;
    if (d <= 0.0f) {
        return; /* (the hue stays as it was: a grey has none) */
    }
    *h = mx == r ? (g - b) / d : mx == g ? 2.0f + (b - r) / d : 4.0f + (r - g) / d;
    *h /= 6.0f;
    if (*h < 0.0f) {
        *h += 1.0f;
    }
}

static u32 Co_FromHsv(f32 h, f32 s, f32 v) {
    f32 f, p, q, t, c[3];
    s32 i, k;
    u32 out = 0;

    h = (h - (f32)(s32)h) * 6.0f;
    i = (s32)h;
    f = h - (f32)i;
    p = v * (1.0f - s);
    q = v * (1.0f - s * f);
    t = v * (1.0f - s * (1.0f - f));
    switch (i % 6) {
    case 0: c[0] = v, c[1] = t, c[2] = p; break;
    case 1: c[0] = q, c[1] = v, c[2] = p; break;
    case 2: c[0] = p, c[1] = v, c[2] = t; break;
    case 3: c[0] = p, c[1] = q, c[2] = v; break;
    case 4: c[0] = t, c[1] = p, c[2] = v; break;
    default: c[0] = v, c[1] = p, c[2] = q; break;
    }
    for (k = 0; k < 3; k++) {
        s32 x = (s32)(c[k] * 255.0f + 0.5f);

        out |= (u32)(x < 0 ? 0 : x > 255 ? 255 : x) << (k * 8);
    }
    return out;
}

static u32 Co_Rgba(u32 rgb) {
    return Mx_Rgba((s32)(rgb & 0xFF), (s32)((rgb >> 8) & 0xFF), (s32)((rgb >> 16) & 0xFF), 0x80);
}

/* The regions read again (after the warrior's parts or its parts with own colours changed), the list made: shown
   regions only, BODY (skin, hair, face) then OUTFIT then DETAILS (the small ones). */
static void Co_Load(void) {
    s32 i;
    s32 n = 0;
    s32 g;

    /* the look's textures, the head's first (its hair, face, head), then the rest; the last region is the
       eyedropper's (a colour of a texture, or all of it) */
    sZw->regs = Port_WarriorTextures(&sZw->w, sZw->reg, ZW_REGS - 1);
    { /* (and their colours unpainted: the rows' names, and "was" beside a changed one) */
        static s32 tmp[ZW_REGS * 4];
        s32 m = Port_WarriorTextures(&sZw->w, tmp, (ZW_REGS - 1) | 0x20000);

        for (i = 0; i < sZw->regs; i++) {
            sZw->regWas[i] = i < m && tmp[i * 4] == sZw->reg[i * 4] ? (u32)tmp[i * 4 + 1] : (u32)sZw->reg[i * 4 + 1];
        }
    }
    for (g = 0; g < 2; g++) {
        s32 heading = 0;

        for (i = 0; i < sZw->regs; i++) {
            s32 kind = (sZw->reg[i * 4 + 2] >> 24) & 0xFF;
            s32 head = kind != 0 || (sZw->reg[i * 4 + 3] & 7) == sZw->reg[i * 4 + 3];

            if (head != (g == 0)) {
                continue;
            }
            if (!heading) {
                sZw->coList[n++] = -1 - g; /* (a heading) */
                heading = 1;
            }
            sZw->coList[n++] = i;
        }
    }
    sZw->coRows = n;
    if (sZw->coCur >= n || sZw->coCur < 0 || sZw->coList[sZw->coCur] < 0) {
        for (sZw->coCur = 0; sZw->coCur < n && sZw->coList[sZw->coCur] < 0; sZw->coCur++) {
        }
    }
    sZw->paintKey = -1; /* (paint the model again) */
}

/* The region under the cursor, -1 none. */
static s32 Co_Region(void) {
    if (sZw->colourStage == 1 && sZw->picFrom) {
        return ZW_REGS - 1; /* (the eyedropper's colour) */
    }
    return sZw->coCur >= 0 && sZw->coCur < sZw->coRows ? sZw->coList[sZw->coCur] : -1;
}

static void Co_Set(s32 k, u32 rgb) {
    PortWarrior *w = &sZw->w;
    s32 at;

    Co_Now(k, &at);
    if (at < 0) {
        if (w->colours >= ZW_COLOURS) {
            Zw_Note("Forty-eight colours is the most a warrior can hold!");
            return;
        }
        at = w->colours++;
        w->colourKey[at] = sZw->reg[k * 4];
        w->colourPart[at] = (sZw->reg[k * 4 + 3] >> 16) & 0x1FFF;
    }
    w->colourRgb[at] = rgb;
    sZw->dirty = 1;
    sZw->paintDue = ZW_PAINT_REST; /* (painted into the preview's model file a moment later, which the screen reads again) */
}

static void Co_ClearAt(s32 at) {
    PortWarrior *w = &sZw->w;
    s32 i;

    for (i = at; i < w->colours - 1; i++) {
        w->colourKey[i] = w->colourKey[i + 1];
        w->colourRgb[i] = w->colourRgb[i + 1];
        w->colourPart[i] = w->colourPart[i + 1];
    }
    w->colours--;
    sZw->dirty = 1;
    sZw->paintDue = ZW_PAINT_REST;
}

static void Co_Clear(s32 k) {
    s32 at;

    Co_Now(k, &at);
    if (at >= 0) {
        Co_ClearAt(at);
    }
}

/* Paints the warrior's colours into the model on the screen when they changed or another model is shown. */
static void Co_PaintTick(void) {
    void *tex = Mm_ViewTex();
    s32 view = Mm_ViewKey();

    if (tex == NULL || view < 0 || (view == sZw->paintKey && tex == sZw->paintTex)) {
        return;
    }
    if (1) { /* (the model on the screen is always one made with its colours: Port_WarriorPreviewPaint; nothing painted
                into it here any more) */
        sZw->paintKey = view;
        sZw->paintTex = tex;
        return;
    }
    Glow_Off(); /* (its colours back first: the paint reads them) */
    if (Port_WarriorPaintLive(&sZw->w, tex)) {
        s32 i;

        for (i = 0; i < 5; i++) { /* the eyes and brows, the mouths */
            Port_WarriorPaintFollow(Mm_ViewExpr(i));
        }
        sZw->glowOn = 0; /* (its palettes are new: the glow takes them again) */
        sZw->glowState = -1;
    }
    sZw->paintKey = view;
    sZw->paintTex = tex;
}

static void Co_Open(s32 k);

/* The eyedropper: texture row k as a picture (Port_WarriorTexturePicture); its colours as they are now. */
static void Co_PicLoad(s32 k) {
    s32 wh;

    if (sZw->pic == NULL) {
        sZw->pic = Heap_Alloc(256 * 256, 0x40, 0, 2);
    }
    if (sZw->pic == NULL || k < 0) {
        return;
    }
    wh = Port_WarriorTexturePicture(&sZw->w, sZw->reg[k * 4], sZw->pic, sZw->picPal);
    sZw->picGen++; /* (its texture made again) */
    if (wh == 0) {
        sZw->picW = sZw->picH = 0;
        return;
    }
    if (k != sZw->picRow || sZw->picW != (wh & 0xFFFF) || sZw->picH != (wh >> 16)) {
        sZw->picX = (wh & 0xFFFF) / 2;
        sZw->picY = (wh >> 16) / 2;
    }
    sZw->picW = wh & 0xFFFF;
    sZw->picH = wh >> 16;
    sZw->picRow = k;
}

/* The eyedropper's region (the last): colour `entry` of texture row k (16: all its colours). */
static s32 Co_PicTarget(s32 k, s32 entry) {
    s32 t = ZW_REGS - 1;
    s32 most = 0, i;
    static s32 cnt[16];

    if (entry == 16) { /* (all of it: as its most drawn colour was) */
        memset(cnt, 0, sizeof(cnt));
        for (i = 0; i < sZw->picW * sZw->picH; i++) {
            cnt[sZw->pic[i] & 15]++;
        }
        for (i = 1; i < 16; i++) {
            most = cnt[i] > cnt[most] ? i : most;
        }
    }
    sZw->reg[t * 4] = (sZw->reg[k * 4] & ~31) | entry;
    sZw->reg[t * 4 + 1] = (s32)(sZw->picPal[16 + (entry == 16 ? most : entry)] & 0xFFFFFF);
    sZw->reg[t * 4 + 2] = 1;
    sZw->reg[t * 4 + 3] = sZw->reg[k * 4 + 3] & 0x1FFF;
    return t;
}

/* ---- drawing on the eyedropper's texture: a brush of its 16 colours, a fill, a picture file put on it (kept in
   paint/ of the warrior's folder: Port_PaintSave) ---- */
static s32 Co_PicKey(void) {
    return (sZw->reg[sZw->picRow * 4] & ~31) | 16;
}

static void Co_PicStore(void) { /* (the stroke kept: the preview painted with it a moment later) */
    if (!sZw->picDirty) {
        return;
    }
    sZw->picDirty = 0;
    if (Port_PaintSave(Co_PicKey(), sZw->pic, sZw->picW, sZw->picH, NULL)) {
        sZw->dirty = 1;
        sZw->paintDue = ZW_PAINT_REST;
    }
}

static void Co_PicPaintAt(s32 cx, s32 cy) {
    static const s32 kRad[5] = {0, 1, 2, 3, 5};
    s32 r = kRad[sZw->picSize >= 0 && sZw->picSize < 5 ? sZw->picSize : 0], x, y;

    for (y = cy - r; y <= cy + r; y++) {
        for (x = cx - r; x <= cx + r; x++) {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r + r && x >= 0 && y >= 0 && x < sZw->picW && y < sZw->picH && (sZw->pic[y * sZw->picW + x] & 15) != sZw->picBrush) {
                sZw->pic[y * sZw->picW + x] = (u8)sZw->picBrush;
                sZw->picDirty = 1;
                sZw->picGen++;
            }
        }
    }
}

static void Co_PicFill(s32 x0, s32 y0) { /* (the run of one colour under the cursor, four ways) */
    static u32 queue[256 * 256];
    s32 w = sZw->picW, h = sZw->picH, from = sZw->pic[y0 * w + x0] & 15, head = 0, tail = 0;

    if (from == sZw->picBrush) {
        return;
    }
    sZw->pic[y0 * w + x0] = (u8)sZw->picBrush;
    queue[tail++] = (u32)(y0 * w + x0);
    while (head < tail) {
        s32 at = (s32)queue[head++], x = at % w, y = at / w, d;

        for (d = 0; d < 4; d++) {
            s32 nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);

            if (nx >= 0 && ny >= 0 && nx < w && ny < h && (sZw->pic[ny * w + nx] & 15) == from && tail < 256 * 256) {
                sZw->pic[ny * w + nx] = (u8)sZw->picBrush;
                queue[tail++] = (u32)(ny * w + nx);
            }
        }
    }
    sZw->picDirty = 1;
    sZw->picGen++;
}

static void Co_PicLoad(s32 k);

static void Co_PicDrawUpdate(Pad *pad) {
    if (pad->gameHeld & PADG_CROSS) { /* the brush, as the cursor moves */
        Co_PicPaintAt(sZw->picX, sZw->picY);
    } else {
        Co_PicStore();
    }
    if (pad->gamePressed & PADG_CIRCLE) {
        Co_PicFill(sZw->picX, sZw->picY);
        Co_PicStore();
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_SQUARE) { /* its colour for the brush */
        sZw->picBrush = sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15;
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_R1) {
        sZw->picBrush = (sZw->picBrush + 1) % 16;
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_START) {
        sZw->picSize = (sZw->picSize + 1) % 3;
        Zw_Note(sZw->picSize == 0 ? "Fine brush." : sZw->picSize == 1 ? "Medium brush." : "Big brush.");
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_L1) { /* the drawing gone: the texture as it was made */
        Port_PaintClear(Co_PicKey());
        sZw->picDirty = 0;
        Co_PicLoad(sZw->picRow);
        sZw->dirty = 1;
        sZw->paintDue = ZW_PAINT_REST;
        Zw_Note("Drawing wiped: the texture is as it was.");
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Co_PicStore();
        sZw->picDraw = 0;
        Snd_PlaySe(1, 2);
    }
}

static void Co_PicUpdate(Pad *pad) {
    s32 k = sZw->picRow;
    s32 dx = 0, dy = 0, step;
    f32 ax = pad->left[0], ay = pad->left[1];

    if (sZw->picW <= 0) {
        sZw->colourStage = 0;
        return;
    }
    if (pad->gameHeld & (PADG_LEFT | PADG_RIGHT | PADG_UP | PADG_DOWN)) {
        sZw->picHold++;
    } else {
        sZw->picHold = 0;
    }
    step = sZw->picHold > 30 ? 3 : 1;
    if (pad->gameRepeat & PADG_LEFT) {
        dx = -step;
    } else if (pad->gameRepeat & PADG_RIGHT) {
        dx = step;
    }
    if (pad->gameRepeat & PADG_UP) {
        dy = -step;
    } else if (pad->gameRepeat & PADG_DOWN) {
        dy = step;
    }
    { /* (the stick: gently, faster the further) */
        static f32 accX, accY;

        accX = ax * ax > 0.04f ? accX + ax * (0.15f + 0.9f * ax * ax) : 0.0f;
        accY = ay * ay > 0.04f ? accY + ay * (0.15f + 0.9f * ay * ay) : 0.0f;
        while (accX >= 1.0f) { dx++; accX -= 1.0f; }
        while (accX <= -1.0f) { dx--; accX += 1.0f; }
        while (accY >= 1.0f) { dy++; accY -= 1.0f; }
        while (accY <= -1.0f) { dy--; accY += 1.0f; }
    }
    sZw->picX = sZw->picX + dx < 0 ? 0 : sZw->picX + dx >= sZw->picW ? sZw->picW - 1 : sZw->picX + dx;
    sZw->picY = sZw->picY + dy < 0 ? 0 : sZw->picY + dy >= sZw->picH ? sZw->picH - 1 : sZw->picY + dy;
    if (sZw->picImport) { /* a picture file chosen: put on this texture */
        char path[700];

        if (Port_DropTake(path)) {
            sZw->picImport = 0;
            if (Port_PaintImport(Co_PicKey(), path, sZw->picW, sZw->picH, sZw->pic, sZw->picPal)) {
                Co_PicLoad(k);
                sZw->dirty = 1;
                sZw->paintDue = ZW_PAINT_REST;
                Zw_Note("Picture on! Fitted to the texture in 16 colours. R1 to draw on it more.");
                Snd_PlaySe(1, 1);
            } else {
                Zw_Note("That picture could not be read. Try a PNG or JPG.");
                Snd_PlaySe(1, 2);
            }
        }
    }
    if (pad->gamePressed & (PADG_L1 | PADG_R1)) { /* the next colour of the texture, the cursor onto its nearest pixel */
        s32 step = (pad->gamePressed & PADG_R1) ? 1 : -1, cur = sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15, t, i;

        for (t = 1; t < 16; t++) {
            s32 c = (cur + step * t + 32) % 16, best = -1, bd = 0;

            for (i = 0; i < sZw->picW * sZw->picH; i++) {
                if ((sZw->pic[i] & 15) == c) {
                    s32 ddx = i % sZw->picW - sZw->picX, ddy = i / sZw->picW - sZw->picY, d = ddx * ddx + ddy * ddy;

                    if (best < 0 || d < bd) {
                        best = i;
                        bd = d;
                    }
                }
            }
            if (best >= 0) {
                sZw->picX = best % sZw->picW;
                sZw->picY = best / sZw->picW;
                break;
            }
        }
        Snd_PlaySe(1, 0);
        return;
    }
    if (pad->gamePressed & PADG_CROSS) { /* this colour */
        sZw->picFrom = 1;
        Co_Open(Co_PicTarget(k, sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15));
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_CIRCLE) { /* all of the texture */
        sZw->picFrom = 1;
        Co_Open(Co_PicTarget(k, 16));
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_SQUARE) { /* this colour as it was */
        s32 at;

        Co_Now(Co_PicTarget(k, sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15), &at);
        if (at >= 0) {
            Co_ClearAt(at);
            Co_PicLoad(k);
            Snd_PlaySe(1, 2);
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        sZw->colourStage = 0;
        sZw->picFrom = 0;
        Co_Load();
        Snd_PlaySe(1, 2);
    }
}

/* The eyedropper's picture as one texture (TBO_PICTURE), each pixel made a square of f x f so it stays sharp when
   drawn large; the colour under the cursor mixed a little towards pink, gently in and out (`glow` 0..8), not blinked
   (a blink of a whole outfit was a flicker). Made again only when something it shows changes. */
static void Co_PicTexture(s32 cur, s32 glow) {
    static u32 sig = 0;
    u32 now = 0;
    s32 w = sZw->picW, h = sZw->picH, i;
    u32 pal[16];

    for (i = 0; i < 16; i++) {
        now = now * 31u + sZw->picPal[i];
    }
    now = now * 31u + (u32)(cur * 16 + glow) + (u32)(w * 7919 + h) + (u32)sZw->picRow * 104729u + (u32)sZw->picGen;
    if (now == sig) {
        return;
    }
    for (i = 0; i < 16; i++) {
        u32 v = sZw->picPal[i];
        s32 r = (s32)(v & 0xFF), g = (s32)((v >> 8) & 0xFF), b = (s32)((v >> 16) & 0xFF);

        if (i == cur) {
            r += (255 - r) * glow / 16;
            g += (64 - g) * glow / 16;
            b += (208 - b) * glow / 16;
        }
        pal[i] = (u32)r | ((u32)g << 8) | ((u32)b << 16) | (0x80u << 24);
    }
    if (Port_PictureIndexed(sZw->pic, w, h, pal)) { /* (made large and sent to the screen on the PC's side) */
        sig = now;
    }
}

static void Co_PicDraw(f32 ox) {
    s32 w = sZw->picW, h = sZw->picH, x, cur, glow;
    f32 sc, px, py;
    char line[48];

    if (w <= 0 || h <= 0) {
        return;
    }
    sc = 540.0f / (f32)w < 330.0f / (f32)h ? 540.0f / (f32)w : 330.0f / (f32)h;
    px = 325.0f + ox - (f32)w * sc * 0.5f;
    py = 150.0f;
    cur = sZw->pic[sZw->picY * w + sZw->picX] & 15;
    glow = (sZw->frames / 10) % 12; /* (0..6..0: in and out, slowly) */
    glow = glow > 6 ? 12 - glow : glow;
    Mx_TextA(325.0f + ox, 135.0f, Co_RegionName(sZw->picRow), 20.0f, 540.0f, MX_GOLD, 1, 0x80);
    Co_PicTexture(cur, glow);
    Mx_PicEx(TBO_PICTURE, px, py, px + (f32)w * sc, py + (f32)h * sc, 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    Mx_Frame(px - 2.0f, py - 2.0f, px + (f32)w * sc + 2.0f, py + (f32)h * sc + 2.0f, 2.0f, Mx_Rgba(250, 205, 90, 0x80));
    Mx_Frame(px + (f32)sZw->picX * sc - 3.0f, py + (f32)sZw->picY * sc - 3.0f, px + (f32)(sZw->picX + 1) * sc + 3.0f,
             py + (f32)(sZw->picY + 1) * sc + 3.0f, 2.0f, Mx_Rgba(255, 255, 255, 0x80));
    for (x = 0; x < 16; x++) { /* its sixteen colours, the one under the cursor lit */
        u32 v = sZw->picPal[x];

        Zw_Swatch(v & 0xFFFFFF, 70.0f + ox + (f32)x * 34.0f, 512.0f, x == cur ? 30.0f : 22.0f, x == cur);
    }
    Co_Describe(sZw->picPal[cur] & 0xFFFFFF, line);
    Mx_TextA(70.0f + ox, 552.0f, "Colour under the cursor:", 18.0f, 0.0f, MX_WHITE, 0, 0x80);
    Mx_TextA(330.0f + ox, 552.0f, line, 18.0f, 0.0f, MX_GOLD, 0, 0x80);
    if ((sZw->picPal[cur] & 0xFFFFFF) != (sZw->picPal[16 + cur] & 0xFFFFFF)) {
        Mx_TextA(70.0f + ox, 584.0f, "was", 16.0f, 0.0f, MX_LAV, 0, 0x80);
        Zw_Swatch(sZw->picPal[16 + cur] & 0xFFFFFF, 120.0f + ox, 584.0f, 20.0f, 0);
    }
    if (sZw->picDraw) { /* (drawing: the brush's colour and size) */
        Zw_Swatch(sZw->picPal[sZw->picBrush & 15] & 0xFFFFFF, 588.0f + ox, 135.0f, 34.0f, 1);
        Mx_TextA(560.0f + ox, 135.0f, sZw->picSize == 0 ? "fine" : sZw->picSize == 1 ? "medium" : "big", 14.0f, 80.0f, MX_LAV, 2, 0x80);
        Zw_Prompt(3, "Move");
        Zw_Prompt(0, "Paint");
        Zw_Prompt(4, "Fill");
        Zw_Prompt(1, "Take colour");
        Zw_Prompt(8, "Next colour");
        Zw_Prompt(7, "Brush size");
        Zw_Prompt(9, "Wipe drawing");
        Zw_Prompt(2, "Done");
        return;
    }
    Zw_Prompt(3, "Move");
    Zw_Prompt(0, "This colour");
    Zw_Prompt(4, "Whole texture");
    Zw_Prompt(1, "Original");
    Zw_Prompt(14, "Colours");
    Zw_Prompt(2, "Back");
}

/* The picker opened on region k: its colour now as hue / strength / lightness; what to go back to. */
static void Co_Open(s32 k) {
    s32 at;
    u32 now = Co_Now(k, &at);

    sZw->pkBack = sZw->w;
    sZw->pkRow = CP_HUE;
    Co_ToHsv(now & 0xFFFFFF, &sZw->pkH, &sZw->pkS, &sZw->pkV);
    sZw->pkC = (now >> 24) != 0 ? (s32)(now >> 24) : 100;
    sZw->colourStage = 1;
    sZw->modeT = 0;
}

/* Whether region k is a whole texture (the eyedropper's "all of it"). */
static s32 Co_Whole(s32 k) {
    return k >= 0 && (sZw->reg[k * 4] & 31) == 16 && sZw->colourStage != 0;
}

/* The picker's colour onto the region. */
static void Co_Apply(void) {
    s32 k = Co_Region();

    if (k >= 0) { /* (a whole texture's contrast in the colour's top byte) */
        u32 con = Co_Whole(k) && sZw->pkC != 100 ? (u32)sZw->pkC : 0;

        Co_Set(k, Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV) | con << 24);
    }
}

static void Co_Remember(u32 rgb) { /* (the colours used lately, newest first) */
    s32 i;
    s32 j = ZW_RECENT - 1;

    for (i = 0; i < ZW_RECENT; i++) {
        if (sZw->recent[i] == rgb) {
            j = i;
            break;
        }
    }
    for (; j > 0; j--) {
        sZw->recent[j] = sZw->recent[j - 1];
    }
    sZw->recent[0] = rgb;
    if (sZw->recents < ZW_RECENT) {
        sZw->recents++;
    }
}

/* The parts the region is on, as choices for "only on": 0 everywhere, then each part. */
static s32 Co_OnlyChoices(s32 k, s32 *part) {
    s32 n = 0, s;
    s32 parts = sZw->reg[k * 4 + 3] & 0x1FFF;

    part[n++] = 0;
    for (s = 0; s < 13; s++) {
        if (parts & (1 << s)) {
            part[n++] = 1 << s;
        }
    }
    return n;
}

static void Co_Bar(f32 x0, f32 y, f32 w, s32 which, f32 at, s32 lit) { /* a slider of hue, strength or lightness */
    s32 i;
    f32 seg = w / 36.0f;

    for (i = 0; i < 36; i++) {
        f32 k = ((f32)i + 0.5f) / 36.0f;
        u32 c = which == 0 ? Co_FromHsv(k, 0.85f, 0.95f) : which == 1 ? Co_FromHsv(sZw->pkH, k, sZw->pkV) : Co_FromHsv(sZw->pkH, sZw->pkS, k);

        Mx_Rect(x0 + seg * (f32)i, y - 10.0f, x0 + seg * (f32)(i + 1) + 0.5f, y + 10.0f, Co_Rgba(c), Co_Rgba(c));
    }
    Mx_Frame(x0 - 2.0f, y - 12.0f, x0 + w + 2.0f, y + 12.0f, 2.0f, lit ? Mx_Rgba(0xFF, 0xD8, 0x60, 0x80) : Mx_Rgba(0x30, 0x20, 0x50, 0x80));
    Zw_Swatch(which == 0 ? Co_FromHsv(sZw->pkH, 0.85f, 0.95f) : Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV), x0 + w * at, y, lit ? 30.0f : 24.0f, lit);
}

static void Co_Draw(void) {
    s32 i;
    s32 at;
    s32 k = Co_Region();
    f32 slide = 1.0f - Zw_In(sZw->modeT, 12);
    f32 ox = -60.0f * slide;

    Zw_Host(1250.0f, 330.0f, 300.0f, 1); /* (behind the model) */
    Zw_EditStage(900.0f, 540.0f, 400.0f); /* (first: the list goes over the model, an arm held out does not cover it) */
    Co_PaintTick();
    Ws_ShapeTick();
    Zw_Title("COLOURS");
    { /* (how many of the colours are used) */
        char budget[40];

        sprintf(budget, "Colours %d / %d", (int)sZw->w.colours, (int)ZW_COLOURS);
        Mx_Text(1340.0f, 160.0f, budget, 17.0f, 0.0f, sZw->w.colours >= ZW_COLOURS - 2 ? MX_GOLD : MX_LAV, 2);
    }
    Mx_Box(ZW_WINDOW, 30.0f + ox, 122.0f, 620.0f + ox, 628.0f, 22.0f, (s32)(128.0f * (1.0f - slide)));
    if (sZw->colourStage == 3) {
        Co_PicDraw(ox);
    } else if (sZw->colourStage == 0) {
        f32 want = CO_Y + (f32)(sZw->coCur - sZw->coTop) * CO_PITCH;

        sZw->zoneY += (want - sZw->zoneY) * 0.4f;
        for (i = 0; i < CO_ROWS && sZw->coTop + i < sZw->coRows; i++) {
            s32 row = sZw->coTop + i;
            s32 z = sZw->coList[row];
            f32 y = CO_Y + (f32)i * CO_PITCH;
            s32 sel = row == sZw->coCur;
            s32 a = (s32)(128.0f * Zw_In(sZw->modeT - i, 10));
            u32 now;

            if (z < 0) { /* a heading */
                static char *heads[3] = {"HEAD", "BODY", "DETAILS"};

                Mx_Bar(ZW_SECTION, 50.0f + ox, y - 14.0f, 260.0f + ox, y + 14.0f, a);
                Mx_TextA(66.0f + ox, y, heads[-1 - z], 18.0f, 180.0f, MX_DARK, 0, a);
                continue;
            }
            now = Co_Now(z, &at);
            if (sel) {
                Zw_SelBar(42.0f + ox, sZw->zoneY - 19.0f, 608.0f + ox, sZw->zoneY + 19.0f);
            }
            Zw_Swatch(now, 80.0f + ox, y, 34.0f, sel);
            Mx_TextA(110.0f + ox, y, Co_RegionName(z), 20.0f, 360.0f, sel ? MX_DARK : MX_WHITE, 0, a);
            if (at >= 0) { /* (what it was -> what it is) */
                Zw_Swatch(z < sZw->regs ? sZw->regWas[z] : (u32)sZw->reg[z * 4 + 1], 500.0f + ox, y, 24.0f, 0);
                Mx_TextA(522.0f + ox, y, ">", 18.0f, 0.0f, sel ? MX_DARK : MX_GOLD, 0, a);
                Zw_Swatch(now, 552.0f + ox, y, 24.0f, 0);
            }
        }
        if (sZw->coTop > 0) {
            Mx_Fit(ZW_UP, 325.0f + ox, 134.0f + 3.0f * Zw_Beat(36), 24.0f, 0x80);
        }
        if (sZw->coTop + CO_ROWS < sZw->coRows) {
            Mx_Fit(ZW_DOWN, 325.0f + ox, 618.0f - 3.0f * Zw_Beat(36), 24.0f, 0x80);
        }
        if (sZw->coRows == 0) {
            Mx_Text(325.0f, 360.0f, "NO COLOURS FOUND", 24.0f, 0.0f, MX_WHITE, 1);
        }
        Zw_Prompt(0, "Open texture");
        Zw_Prompt(8, "Whole texture");
        Zw_Prompt(1, "Original");
        Zw_Prompt(4, "All original");
        Zw_Prompt(7, "Save");
        Zw_Prompt(2, "Done");
    } else if (k >= 0) { /* the picker */
        static char *labels[CP_ROWS] = {"Hue", "Strength", "Light", "", "", "Lately", "Only on"};
        f32 pop = Zw_In(sZw->modeT, 8);
        u32 col = Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV);
        s32 choice[14];
        s32 nc = Co_OnlyChoices(k, choice);
        s32 own = (sZw->reg[k * 4 + 3] >> 16) & 0x1FFF;

        Mx_TextA(325.0f + ox, 150.0f, Co_RegionName(k), 22.0f, 540.0f, MX_GOLD, 1, (s32)(128.0f * pop));
        Zw_Swatch(col, 90.0f + ox, 208.0f, 64.0f * pop, 1);
        Mx_TextA(140.0f + ox, 208.0f, "Your colour", 18.0f, 200.0f, MX_LAV, 0, (s32)(128.0f * pop));
        if (Co_Whole(k)) { /* (the whole texture: its contrast too) */
            char con[32];

            sprintf(con, "Contrast %d%%  (L1 / R1)", (int)sZw->pkC);
            Mx_TextA(330.0f + ox, 208.0f, con, 17.0f, 200.0f, MX_GOLD, 0, (s32)(128.0f * pop));
        }
        Zw_Swatch((u32)sZw->reg[k * 4 + 1], 560.0f + ox, 208.0f, 40.0f * pop, 0);
        Mx_TextA(530.0f + ox, 208.0f, "Was", 16.0f, 0.0f, MX_LAV, 2, (s32)(128.0f * pop));
        for (i = 0; i < CP_ROWS; i++) {
            f32 y = 266.0f + (f32)i * 50.0f;
            s32 lit = i == sZw->pkRow;
            s32 j;

            if (labels[i][0]) {
                static char *whole[3] = {"Hue", "Saturation", "Brightness"}; /* (the whole texture: its three as they are known) */

                Mx_TextA(50.0f + ox, y, Co_Whole(k) && i < 3 ? whole[i] : labels[i], 18.0f, 110.0f, lit ? MX_GOLD : MX_LAV, 0, (s32)(128.0f * pop));
            }
            if (i <= CP_VAL) {
                Co_Bar(170.0f + ox, y, 410.0f, i, i == CP_HUE ? sZw->pkH : i == CP_SAT ? sZw->pkS : sZw->pkV, lit);
            } else if (i == CP_SW1 || i == CP_SW2) {
                for (j = 0; j < 15; j++) {
                    s32 n = (i - CP_SW1) * 15 + j;

                    Zw_Swatch(kSwatch[n], 70.0f + ox + (f32)j * 35.5f, y, lit && sZw->pkSwatch == j ? 34.0f : 28.0f, lit && sZw->pkSwatch == j);
                }
            } else if (i == CP_RECENT) {
                for (j = 0; j < sZw->recents; j++) {
                    Zw_Swatch(sZw->recent[j], 190.0f + ox + (f32)j * 46.0f, y, lit && sZw->pkSwatch == j ? 38.0f : 32.0f, lit && sZw->pkSwatch == j);
                }
                if (sZw->recents == 0) {
                    Mx_TextA(190.0f + ox, y, "(none yet)", 16.0f, 0.0f, MX_LAV, 0, (s32)(128.0f * pop));
                }
            } else if (i == CP_ONLY) {
                char line[64];

                if (own != 0) {
                    sprintf(line, "This part's own colours  (Cross: shared again)");
                } else if (nc <= 2) {
                    sprintf(line, "Everywhere it is");
                } else {
                    s32 c = sZw->pkOnly;
                    s32 s = 0;

                    while (c > 0 && s < 13 && !(choice[c] & (1 << s))) {
                        s++;
                    }
                    sprintf(line, c == 0 ? "< Everywhere >" : "< %s only >  (Cross: its own colours)", kCoPart[s]);
                }
                Mx_TextA(190.0f + ox, y, line, 18.0f, 410.0f, lit ? MX_WHITE : MX_LAV, 0, (s32)(128.0f * pop));
            }
        }
        Zw_Prompt(15, "Choose");
        if (Co_Whole(Co_Region())) {
            Zw_Prompt(14, "Contrast");
        }
        Zw_Prompt(0, "Keep");
        Zw_Prompt(1, "Original");
        Zw_Prompt(2, "Cancel");
    }
    if (k >= 0 && !sZw->saving) { /* the region under the cursor blinks on the model (not while it is being painted) */
        s32 own = (sZw->reg[k * 4 + 3] >> 16) & 0x1FFF;

        Glow_Update(sZw->colourStage == 0 ? sZw->reg[k * 4] : 0, own, 1);
    } else {
        Glow_Off();
    }
    if (sZw->dirty) {
        Mx_Bar(ZW_PILL, 1150.0f, 110.0f, 1340.0f, 146.0f, 0x60 + (s32)(0x20 * Zw_Beat(60)));
        Mx_Text(1245.0f, 128.0f, "NOT SAVED", 17.0f, 170.0f, MX_DARK, 1);
    }
    if (sZw->saving) {
        Zw_Talk("Painting your warrior... don't smudge it!");
    } else if (sZw->colourStage == 3) {
        Zw_Talk("Point at a colour and press Cross! Circle paints the whole texture.");
    } else if (sZw->colourStage) {
        Zw_Talk(sZw->pkRow == CP_ONLY ? "One part only? It gets colours of its own, heh heh!" : "Slide it around! Your warrior changes as you go.");
    } else {
        Zw_Talk("Pick a texture of your warrior, then the colour to change on it!");
    }
}

static void Co_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 k = Co_Region();

    if (Zw_SaveTick() || !live) {
        return;
    }
    if (sZw->colourStage == 0) {
        s32 was = sZw->coCur;
        s32 n = sZw->coRows;

        if (n > 0 && (pad->gameRepeat & (PADG_UP | PADG_UP2))) {
            do {
                sZw->coCur = sZw->coCur > 0 ? sZw->coCur - 1 : n - 1;
            } while (sZw->coList[sZw->coCur] < 0);
        } else if (n > 0 && (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2))) {
            do {
                sZw->coCur = sZw->coCur < n - 1 ? sZw->coCur + 1 : 0;
            } while (sZw->coList[sZw->coCur] < 0);
        }
        if (sZw->coCur != was) {
            Snd_PlaySe(1, 0);
        }
        if (sZw->coCur - 1 < sZw->coTop) {
            sZw->coTop = sZw->coCur - 1 < 0 ? 0 : sZw->coCur - 1;
        }
        if (sZw->coCur >= sZw->coTop + CO_ROWS) {
            sZw->coTop = sZw->coCur - CO_ROWS + 1;
        }
        if ((pad->gamePressed & PADG_CROSS) && k >= 0) { /* the texture as a picture: the eyedropper */
            Co_PicLoad(k);
            if (sZw->picW > 0) {
                sZw->colourStage = 3;
                sZw->picFrom = 0;
                sZw->modeT = 12;
            }
            Snd_PlaySe(1, 1);
        } else if ((pad->gamePressed & PADG_R1) && k >= 0) { /* the whole texture at once (as the eyedropper's "all of it") */
            Co_PicLoad(k);
            if (sZw->picW > 0) {
                sZw->picFrom = 1;
                sZw->colourStage = 3;
                Co_Open(Co_PicTarget(k, 16));
            }
            Snd_PlaySe(1, 1);
        } else if ((pad->gamePressed & PADG_SQUARE) && k >= 0) { /* all its colours as they were */
            s32 i;

            for (i = sZw->w.colours - 1; i >= 0; i--) {
                if ((sZw->w.colourKey[i] & ~31) == (sZw->reg[k * 4] & ~31)) {
                    Co_ClearAt(i);
                }
            }
            Snd_PlaySe(1, 2);
        } else if (pad->gamePressed & PADG_CIRCLE) { /* all back to how they were */
            s32 parts = 0, i;

            for (i = 0; i < sZw->w.colours; i++) {
                parts |= sZw->w.colourPart[i];
            }
            sZw->w.colours = 0;
            sZw->dirty = 1;
            sZw->paintDue = ZW_PAINT_REST;
            if (parts) { /* (parts with own colours: the look changes) */
                Zw_Changed();
                Co_Load();
            }
            Zw_Note("Back to the colours it came with!");
            Snd_PlaySe(1, 2);
        } else if (pad->gamePressed & PADG_START) {
            sZw->saving = 2;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_TRIANGLE) {
            Zw_SetMode(ZW_EDIT);
            Snd_PlaySe(1, 2);
        }
        return;
    }
    if (sZw->colourStage == 3) {
        Co_PicUpdate(pad);
        return;
    }
    if (k < 0) {
        sZw->colourStage = 0;
        return;
    }
    { /* the picker */
        s32 rows = CP_ROWS;
        s32 left = pad->gameRepeat & (PADG_LEFT | PADG_LEFT2), right = pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2);
        s32 hl = pad->gameHeld & (PADG_LEFT | PADG_LEFT2), hr = pad->gameHeld & (PADG_RIGHT | PADG_RIGHT2);
        s32 changed = 0;
        s32 choice[14];
        s32 nc = Co_OnlyChoices(k, choice);

        if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            sZw->pkRow = (sZw->pkRow + rows - 1) % rows;
            sZw->pkSwatch = 0;
            Snd_PlaySe(1, 0);
        } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            sZw->pkRow = (sZw->pkRow + 1) % rows;
            sZw->pkSwatch = 0;
            Snd_PlaySe(1, 0);
        }
        if (sZw->pkRow <= CP_VAL && (hl || hr)) { /* sliders: held, they glide */
            f32 *v = sZw->pkRow == CP_HUE ? &sZw->pkH : sZw->pkRow == CP_SAT ? &sZw->pkS : &sZw->pkV;
            f32 step = sZw->pkRow == CP_HUE ? 0.006f : 0.008f;

            *v += hl ? -step : step;
            if (sZw->pkRow == CP_HUE) {
                *v = *v < 0.0f ? *v + 1.0f : *v >= 1.0f ? *v - 1.0f : *v;
            } else {
                *v = *v < 0.0f ? 0.0f : *v > 1.0f ? 1.0f : *v;
            }
            changed = 1;
        } else if ((sZw->pkRow == CP_SW1 || sZw->pkRow == CP_SW2 || sZw->pkRow == CP_RECENT) && (left || right)) {
            s32 count = sZw->pkRow == CP_RECENT ? sZw->recents : 15;

            if (count > 0) {
                sZw->pkSwatch = (sZw->pkSwatch + (left ? count - 1 : 1)) % count;
                Co_ToHsv(sZw->pkRow == CP_RECENT ? sZw->recent[sZw->pkSwatch] : kSwatch[(sZw->pkRow - CP_SW1) * 15 + sZw->pkSwatch], &sZw->pkH, &sZw->pkS,
                         &sZw->pkV);
                changed = 1;
                Snd_PlaySe(1, 0);
            }
        } else if (sZw->pkRow == CP_ONLY && (left || right) && nc > 2) {
            sZw->pkOnly = (sZw->pkOnly + (left ? nc - 1 : 1)) % nc;
            Snd_PlaySe(1, 0);
        }
        if ((pad->gameRepeat & (PADG_L1 | PADG_R1)) && Co_Whole(k)) { /* a whole texture: its contrast */
            sZw->pkC += (pad->gameRepeat & PADG_R1) ? 5 : -5;
            sZw->pkC = sZw->pkC < 20 ? 20 : sZw->pkC > 250 ? 250 : sZw->pkC;
            changed = 1;
            Snd_PlaySe(1, 0);
        }
        if (changed) {
            Co_Apply();
        }
        if (pad->gamePressed & PADG_CROSS) {
            s32 own = (sZw->reg[k * 4 + 3] >> 16) & 0x1FFF;

            if ((sZw->pkRow == CP_SW1 || sZw->pkRow == CP_SW2 || (sZw->pkRow == CP_RECENT && sZw->recents > 0))) { /* (the swatch under
                                                                     the cursor taken, also when it was come to from above) */
                Co_ToHsv(sZw->pkRow == CP_RECENT ? sZw->recent[sZw->pkSwatch] : kSwatch[(sZw->pkRow - CP_SW1) * 15 + sZw->pkSwatch], &sZw->pkH, &sZw->pkS,
                         &sZw->pkV);
                Co_Apply();
            }
            if (sZw->pkRow == CP_ONLY && own != 0) { /* shared again: its line goes, the look changes */
                Co_Clear(k);
                Zw_Changed();
                Co_Load();
                sZw->colourStage = 0;
                Snd_PlaySe(1, 2);
                return;
            }
            if (sZw->pkRow == CP_ONLY && sZw->pkOnly > 0 && nc > 2) { /* one part's own colours: a new line, a new look */
                PortWarrior *w = &sZw->w;
                s32 groups = 0, i;

                for (i = 0; i < w->colours; i++) {
                    if (w->colourPart[i] != 0 && w->colourPart[i] != choice[sZw->pkOnly]) {
                        groups++;
                    }
                }
                if (groups >= 4 || w->colours >= ZW_COLOURS) {
                    Zw_Note("Four parts with colours of their own is the most!");
                } else {
                    i = w->colours++;
                    w->colourKey[i] = sZw->reg[k * 4];
                    w->colourPart[i] = choice[sZw->pkOnly];
                    w->colourRgb[i] = Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV);
                    Zw_Changed();
                    Co_Load();
                    sZw->pkOnly = 0;
                    Zw_Note("That part has colours of its own now!");
                }
                sZw->colourStage = 0;
                Snd_PlaySe(1, 1);
                return;
            }
            Co_Apply();
            Co_Remember(Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV));
            sZw->colourStage = sZw->picFrom ? 3 : 0;
            if (sZw->picFrom) {
                sZw->picFrom = 0;
                Co_PicLoad(sZw->picRow);
            }
            sZw->modeT = 12;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_SQUARE) { /* the original */
            Co_Clear(k);
            Co_ToHsv((u32)sZw->reg[k * 4 + 1], &sZw->pkH, &sZw->pkS, &sZw->pkV);
            Snd_PlaySe(1, 2);
        } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) { /* as it was before the picker */
            sZw->w = sZw->pkBack;
            sZw->paintKey = -1;
            sZw->paintDue = ZW_PAINT_REST;
            sZw->colourStage = sZw->picFrom ? 3 : 0;
            if (sZw->picFrom) {
                sZw->picFrom = 0;
                Co_PicLoad(sZw->picRow);
            }
            sZw->modeT = 12;
            Snd_PlaySe(1, 2);
        }
    }
}

/* ---------------------------------------------------------------------------------------------- the workshop
 * Every piece of the warrior's look (Port_WarriorPieces: the lumps of each part's meshes: a strand of hair, a lump of
 * hair on the skull, an ear, a cuff), grouped by part, each with "Whole part" first: shown or hidden, stretched along
 * x / y / z, sized, moved, turned. Every change is made straight on the model on the screen (Port_WarriorShapeLive);
 * the piece under the cursor blinks. Saving keeps the edits (warrior.txt edit=, plat_warrior_parts.inc). */
#define WS_ROWS 11
enum { WE_SHOWN, WE_SX, WE_SY, WE_SZ, WE_SIZE, WE_MX, WE_MY, WE_MZ, WE_TX, WE_TY, WE_TZ, WE_RESET, WE_ROWS };
static char *kWsSlot[29] = {"HAIR", "HEAD", "FACE", "CHEST", "WAIST", "UPPER ARMS", "FOREARMS", "HANDS", "HIPS & THIGHS", "SHINS", "FEET", "ACCESSORIES",
                            "OTHER", "HAIR - LAYER", "HEAD - LAYER", "FACE - LAYER", "CHEST - LAYER", "WAIST - LAYER", "UPPER ARMS - LAYER",
                            "FOREARMS - LAYER", "HANDS - LAYER", "HIPS - LAYER", "SHINS - LAYER", "FEET - LAYER", "ACCESSORIES - LAYER",
                            "EXTRA LAYER 1", "EXTRA LAYER 2", "EXTRA LAYER 3", "EXTRA LAYER 4"};

/* The list made from the pieces: per part a heading (-1 - group), its "whole part" (-100 - group), its pieces. A group
   is the part's slot (0..11), 12 the body's own, 13 + slot the layer over a part (its own entries: moved apart). */
static u32 sWsOpen; /* the workshop's parts whose pieces are shown (a bit per group; kept while the game runs) */

static void Ws_Load(void) {
    s32 n = Port_WarriorPieces(&sZw->w, sZw->piece, ZW_PIECES);
    s32 rows = 0, slot, i, g;

    for (g = 0; g < 29; g++) {
        s32 any = 0;

        slot = g < 12 ? (g & 1 ? 13 + g / 2 : g / 2) : g < 24 ? (g & 1 ? 13 + 6 + (g - 12) / 2 : 6 + (g - 12) / 2) : g < 28 ? 25 + g - 24 : 12;
        /* (each layer after its part, then the extra layers, each its own, then the body's own) */

        for (i = 0; i < n; i++) {
            if ((sZw->piece[i * 4 + 2] < 0 ? 12 : sZw->piece[i * 4 + 2]) == slot) {
                any = 1;
                break;
            }
        }
        if (!any || rows + 2 >= ZW_PIECES + 30) {
            continue;
        }
        sZw->wsList[rows++] = -1 - slot;
        if (slot != 12) {
            sZw->wsList[rows++] = -100 - slot;
        }
        for (i = 0; i < n && rows < ZW_PIECES + 30 && (slot == 12 || ((sWsOpen >> slot) & 1)); i++) { /* (a folded part: its heading
                                                                                                    and "whole part" only) */
            if ((sZw->piece[i * 4 + 2] < 0 ? 12 : sZw->piece[i * 4 + 2]) == slot) {
                sZw->wsList[rows++] = i;
            }
        }
    }
    sZw->pieces = n;
    sZw->wsRows = rows;
    if (sZw->wsCur >= rows || sZw->wsCur < 0) {
        for (sZw->wsCur = 0; sZw->wsCur < rows && sZw->wsList[sZw->wsCur] > -100 && sZw->wsList[sZw->wsCur] < 0; sZw->wsCur++) {
        }
    }
    sZw->shapeKey = -1;
}

/* What the row under the cursor edits: its node (-1 - slot for a whole part) and piece (-1 all). */
static void Ws_Target(s32 row, s32 *node, s32 *comp) {
    s32 r = sZw->wsList[row];

    if (r < 0 && r > -100) { /* (a heading: nothing) */
        *node = -1000;
        *comp = -1;
        return;
    }
    if (r <= -100) { /* (a layer's whole part: -1 - (12 + slot)) */
        *node = -100 - r >= 25 ? -1 - (-100 - r) : -100 - r > 12 ? -(-100 - r) : -1 - (-100 - r); /* (extra layer x: -1 - (25 + x)) */
        *comp = -1;
    } else {
        *node = sZw->piece[r * 4];
        *comp = sZw->piece[r * 4 + 1];
    }
}

/* The edit of (node, piece): its index in the warrior's edits, made when `make` (-1 none / no room). */
static s32 Ws_Edit(s32 node, s32 comp, s32 make) {
    PortWarrior *w = &sZw->w;
    s32 i;

    for (i = 0; i < w->edits; i++) {
        if (w->edit[i][0] == node && w->edit[i][1] == comp) {
            return i;
        }
    }
    if (!make) {
        return -1;
    }
    if (w->edits >= ZW_EDITS) {
        Zw_Note("Thirty-two edits is the most a warrior can hold!");
        return -1;
    }
    i = w->edits++;
    memset(w->edit[i], 0, sizeof(w->edit[i]));
    w->edit[i][0] = node;
    w->edit[i][1] = comp;
    w->edit[i][3] = w->edit[i][4] = w->edit[i][5] = 100;
    return i;
}

static s32 Ws_Value(s32 e, s32 row) { /* an edit's value for a slider row (no edit: as it is) */
    PortWarrior *w = &sZw->w;

    if (e < 0) {
        return row >= WE_SX && row <= WE_SIZE ? 100 : 0;
    }
    switch (row) {
    case WE_SHOWN: return !w->edit[e][2];
    case WE_SX: case WE_SY: case WE_SZ: return w->edit[e][3 + row - WE_SX];
    case WE_SIZE: return (w->edit[e][3] + w->edit[e][4] + w->edit[e][5]) / 3;
    case WE_MX: case WE_MY: case WE_MZ: return w->edit[e][6 + row - WE_MX];
    case WE_TX: case WE_TY: case WE_TZ: return w->edit[e][9 + row - WE_TX];
    }
    return 0;
}

/* A copy of piece (node, comp) (a copy's: of its source, where the copy is): a new piece of the look, edited as any. */
static void Ws_Duplicate(s32 node, s32 comp) {
    PortWarrior *w = &sZw->w;
    s32 k = 0, i, e, from = -1, cur = sZw->wsCur;

    if (node < 0 || comp < 0) {
        Zw_Note("Pick one piece to copy - a whole part can't be copied!");
        Snd_PlaySe(1, 3);
        return;
    }
    if (comp >= ZW_DUP_FIRST) { /* (a copy of a copy: of its source, placed as that copy is) */
        e = Ws_Edit(ZW_DUP, comp - ZW_DUP_FIRST, 0);
        from = Ws_Edit(node, comp, 0);
        if (e < 0) {
            return;
        }
        node = w->edit[e][3];
        comp = w->edit[e][4];
    }
    for (i = 0; i < w->edits; i++) { /* (a number no copy has) */
        k = w->edit[i][0] == ZW_DUP && w->edit[i][1] >= k ? w->edit[i][1] + 1 : k;
    }
    if (w->edits + (from >= 0) >= ZW_EDITS) {
        Zw_Note("Thirty-two edits is the most a warrior can hold!");
        Snd_PlaySe(1, 3);
        return;
    }
    e = Ws_Edit(ZW_DUP, k, 1);
    if (e < 0) {
        return;
    }
    w->edit[e][3] = node;
    w->edit[e][4] = comp;
    w->edit[e][5] = 0;
    if (from >= 0) { /* (where the copy it was made from is) */
        s32 to = Ws_Edit(node, ZW_DUP_FIRST + k, 1);

        if (to >= 0) {
            memcpy(&w->edit[to][2], &sZw->w.edit[from][2], sizeof(w->edit[0]) - 2 * sizeof(s32));
        }
    }
    sZw->dirty = 1;
    sZw->shapeDirty = 1;
    sZw->previewDue = ZW_PREVIEW_REST;
    if (sZw->wsList[sZw->wsCur] >= 0) { /* (its part's pieces shown) */
        s32 sl = sZw->piece[sZw->wsList[sZw->wsCur] * 4 + 2];

        sWsOpen |= sl >= 0 && sl < 32 && sl != 12 ? 1u << sl : 0;
    }
    Ws_Load();
    for (i = 0; i < sZw->wsRows; i++) { /* (the cursor on the copy) */
        s32 r = sZw->wsList[i];

        if (r >= 0 && sZw->piece[r * 4] == node && sZw->piece[r * 4 + 1] == ZW_DUP_FIRST + k) {
            cur = i;
        }
    }
    sZw->wsCur = cur;
    sZw->wsTop = cur - WS_ROWS / 2 < 0 ? 0 : cur - WS_ROWS / 2;
    Zw_Note("Copied! It sits right on the piece: move it where you want it.");
    Snd_PlaySe(1, 1);
}

/* Copy k gone, and its edits. */
static void Ws_DupRemove(s32 k, s32 node) {
    PortWarrior *w = &sZw->w;
    s32 e;

    while ((e = Ws_Edit(ZW_DUP, k, 0)) >= 0 || (e = Ws_Edit(node, ZW_DUP_FIRST + k, 0)) >= 0) {
        memmove(w->edit[e], w->edit[e + 1], sizeof(w->edit[0]) * (u32)(w->edits - e - 1));
        w->edits--;
    }
    sZw->dirty = 1;
    sZw->shapeDirty = 1;
    sZw->previewDue = ZW_PREVIEW_REST;
    Ws_Load();
    if (sZw->wsTop > sZw->wsCur) {
        sZw->wsTop = sZw->wsCur;
    }
}

static char *Ws_RowName(s32 row) {
    static char out[48];
    s32 r = sZw->wsList[row];

    if (r <= -100) {
        return "Whole part";
    }
    if (r < 0) {
        return kWsSlot[-1 - r];
    }
    sprintf(out, "Piece %d", (int)(sZw->piece[r * 4 + 1] + 1));
    if (sZw->piece[r * 4] >= 0x47) { /* (a chain: a strand, a cape, a halo) */
        sprintf(out, "Strand %d.%d", (int)(sZw->piece[r * 4] - 0x46), (int)(sZw->piece[r * 4 + 1] + 1));
    }
    if (sZw->piece[r * 4 + 1] >= ZW_DUP_FIRST) { /* (a copy: of which piece) */
        s32 e = Ws_Edit(ZW_DUP, sZw->piece[r * 4 + 1] - ZW_DUP_FIRST, 0);

        if (e >= 0 && sZw->w.edit[e][3] >= 0x47) {
            sprintf(out, "Copy of strand %d.%d", (int)(sZw->w.edit[e][3] - 0x46), (int)(sZw->w.edit[e][4] + 1));
        } else {
            sprintf(out, "Copy of piece %d", e >= 0 ? (int)(sZw->w.edit[e][4] + 1) : 0);
        }
    }
    return out;
}

/* Makes the edits on the model on the screen when they changed, another model is shown, or the blinking turns. */
static void Ws_ShapeTick(void) {
    void *m = Mm_ViewModel();
    s32 view = Mm_ViewKey();
    s32 blinkNode = -1000;
    s32 blinkComp = -1;
    s32 key;

    if (m == NULL || view < 0) {
        return;
    }
    if (sZw->mode == ZW_WORK && sZw->wsStage == 0 && sZw->wsRows > 0 && ((sZw->frames / 16) & 1)) { /* (the piece under the cursor blinks) */
        Ws_Target(sZw->wsCur, &blinkNode, &blinkComp);
    }
    if (sZw->mode == ZW_SHADEPK && ((sZw->frames / 16) & 1)) { /* (the shading screen: the part under the cursor blinks) */
        blinkNode = Sh_BlinkNode();
    }
    key = view ^ (blinkNode * 7919) ^ (blinkComp * 131);
    if (key == sZw->shapeKey && m == sZw->shapeMdl && !sZw->shapeDirty) {
        return;
    }
    if (sZw->w.edits > 0 || blinkNode != -1000 || sZw->shapeEdited) {
        Port_WarriorShapeLive(&sZw->w, m, blinkNode, blinkComp);
        sZw->shapeEdited = sZw->w.edits > 0 || blinkNode != -1000;
    }
    sZw->shapeKey = key;
    sZw->shapeMdl = m;
    sZw->shapeDirty = 0;
}

static void Ws_Draw(void) {
    s32 i;
    f32 slide = 1.0f - Zw_In(sZw->modeT, 12);
    f32 ox = -60.0f * slide;

    Zw_Host(1250.0f, 330.0f, 300.0f, 1); /* (behind the model) */
    Zw_EditStage(900.0f, 540.0f, 400.0f); /* (first: the list goes over the model) */
    Co_PaintTick();
    Ws_ShapeTick();
    Zw_Title("WORKSHOP");
    { /* (how many of the edits are used) */
        char budget[40];
        s32 i, n = 0;

        for (i = 0; i < sZw->w.edits; i++) {
            n += sZw->w.edit[i][0] != ZW_PATCH && sZw->w.edit[i][0] != ZW_SHADE;
        }
        sprintf(budget, "Edits %d / %d", (int)n, (int)(ZW_EDITS - (sZw->w.edits - n)));
        Mx_Text(1340.0f, 160.0f, budget, 17.0f, 0.0f, n >= ZW_EDITS - 3 ? MX_GOLD : MX_LAV, 2);
    }
    Mx_Box(ZW_WINDOW, 30.0f + ox, 122.0f, 620.0f + ox, 628.0f, 22.0f, (s32)(128.0f * (1.0f - slide)));
    if (sZw->wsStage == 0) {
        f32 want = CO_Y + (f32)(sZw->wsCur - sZw->wsTop) * CO_PITCH;

        sZw->zoneY += (want - sZw->zoneY) * 0.4f;
        for (i = 0; i < WS_ROWS && sZw->wsTop + i < sZw->wsRows; i++) {
            s32 row = sZw->wsTop + i;
            s32 r = sZw->wsList[row];
            f32 y = CO_Y + (f32)i * CO_PITCH;
            s32 sel = row == sZw->wsCur;
            s32 a = (s32)(128.0f * Zw_In(sZw->modeT - i, 10));
            s32 node, comp, e;
            char info[40];

            if (r < 0 && r > -100) { /* (a part's heading: "+" its pieces folded, "-" shown) */
                if (sel) {
                    Zw_SelBar(42.0f + ox, sZw->zoneY - 19.0f, 608.0f + ox, sZw->zoneY + 19.0f);
                }
                Mx_Bar(ZW_SECTION, 50.0f + ox, y - 14.0f, 300.0f + ox, y + 14.0f, a);
                Mx_TextA(66.0f + ox, y, kWsSlot[-1 - r], 18.0f, 200.0f, MX_DARK, 0, a);
                if (-1 - r != 12) {
                    Mx_TextA(285.0f + ox, y, ((sWsOpen >> (-1 - r)) & 1) ? "-" : "+", 22.0f, 20.0f, MX_DARK, 1, a);
                    if (!((sWsOpen >> (-1 - r)) & 1)) {
                        Mx_TextA(320.0f + ox, y, "pieces folded", 16.0f, 200.0f, sel ? MX_DARK : MX_LAV, 0, a);
                    }
                }
                continue;
            }
            if (sel) {
                Zw_SelBar(42.0f + ox, sZw->zoneY - 19.0f, 608.0f + ox, sZw->zoneY + 19.0f);
            }
            Ws_Target(row, &node, &comp);
            e = Ws_Edit(node, comp, 0);
            Mx_TextA(80.0f + ox, y, Ws_RowName(row), 20.0f, 260.0f, sel ? MX_DARK : MX_WHITE, 0, a);
            if (e >= 0 && sZw->w.edit[e][2] == 2) { /* (only this one of its layer kept) */
                Ps_Copy(info, "ONLY THIS", sizeof(info));
            } else if (e >= 0 && sZw->w.edit[e][2]) { /* (an outfit's piece hidden is taken off: what it covered shows again) */
                Ps_Copy(info, (r >= 0 && sZw->piece[r * 4 + 2] >= 13) || (r <= -100 && -100 - r >= 13) ? "taken off" : "hidden", sizeof(info));
            } else if (e >= 0) {
                sprintf(info, "edited");
            } else {
                info[0] = 0;
            }
            if (r >= 0) {
                char n2[24];

                sprintf(n2, "%d pts", (int)sZw->piece[r * 4 + 3]);
                Mx_TextA(380.0f + ox, y, n2, 16.0f, 120.0f, sel ? MX_DARK : MX_LAV, 0, a);
            }
            Mx_TextA(500.0f + ox, y, info, 16.0f, 100.0f, sel ? MX_DARK : MX_GOLD, 0, a);
        }
        if (sZw->wsTop > 0) {
            Mx_Fit(ZW_UP, 325.0f + ox, 134.0f + 3.0f * Zw_Beat(36), 24.0f, 0x80);
        }
        if (sZw->wsTop + WS_ROWS < sZw->wsRows) {
            Mx_Fit(ZW_DOWN, 325.0f + ox, 618.0f - 3.0f * Zw_Beat(36), 24.0f, 0x80);
        }
        if (sZw->wsList[sZw->wsCur] < 0 && sZw->wsList[sZw->wsCur] > -100) { /* (a heading) */
            Zw_Prompt(0, ((sWsOpen >> (-1 - sZw->wsList[sZw->wsCur])) & 1) ? "Fold pieces" : "Show pieces");
            Zw_Prompt(7, "Save");
            Zw_Prompt(2, "Done");
        } else {
        Zw_Prompt(0, "Edit");
        Zw_Prompt(4, "Show / hide");
        Zw_Prompt(9, "Duplicate");
        Zw_Prompt(1, sZw->wsList[sZw->wsCur] >= 0 && sZw->piece[sZw->wsList[sZw->wsCur] * 4 + 1] >= ZW_DUP_FIRST ? "Remove copy" : "Undo all of it");
        Zw_Prompt(8, Ws_Edit(ZW_PATCH, -1, 0) >= 0 ? "Patch holes (EXPERIMENTAL): ON" : "Patch holes (EXPERIMENTAL): OFF");
        Zw_Prompt(7, "Save");
        Zw_Prompt(2, "Done");
        }
    } else { /* the piece's sliders */
        static char *labels[WE_ROWS] = {"Shown", "Stretch X", "Stretch Y", "Stretch Z", "Size", "Move X", "Move Y", "Move Z", "Turn X", "Turn Y", "Turn Z",
                                        "Back to as it was"};
        s32 node, comp, e;
        f32 pop = Zw_In(sZw->modeT, 8);

        Ws_Target(sZw->wsCur, &node, &comp);
        e = Ws_Edit(node, comp, 0);
        Mx_TextA(325.0f + ox, 150.0f, Ws_RowName(sZw->wsCur), 22.0f, 540.0f, MX_GOLD, 1, (s32)(128.0f * pop));
        for (i = 0; i < WE_ROWS; i++) {
            f32 y = 192.0f + (f32)i * 37.0f;
            s32 lit = i == sZw->wsRow;
            s32 v = Ws_Value(e, i);
            char val[24];
            f32 k;

            if (lit) {
                Zw_SelBar(42.0f + ox, y - 16.0f, 608.0f + ox, y + 16.0f);
            }
            Mx_TextA(60.0f + ox, y, labels[i], 18.0f, 170.0f, lit ? MX_DARK : MX_LAV, 0, (s32)(128.0f * pop));
            if (i == WE_RESET) {
                continue;
            }
            if (i == WE_SHOWN) {
                Mx_TextA(250.0f + ox, y, v ? "On" : "Off", 18.0f, 100.0f, lit ? MX_DARK : MX_WHITE, 0, (s32)(128.0f * pop));
                continue;
            }
            k = i <= WE_SIZE ? (f32)(v - 20) / 280.0f : i <= WE_MZ ? (f32)(v + 300) / 600.0f : (f32)(v + 180) / 360.0f;
            k = k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k;
            Mx_Rect(250.0f + ox, y - 3.0f, 500.0f + ox, y + 3.0f, Mx_Rgba(0x30, 0x20, 0x50, 0x80), Mx_Rgba(0x30, 0x20, 0x50, 0x80));
            Mx_Rect(250.0f + ox, y - 3.0f, 250.0f + 250.0f * k + ox, y + 3.0f, Mx_Rgba(0xF0, 0xC0, 0x40, 0x80), Mx_Rgba(0xC0, 0x80, 0x20, 0x80));
            Zw_Swatch(0x40C8F0, 250.0f + 250.0f * k + ox, y, lit ? 22.0f : 16.0f, lit);
            sprintf(val, i <= WE_SIZE ? "%d%%" : i <= WE_MZ ? "%+d" : "%+d deg", (int)v);
            Mx_TextA(520.0f + ox, y, val, 16.0f, 90.0f, lit ? MX_DARK : MX_WHITE, 0, (s32)(128.0f * pop));
        }
        Zw_Prompt(3, "Change");
        Zw_Prompt(0, sZw->wsRow == WE_RESET ? "Reset" : sZw->wsRow == WE_SHOWN ? "Show / hide" : "OK");
        Zw_Prompt(2, "Back");
    }
    if (sZw->dirty) {
        Mx_Bar(ZW_PILL, 1150.0f, 110.0f, 1340.0f, 146.0f, 0x60 + (s32)(0x20 * Zw_Beat(60)));
        Mx_Text(1245.0f, 128.0f, "NOT SAVED", 17.0f, 170.0f, MX_DARK, 1);
    }
    if (sZw->saving) {
        Zw_Talk("Putting your warrior together... hold still!");
    } else if (sZw->wsStage) {
        Zw_Talk("Stretch it, move it, turn it! Your warrior changes as you go.");
    } else {
        Zw_Talk("Every piece of your warrior! The one under the cursor blinks.");
    }
}

static void Ws_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 node, comp, e;

    if (Zw_SaveTick() || !live || sZw->wsRows == 0) {
        if (live && (pad->gamePressed & PADG_TRIANGLE)) {
            Zw_SetMode(ZW_EDIT);
        }
        return;
    }
    Ws_Target(sZw->wsCur, &node, &comp);
    if (sZw->wsStage == 0) {
        s32 was = sZw->wsCur;
        s32 n = sZw->wsRows;

        if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            sZw->wsCur = sZw->wsCur > 0 ? sZw->wsCur - 1 : n - 1;
        } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            sZw->wsCur = sZw->wsCur < n - 1 ? sZw->wsCur + 1 : 0;
        }
        if (sZw->wsCur != was) {
            Snd_PlaySe(1, 0);
        }
        if (sZw->wsCur - 1 < sZw->wsTop) {
            sZw->wsTop = sZw->wsCur - 1 < 0 ? 0 : sZw->wsCur - 1;
        }
        if (sZw->wsCur >= sZw->wsTop + WS_ROWS) {
            sZw->wsTop = sZw->wsCur - WS_ROWS + 1;
        }
        if (sZw->wsList[sZw->wsCur] < 0 && sZw->wsList[sZw->wsCur] > -100) { /* a part's heading: its pieces shown or folded */
            if (pad->gamePressed & (PADG_CROSS | PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) {
                s32 slot = -1 - sZw->wsList[sZw->wsCur];

                if (slot != 12) {
                    sWsOpen ^= 1u << slot;
                    Ws_Load();
                    Snd_PlaySe(1, ((sWsOpen >> slot) & 1) ? 1 : 2);
                }
            } else if (pad->gamePressed & PADG_START) {
                sZw->saving = 2;
                Snd_PlaySe(1, 1);
            } else if (pad->gamePressed & PADG_TRIANGLE) {
                Zw_SetMode(ZW_EDIT);
                Snd_PlaySe(1, 2);
            }
            return;
        }
        if (pad->gamePressed & PADG_CROSS) {
            sZw->wsStage = 1;
            sZw->wsRow = WE_SX;
            sZw->modeT = 0;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_CIRCLE) { /* shown / hidden (a layer's piece: then only this one of its layer) */
            s32 r0 = sZw->wsList[sZw->wsCur];
            s32 layerPiece = r0 >= 0 && sZw->piece[r0 * 4 + 2] >= 13 && node >= 0;

            e = Ws_Edit(node, comp, 1);
            if (e >= 0) {
                sZw->w.edit[e][2] = layerPiece ? (sZw->w.edit[e][2] + 1) % 3 : !sZw->w.edit[e][2];
                if (layerPiece) {
                    Zw_Note(sZw->w.edit[e][2] == 2 ? "Only this piece of the layer is kept! Mark more with Circle to keep them too."
                            : sZw->w.edit[e][2] == 1 ? "Taken off." : "Back on.");
                }
                sZw->dirty = 1;
                sZw->shapeDirty = 1;
                sZw->previewDue = ZW_PREVIEW_REST; /* (made for real a moment later) */
            }
            Snd_PlaySe(1, 1);
        } else if ((pad->gamePressed & PADG_SQUARE) && node >= 0 && comp >= ZW_DUP_FIRST) { /* a copy: gone, with its edits */
            Ws_DupRemove(comp - ZW_DUP_FIRST, node);
            Snd_PlaySe(1, 2);
        } else if (pad->gamePressed & PADG_SQUARE) { /* this one back to as it was */
            e = Ws_Edit(node, comp, 0);
            if (e >= 0) {
                memmove(sZw->w.edit[e], sZw->w.edit[e + 1], sizeof(sZw->w.edit[0]) * (u32)(sZw->w.edits - e - 1));
                sZw->w.edits--;
                sZw->dirty = 1;
                sZw->shapeDirty = 1;
                sZw->previewDue = ZW_PREVIEW_REST; /* (made for real a moment later) */
            }
            Snd_PlaySe(1, 2);
        } else if (pad->gamePressed & PADG_L1) { /* the piece copied */
            Ws_Duplicate(node, comp);
        } else if (pad->gamePressed & PADG_R1) { /* the holes patched or not (made when the preview is) */
            e = Ws_Edit(ZW_PATCH, -1, 0);
            if (e >= 0) {
                memmove(sZw->w.edit[e], sZw->w.edit[e + 1], sizeof(sZw->w.edit[0]) * (u32)(sZw->w.edits - e - 1));
                sZw->w.edits--;
                Zw_Note("Holes left open.");
            } else if (Ws_Edit(ZW_PATCH, -1, 1) >= 0) {
                Zw_Note("Holes patched (experimental: it can pull parts out of shape; the face is left alone). Check the model!");
            }
            sZw->dirty = 1;
            sZw->shapeDirty = 1;
            sZw->previewDue = ZW_PREVIEW_REST;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_START) {
            sZw->saving = 2;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_TRIANGLE) {
            Zw_SetMode(ZW_EDIT);
            Snd_PlaySe(1, 2);
        }
        return;
    }
    { /* the sliders: held, they glide */
        s32 hl = pad->gameHeld & (PADG_LEFT | PADG_LEFT2), hr = pad->gameHeld & (PADG_RIGHT | PADG_RIGHT2);
        s32 row = sZw->wsRow;

        if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            sZw->wsRow = (sZw->wsRow + WE_ROWS - 1) % WE_ROWS;
            Snd_PlaySe(1, 0);
        } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            sZw->wsRow = (sZw->wsRow + 1) % WE_ROWS;
            Snd_PlaySe(1, 0);
        }
        if ((hl || hr) && row >= WE_SX && row <= WE_TZ && (sZw->frames & 1) == 0) {
            s32 d = hl ? -1 : 1;

            e = Ws_Edit(node, comp, 1);
            if (e >= 0) {
                s32 *v = sZw->w.edit[e];

                if (row <= WE_SZ) {
                    v[3 + row - WE_SX] += d * 2;
                    v[3 + row - WE_SX] = v[3 + row - WE_SX] < 20 ? 20 : v[3 + row - WE_SX] > 300 ? 300 : v[3 + row - WE_SX];
                } else if (row == WE_SIZE) {
                    s32 j;

                    for (j = 3; j < 6; j++) {
                        v[j] += d * 2;
                        v[j] = v[j] < 20 ? 20 : v[j] > 300 ? 300 : v[j];
                    }
                } else if (row <= WE_MZ) {
                    v[6 + row - WE_MX] += d * 2;
                    v[6 + row - WE_MX] = v[6 + row - WE_MX] < -300 ? -300 : v[6 + row - WE_MX] > 300 ? 300 : v[6 + row - WE_MX];
                } else {
                    v[9 + row - WE_TX] += d * 2;
                    v[9 + row - WE_TX] = v[9 + row - WE_TX] < -180 ? -180 : v[9 + row - WE_TX] > 180 ? 180 : v[9 + row - WE_TX];
                }
                sZw->dirty = 1;
                sZw->shapeDirty = 1;
                sZw->previewDue = ZW_PREVIEW_REST; /* (made for real a moment later) */
            }
        }
        if (pad->gamePressed & PADG_CROSS) {
            if (row == WE_SHOWN) {
                e = Ws_Edit(node, comp, 1);
                if (e >= 0) {
                    sZw->w.edit[e][2] = !sZw->w.edit[e][2];
                }
            } else if (row == WE_RESET) {
                e = Ws_Edit(node, comp, 0);
                if (e >= 0) {
                    memmove(sZw->w.edit[e], sZw->w.edit[e + 1], sizeof(sZw->w.edit[0]) * (u32)(sZw->w.edits - e - 1));
                    sZw->w.edits--;
                }
            } else {
                sZw->wsStage = 0;
                sZw->modeT = 12;
            }
            sZw->dirty = 1;
            sZw->shapeDirty = 1;
            sZw->previewDue = ZW_PREVIEW_REST; /* (made for real a moment later) */
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
            sZw->wsStage = 0;
            sZw->modeT = 12;
            Snd_PlaySe(1, 2);
        }
    }
}

/* ---------------------------------------------------------------------------------------------- the small portrait
 * A square on the warrior's render: the face of the grids (the Mod Manager's chip: x, y and size in the render's
 * pixels, 256 x 512). */
static void Zw_ArtLoad(s32 mod) {
    MmRes *res;

    sZw->artTex = NULL;
    if (sZw->artFile == NULL) {
        sZw->artFile = Heap_Alloc(ZW_ART_FILE, 0x40, 0, 2);
        sZw->artRes = Heap_Alloc(ZW_ART_RES, 0x40, 0, 2);
    }
    if (Port_ModPortrait(mod, sZw->artFile, ZW_ART_FILE) < 0x100 || *(u32 *)sZw->artFile > ZW_ART_RES) {
        return;
    }
    Sprite_Unpack(sZw->artFile, sZw->artRes, NULL);
    res = (MmRes *)sZw->artRes;
    Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
    sZw->artTex = res->tex;
}

static void Cr_Draw(void) {
    s32 *c = sZw->crop;
    f32 k = 0.92f;
    f32 kx = Mx_SameX(k);
    f32 px = 420.0f - 128.0f * kx;
    f32 py = 150.0f;
    f32 x0 = px + (f32)c[0] * kx;
    f32 y0 = py + (f32)c[1] * k;
    f32 b = 2.0f * Zw_Beat(40);

    Zw_Title("SMALL PORTRAIT");
    Mx_Box(ZW_WINDOW, px - 40.0f, py - 26.0f, px + 256.0f * kx + 40.0f, 630.0f, 22.0f, 0x80);
    if (sZw->artTex != NULL) {
        f32 r = Mx_SameX(70.0f);
        s32 i;

        Mx_Tex(sZw->artTex, px, py, px + 256.0f * kx, py + 471.0f, 0, 0, 256, 512, 0x48, 0x80);
        Mx_Tex(sZw->artTex, x0, y0, x0 + (f32)c[2] * kx, y0 + (f32)c[2] * k, c[0], c[1], c[0] + c[2], c[1] + c[2], 0x80, 0x80);
        Mx_Brackets(x0 - 6.0f - b, y0 - 6.0f - b, x0 + (f32)c[2] * kx + 6.0f + b, y0 + (f32)c[2] * k + 6.0f + b, 16.0f, 4.0f, Mx_Rgba(0xFF, 0xD0, 0x50, 0x80));
        /* what it gives: the round face of the lists */
        Mx_Box(ZW_WINDOW, 820.0f, 200.0f, 1120.0f, 500.0f, 20.0f, 0x80);
        Mx_Text(970.0f, 236.0f, "ON THE GRIDS", 22.0f, 260.0f, MX_GOLD, 1);
        for (i = 0; i < 16; i++) { /* (cut round, as the lists show it) */
            f32 a = -1.0f + 2.0f * (f32)i / 16.0f;
            f32 m = a + 1.0f / 16.0f;
            f32 half = m * m < 1.0f ? (f32)__builtin_sqrtf(1.0f - m * m) : 0.0f;
            f32 uc = (f32)c[0] + (f32)c[2] * 0.5f;
            f32 uh = (f32)c[2] * 0.5f * half;

            Mx_Tex(sZw->artTex, 970.0f - r * half, 370.0f + a * 70.0f, 970.0f + r * half, 370.0f + (a + 0.125f) * 70.0f, (s32)(uc - uh),
                   c[1] + c[2] * i / 16, (s32)(uc + uh), c[1] + c[2] * (i + 1) / 16, 0x80, 0x80);
        }
        Mx_Fit(ZW_RING, 970.0f, 370.0f, 176.0f, 0x80);
    } else {
        Mx_Text(420.0f, 390.0f, "NO PORTRAIT YET", 26.0f, 0.0f, MX_WHITE, 1);
    }
    Zw_Host(1240.0f, 420.0f, 320.0f, 0);
    Zw_Prompt(0, "Keep");
    Zw_Prompt(15, "Move");
    Zw_Prompt(10, "Size");
    Zw_Prompt(1, "Automatic");
    Zw_Prompt(2, "Cancel");
    Zw_Talk("Frame that face! A little to the left... perfect!");
}

static void Cr_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 *c = sZw->crop;

    if (!live) {
        return;
    }
    if (pad->gameHeld & (PADG_RIGHT | PADG_RIGHT2)) {
        c[0] += 2;
    }
    if (pad->gameHeld & (PADG_LEFT | PADG_LEFT2)) {
        c[0] -= 2;
    }
    if (pad->gameHeld & (PADG_DOWN | PADG_DOWN2)) {
        c[1] += 2;
    }
    if (pad->gameHeld & (PADG_UP | PADG_UP2)) {
        c[1] -= 2;
    }
    if (pad->gameHeld & PADG_R2) {
        c[2] += 2;
    }
    if (pad->gameHeld & PADG_L2) {
        c[2] -= 2;
    }
    c[2] = c[2] < 32 ? 32 : c[2] > 256 ? 256 : c[2];
    c[0] = c[0] < 0 ? 0 : c[0] + c[2] > 256 ? 256 - c[2] : c[0];
    c[1] = c[1] < 0 ? 0 : c[1] + c[2] > 512 ? 512 - c[2] : c[1];
    if (pad->gamePressed & PADG_CROSS) {
        Port_ModSet(sZw->w.mod, 3, c[0]);
        Port_ModSet(sZw->w.mod, 4, c[1]);
        Port_ModSet(sZw->w.mod, 5, c[2]);
        Mx_FreeFaces();
        Zw_Note("A face fit for the Other World Tournament!");
        Zw_SetMode(ZW_EDIT);
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_SQUARE) {
        Port_ModSet(sZw->w.mod, 3, 0);
        Port_ModSet(sZw->w.mod, 4, 0);
        Port_ModSet(sZw->w.mod, 5, 0);
        Mx_FreeFaces();
        Zw_SetMode(ZW_EDIT);
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Zw_SetMode(ZW_EDIT);
        Snd_PlaySe(1, 2);
    }
}

/* ---------------------------------------------------------------------------------------------- the HUD face photo
 * The saved warrior's face close up behind a 2:1 frame: what is inside becomes the gauge faces (64 x 32; the second
 * side mirrored), photographed off the screen without anything drawn over it (plat_mods.c Port_WarriorHudPhoto). */
#define PH_X0 393.0f
#define PH_X1 983.0f
#define PH_Y0 175.0f
#define PH_Y1 470.0f

/* The frame of each photo: the HUD face 2:1, the render 1:2 (256 x 512), the small portrait square. */
static void Ph_Frame(f32 *x0, f32 *y0, f32 *x1, f32 *y1) {
    if (sZw->phKind == 1) { /* (the places are not square on the screen: the width from the height, Mx_SameX) */
        f32 w = Mx_SameX(540.0f) * 0.5f;

        *x0 = 688.0f - w * 0.5f, *y0 = 106.0f, *x1 = 688.0f + w * 0.5f, *y1 = 646.0f;
    } else if (sZw->phKind == 2) {
        f32 w = Mx_SameX(340.0f);

        *x0 = 688.0f - w * 0.5f, *y0 = 150.0f, *x1 = 688.0f + w * 0.5f, *y1 = 490.0f;
    } else { /* the gauge's face: 64 x 40 */
        f32 w = Mx_SameX(PH_Y1 - PH_Y0) * 1.6f;

        *x0 = 688.0f - w * 0.5f, *y0 = PH_Y0, *x1 = 688.0f + w * 0.5f, *y1 = PH_Y1;
    }
}

static void Ph_Reset(void) {
    sZw->phTilt = 0.0f;
    sZw->phPan = 0.0f;
    if (sZw->phKind == 1) { /* as the game's renders: the warrior filling the frame, hair to knees, turned a little */
        sZw->phYaw = 3.59f - 0.35f;
        sZw->phZoom = 0.235f;
        sZw->phFocus = 0.25f;
    } else if (sZw->phKind == 2) {
        sZw->phYaw = 3.59f - 0.3f;
        sZw->phZoom = 1.15f;
        sZw->phFocus = 0.0f;
    } else {
        sZw->phYaw = 3.59f - 0.55f; /* (looking a little to the right, as the first side's face does) */
        sZw->phZoom = 1.35f;
        sZw->phFocus = 0.0f;
    }
}

static void Ph_Open(s32 kind) {
    if (sZw->w.mod < 0 || sZw->dirty) {
        Zw_Note("Save the warrior first, then take its photo.");
        return;
    }
    sZw->phKind = kind;
    Ph_Reset();
    sZw->phPose = 0;
    sZw->phHold = 0;
    sZw->phShot = 0;
    sZw->phWhite = 0;
    Zw_SetMode(ZW_PHOTO);
}

/* The render's poses: motions of the warrior's animation file (as BT3's animation lists name them). */
#define PH_POSES 16
#define PH_MOTION_MAX 0x40000
static const s32 kPhPoseAnim[PH_POSES] = {-1, -2, 53, 54, 262, 264, 296, 298, 363, 330, 332, 385, 387, 389, 376, 382}; /* (-2: the T-pose) */
static char *kPhPoseName[PH_POSES] = {"Stance", "T-pose (a clear look at the face)", "Charging", "Full power", "Blast 2 (1): charge", "Blast 2 (1): fire",
                                      "Blast 2 (2): charge", "Blast 2 (2): fire", "Ultimate: opening pose", "Ultimate: charge",
                                      "Ultimate: fire", "Intro", "Victory", "Taunt", "Transforming", "Sensing ki"};

/* The render's and the small portrait's photo: the frame's corners for the platform, then its steps. */
static s32 Ph_PortraitStep(s32 step) {
    f32 x0, y0, x1, y1;

    Ph_Frame(&x0, &y0, &x1, &y1);
    return Port_WarriorPortraitPhoto(sZw->w.mod, step | sZw->phKind << 8,
                                     (s32)(Mx_ScreenFracX(x0) * 10000.0f) | ((s32)(Mx_ScreenFracY(y0) * 10000.0f) << 16),
                                     (s32)(Mx_ScreenFracX(x1) * 10000.0f) | ((s32)(Mx_ScreenFracY(y1) * 10000.0f) << 16));
}

static void Ph_Done(s32 r) {
    sZw->phShot = 0;
    sZw->phWhite = 0;
    if (r > 0) {
        Zw_Note(sZw->phKind == 1 ? "What a pose! That's its portrait in the character select now."
                                 : "Snap! That little face is on the grids now.");
        sZw->viewGen++;
    } else {
        Zw_Note("Hmm, the photo did not come out. Try again?");
    }
    Snd_PlaySe(1, r > 0 ? 1 : 2);
}

static void Ph_Update(s32 live) {
    Pad *pad = &gPad[0];
    f32 x = pad->right[0];                 /* turn: the right stick across */
    f32 y = pad->left[1];                  /* move up / down: the left stick, the d-pad */
    f32 p = pad->left[0];                  /* move left / right: the left stick, the d-pad */
    f32 t = pad->right[1];                 /* tilt: the right stick up / down */

    if (sZw->phShot > 0 && sZw->phKind != 0) { /* the render (on black, then on white) and the small portrait */
        s32 r;

        sZw->phShot++;
        if (sZw->phShot == 5) {
            Ph_PortraitStep(1);
            sZw->phStep = 2;
        } else if (sZw->phShot > 5 && sZw->phStep == 2) {
            r = Ph_PortraitStep(2);
            if (r == 1 && sZw->phKind == 1) {
                sZw->phStep = 3;
                sZw->phWhite = 1;
                sZw->phAt = sZw->phShot;
            } else if (r != 0 || sZw->phShot > 120) {
                Ph_Done(r);
            }
        } else if (sZw->phStep == 3 && sZw->phShot >= sZw->phAt + 4) {
            Ph_PortraitStep(3);
            sZw->phStep = 4;
        } else if (sZw->phStep == 4) {
            r = Ph_PortraitStep(4);
            if (r != 0 || sZw->phShot > 260) {
                Ph_Done(r);
            }
        }
        return;
    }
    if (sZw->phShot > 0) { /* a few clean frames, the photo of one, then its faces made */
        s32 r;

        sZw->phShot++;
        if (sZw->phShot == 5) {
            Port_WarriorHudPhoto(sZw->w.mod, 1, 0, 0);
        } else if (sZw->phShot > 5) {
            f32 hx0, hy0, hx1, hy1;

            Ph_Frame(&hx0, &hy0, &hx1, &hy1);
            r = Port_WarriorHudPhoto(sZw->w.mod, 0, (s32)(Mx_ScreenFracX(hx0) * 10000.0f) | ((s32)(Mx_ScreenFracY(hy0) * 10000.0f) << 16),
                                     (s32)(Mx_ScreenFracX(hx1) * 10000.0f) | ((s32)(Mx_ScreenFracY(hy1) * 10000.0f) << 16));
            if (r != 0 || sZw->phShot > 120) {
                sZw->phShot = 0;
                Zw_Note(r > 0 ? "Say cheese! That face is on its health bar now." : "Hmm, the photo did not come out. Try again?");
                Snd_PlaySe(1, r > 0 ? 1 : 2);
            }
        }
        return;
    }
    if (!live) {
        return;
    }
    if (pad->gameHeld & PADG_RSTICK_LEFT) {
        x = -1.0f;
    } else if (pad->gameHeld & PADG_RSTICK_RIGHT) {
        x = 1.0f;
    }
    if (pad->gameHeld & (PADG_LEFT | PADG_LEFT2)) {
        p = -1.0f;
    } else if (pad->gameHeld & (PADG_RIGHT | PADG_RIGHT2)) {
        p = 1.0f;
    }
    if (p * p > 0.04f) { /* the model across the frame, a frame's width in about two seconds */
        sZw->phPan += p * 4.0f;
        sZw->phPan = sZw->phPan < -500.0f ? -500.0f : sZw->phPan > 500.0f ? 500.0f : sZw->phPan;
    }
    if (pad->gameHeld & (PADG_UP | PADG_UP2)) {
        y = -1.0f;
    } else if (pad->gameHeld & (PADG_DOWN | PADG_DOWN2)) {
        y = 1.0f;
    }
    if (pad->gameHeld & PADG_RSTICK_UP) {
        t = -1.0f;
    } else if (pad->gameHeld & PADG_RSTICK_DOWN) {
        t = 1.0f;
    }
    x = x < -1.0f ? -1.0f : x > 1.0f ? 1.0f : x;
    if (x * x > 0.04f) {
        sZw->phYaw += x * 0.03f;
    }
    if (y * y > 0.04f) { /* down: the camera goes down the body (the face comes up the frame), a frame's height a second */
        sZw->phFocus += y * 0.004f / sZw->phZoom;
        sZw->phFocus = sZw->phFocus < -0.6f ? -0.6f : sZw->phFocus > 0.6f ? 0.6f : sZw->phFocus;
    }
    if (t * t > 0.04f) {
        sZw->phTilt += t * 0.01f;
        sZw->phTilt = sZw->phTilt < -0.6f ? -0.6f : sZw->phTilt > 0.6f ? 0.6f : sZw->phTilt;
    }
    if (pad->gamePressed & PADG_R3) { /* (as it opened) */
        Ph_Reset();
        Snd_PlaySe(1, 2);
    }
    if ((pad->gamePressed & (PADG_L1 | PADG_R1)) && Mm_ViewObjPtr() != NULL) { /* another pose: the model
                                                                                                     file's own motions */
        extern void BtlObjAnim_PlayModel(void *obj, s32 anim, s32 mode);
        extern void PortObjAnim_PlayData(void *obj, void *data, s32 mode);
        extern s32 Port_WarriorMotion(s32 mod, s32 anim, void *out, s32 max);
        s32 step = (pad->gamePressed & PADG_R1) ? 1 : PH_POSES - 1, k, got = 0;

        if (sZw->phMotion == NULL) {
            sZw->phMotion = Heap_Alloc(PH_MOTION_MAX, 0x40, 0, 2);
        }
        for (k = 0; k < PH_POSES && sZw->phMotion != NULL; k++) { /* (one its file lacks is passed over) */
            sZw->phPose = (sZw->phPose + step) % PH_POSES;
            if (sZw->phPose == 0 || kPhPoseAnim[sZw->phPose] == -2) {
                got = 1;
                break;
            }
            got = Port_WarriorMotion(sZw->w.mod, kPhPoseAnim[sZw->phPose], sZw->phMotion, PH_MOTION_MAX);
            if (got > 0) {
                break;
            }
        }
        if (sZw->phPose == 0 || got <= 0 || kPhPoseAnim[sZw->phPose] == -2) {
            sZw->phPose = got <= 0 ? 0 : sZw->phPose;
            BtlObjAnim_PlayModel(Mm_ViewObjPtr(), 7, 2); /* (the stance: the menus' own; the T-pose is the viewer's) */
        } else {
            PortObjAnim_PlayData(Mm_ViewObjPtr(), sZw->phMotion, 2);
        }
        sZw->phHold = 0;
        Snd_PlaySe(1, 0);
    }
    if (sZw->phKind != 0 && (pad->gamePressed & PADG_L3)) { /* hold this moment (or let it move again) */
        sZw->phHold ^= 1;
        Snd_PlaySe(1, sZw->phHold ? 1 : 2);
    }
    if (pad->gameHeld & PADG_R2) {
        sZw->phZoom *= 1.02f;
    }
    if (pad->gameHeld & PADG_L2) {
        sZw->phZoom /= 1.02f;
    }
    sZw->phZoom = sZw->phZoom < 0.08f ? 0.08f : sZw->phZoom > 3.5f ? 3.5f : sZw->phZoom;
    if (pad->gamePressed & PADG_CROSS) {
        Snd_PlaySe(1, 1);
        sZw->phShot = 1;
        sZw->phStep = 0;
    } else if ((pad->gamePressed & PADG_SQUARE) && sZw->phKind == 1) { /* a picture file instead */
        Zw_SetMode(ZW_EDIT);
        Snd_PlaySe(1, 1);
        Port_PickPicture();
    } else if ((pad->gamePressed & PADG_SQUARE) && sZw->phKind == 2) {
        Port_WarriorPortraitPhoto(sZw->w.mod, 9 | 2 << 8, 0, 0);
        sZw->viewGen++;
        Zw_Note("Photo gone: the little face is cropped from the render again.");
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & PADG_SQUARE) {
        Port_WarriorHudPhoto(sZw->w.mod, -1, 0, 0);
        Zw_Note("Photo gone: the health bar gets the HUD face fighter's again.");
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Zw_SetMode(ZW_EDIT);
        Snd_PlaySe(1, 2);
    }
}

static void Ph_Draw(void) {
    PortModInfo info;
    f32 fx0, fy0, fx1, fy1, cx, cy;
    f32 height = 3000.0f * sZw->phZoom; /* the whole body's height: the face fills the frame */
    f32 eyes = 0.875f - sZw->phFocus;   /* how high on the body the frame's middle is (0.875: about its eyes) */
    u32 dim = Mx_Rgba(0, 0, 0, 0x50);
    static char *heads[3] = {"HUD FACE PHOTO", "RENDER PHOTO", "SMALL PORTRAIT PHOTO"};
    static char *lines[3] = {"Eyes in the middle, looking to the right: the other side gets it mirrored.",
                             "Fill the frame like the game's renders: hair at the top, knees at the bottom. The background is cut away.",
                             "The face, nice and big: it goes on the fighter grids."};

    Ph_Frame(&fx0, &fy0, &fx1, &fy1);
    cx = (fx0 + fx1) * 0.5f;
    cy = (fy0 + fy1) * 0.5f;
    if (sZw->phShot > 0 && sZw->phKind == 1) { /* (the cut-out's backgrounds: black, then white) */
        u32 c = sZw->phWhite ? Mx_Rgba(255, 255, 255, 0x80) : Mx_Rgba(0, 0, 0, 0x80);

        Mx_Rect(0.0f, 0.0f, MX_W, MX_H, c, c);
    } else if (sZw->phShot > 0 && sZw->phKind == 2) { /* (the plate's dark blue, as on the grids) */
        u32 c = Mx_Rgba(0x18, 0x2C, 0x90, 0x80);

        Mx_Rect(0.0f, 0.0f, MX_W, MX_H, c, c);
    } else {
        Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(22, 22, 30, 0x80), Mx_Rgba(10, 10, 14, 0x80));
    }
    Port_ModGetInfo(sZw->w.mod, &info);
    Mm_ViewWant(0x20000 + sZw->w.mod * 64 + (sZw->viewGen & 63), info.chara, 0, sZw->w.mod, 1);
    Mm_ViewCamera(sZw->phYaw, sZw->phTilt, kPhPoseAnim[sZw->phPose] == -2);
    Mm_ViewDraw(cx + sZw->phPan, cy + height * eyes, height);
    if (sZw->phShot > 0) { /* (nothing over the frame while it is photographed: the progress below it, outside the photo) */
        f32 done = sZw->phKind == 1 ? (sZw->phStep >= 4 ? 0.75f : sZw->phStep >= 3 ? 0.5f : 0.25f) : 0.5f;
        f32 bx0 = 488.0f, bx1 = 888.0f, by = fy1 + 34.0f > 700.0f ? 700.0f : fy1 + 34.0f;
        u32 rim = Mx_Rgba(40, 40, 60, 0x80), fill = Mx_Rgba(250, 205, 90, 0x80);

        done += (f32)(sZw->phShot % 30) / 30.0f * 0.2f;
        done = done > 1.0f ? 1.0f : done;
        Mx_Rect(bx0 - 3.0f, by - 9.0f, bx1 + 3.0f, by + 9.0f, rim, rim);
        Mx_Rect(bx0, by - 6.0f, bx0 + (bx1 - bx0) * done, by + 6.0f, fill, fill);
        return;
    }
    Mx_Rect(0.0f, 0.0f, MX_W, fy0, dim, dim);
    Mx_Rect(0.0f, fy1, MX_W, MX_H, dim, dim);
    Mx_Rect(0.0f, fy0, fx0, fy1, dim, dim);
    Mx_Rect(fx1, fy0, MX_W, fy1, dim, dim);
    Mx_Frame(fx0, fy0, fx1, fy1, 3.0f, Mx_Rgba(250, 205, 90, 0x80));
    if (sZw->phKind == 1) { /* a faint guide: where the character select's name plates and reel go over the portrait */
        f32 h = fy1 - fy0;
        u32 g = Mx_Rgba(120, 140, 255, 0x18);

        Mx_Rect(fx0, fy0 + h * 0.65f, fx1, fy0 + h * 0.78f, g, g);
        Mx_Rect(fx0, fy0 + h * 0.82f, fx1, fy0 + h * 0.95f, g, g);
        Mx_TextA((fx0 + fx1) * 0.5f, fy0 + h * 0.715f, "name plates", 13.0f, fx1 - fx0, MX_LAV, 1, 0x40);
        Mx_TextA((fx0 + fx1) * 0.5f, fy0 + h * 0.885f, "fighter reel", 13.0f, fx1 - fx0, MX_LAV, 1, 0x40);
    }
    if (sZw->phKind == 0) {
        Mx_Heading(cx, 110.0f, heads[0], 34.0f, 700.0f, 1);
        Mx_Text(cx, 150.0f, lines[0], 17.0f, 900.0f, MX_LAV, 1);
    } else { /* (the tall frames: the words beside them) */
        Mx_Heading(1110.0f, 200.0f, heads[sZw->phKind], 30.0f, 440.0f, 1);
        Mx_Text(1110.0f, 240.0f, lines[sZw->phKind], 16.0f, 440.0f, MX_LAV, 1);
    }
    {
        f32 bx = sZw->phKind == 0 ? PH_X0 : 900.0f;
        f32 by = sZw->phKind == 0 ? PH_Y1 + 40.0f : 300.0f;
        static const s32 kinds[3] = {16, 10, 17};
        static char *labels[3] = {"Turn / tilt   D-pad: move", "Zoom", "Reset"};
        if (sZw->phKind == 0) { /* (the health bar's photo: the poses too, L1 / R1) */
            char line[64];

            Ps_Copy(line, kPhPoseName[sZw->phPose], 40);
            Mx_Text(cx, PH_Y1 + 80.0f, line, 18.0f, 500.0f, MX_GOLD, 1);
            Mx_Text(cx, PH_Y1 + 104.0f, "L1 / R1  another pose", 14.0f, 400.0f, MX_LAV, 1);
        }
        if (sZw->phKind != 0) { /* (the poses: L1 / R1, L3 holds) */
            char line[64];

            Ps_Copy(line, kPhPoseName[sZw->phPose], 40);
            Mx_Text(1110.0f, 470.0f, line, 22.0f, 400.0f, MX_GOLD, 1);
            Mx_Text(1110.0f, 500.0f, sZw->phHold ? "L1 / R1  another pose     L3  let it move" : "L1 / R1  another pose     L3  hold this moment",
                    15.0f, 440.0f, MX_LAV, 1);
        }
        s32 i;

        for (i = 0; i < 3; i++) {
            Mx_Button(kinds[i], bx, by, 20.0f);
            bx += Mx_ButtonW(kinds[i], 20.0f) + 5.0f;
            Mx_Text(bx, by, labels[i], 16.0f, 0.0f, MX_LAV, 0);
            bx += Mx_TextW(labels[i], 16.0f) + 16.0f;
        }
    }
    Zw_Prompt(0, "Take photo");
    Zw_Prompt(1, sZw->phKind == 1 ? "Use a picture file" : "Remove photo");
    Zw_Prompt(2, "Back");
    Zw_Talk(sZw->phKind == 1 ? "Strike a pose! Frame the whole warrior, then snap." : "Smile! Frame its face, then snap. Heh heh!");
}

/* ---------------------------------------------------------------------------------------------- the screen */
/* Left for the character select or a test fight (the portraits for real): the warrior, its tab and row, to come back to. */
static struct {
    s32 on;
    char folder[64];
    s32 tab, row;
} sZwResume;
s32 gPortZwSelPreview; /* the character select runs for Z Warriors' preview: the mod grid opens on this skin + 1 */
extern s32 gPortModGridStart;
extern s32 gPortRbSelOff;
extern s32 Port_SkinCount(void);
extern s32 Port_SkinMod(s32 skin);

static s32 Zw_SkinOf(s32 mod) {
    s32 k, n = Port_SkinCount();

    for (k = 0; k < n; k++) {
        if (Port_SkinMod(k) == mod) {
            return k;
        }
    }
    return -1;
}

/* Leaves for the real thing: 1 a test fight (the HUD face on the health bar), 2 the character select (the small
   portrait on the mod grid, the render once picked); back here after. */
static void Zw_LeaveFor(s32 to) {
    if (sZw->w.mod < 0 || sZw->dirty) {
        Zw_Note("Save the warrior first, then see it for real!");
        Snd_PlaySe(1, 2);
        return;
    }
    if (to == 2 && Zw_SkinOf(sZw->w.mod) < 0) {
        Zw_Note("Hmm, it is not in the mod grid. Is it switched on in the Mod Manager?");
        Snd_PlaySe(1, 2);
        return;
    }
    sZwResume.on = 1;
    Ps_Copy(sZwResume.folder, sZw->w.folder, sizeof(sZwResume.folder));
    sZwResume.tab = sZw->tab;
    sZwResume.row = sZw->row[sZw->tab];
    sZw->exitTo = to;
    sZw->leaving = 1;
    ColorFade_StartOut(0, 0, 0, 0x14);
    Snd_PlaySe(1, 1);
}

static void Zw_Init(void) {
    sZw = Heap_Alloc(sizeof(Zw), 0x20, 0, 2);
    memset(sZw, 0, sizeof(Zw));
    sZw->at = -1;
    sZw->previewMod = -1;
    sZw->previewDue = -1;
    sZw->paintDue = -1;
    sZw->paintKey = -1;
    sZw->shapeKey = -1;
    Mx_ChipsOpen();
    Mm_ViewOpen();
    Zw_Reload();
    sZw->cur = sZw->count > 0 ? 1 : 0;
    sZw->curY = LIB_Y + (f32)sZw->cur * LIB_PITCH;
    sZw->camYaw = 3.59f;
    sZw->camZoom = sZw->camShownZoom = 1.0f;
    if (sZwResume.on) { /* back from the character select or a test fight: on that warrior, that row */
        s32 k;

        sZwResume.on = 0;
        for (k = 0; k < sZw->count; k++) {
            if (strcmp(sZw->list[k].folder, sZwResume.folder) == 0) {
                sZw->cur = k + 1;
                sZw->curY = LIB_Y + (f32)sZw->cur * LIB_PITCH;
                Zw_Edit(k);
                sZw->tab = sZwResume.tab;
                sZw->row[sZw->tab] = sZwResume.row;
                break;
            }
        }
    }
}

static void Zw_Term(void) {
    Glow_Off();
    Port_TextMode(0);
    Mm_ViewClose();
    Mx_FreeFaces();
    Mx_ChipsClose();
    if (sZw->artFile != NULL) {
        Heap_Free(sZw->artFile);
        Heap_Free(sZw->artRes);
    }
    if (sZw->pic != NULL) {
        Heap_Free(sZw->pic);
    }
    if (sZw->hist != NULL) {
        Heap_Free(sZw->hist);
    }
    {
        s32 r;

        for (r = 0; r < 64; r++) {
            if (sZw->refChip[r] != NULL) {
                Heap_Free(sZw->refChip[r]);
                sZw->refChip[r] = NULL;
            }
            sZw->refChipTried[r] = 0;
        }
    }
    if (sZw->cos != NULL) {
        Heap_Free(sZw->cos);
    }
    Wk_FreeChips();
    if (sZw->wk != NULL) {
        Heap_Free(sZw->wk);
    }
    if (sZw->phMotion != NULL) {
        { extern void *gPortFreezeObj; gPortFreezeObj = NULL; }
        Heap_Free(sZw->phMotion);
    }
    Heap_Free(sZw);
    sZw = NULL;
}

/* ---------------------------------------------------------------------------------------------- undo, redo
 * The warrior after each change is kept (ZW_UNDO of them): a change counts once nothing has changed for a moment (a
 * slider held is one change). L3 steps back, R3 forward again (R3 with nothing to go forward to: the camera, as
 * before). */
static void Zw_HistPush(void) {
    if (sZw->histPos + 1 >= ZW_UNDO) { /* (full: the oldest goes) */
        memmove(&sZw->hist[0], &sZw->hist[1], sizeof(PortWarrior) * (ZW_UNDO - 1));
        sZw->histPos--;
    }
    sZw->histPos++;
    sZw->hist[sZw->histPos] = sZw->w;
    sZw->histN = sZw->histPos + 1; /* (what was undone is gone) */
}

static void Zw_HistGo(s32 to) {
    sZw->histPos = to;
    sZw->w = sZw->hist[to];
    sZw->hist[ZW_UNDO] = sZw->w;
    sZw->histQuiet = 0;
    sZw->dirty = 1;
    sZw->previewDue = ZW_PREVIEW_REST;
    sZw->paintDue = ZW_PAINT_REST;
    sZw->paintKey = -1;
    sZw->shapeDirty = 1;
    if (sZw->mode == ZW_COLOUR) {
        Co_Load();
    }
    Snd_PlaySe(1, 2);
}

static void Zw_History(void) {
    Pad *pad = &gPad[0];
    s32 on = sZw->hist != NULL && sZw->ask == ASK_NONE &&
             (sZw->mode == ZW_EDIT || (sZw->mode == ZW_WORK && sZw->wsStage == 0) || (sZw->mode == ZW_COLOUR && sZw->colourStage == 0));

    if (sZw->hist == NULL || sZw->mode == ZW_LIBRARY) {
        return;
    }
    if (memcmp(&sZw->w, &sZw->hist[ZW_UNDO], sizeof(PortWarrior)) != 0) { /* (changing still) */
        sZw->hist[ZW_UNDO] = sZw->w;
        sZw->histQuiet = 0;
    } else if (++sZw->histQuiet == 15 && memcmp(&sZw->w, &sZw->hist[sZw->histPos], sizeof(PortWarrior)) != 0) {
        Zw_HistPush();
    }
    if (!on) {
        return;
    }
    if (pad->gamePressed & PADG_L3) {
        if (memcmp(&sZw->w, &sZw->hist[sZw->histPos], sizeof(PortWarrior)) != 0) {
            Zw_HistPush(); /* (the change not counted yet: kept, to come back to) */
        }
        if (sZw->histPos > 0) {
            Zw_HistGo(sZw->histPos - 1);
            Zw_Note("Undone! Back a step.");
        } else {
            Zw_Note("Nothing more to undo!");
        }
    } else if ((pad->gamePressed & PADG_R3) && sZw->histPos < sZw->histN - 1) {
        Zw_HistGo(sZw->histPos + 1);
        Zw_Note("Redone!");
        pad->gamePressed &= ~PADG_R3; /* (not the camera) */
    }
}


/* ---------------------------------------------------------------------------------------------- the stance maker
 * Each of 16 bones turned about its own x, y and z (degrees), on top of the stance's own motions: the pose is the
 * stance with those turns, and the breathing and the sway of the stance stay (plat_mods.c pose_apply). The model is
 * made again a moment after a change. */
static char *kPoBone[ZW_POSE_BONES] = {"Head", "Neck", "Upper body", "Waist", "Left upper arm", "Left forearm", "Left hand", "Right upper arm",
                                       "Right forearm", "Right hand", "Left thigh", "Left shin", "Left foot", "Right thigh", "Right shin", "Right foot"};
static const s32 kPoMirror[ZW_POSE_BONES] = {-1, -1, -1, -1, 7, 8, 9, 4, 5, 6, 13, 14, 15, 10, 11, 12};
static s32 sPoBone, sPoAxis;


/* ---- the stance maker's simple way: dots on the model's joints, dragged with the mouse (or picked with L1 / R1 and
 * moved with the stick), as in a 3D program: the bone a dot hangs from turns about its own three axes so the joint goes
 * where the cursor is (damped least squares, each frame, from where the joints are on the screen now). While the
 * stance maker is open the model is posed live (bobj_a.c gPortLivePose: the change since it was made), made again
 * with the pose when it closes. */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharApi_GetNodeMtx(s32 objId, s32 node, Mtx44 *out);
extern void Vu0Screen_StoreMtx(Mtx44 *dst);
extern void *gPortLivePoseObj;
extern void *gPortFreezeObj;
extern Quat gPortLivePose[16];
extern s32 gPortLivePoseNode[16];
static const s32 kPoNode[ZW_POSE_BONES] = {47, 46, 17, 16, 19, 20, 21, 33, 34, 35, 9, 10, 11, 13, 14, 15}; /* (plat kPoseNode) */
enum { PD_HEAD, PD_BODY, PD_LELB, PD_LHAND, PD_RELB, PD_RHAND, PD_LKNEE, PD_LFOOT, PD_RKNEE, PD_RFOOT, PD_DOTS };
static const s32 kPdBone[PD_DOTS] = {0, 2, 4, 5, 7, 8, 10, 11, 13, 14};     /* the bone each dot turns */
static const s32 kPdAt[PD_DOTS] = {-1, 46, 20, 21, 34, 35, 10, 11, 14, 15}; /* the node it is on (-1: the head's top) */
static char *kPdName[PD_DOTS] = {"Head", "Upper body", "Left elbow", "Left hand", "Right elbow", "Right hand", "Left knee", "Left foot",
                                 "Right knee", "Right foot"};
static s32 sPoSimple = 1, sPoDrag = -1, sPoSel, sPoMouse, sPoBaked[ZW_POSE_BONES][3], sPoLive;
static f32 sPoQ[ZW_POSE_BONES][4];          /* (a dragged bone's turn as it really is: the degrees are only its rounding) */
static s32 sPoQDeg[ZW_POSE_BONES][3], sPoQOk[ZW_POSE_BONES]; /* (the degrees it was rounded to: changed elsewhere, it is read again) */
static f32 sPoDot[PD_DOTS][2];
static s32 sPoDotOk[PD_DOTS];

static void Po_QuatOf(const s32 *deg, f32 *q) { /* (x, then y, then z: as plat pose_apply) */
    s32 ax;

    q[0] = q[1] = q[2] = 0.0f;
    q[3] = 1.0f;
    for (ax = 0; ax < 3; ax++) {
        f32 h = (f32)deg[ax] * (3.14159265f / 360.0f), r[4] = {0.0f, 0.0f, 0.0f, 0.0f}, o[4];

        r[ax] = sinf(h);
        r[3] = cosf(h);
        o[0] = q[3] * r[0] + q[0] * r[3] + q[1] * r[2] - q[2] * r[1];
        o[1] = q[3] * r[1] - q[0] * r[2] + q[1] * r[3] + q[2] * r[0];
        o[2] = q[3] * r[2] + q[0] * r[1] - q[1] * r[0] + q[2] * r[3];
        o[3] = q[3] * r[3] - q[0] * r[0] - q[1] * r[1] - q[2] * r[2];
        q[0] = o[0], q[1] = o[1], q[2] = o[2], q[3] = o[3];
    }
}

static void Po_Mul(f32 *o, const f32 *a, const f32 *b) {
    f32 r[4];

    r[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    r[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    r[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    r[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    o[0] = r[0], o[1] = r[1], o[2] = r[2], o[3] = r[3];
}

static void Po_EulerOf(const f32 *q, s32 *deg) { /* (q = rx * ry * rz: its x, y, z turns back, degrees) */
    f32 x = q[0], y = q[1], z = q[2], w = q[3];
    f32 r00 = 1.0f - 2.0f * (y * y + z * z), r01 = 2.0f * (x * y - w * z), r02 = 2.0f * (x * z + w * y);
    f32 r12 = 2.0f * (y * z - w * x), r22 = 1.0f - 2.0f * (x * x + y * y), k = 180.0f / 3.14159265f, v[3];
    s32 i;

    r02 = r02 > 1.0f ? 1.0f : r02 < -1.0f ? -1.0f : r02;
    v[1] = asinf(r02) * k;
    v[0] = atan2f(-r12, r22) * k;
    v[2] = atan2f(-r01, r00) * k;
    for (i = 0; i < 3; i++) {
        deg[i] = (s32)(v[i] + (v[i] < 0.0f ? -0.5f : 0.5f));
    }
}

/* The live pose: each bone's change since the model was made (made pose^-1 * pose now). */
static void Po_LiveSet(void) {
    s32 b;

    gPortLivePoseObj = Mm_ViewObjPtr();
    for (b = 0; b < ZW_POSE_BONES; b++) {
        f32 now[4], was[4];

        if (sPoQOk[b] && memcmp(sPoQDeg[b], sZw->w.pose[b], sizeof(sPoQDeg[b])) == 0) {
            memcpy(now, sPoQ[b], sizeof(now));
        } else {
            sPoQOk[b] = 0;
            Po_QuatOf(sZw->w.pose[b], now);
        }
        Po_QuatOf(sPoBaked[b], was);
        was[0] = -was[0], was[1] = -was[1], was[2] = -was[2];
        Po_Mul((f32 *)&gPortLivePose[b], was, now);
        gPortLivePoseNode[b] = kPoNode[b];
    }
    sPoLive = 1;
}

/* A point of the world on the screen (mockup pixels); 0 when it is behind the camera. */
static s32 Po_Screen(const f32 *p, f32 *out) {
    Mtx44 m;
    f32 v[4];
    s32 i;

    Vu0Screen_StoreMtx(&m);
    for (i = 0; i < 4; i++) {
        v[i] = p[0] * m.m[0][i] + p[1] * m.m[1][i] + p[2] * m.m[2][i] + m.m[3][i];
    }
    if (v[3] <= 0.0001f) {
        return 0;
    }
    out[0] = (v[0] / v[3] - 1792.0f) / 512.0f / Mx_ScreenFracX(1.0f);
    out[1] = (v[1] / v[3] - 1824.0f) / 448.0f / Mx_ScreenFracY(1.0f);
    return 1;
}

/* Where dot d is in the world (its joint; the head's: past its top). */
static s32 Po_DotWorld(s32 id, s32 d, f32 *p) {
    Vec4 a, b;

    if (kPdAt[d] >= 0) {
        BtlCharApi_GetNodePos(id, kPdAt[d], &a);
        p[0] = a.x, p[1] = a.y, p[2] = a.z;
        return 1;
    }
    BtlCharApi_GetNodePos(id, 47, &a);
    BtlCharApi_GetNodePos(id, 46, &b);
    p[0] = a.x + (a.x - b.x) * 3.0f, p[1] = a.y + (a.y - b.y) * 3.0f, p[2] = a.z + (a.z - b.z) * 3.0f;
    return 1;
}

/* Dot d moved towards (tx, ty) on the screen: its bone turned a step (about its own axes, as the screen sees them). */
static void Po_Pull(s32 d, f32 tx, f32 ty) {
    s32 id = Mm_ViewObjId(), bone = kPdBone[d], k, i;
    Mtx44 bm;
    Vec4 o;
    f32 c[3], cs[2], j[2][3], e[2], a00, a01, a11, det, inv00, inv01, inv11, g[2], dq[4], q[4], step[3];

    if (id < 0 || !Po_DotWorld(id, d, c) || !Po_Screen(c, cs)) {
        return;
    }
    BtlCharApi_GetNodeMtx(id, kPoNode[bone], &bm);
    BtlCharApi_GetNodePos(id, kPoNode[bone], &o);
    e[0] = tx - cs[0];
    e[1] = ty - cs[1];
    if (e[0] * e[0] + e[1] * e[1] < 4.0f) {
        return; /* (close enough: no trembling) */
    }
    for (k = 0; k < 3; k++) { /* each axis: how the joint goes on the screen for a small turn about it */
        f32 ax[3], len, r[3], moved[3], ms[2], h = 0.01f;

        for (i = 0; i < 3; i++) {
            ax[i] = bm.m[k][i];
        }
        len = sqrtf(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
        len = len > 1e-6f ? len : 1.0f;
        for (i = 0; i < 3; i++) {
            ax[i] /= len;
            r[i] = c[i] - ((f32 *)&o)[i];
        }
        moved[0] = c[0] + h * (ax[1] * r[2] - ax[2] * r[1]);
        moved[1] = c[1] + h * (ax[2] * r[0] - ax[0] * r[2]);
        moved[2] = c[2] + h * (ax[0] * r[1] - ax[1] * r[0]);
        if (!Po_Screen(moved, ms)) {
            return;
        }
        j[0][k] = (ms[0] - cs[0]) / h;
        j[1][k] = (ms[1] - cs[1]) / h;
        if (Port_AxisKey() != 0 && Port_AxisKey() != k + 1) { /* (kept to one axis: the others do not move) */
            j[0][k] = j[1][k] = 0.0f;
        }
    }
    a00 = j[0][0] * j[0][0] + j[0][1] * j[0][1] + j[0][2] * j[0][2];
    a01 = j[0][0] * j[1][0] + j[0][1] * j[1][1] + j[0][2] * j[1][2];
    a11 = j[1][0] * j[1][0] + j[1][1] * j[1][1] + j[1][2] * j[1][2];
    {
        f32 lam = (a00 + a11) * 0.05f + 1e-3f;

        a00 += lam;
        a11 += lam;
    }
    det = a00 * a11 - a01 * a01;
    if (det < 1e-9f && det > -1e-9f) {
        return;
    }
    inv00 = a11 / det, inv01 = -a01 / det, inv11 = a00 / det;
    g[0] = inv00 * e[0] + inv01 * e[1];
    g[1] = inv01 * e[0] + inv11 * e[1];
    for (k = 0; k < 3; k++) { /* (half the way, a step of at most 3 degrees an axis a frame: smooth, no spinning) */
        step[k] = 0.6f * (j[0][k] * g[0] + j[1][k] * g[1]);
        step[k] = step[k] > 0.07f ? 0.07f : step[k] < -0.07f ? -0.07f : step[k];
    }
    {
        f32 ang = sqrtf(step[0] * step[0] + step[1] * step[1] + step[2] * step[2]);

        if (ang < 1e-5f) {
            return;
        }
        dq[3] = cosf(ang * 0.5f);
        for (k = 0; k < 3; k++) {
            dq[k] = sinf(ang * 0.5f) * step[k] / ang;
        }
    }
    if (sPoQOk[bone] && memcmp(sPoQDeg[bone], sZw->w.pose[bone], sizeof(sPoQDeg[bone])) == 0) {
        memcpy(q, sPoQ[bone], sizeof(q));
    } else {
        Po_QuatOf(sZw->w.pose[bone], q);
    }
    Po_Mul(q, q, dq); /* (the turn about the bone's own axes, after its pose) */
    {
        f32 n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

        for (k = 0; k < 4; k++) {
            q[k] /= n > 1e-6f ? n : 1.0f;
        }
    }
    memcpy(sPoQ[bone], q, sizeof(q));
    Po_EulerOf(q, sZw->w.pose[bone]);
    memcpy(sPoQDeg[bone], sZw->w.pose[bone], sizeof(sPoQDeg[bone]));
    sPoQOk[bone] = 1;
    sZw->dirty = 1;
}

static void Po_SimpleUpdate(Pad *pad) {
    s32 m[5], press, d;
    f32 mx, my;

    Port_MouseState(m);
    mx = (f32)m[0] / 10000.0f / Mx_ScreenFracX(1.0f);
    my = (f32)m[1] / 10000.0f / Mx_ScreenFracY(1.0f);
    press = m[2] & ~sPoMouse;
    sPoMouse = m[2];
    if (press & 1) { /* a dot under the cursor taken, or the button */
        f32 best = 26.0f * 26.0f;

        sPoDrag = -1;
        for (d = 0; d < PD_DOTS; d++) {
            f32 ddx = mx - sPoDot[d][0], ddy = my - sPoDot[d][1];

            if (sPoDotOk[d] && ddx * ddx + ddy * ddy < best) {
                best = ddx * ddx + ddy * ddy;
                sPoDrag = d;
            }
        }
        if (sPoDrag >= 0) {
            sPoSel = sPoDrag;
        } else if (my >= 128.0f && my < 158.0f && mx >= 44.0f && mx < 254.0f) {
            sPoSimple = 0;
            Snd_PlaySe(1, 1);
            return;
        }
    }
    if (!(m[2] & 1)) {
        sPoDrag = -1;
    }
    if (sPoDrag >= 0) {
        Po_Pull(sPoDrag, mx, my);
    }
    if (pad->gamePressed & (PADG_L1 | PADG_R1)) {
        sPoSel = (sPoSel + ((pad->gamePressed & PADG_R1) ? 1 : PD_DOTS - 1)) % PD_DOTS;
        Snd_PlaySe(1, 0);
    } else if (sPoDotOk[sPoSel]) { /* the stick, the D-pad: the picked dot pulled that way */
        f32 ax = pad->left[0], ay = pad->left[1], vx, vy;

        vx = (pad->gameHeld & PADG_LEFT) ? -1.0f : (pad->gameHeld & PADG_RIGHT) ? 1.0f : ax * ax > 0.04f ? ax : 0.0f;
        vy = (pad->gameHeld & PADG_UP) ? -1.0f : (pad->gameHeld & PADG_DOWN) ? 1.0f : ay * ay > 0.04f ? ay : 0.0f;
        if (vx != 0.0f || vy != 0.0f) {
            Po_Pull(sPoSel, sPoDot[sPoSel][0] + vx * 6.0f, sPoDot[sPoSel][1] + vy * 6.0f);
        }
    }
    if (pad->gamePressed & PADG_CIRCLE) { /* kept to an axis: x, y, z, none */
        Port_AxisKeySet((Port_AxisKey() + 1) % 4);
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & PADG_SQUARE) { /* the picked dot's bone as the stance has it */
        s32 b = kPdBone[sPoSel];

        sZw->w.pose[b][0] = sZw->w.pose[b][1] = sZw->w.pose[b][2] = 0;
        sZw->dirty = 1;
        Snd_PlaySe(1, 2);
    }
}

static void Po_SimpleDraw(s32 a) {
    s32 id = Mm_ViewObjId(), d;
    u32 dot = Mx_Rgba(0xF8, 0xD0, 0x40, 0x80), sel = Mx_Rgba(0xFF, 0xFF, 0xFF, 0x80), rim = Mx_Rgba(0x20, 0x10, 0x30, 0x80);

    Mx_Bar(ZW_PILL, 44.0f, 128.0f, 254.0f, 158.0f, a);
    Mx_TextA(149.0f, 143.0f, "Advanced (numbers)", 15.0f, 200.0f, MX_DARK, 1, a);
    Mx_TextA(52.0f, 190.0f, "Drag the dots on your warrior.", 16.0f, 520.0f, MX_WHITE, 0, a);
    Mx_TextA(52.0f, 222.0f, "Turn the camera (right stick) to pose from the side.", 14.0f, 520.0f, MX_LAV, 0, a);
    Mx_TextA(52.0f, 270.0f, kPdName[sPoSel], 20.0f, 520.0f, MX_GOLD, 0, a);
    {
        static char *axes[4] = {"Free (press X, Y or Z to keep to one axis)", "Kept to X", "Kept to Y", "Kept to Z"};
        s32 ak = Port_AxisKey() & 3;

        Mx_TextA(52.0f, 306.0f, axes[ak], 16.0f, 520.0f, ak == 1 ? MX_RED : ak == 2 ? MX_GREEN : ak == 3 ? MX_BLUE : MX_LAV, 0, a);
        if (ak != 0 && id >= 0) { /* (the axis through the joint the picked dot turns) */
            Mtx44 bm;
            Vec4 o;
            f32 p0[3], p1[3], s0[2], s1[2], len, ax[3];
            s32 i;
            u32 col = ak == 1 ? Mx_Rgba(0xFF, 0x50, 0x50, 0x80) : ak == 2 ? Mx_Rgba(0x60, 0xF0, 0x60, 0x80) : Mx_Rgba(0x60, 0x90, 0xFF, 0x80);

            BtlCharApi_GetNodeMtx(id, kPoNode[kPdBone[sPoSel]], &bm);
            BtlCharApi_GetNodePos(id, kPoNode[kPdBone[sPoSel]], &o);
            for (i = 0; i < 3; i++) {
                ax[i] = bm.m[ak - 1][i];
            }
            len = sqrtf(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
            len = len > 1e-6f ? len : 1.0f;
            {
                f32 dw[3], bl = 1.0f;

                if (Po_DotWorld(id, sPoSel, dw)) { /* (as long as the bone, about) */
                    bl = sqrtf((dw[0] - o.x) * (dw[0] - o.x) + (dw[1] - o.y) * (dw[1] - o.y) + (dw[2] - o.z) * (dw[2] - o.z)) * 0.8f;
                    bl = bl > 1e-4f ? bl : 1.0f;
                }
                for (i = 0; i < 3; i++) {
                    p0[i] = ((f32 *)&o)[i] - ax[i] / len * bl;
                    p1[i] = ((f32 *)&o)[i] + ax[i] / len * bl;
                }
            }
            if (Po_Screen(p0, s0) && Po_Screen(p1, s1)) {
                for (i = 0; i <= 24; i++) {
                    f32 t = (f32)i / 24.0f;

                    Mx_Disc(s0[0] + (s1[0] - s0[0]) * t, s0[1] + (s1[1] - s0[1]) * t, 7.0f, col, col);
                }
            }
        }
    }
    for (d = 0; d < PD_DOTS; d++) {
        f32 w[3];

        sPoDotOk[d] = id >= 0 && Po_DotWorld(id, d, w) && Po_Screen(w, sPoDot[d]);
        if (sPoDotOk[d]) {
            s32 lit = d == sPoSel || d == sPoDrag;

            Mx_Disc(sPoDot[d][0], sPoDot[d][1], lit ? 30.0f : 22.0f, rim, rim);
            Mx_Disc(sPoDot[d][0], sPoDot[d][1], lit ? 24.0f : 16.0f, lit ? sel : dot, lit ? sel : dot);
        }
    }
}

static void Po_Open(void) {
    sPoBone = sPoBone >= 0 && sPoBone < ZW_POSE_BONES ? sPoBone : 0;
    sPoAxis = 0;
    sZw->tpose = 0;
    memcpy(sPoBaked, sZw->w.pose, sizeof(sPoBaked)); /* (the model on the screen has this pose made in) */
    Zw_SetMode(ZW_POSE);
}

static void Po_Leave(void) { /* (the live pose off; the model made again with the pose, if it changed) */
    gPortLivePoseObj = NULL;
    sPoLive = 0;
    if (memcmp(sPoBaked, sZw->w.pose, sizeof(sPoBaked)) != 0) {
        Zw_Changed();
    }
    Zw_SetMode(ZW_EDIT);
}

static void Po_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 *v = sZw->w.pose[sPoBone];

    if (Zw_SaveTick()) {
        if (!sZw->saving) { /* (saved: made with the pose) */
            memcpy(sPoBaked, sZw->w.pose, sizeof(sPoBaked));
        }
        return;
    }
    Po_LiveSet();
    if (!live) {
        return;
    }
    if (pad->gamePressed & PADG_SELECT) { /* simple (dots on the model) / advanced (the numbers) */
        pad->gamePressed &= ~PADG_SELECT; /* (not the T-pose) */
        sPoSimple ^= 1;
        Snd_PlaySe(1, 1);
        return;
    }
    if (sPoSimple) {
        if (pad->gamePressed & PADG_START) {
            sZw->saving = 2;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_TRIANGLE) {
            Po_Leave();
            Snd_PlaySe(1, 2);
        } else {
            Po_SimpleUpdate(pad);
        }
        pad->gamePressed &= ~(PADG_L1 | PADG_R1 | PADG_SQUARE | PADG_CIRCLE);
        return;
    }
    if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sPoBone = (sPoBone + ZW_POSE_BONES - 1) % ZW_POSE_BONES;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sPoBone = (sPoBone + 1) % ZW_POSE_BONES;
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & (PADG_L1 | PADG_R1)) { /* which way it turns */
        sPoAxis = (sPoAxis + ((pad->gamePressed & PADG_R1) ? 1 : 2)) % 3;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) { /* turned, 5 degrees a step */
        s32 step = (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) ? 5 : -5, n = v[sPoAxis] + step;

        v[sPoAxis] = n > 180 ? n - 360 : n < -180 ? n + 360 : n;
        sZw->dirty = 1;
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_SQUARE) { /* this bone as the stance has it */
        v[0] = v[1] = v[2] = 0;
        sZw->dirty = 1;
        Snd_PlaySe(1, 2);
    } else if ((pad->gamePressed & PADG_CIRCLE) && kPoMirror[sPoBone] >= 0) { /* the other side the same, mirrored */
        s32 *o = sZw->w.pose[kPoMirror[sPoBone]];

        o[0] = v[0];
        o[1] = -v[1];
        o[2] = -v[2];
        sZw->dirty = 1;
        Zw_Note("The other side posed the same, mirrored.");
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_START) {
        sZw->saving = 2;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Po_Leave();
        Snd_PlaySe(1, 2);
    }
}

static void Po_Draw(void) {
    f32 pop = Zw_In(sZw->modeT, 8);
    s32 a = (s32)(128.0f * pop), b, ax;
    char line[16];

    Zw_EditStage(980.0f, 700.0f, 520.0f);
    Zw_Title("STANCE MAKER");
    Mx_Box(ZW_WINDOW, 30.0f, 118.0f, 600.0f, 690.0f, 20.0f, 0x80);
    if (sPoSimple) {
        Po_SimpleDraw(a);
        Zw_Prompt(15, "Pull the dot");
        Zw_Prompt(14, "Dot");
        Zw_Prompt(4, "Axis");
        Zw_Prompt(1, "Reset its bone");
        Zw_Prompt(13, "Advanced");
        Zw_Prompt(7, "Save");
        Zw_Prompt(2, "Back");
        return;
    }
    Mx_TextA(370.0f, 140.0f, "Turn X", 14.0f, 70.0f, sPoAxis == 0 ? MX_GOLD : MX_LAV, 1, a);
    Mx_TextA(455.0f, 140.0f, "Turn Y", 14.0f, 70.0f, sPoAxis == 1 ? MX_GOLD : MX_LAV, 1, a);
    Mx_TextA(540.0f, 140.0f, "Turn Z", 14.0f, 70.0f, sPoAxis == 2 ? MX_GOLD : MX_LAV, 1, a);
    for (b = 0; b < ZW_POSE_BONES; b++) {
        f32 y = 172.0f + (f32)b * 32.0f;
        s32 lit = b == sPoBone;

        if (lit) {
            Zw_SelBar(40.0f, y - 14.0f, 590.0f, y + 14.0f);
        }
        Mx_TextA(52.0f, y, kPoBone[b], 15.0f, 240.0f, lit ? MX_DARK : MX_WHITE, 0, a);
        for (ax = 0; ax < 3; ax++) {
            f32 cx = 370.0f + (f32)ax * 85.0f;
            s32 val = sZw->w.pose[b][ax];

            if (lit && ax == sPoAxis) {
                Mx_Frame(cx - 36.0f, y - 12.0f, cx + 36.0f, y + 12.0f, 2.0f, (sZw->frames / 10) & 1 ? Mx_Rgba(255, 255, 255, 0x80) : Mx_Rgba(40, 20, 60, 0x80));
            }
            sprintf(line, "%d", (int)val);
            Mx_TextA(cx, y, line, 15.0f, 70.0f, lit ? MX_DARK : val != 0 ? MX_GOLD : MX_DIM, 1, a);
        }
    }
    Zw_Prompt(15, "Bone");
    Zw_Prompt(3, "Turn");
    Zw_Prompt(14, "X / Y / Z");
    Zw_Prompt(1, "Reset bone");
    if (kPoMirror[sPoBone] >= 0) {
        Zw_Prompt(4, "Mirror to the other side");
    }
    Zw_Prompt(13, "Simple");
    Zw_Prompt(7, "Save");
    Zw_Prompt(2, "Back");
}

/* ---------------------------------------------------------------------------------------------- the textures
 * A screen of its own for drawing on the warrior's textures: the list of them on the left, the one chosen large in the
 * middle with its 16 colours and the tools under it, the model on the right (painted with every stroke). With a pad:
 * the cursor (the D-pad a pixel at a time, the stick gently), Cross paints, Circle fills, Square takes a colour, L1 / R1
 * the colours, Start the brush's size. With a mouse: the left button does what the tool chosen does (brush, fill, take
 * colour), the right one takes a colour, the wheel goes through the colours, everything on the screen is clicked.
 * The drawings are kept in paint/ of the warrior's folder (Port_PaintSave). */
#define TX_MAX 40
#define TX_CX0 330.0f /* the canvas */
#define TX_CY0 128.0f
#define TX_CX1 890.0f
#define TX_CY1 600.0f
enum { TXT_BRUSH, TXT_FILL, TXT_PICK, TXT_SIZE, TXT_UNDO, TXT_REDO, TXT_IMPORT, TXT_WIPE, TXT_DONE, TXT_TOOLS };
static char *kTxTool[TXT_TOOLS] = {"Brush", "Fill", "Take colour", "Brush size", "Undo", "Redo", "Pictures: add / edit", "Wipe drawing", "Done"};
static s32 sTxPlace, sTxTurn; /* (a picture being placed: its middle, width in texture pixels, its shape, quarter turns) */
static s32 sTxLayer = -1;     /* (the picture layer being placed again, -1 a new picture) */
static char sTxPath[700];     /* (the new picture's file) */
static f32 sTxPX, sTxPY, sTxPW, sTxPAsp;
static s32 sTxToolCur = -1; /* (the pad's cursor on the tools: which, -1 on the textures) */
static s32 sTxLastX = -1, sTxLastY = -1; /* (the stroke's last point: a line from it, not dots) */
static s32 sTxKey[TX_MAX], sTxInfo[TX_MAX], sTxSlots[TX_MAX], sTxN, sTxCur, sTxFocus, sTxTool, sTxButtons, sTxOver;
static s32 sTxMouse; /* 1: the mouse draws (the pad's cursor left alone), 0: the pad (the mouse left alone); Select switches */
static f32 sTxAccX, sTxAccY;

static void Tx_Load(s32 i) {
    s32 wh;

    if (sZw->pic == NULL) {
        sZw->pic = Heap_Alloc(256 * 256, 0x40, 0, 2);
    }
    if (sZw->pic == NULL || i < 0 || i >= sTxN) {
        sZw->picW = sZw->picH = 0;
        return;
    }
    wh = Port_WarriorTexturePicture(&sZw->w, sTxKey[i], sZw->pic, sZw->picPal);
    sZw->picW = wh & 0xFFFF;
    sZw->picH = wh >> 16;
    if (sZw->picRow != 1000 + i) {
        Port_PaintUndo(3, sZw->pic, sZw->picW, sZw->picH);
    }
    sZw->picRow = 1000 + i;
    sZw->picGen++;
    if (sZw->picX >= sZw->picW || sZw->picY >= sZw->picH) {
        sZw->picX = sZw->picW / 2;
        sZw->picY = sZw->picH / 2;
    }
}

static void Tx_Open(void) {
    s32 info[TX_MAX * 4], i;

    sTxN = Port_WarriorTextures(&sZw->w, info, TX_MAX | 0x10000); /* (with the face's expressions: the blink, the hurt face) */
    for (i = 0; i < sTxN; i++) {
        sTxKey[i] = (info[i * 4] & ~31) | 16;
        sTxInfo[i] = info[i * 4 + 2];
        sTxSlots[i] = info[i * 4 + 3];
    }
    sTxCur = sTxCur < sTxN ? sTxCur : 0;
    sTxFocus = 0;
    sTxTool = TXT_BRUSH;
    sZw->picDirty = 0;
    sZw->picImport = 0;
    Tx_Load(sTxCur);
    Zw_SetMode(ZW_TEXTURE);
}

static void Tx_Name(s32 i, char *out) {
    static char *parts[13] = {"Hair", "Head", "Face", "Chest", "Waist", "Upper arms", "Forearms", "Hands", "Hips", "Shins", "Feet",
                              "Accessories", "Other"};
    s32 kind = (sTxInfo[i] >> 24) & 0xFF, s;

    if (kind == 1) {
        Ps_Copy(out, "Face", 32);
        return;
    }
    if (kind == 3) {
        Ps_Copy(out, "Face: blink", 32);
        return;
    }
    if (kind == 4) {
        Ps_Copy(out, "Face: hurt", 32);
        return;
    }
    if (kind >= 5 && kind <= 7) {
        sprintf(out, "Face: expression %d", (int)(kind - 2));
        return;
    }
    if (kind == 2) {
        Ps_Copy(out, "Hair", 32);
        return;
    }
    for (s = 0; s < 13 && !((sTxSlots[i] >> s) & 1); s++) {
    }
    Ps_Copy(out, parts[s < 13 ? s : 12], 32);
}

static void Tx_Keep(void) { /* (the picture as it is, for undo) */
    if (sZw->picW > 0) {
        Port_PaintUndo(0, sZw->pic, sZw->picW, sZw->picH);
    }
}

/* The brush along the stroke: a line from where it was last frame (the cursor moves faster than a pixel a frame). */
static void Tx_Stroke(s32 start) {
    s32 x0 = sTxLastX, y0 = sTxLastY, x1 = sZw->picX, y1 = sZw->picY, n, k;

    if (start || x0 < 0) {
        Tx_Keep();
        x0 = x1;
        y0 = y1;
    }
    n = (x1 - x0 < 0 ? x0 - x1 : x1 - x0) > (y1 - y0 < 0 ? y0 - y1 : y1 - y0) ? (x1 - x0 < 0 ? x0 - x1 : x1 - x0) : (y1 - y0 < 0 ? y0 - y1 : y1 - y0);
    for (k = 0; k <= n; k++) {
        Co_PicPaintAt(n > 0 ? x0 + (x1 - x0) * k / n : x1, n > 0 ? y0 + (y1 - y0) * k / n : y1);
    }
    sTxLastX = x1;
    sTxLastY = y1;
}

static void Tx_UndoStep(s32 redo) {
    if (sZw->picW > 0 && Port_PaintUndo(redo ? 2 : 1, sZw->pic, sZw->picW, sZw->picH)) {
        sZw->picDirty = 1;
        sZw->picGen++;
        Snd_PlaySe(1, 2);
    } else {
        Zw_Note(redo ? "Nothing to redo." : "Nothing to undo.");
    }
}

static void Tx_Store(void) { /* (the stroke kept: the model painted with it a moment later) */
    sTxLastX = sTxLastY = -1;
    if (!sZw->picDirty || sTxN <= 0) {
        return;
    }
    sZw->picDirty = 0;
    if (Port_PaintSave(sTxKey[sTxCur], sZw->pic, sZw->picW, sZw->picH, NULL)) {
        sZw->dirty = 1;
        sZw->paintDue = ZW_PAINT_REST;
    }
}

static void Tx_Canvas(f32 *px, f32 *py, f32 *sc) {
    f32 w = (f32)(sZw->picW > 0 ? sZw->picW : 1), h = (f32)(sZw->picH > 0 ? sZw->picH : 1);

    *sc = (TX_CX1 - TX_CX0) / w < (TX_CY1 - TX_CY0) / h ? (TX_CX1 - TX_CX0) / w : (TX_CY1 - TX_CY0) / h;
    *px = (TX_CX0 + TX_CX1) * 0.5f - w * *sc * 0.5f;
    *py = TX_CY0;
}

static void Tx_Colour(s32 step) { /* (the brush's colour: the next one, round) */
    sZw->picBrush = (sZw->picBrush + step + 16) % 16;
    Snd_PlaySe(1, 0);
}

/* A colour of your own: the texture's least used colour (not a see-through one) is freed, its few pixels taken by the
   nearest other colour, and becomes the one chosen in the colour picker (fxTarget 8). */
static s32 sTxNewSlot = -1;

static void Tx_NewColourOf(s32 wand);

static void Tx_NewColour(void) {
    Tx_NewColourOf(0);
}

/* The magic wand: the area of one colour under the cursor (joined side by side) gets a colour of its own, chosen in
   the colour picker - its colour, saturation and brightness changed without touching the rest of that colour. */
static void Tx_Wand(void) {
    Tx_NewColourOf(1);
}

static void Tx_NewColourOf(s32 wand) {
    s32 cnt[16], i, j, best = -1, near = -1, nd = 0, from = -1;

    if (sTxN <= 0 || sZw->picW <= 0 || sZw->picH <= 0) {
        return;
    }
    if (wand) {
        from = sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15;
    }
    for (i = 0; i < 16; i++) {
        cnt[i] = 0;
    }
    for (i = 0; i < sZw->picW * sZw->picH; i++) {
        cnt[sZw->pic[i] & 15]++;
    }
    for (i = 0; i < 16; i++) {
        if (((sZw->picPal[i] >> 24) & 0xFF) >= 0x40 && i != from && (best < 0 || cnt[i] < cnt[best])) {
            best = i;
        }
    }
    if (best < 0) {
        return;
    }
    for (j = 0; j < 16; j++) { /* (its pixels: the nearest other colour) */
        s32 dr = (s32)(sZw->picPal[j] & 0xFF) - (s32)(sZw->picPal[best] & 0xFF);
        s32 dg = (s32)((sZw->picPal[j] >> 8) & 0xFF) - (s32)((sZw->picPal[best] >> 8) & 0xFF);
        s32 db = (s32)((sZw->picPal[j] >> 16) & 0xFF) - (s32)((sZw->picPal[best] >> 16) & 0xFF);
        s32 d = dr * dr + dg * dg + db * db;

        if (j != best && ((sZw->picPal[j] >> 24) & 0xFF) >= 0x40 && (near < 0 || d < nd)) {
            near = j;
            nd = d;
        }
    }
    Tx_Keep();
    if (cnt[best] > 0 && near >= 0) {
        for (i = 0; i < sZw->picW * sZw->picH; i++) {
            if ((sZw->pic[i] & 15) == best) {
                sZw->pic[i] = (u8)((sZw->pic[i] & 0xF0) | near);
            }
        }
        sZw->picDirty = 1;
    }
    if (wand) { /* (the area: a flood of `from` from the cursor, four ways, onto the freed colour) */
        s32 w = sZw->picW, h = sZw->picH, changed = 1, pass, x, y;

        sZw->picPal[best] = sZw->picPal[from];
        sZw->pic[sZw->picY * w + sZw->picX] = (u8)((sZw->pic[sZw->picY * w + sZw->picX] & 0xF0) | best);
        for (pass = 0; changed && pass < 512; pass++) { /* (grown from the cursor, sweep after sweep, down then up) */
            changed = 0;
            for (y = 0; y < h; y++) {
                s32 yy = (pass & 1) ? h - 1 - y : y;

                for (x = 0; x < w; x++) {
                    s32 xx = (pass & 1) ? w - 1 - x : x, q = yy * w + xx;

                    if ((sZw->pic[q] & 15) == from &&
                        ((xx > 0 && (sZw->pic[q - 1] & 15) == best) || (xx < w - 1 && (sZw->pic[q + 1] & 15) == best) ||
                         (yy > 0 && (sZw->pic[q - w] & 15) == best) || (yy < h - 1 && (sZw->pic[q + w] & 15) == best))) {
                        sZw->pic[q] = (u8)((sZw->pic[q] & 0xF0) | best);
                        changed = 1;
                    }
                }
            }
        }
        sZw->picDirty = 1;
        Zw_Note("Area picked! Give it any colour, saturation and brightness.");
    }
    sTxNewSlot = best;
    sZw->picBrush = best;
    sZw->fxTarget = 8;
    sZw->fxBack = 0;
    Co_ToHsv(sZw->picPal[best] & 0xFFFFFF, &sZw->pkH, &sZw->pkS, &sZw->pkV);
    sZw->pkRow = 0;
    sZw->pkSwatch = 0;
    Zw_SetMode(ZW_FXCOL);
    Snd_PlaySe(1, 1);
}

static void Tx_Pick(void) {
    sZw->picBrush = sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15;
    Snd_PlaySe(1, 0);
}

static void Tx_Import(void) {
    if (sTxN <= 0) {
        return;
    }
    sZw->picImport = 1;
    Port_PickPicture();
    Zw_Note("Choose a picture: then move it, size it and put it on.");
    Snd_PlaySe(1, 1);
}

static void Tx_Keep(void);
static void Tx_Store(void);

/* Picture layer i of this texture taken up to be moved, sized, turned (or taken off) again. 1 done. */
static s32 Tx_LayerOpen(s32 i) {
    s32 o[5];

    if (sTxN <= 0 || sZw->picW <= 0) {
        return 0;
    }
    Tx_Store();
    Tx_Keep();
    if (!Port_LayerEdit(sTxKey[sTxCur], i, sZw->pic, sZw->picW, sZw->picH, sZw->picPal, o)) {
        return 0;
    }
    sTxLayer = i;
    sTxPlace = 1;
    sTxPX = (f32)o[0] / 100.0f;
    sTxPY = (f32)o[1] / 100.0f;
    sTxPW = (f32)o[2] / 100.0f;
    sTxTurn = o[3] & 3;
    sTxPAsp = (f32)(o[4] >> 16) / (f32)((o[4] & 0xFFFF) > 0 ? (o[4] & 0xFFFF) : 1);
    Zw_Note("Picture picked up: move it, size it, turn it. L2 / R2 another picture, Circle takes it off, R3 a new one.");
    return 1;
}

/* R3: the texture's pictures - its top one picked up again; with none, a new one. */
static void Tx_Pictures(void) {
    s32 n = sTxN > 0 ? Port_LayerCount(sTxKey[sTxCur]) : 0;

    if (n > 0 && Tx_LayerOpen(n - 1)) {
        Snd_PlaySe(1, 1);
        return;
    }
    Tx_Import();
}

static void Tx_Wipe(void) {
    if (sTxN <= 0) {
        return;
    }
    Port_PaintClear(sTxKey[sTxCur]);
    sZw->picDirty = 0;
    Tx_Load(sTxCur);
    sZw->dirty = 1;
    sZw->paintDue = ZW_PAINT_REST;
    Zw_Note("Drawing wiped: this texture is as it was.");
    Snd_PlaySe(1, 2);
}

static void Tx_Leave(void) {
    Tx_Store();
    Zw_SetMode(ZW_EDIT);
    Snd_PlaySe(1, 2);
}

/* A tool's button (the mouse), or the pad's choice of it. */
static void Tx_UseTool(s32 t) {
    switch (t) {
    case TXT_BRUSH: case TXT_FILL: case TXT_PICK: sTxTool = t; Snd_PlaySe(1, 0); break;
    case TXT_SIZE:
        sZw->picSize = (sZw->picSize + 1) % 5;
        Snd_PlaySe(1, 0);
        break;
    case TXT_UNDO: Tx_UndoStep(0); Tx_Store(); break;
    case TXT_REDO: Tx_UndoStep(1); Tx_Store(); break;
    case TXT_IMPORT: Tx_Pictures(); break;
    case TXT_WIPE: Tx_Wipe(); break;
    default: Tx_Leave(); break;
    }
}

static void Tx_ToolRect(s32 t, f32 *r) { /* (the buttons at the foot of the controls panel: picture, size, undo, redo, wipe) */
    static const s32 kPill[TXT_TOOLS] = {-1, -1, -1, 1, 2, 3, 0, 4, -1};
    s32 k = kPill[t];

    if (k < 0) {
        r[0] = r[1] = -1000.0f;
        r[2] = r[3] = -999.0f;
        return;
    }
    r[0] = 922.0f + (f32)k * 88.0f;
    r[1] = 308.0f;
    r[2] = r[0] + 84.0f;
    r[3] = 338.0f;
}

static void Tx_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 m[5], press, i;
    f32 mx, my, px, py, sc, r[4];

    if (Zw_SaveTick() || !live) {
        return;
    }
    if (sZw->picImport) { /* a picture file chosen: put on this texture */
        char path[700];

        if (Port_DropTake(path)) {
            s32 wh;

            sZw->picImport = 0;
            Tx_Store();
            if (sTxN > 0 && sZw->picW > 0 && (wh = Port_DecalLoad(path, sZw->pic, sZw->picW, sZw->picH, sZw->picPal)) != 0) {
                Ps_Copy(sTxPath, path, sizeof(sTxPath));
                sTxLayer = -1;
                sTxPlace = 1;
                sTxTurn = 0;
                sTxPAsp = (f32)(wh >> 16) / (f32)(wh & 0xFFFF);
                sTxPW = (f32)sZw->picW * 0.5f;
                sTxPX = (f32)sZw->picW * 0.5f;
                sTxPY = (f32)sZw->picH * 0.5f;
                Tx_Keep();
                Zw_Note("Move it, size it, turn it, then put it on! It takes the texture's own colours.");
                Snd_PlaySe(1, 1);
            } else {
                Zw_Note("That picture could not be read. Try a PNG or JPG.");
                Snd_PlaySe(1, 2);
            }
        }
    }
    /* the mouse */
    Port_MouseState(m);
    mx = (f32)m[0] / 10000.0f / Mx_ScreenFracX(1.0f);
    my = (f32)m[1] / 10000.0f / Mx_ScreenFracY(1.0f);
    press = m[2] & ~sTxButtons;
    Tx_Canvas(&px, &py, &sc);
    if (sTxPlace) { /* a picture being placed: moved (D-pad, stick, a mouse drag), sized (L1 / R1, the wheel), turned (Square) */
        s32 done = 0, stop = 0;
        f32 ax = pad->left[0], ay = pad->left[1], step = (f32)sZw->picW / 128.0f;

        if (pad->gameHeld & PADG_LEFT) sTxPX -= step;
        if (pad->gameHeld & PADG_RIGHT) sTxPX += step;
        if (pad->gameHeld & PADG_UP) sTxPY -= step;
        if (pad->gameHeld & PADG_DOWN) sTxPY += step;
        if (ax * ax > 0.04f) sTxPX += ax * step * 2.0f;
        if (ay * ay > 0.04f) sTxPY += ay * step * 2.0f;
        if (pad->gameHeld & PADG_R1) sTxPW *= 1.02f;
        if (pad->gameHeld & PADG_L1) sTxPW /= 1.02f;
        if (m[3] != 0) sTxPW *= m[3] > 0 ? 1.08f : 1.0f / 1.08f;
        if ((m[2] & 1) && mx >= px && my >= py && mx < px + (f32)sZw->picW * sc && my < py + (f32)sZw->picH * sc && !(press & 1 && my >= 632.0f)) {
            sTxPX = (mx - px) / sc;
            sTxPY = (my - py) / sc;
        }
        if (pad->gamePressed & PADG_SQUARE) {
            sTxTurn = (sTxTurn + 1) & 3;
            Snd_PlaySe(1, 0);
        }
        sTxPW = sTxPW < 2.0f ? 2.0f : sTxPW > (f32)sZw->picW * 4.0f ? (f32)sZw->picW * 4.0f : sTxPW;
        sTxPX = sTxPX < 0.0f ? 0.0f : sTxPX > (f32)sZw->picW ? (f32)sZw->picW : sTxPX;
        sTxPY = sTxPY < 0.0f ? 0.0f : sTxPY > (f32)sZw->picH ? (f32)sZw->picH : sTxPY;
        if (press & 1) { /* (its buttons under the canvas) */
            done = my >= 632.0f && my < 666.0f && mx >= 420.0f && mx < 600.0f;
            stop = my >= 632.0f && my < 666.0f && mx >= 620.0f && mx < 800.0f;
        }
        done |= (pad->gamePressed & (PADG_CROSS | PADG_START)) != 0;
        if (sTxLayer >= 0 && (pad->gamePressed & PADG_CIRCLE)) { /* (a layer: Circle takes it off) */
            sTxPlace = 0;
            Port_DecalEnd();
            Port_PaintUndo(1, sZw->pic, sZw->picW, sZw->picH);
            Tx_Keep();
            if (Port_LayerRemove(sTxKey[sTxCur], sTxLayer, sZw->pic, sZw->picW, sZw->picH, sZw->picPal)) {
                sZw->picDirty = 1;
                Tx_Store();
                Zw_Note("Picture taken off.");
            }
            sTxLayer = -1;
            sZw->picGen++;
            Snd_PlaySe(1, 2);
            sTxButtons = m[2];
            return;
        }
        if (sTxLayer >= 0 && (pad->gamePressed & (PADG_L2 | PADG_R2))) { /* (another of its pictures) */
            s32 n = Port_LayerCount(sTxKey[sTxCur]);
            s32 to = n > 0 ? (sTxLayer + ((pad->gamePressed & PADG_R2) ? 1 : n - 1)) % n : 0;

            sTxPlace = 0;
            Port_DecalEnd();
            Port_PaintUndo(1, sZw->pic, sZw->picW, sZw->picH);
            sZw->picGen++;
            sTxLayer = -1;
            Tx_LayerOpen(to);
            Snd_PlaySe(1, 0);
            sTxButtons = m[2];
            return;
        }
        if (pad->gamePressed & PADG_R3) { /* (a new picture instead) */
            sTxPlace = 0;
            Port_DecalEnd();
            Port_PaintUndo(1, sZw->pic, sZw->picW, sZw->picH);
            sZw->picGen++;
            sTxLayer = -1;
            Tx_Import();
            sTxButtons = m[2];
            return;
        }
        stop |= (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) != 0 || (press & 2);
        Port_DecalShow((s32)(sTxPX * 100.0f), (s32)(sTxPY * 100.0f), (s32)(sTxPW * 100.0f), sTxTurn, sZw->pic);
        sZw->picGen++;
        if (done) {
            Port_LayerKeep(sTxKey[sTxCur], sTxLayer, sTxPath, (s32)(sTxPX * 100.0f), (s32)(sTxPY * 100.0f), (s32)(sTxPW * 100.0f), sTxTurn);
            sTxPlace = 0;
            sTxLayer = -1;
            Port_DecalEnd();
            sZw->picDirty = 1;
            Tx_Store();
            Zw_Note("Picture on! R3 picks it up again any time: move it, size it, turn it or take it off.");
            Snd_PlaySe(1, 1);
        } else if (stop) {
            sTxPlace = 0;
            sTxLayer = -1;
            Port_DecalEnd();
            Port_PaintUndo(1, sZw->pic, sZw->picW, sZw->picH); /* (the texture as it was) */
            sZw->picGen++;
            Snd_PlaySe(1, 2);
        }
        sTxButtons = m[2];
        return;
    }
    sTxOver = 0;
    if (pad->gamePressed & PADG_SELECT) {
        sTxMouse ^= 1;
        Tx_Store();
        Zw_Note(sTxMouse ? "Mouse drawing: left button uses the tool, right takes a colour, the wheel picks colours."
                         : "Pad drawing: the D-pad or stick moves, Cross paints.");
        Snd_PlaySe(1, 1);
    } else if (!sTxMouse && (press & 3)) { /* (a click: the mouse it is) */
        sTxMouse = 1;
        Zw_Note("Mouse drawing! Select goes back to the pad.");
    }
    if (!sTxMouse) {
        sTxButtons = m[2];
        m[3] = 0;
        goto padInput;
    }
    if (m[4] && sZw->picW > 0 && mx >= px && my >= py && mx < px + (f32)sZw->picW * sc && my < py + (f32)sZw->picH * sc) {
        s32 x = (s32)((mx - px) / sc), y = (s32)((my - py) / sc);

        sTxOver = 1;
        if (x != sZw->picX || y != sZw->picY || m[2]) {
            sZw->picX = x < sZw->picW ? x : sZw->picW - 1;
            sZw->picY = y < sZw->picH ? y : sZw->picH - 1;
            sTxFocus = 1;
        }
        if (m[2] & 1) {
            if (sTxTool == TXT_BRUSH) {
                Tx_Stroke(press & 1);
            } else if ((press & 1) && sTxTool == TXT_FILL) {
                Tx_Keep();
                Co_PicFill(sZw->picX, sZw->picY);
                Tx_Store();
                Snd_PlaySe(1, 1);
            } else if ((press & 1) && sTxTool == TXT_PICK) {
                Tx_Pick();
                sTxTool = TXT_BRUSH;
            }
        }
        if (press & 2) {
            Tx_Pick();
        } else if (press & 4) { /* (the middle button: fill) */
            Tx_Keep();
            Co_PicFill(sZw->picX, sZw->picY);
            Tx_Store();
            Snd_PlaySe(1, 1);
        }
    }
    if (!(m[2] & 1) && (sTxButtons & 1)) {
        Tx_Store(); /* (a stroke done) */
    }
    if (m[3] != 0) {
        Tx_Colour(m[3] > 0 ? -1 : 1);
    }
    if (press & 1) { /* the list, the colours, the tools */
        for (i = 0; i < 16; i++) { /* (the rows shown: the list scrolled as it is drawn) */
            s32 at = (sTxCur > 15 ? sTxCur - 15 : 0) + i;

            if (at < sTxN && mx >= 40.0f && mx < 300.0f && my >= 150.0f + (f32)i * 30.0f - 13.0f && my < 150.0f + (f32)i * 30.0f + 13.0f) {
                Tx_Store();
                sTxCur = at;
                Tx_Load(at);
                Snd_PlaySe(1, 0);
            }
        }
        for (i = 0; i < 16; i++) {
            f32 cx = 362.0f + (f32)i * 33.0f;

            if (mx >= cx - 15.0f && mx < cx + 15.0f && my >= 632.0f && my < 664.0f) {
                sZw->picBrush = i;
                Snd_PlaySe(1, 0);
            }
        }
        for (i = 0; i < TXT_TOOLS; i++) {
            Tx_ToolRect(i, r);
            if (mx >= r[0] && mx < r[2] && my >= r[1] && my < r[3]) {
                sTxButtons = m[2];
                Tx_UseTool(i);
                return;
            }
        }
    }
    sTxButtons = m[2];
    if (sTxMouse) { /* (the mouse draws: the pad only for the colours, the brush, leaving) */
        sTxFocus = 1;
        if (pad->gamePressed & (PADG_L2 | PADG_R2)) {
            Tx_UseTool((pad->gamePressed & PADG_R2) ? TXT_REDO : TXT_UNDO);
        } else if (pad->gamePressed & PADG_L1) {
            Tx_Colour(-1);
        } else if (pad->gamePressed & PADG_R1) {
            Tx_Colour(1);
        } else if (pad->gamePressed & PADG_START) {
            Tx_UseTool(TXT_SIZE);
        } else if (pad->gamePressed & PADG_R3) {
            Tx_Pictures();
        } else if (pad->gamePressed & PADG_L3) {
            Tx_Wand();
        } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
            Tx_Leave();
        }
        return;
    }
padInput:
    /* the pad */
    if (sTxFocus == 0) { /* the left column: the textures, then the tools under them */
        if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            if (sTxToolCur > 0) {
                sTxToolCur--;
            } else if (sTxToolCur == 0) {
                sTxToolCur = -1;
            } else if (sTxCur > 0) {
                sTxCur--;
                Tx_Load(sTxCur);
            } else {
                sTxCur = sTxN > 0 ? sTxN - 1 : 0; /* (round: the tools are buttons now) */
                Tx_Load(sTxCur);
            }
            Snd_PlaySe(1, 0);
        } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            if (sTxToolCur >= 0) {
                sTxToolCur = sTxToolCur < TXT_TOOLS - 1 ? sTxToolCur + 1 : -1;
                if (sTxToolCur < 0 && sTxN > 0) {
                    sTxCur = 0;
                    Tx_Load(0);
                }
            } else if (sTxCur < sTxN - 1) {
                sTxCur++;
                Tx_Load(sTxCur);
            } else {
                sTxCur = 0;
                Tx_Load(0);
            }
            Snd_PlaySe(1, 0);
        } else if ((pad->gamePressed & PADG_CROSS) && sTxToolCur >= 0) { /* a tool: used (brush, fill, take colour: then the canvas) */
            s32 t = sTxToolCur;

            Tx_UseTool(t);
            if ((t == TXT_BRUSH || t == TXT_FILL || t == TXT_PICK) && sTxN > 0) {
                sTxFocus = 1;
            }
        } else if ((pad->gamePressed & (PADG_CROSS | PADG_RIGHT | PADG_RIGHT2)) && sTxN > 0) {
            sTxFocus = 1;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_L1) {
            Tx_Pictures();
        } else if (pad->gamePressed & PADG_L3) {
            Tx_NewColour();
        } else if (pad->gamePressed & PADG_SQUARE) {
            Tx_Wipe();
        } else if (pad->gamePressed & PADG_START) {
            sZw->saving = 2;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
            Tx_Leave();
        }
        return;
    }
    if (sZw->picW <= 0) {
        sTxFocus = 0;
        return;
    }
    { /* the cursor: the D-pad a pixel at a time, the stick gently (faster the further) */
        s32 dx = 0, dy = 0;
        f32 ax = pad->left[0], ay = pad->left[1];

        if (pad->gameRepeat & PADG_LEFT) {
            dx = -1;
        } else if (pad->gameRepeat & PADG_RIGHT) {
            dx = 1;
        }
        if (pad->gameRepeat & PADG_UP) {
            dy = -1;
        } else if (pad->gameRepeat & PADG_DOWN) {
            dy = 1;
        }
        sTxAccX = ax * ax > 0.04f ? sTxAccX + ax * (0.15f + 0.9f * ax * ax) : 0.0f;
        sTxAccY = ay * ay > 0.04f ? sTxAccY + ay * (0.15f + 0.9f * ay * ay) : 0.0f;
        while (sTxAccX >= 1.0f) { dx++; sTxAccX -= 1.0f; }
        while (sTxAccX <= -1.0f) { dx--; sTxAccX += 1.0f; }
        while (sTxAccY >= 1.0f) { dy++; sTxAccY -= 1.0f; }
        while (sTxAccY <= -1.0f) { dy--; sTxAccY += 1.0f; }
        sZw->picX = sZw->picX + dx < 0 ? 0 : sZw->picX + dx >= sZw->picW ? sZw->picW - 1 : sZw->picX + dx;
        sZw->picY = sZw->picY + dy < 0 ? 0 : sZw->picY + dy >= sZw->picH ? sZw->picH - 1 : sZw->picY + dy;
    }
    if ((pad->gameHeld & PADG_CROSS) && sTxTool == TXT_BRUSH) { /* Cross: the tool chosen */
        Tx_Stroke((pad->gamePressed & PADG_CROSS) != 0);
    } else if (!(m[2] & 1)) {
        Tx_Store();
    }
    if ((pad->gamePressed & PADG_CROSS) && sTxTool == TXT_FILL) {
        Tx_Keep();
        Co_PicFill(sZw->picX, sZw->picY);
        Tx_Store();
        Snd_PlaySe(1, 1);
    } else if ((pad->gamePressed & PADG_CROSS) && sTxTool == TXT_PICK) {
        Tx_Pick();
        sTxTool = TXT_BRUSH;
    }
    if (pad->gamePressed & (PADG_L2 | PADG_R2)) {
        Tx_UseTool((pad->gamePressed & PADG_R2) ? TXT_REDO : TXT_UNDO);
    } else if (pad->gamePressed & PADG_CIRCLE) {
        Tx_Keep();
        Co_PicFill(sZw->picX, sZw->picY);
        Tx_Store();
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_SQUARE) {
        Tx_Pick();
    } else if (pad->gamePressed & PADG_L1) {
        Tx_Colour(-1);
    } else if (pad->gamePressed & PADG_R1) {
        Tx_Colour(1);
    } else if (pad->gamePressed & PADG_START) {
        Tx_UseTool(TXT_SIZE);
    } else if (pad->gamePressed & PADG_L3) { /* the magic wand: the area under the cursor a colour of its own */
        Tx_Wand();
    } else if (pad->gamePressed & PADG_R3) { /* a picture put on this texture, or its pictures picked up again */
        Tx_Pictures();
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Tx_Store();
        sTxFocus = 0;
        Snd_PlaySe(1, 2);
    }
}

static void Tx_Draw(void) {
    f32 pop = Zw_In(sZw->modeT, 8), px, py, sc, r[4];
    s32 a = (s32)(128.0f * pop), i, top = sTxCur > 15 ? sTxCur - 15 : 0;
    char name[40];

    Zw_EditStage(1140.0f, 745.0f, 395.0f);
    Zw_Title("TEXTURES");
    { /* what does what, for the way it is being drawn now */
        static const s32 kPadList[8] = {15, 0, 9, 1, 7, 2, 13, 18};
        static char *kPadListT[8] = {"Texture, then the tools below", "Draw on it / use the tool", "Pictures: add / edit", "Wipe drawing", "Save", "Back",
                                     "Draw with the mouse", "New colour (any)"};
        static const s32 kPadDraw[11] = {15, 0, 4, 1, 14, 7, 10, 17, 2, 13, 18};
        static char *kPadDrawT[11] = {"Move (stick: gently)", "Paint", "Fill", "Take colour", "Colour", "Brush size", "Undo / redo",
                                      "Pictures: add / edit", "Back to the list", "Draw with the mouse", "Magic wand: area's colour"};
        static char *kMouseT[9] = {"Left button: paint", "Right button: take colour", "Middle button: fill", "Wheel: colour",
                                   "Colour", "Brush size", "Undo / redo", "Draw with the pad", "Magic wand: area's colour"};
        static const s32 kMouseK[9] = {-1, -1, -1, -1, 14, 7, 10, 13, 18};
        s32 n, k;
        f32 y = 150.0f;

        static const s32 kPlaceK[10] = {15, 14, 1, 0, 2, 4, 10, 17, -1, -1};
        static char *kPlaceT[10] = {"Move", "Smaller / bigger", "Turn", "Put it on", "Cancel", "Take it off (a picture on)",
                                    "Another of its pictures", "A new picture", "Mouse: drag, wheel to size", "Right button: cancel"};

        Mx_Box(ZW_WINDOW, 912.0f, 118.0f, 1362.0f, 348.0f, 18.0f, 0x80);
        Mx_TextA(1137.0f, 134.0f, sTxPlace ? "CONTROLS - PLACING A PICTURE" : sTxMouse ? "CONTROLS - MOUSE" : "CONTROLS - PAD", 15.0f, 420.0f, MX_GOLD, 1, 0x80);
        n = sTxPlace ? 10 : sTxMouse ? 9 : sTxFocus == 0 ? 8 : 11;
        for (k = 0; k < n; k++) { /* (two columns) */
            s32 kind = sTxPlace ? kPlaceK[k] : sTxMouse ? kMouseK[k] : sTxFocus == 0 ? kPadList[k] : kPadDraw[k];
            s32 per = n > 10 ? 6 : 5;
            f32 x = k < per ? 924.0f : 1146.0f;

            y = 160.0f + (f32)(k % per) * (per == 6 ? 23.0f : 27.0f);
            if (sTxPlace) {
                if (kind >= 0) {
                    Mx_Button(kind, x, y, 16.0f);
                    x += Mx_ButtonW(kind, 16.0f) + 6.0f;
                }
                Mx_TextA(x, y, kPlaceT[k], 13.0f, 200.0f, MX_WHITE, 0, 0x80);
                continue;
            }
            if (kind >= 0) {
                Mx_Button(kind, x, y, 16.0f);
                x += Mx_ButtonW(kind, 16.0f) + 6.0f;
            }
            if (sTxMouse) {
                Mx_TextA(x, y, kMouseT[k], 13.0f, 200.0f, MX_WHITE, 0, 0x80);
            } else if (sTxFocus == 0) {
                Mx_TextA(x, y, kPadListT[k], 13.0f, 200.0f, MX_WHITE, 0, 0x80);
            } else {
                Mx_TextA(x, y, kPadDrawT[k], 13.0f, 200.0f, MX_WHITE, 0, 0x80);
            }
        }
    }
    Mx_Box(ZW_WINDOW, 30.0f, 118.0f, 310.0f, 640.0f, 20.0f, 0x80);
    Mx_Box(ZW_WINDOW, 318.0f, 118.0f, 902.0f, 686.0f, 20.0f, 0x80);
    if (sTxN <= 0) {
        Mx_TextA(610.0f, 360.0f, "No textures to draw on.", 20.0f, 500.0f, MX_LAV, 1, a);
    }
    for (i = top; i < sTxN && i < top + 16; i++) { /* the textures */
        f32 y = 150.0f + (f32)(i - top) * 30.0f;
        s32 lit = i == sTxCur;

        if (lit && sTxFocus == 0 && sTxToolCur < 0) {
            Zw_SelBar(40.0f, y - 13.0f, 300.0f, y + 13.0f);
        } else if (lit) {
            Mx_Frame(40.0f, y - 13.0f, 300.0f, y + 13.0f, 2.0f, Mx_Rgba(250, 205, 90, 0x80));
        }
        Tx_Name(i, name);
        Mx_TextA(52.0f, y, name, 16.0f, 150.0f, lit && sTxFocus == 0 ? MX_DARK : MX_WHITE, 0, a);
        sprintf(name, "%dx%d", (int)(sTxInfo[i] & 0xFFF), (int)((sTxInfo[i] >> 12) & 0xFFF));
        Mx_TextA(292.0f, y, name, 13.0f, 80.0f, lit && sTxFocus == 0 ? MX_DARK : MX_LAV, 2, a);
    }
    for (i = 0; i < TXT_TOOLS; i++) { /* the buttons (clicked; the pad has its own) */
        Tx_ToolRect(i, r);
        if (r[0] < 0.0f) {
            continue;
        }
        Mx_Bar(i == TXT_IMPORT ? ZW_PILL : ZW_PILL_STEEL, r[0], r[1], r[2], r[3], a);
        if (i == TXT_SIZE) {
            sprintf(name, "Size %d", (int)sZw->picSize + 1);
        } else if (i == TXT_IMPORT) {
            Ps_Copy(name, "Picture", sizeof(name));
        } else if (i == TXT_WIPE) {
            Ps_Copy(name, "Wipe", sizeof(name));
        } else {
            Ps_Copy(name, kTxTool[i], sizeof(name));
        }
        Mx_TextA((r[0] + r[2]) * 0.5f, (r[1] + r[3]) * 0.5f, name, 14.0f, 78.0f, i == TXT_IMPORT ? MX_DARK : MX_WHITE, 1, a);
    }
    if (sZw->picW > 0 && sZw->picH > 0) { /* the canvas */
        static const s32 kRad[5] = {0, 1, 2, 3, 5};
        s32 rr = kRad[sZw->picSize >= 0 && sZw->picSize < 5 ? sZw->picSize : 0];

        Tx_Canvas(&px, &py, &sc);
        Co_PicTexture(sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15, 0);
        Mx_PicEx(TBO_PICTURE, px, py, px + (f32)sZw->picW * sc, py + (f32)sZw->picH * sc, 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
        Mx_Frame(px - 2.0f, py - 2.0f, px + (f32)sZw->picW * sc + 2.0f, py + (f32)sZw->picH * sc + 2.0f, 2.0f, Mx_Rgba(250, 205, 90, 0x80));
        if (sTxFocus == 1 || sTxOver) { /* (the brush where it would paint) */
            Mx_Frame(px + (f32)(sZw->picX - rr) * sc - 1.0f, py + (f32)(sZw->picY - rr) * sc - 1.0f, px + (f32)(sZw->picX + rr + 1) * sc + 1.0f,
                     py + (f32)(sZw->picY + rr + 1) * sc + 1.0f, 2.0f, (sZw->frames / 8) & 1 ? Mx_Rgba(255, 255, 255, 0x80) : Mx_Rgba(20, 20, 30, 0x80));
        }
    }
    if (sTxPlace) { /* (placing a picture: put it on, cancel) */
        Mx_Bar(ZW_PILL, 420.0f, 634.0f, 600.0f, 664.0f, a);
        Mx_TextA(510.0f, 649.0f, "Put it on", 16.0f, 170.0f, MX_DARK, 1, a);
        Mx_Bar(ZW_PILL_STEEL, 620.0f, 634.0f, 800.0f, 664.0f, a);
        Mx_TextA(710.0f, 649.0f, "Cancel", 16.0f, 170.0f, MX_WHITE, 1, a);
    }
    for (i = 0; i < 16 && !sTxPlace; i++) { /* its colours, the brush's lit */
        Zw_Swatch(sZw->picPal[i] & 0xFFFFFF, 362.0f + (f32)i * 33.0f, 648.0f, i == sZw->picBrush ? 30.0f : 21.0f, i == sZw->picBrush);
    }
    if (sTxMouse) {
        Zw_Prompt(13, "Use the pad");
        Zw_Prompt(2, "Back");
    } else if (sTxFocus == 0) {
        Zw_Prompt(13, "Use the mouse");
        Zw_Prompt(15, "Texture");
        Zw_Prompt(0, "Draw on it");
        Zw_Prompt(9, "Pictures: add / edit");
        Zw_Prompt(1, "Wipe drawing");
        Zw_Prompt(7, "Save");
        Zw_Prompt(2, "Back");
    } else {
        Zw_Prompt(15, "Move");
        Zw_Prompt(0, "Paint");
        Zw_Prompt(4, "Fill");
        Zw_Prompt(1, "Take colour");
        Zw_Prompt(14, "Colour");
        Zw_Prompt(7, "Brush size");
        Zw_Prompt(2, "List");
        Zw_Prompt(13, "Use the mouse");
    }
}

/* ---------------------------------------------------------------------------------------------- the shading of each
 * part from another fighter (ZW_SHADE edits, one per entry: 2 * slot a part, + 1 its layer; the piece word the entry,
 * -1 the hair's layer as it was first): how it is shaded (like what it stands for: the part under a layer, the body's own
 * part there; its own; the skin's; the outfit's) and, like what it stands for, each of its shading textures (the most
 * used first) given the one in the same place, one picked, or its own (words 4..11: 0, 1.., -1). */
#define SH_ROWS 64
static char *kShPart[12] = {"Hair", "Head", "Face", "Chest", "Waist", "Upper arms", "Forearms", "Hands", "Hips", "Shins", "Feet", "Accessories"};
static s32 sShList[SH_ROWS][2]; /* the screen's rows: {entry, its shading texture (-1 the entry's own line, -2 its glow, -3
                                   its outline group)};
                                   entry 24: all the parts' glow (the body's own too) */
#define SH_ALL 24
static char *kShGlow[5] = {"its own", "none", "soft", "bright", "blazing"};

static s32 Sh_Glow(s32 e);
static s32 sShRows;

static s32 Sh_Edit(s32 e, s32 make) {
    PortWarrior *w = &sZw->w;
    s32 i;

    for (i = 0; i < w->edits; i++) {
        if (w->edit[i][0] == ZW_SHADE && (w->edit[i][1] == e || (e == 1 && w->edit[i][1] == -1))) {
            return i;
        }
    }
    if (!make) {
        return -1;
    }
    i = Ws_Edit(ZW_SHADE, e, 1);
    if (i >= 0) {
        sZw->w.edit[i][3] = e & 1 ? 0 : 1; /* (as such entries are: a layer like the part under it, a part its own) */
    }
    return i;
}

static s32 Sh_Mode(s32 e) {
    s32 i = Sh_Edit(e, 0);

    return i >= 0 && sZw->w.edit[i][3] >= 0 && sZw->w.edit[i][3] <= 3 ? sZw->w.edit[i][3] : e & 1 ? 0 : 1;
}

/* An entry's glow (the edit's word 2): 0 its own, 1 none, 2 soft, 3 bright (a Super Saiyan's hair), 4 blazing. */
static s32 Sh_Glow(s32 e) {
    s32 i = Sh_Edit(e, 0);

    return i >= 0 && (sZw->w.edit[i][2] & 0xFF) <= 4 ? sZw->w.edit[i][2] & 0xFF : 0;
}

/* An entry's outline group (the edit's word 2 >> 8): 0 with the body, 1 its own, 2 + g group g (0..15). */
static s32 Sh_Line(s32 e) {
    s32 i = Sh_Edit(e, 0);

    return i >= 0 && ((sZw->w.edit[i][2] >> 8) & 0xFF) <= 17 ? (sZw->w.edit[i][2] >> 8) & 0xFF : 0;
}

static s32 Sh_Pick(s32 e, s32 t) {
    s32 i = Sh_Edit(e, 0);

    return i >= 0 && t < 8 ? sZw->w.edit[i][4 + t] : 0;
}

/* ---- the shading screen, made new: on the left what is shaded (the whole warrior, each part, each layer there is),
 * on the right how. A style makes its shadows from the part's own colours as they are after painting (plat
 * wp_gen_ramp: any colour shades right): cel (two tones), soft, deep, glow (light: glowing hair), flat (none); or the
 * game's own shading as it was. Then the shadow's size, strength and tint, the glow, the outline. Kept in the entry's
 * ZW_SHADE edit: word 2 the glow | outline << 8 | style << 16, words 4..7 size, strength, tint, tint colour. */
enum { SHS_STYLE, SHS_SIZE, SHS_STRENGTH, SHS_TINT, SHS_GLOW, SHS_LINE, SHS_ROWS };
static char *kShStyle[7] = {"Like the whole warrior", "Cel (two tones)", "Soft", "Deep", "Glow", "Flat (no shadow)", "The game's own"};
static char *kShSize[5] = {"Tiny", "Small", "Medium", "Large", "Huge"};
static char *kShTint[4] = {"Auto", "Cool", "Warm", "Your colour"};
static char *kShSetName[SHS_ROWS] = {"Style", "Shadow size", "Strength", "Shadow tint", "Glow", "Outline"};

static s32 Sh_Style(s32 e) {
    s32 i = Sh_Edit(e, 0), v = i >= 0 ? (sZw->w.edit[i][2] >> 16) & 0xFF : 0;

    return v <= 8 ? v : 0;
}

static s32 Sh_Styled(s32 e) { /* (shadows made for it: its own settings count) */
    return Sh_Style(e) >= 1 && Sh_Style(e) <= 5;
}

static s32 Sh_Word(s32 e, s32 k, s32 def) {
    s32 i = Sh_Edit(e, 0);

    return i >= 0 && Sh_Styled(e) ? sZw->w.edit[i][k] : def;
}

static s32 Sh_Size(s32 e) {
    s32 v = Sh_Word(e, 4, 2);

    return v < 0 ? 0 : v > 4 ? 4 : v;
}

static s32 Sh_Strength(s32 e) {
    s32 v = Sh_Word(e, 5, 100);

    return v < 0 ? 0 : v > 200 ? 200 : v;
}

static s32 Sh_Tint(s32 e) {
    return Sh_Word(e, 6, 0) & 3;
}

static void Sh_StyleName(s32 e, char *out) { /* (copied, not picked with ?: between a table's and a literal: in the game's
                                                code that reads a 4-byte pointer as 8) */
    if (e == SH_ALL && Sh_Style(e) == 0) {
        Ps_Copy(out, "The game's own", 48);
    } else {
        Ps_Copy(out, kShStyle[Sh_Style(e)], 48);
    }
}

static char *Sh_Name(s32 e, char *out) {
    if (e == SH_ALL) {
        return "Whole warrior";
    }
    sprintf(out, "%s%s", kShPart[e / 2 % 12], e & 1 ? " (layer)" : "");
    return out;
}

/* The style given (its settings to their first values when it starts being made). */
static void Sh_SetStyle(s32 e, s32 st) {
    s32 i = Sh_Edit(e, 1), was = Sh_Styled(e), k;

    if (i < 0) {
        return;
    }
    sZw->w.edit[i][2] = (sZw->w.edit[i][2] & 0xFFFF) | (st << 16);
    if (st >= 1 && st <= 5 && !was) {
        sZw->w.edit[i][4] = 2;
        sZw->w.edit[i][5] = 100;
        sZw->w.edit[i][6] = 0;
        sZw->w.edit[i][7] = 0;
        for (k = 8; k < 12; k++) {
            sZw->w.edit[i][k] = 0;
        }
    } else if (!(st >= 1 && st <= 5) && was) {
        for (k = 4; k < 12; k++) { /* (the game's own: as it was, nothing picked) */
            sZw->w.edit[i][k] = 0;
        }
    }
}

/* The rows: the whole warrior, every part, the layers there are. */
static void Sh_Load(void) {
    s32 own[24], e;

    Port_WarriorShades(-1, own);
    sShRows = 0;
    sShList[sShRows++][0] = SH_ALL;
    for (e = 0; e < 24; e += 2) {
        sShList[sShRows++][0] = e;
    }
    for (e = 1; e < 24 && sShRows < SH_ROWS; e += 2) {
        if (own[e] > 0) {
            sShList[sShRows++][0] = e;
        }
    }
    if (sZw->shRow >= sShRows) {
        sZw->shRow = sShRows > 0 ? sShRows - 1 : 0;
    }
}

/* The whole part the shading screen's cursor is on, as a workshop edit names it (-1 - slot; its layer -1 - (12 +
   slot)), -1000 none: Ws_ShapeTick blinks it on the model. */
static s32 Sh_BlinkNode(void) {
    s32 e;

    if (sShRows <= 0 || sZw->shRow >= sShRows) {
        return -1000;
    }
    e = sShList[sZw->shRow][0];
    if (e == SH_ALL) {
        return -1000;
    }
    return e & 1 ? -1 - (12 + e / 2) : -1 - e / 2;
}

/* What entry e's shading looks like: a strip from lit (left) to the side away from the light, on a pale colour, the
   way the game makes it (plat wp_gen_ramp / wp_gen_amount). */
static void Sh_Demo(s32 e, f32 x0, f32 y, f32 x1) {
    static const s32 kKeep[6] = {100, 60, 66, 45, 82, 100};
    s32 st = Sh_Styled(e) ? Sh_Style(e) : e != SH_ALL && Sh_Style(e) == 0 && Sh_Styled(SH_ALL) ? Sh_Style(SH_ALL) : 0;
    s32 src = Sh_Styled(e) ? e : SH_ALL;
    s32 size = Sh_Size(src), str = Sh_Strength(src), tk = Sh_Tint(src), rgb = Sh_Word(src, 7, 0) & 0xFFFFFF;
    s32 c[3] = {236, 214, 196}, tg[3], sh[3], ch, n;
    f32 w = (x1 - x0) / 24.0f;

    if (st == 0) {
        Mx_TextA(x0, y, "The game's own shading", 16.0f, x1 - x0, MX_LAV, 0, 0x80);
        return;
    }
    for (ch = 0; ch < 3; ch++) {
        tg[ch] = c[ch] * kKeep[st] / 100;
    }
    if (tk <= 1) {
        s32 k = tk == 1 ? 2 : 1;

        tg[0] = tg[0] * (100 - 8 * k) / 100;
        tg[1] = tg[1] * (100 - 4 * k) / 100;
        tg[2] = tg[2] * (100 + 7 * k) / 100 + 3 * k;
    } else if (tk == 2) {
        tg[0] = tg[0] * 114 / 100 + 6;
        tg[1] = tg[1] * 94 / 100;
        tg[2] = tg[2] * 78 / 100;
    } else {
        s32 r = rgb & 0xFF, g = (rgb >> 8) & 0xFF, b = (rgb >> 16) & 0xFF;
        s32 lt = tg[0] * 30 + tg[1] * 59 + tg[2] * 11, lc = r * 30 + g * 59 + b * 11;

        lc = lc < 1 ? 1 : lc;
        tg[0] = (tg[0] + r * lt / lc) / 2;
        tg[1] = (tg[1] + g * lt / lc) / 2;
        tg[2] = (tg[2] + b * lt / lc) / 2;
    }
    for (ch = 0; ch < 3; ch++) {
        tg[ch] = tg[ch] < 0 ? 0 : tg[ch] > 255 ? 255 : tg[ch];
        sh[ch] = (c[ch] - tg[ch] < 0 ? 0 : c[ch] - tg[ch]) * str / 100;
    }
    for (n = 0; n < 24; n++) {
        s32 j = 255 - n * 256 / 24, t = 128 + (size - 2) * 22, a = 0, v[3];
        u32 col;

        switch (st) {
        case 1: a = j < t ? 100 : 0; break;
        case 2: a = j < t - 48 ? 100 : j < t ? 62 : j < t + 28 ? 26 : 0; break;
        case 3: a = j < t - 44 ? 125 : j < t ? 100 : j < t + 44 ? 38 : 0; break;
        case 4: a = j < t - 26 ? 100 : 0; break;
        }
        for (ch = 0; ch < 3; ch++) {
            v[ch] = c[ch] - sh[ch] * a / 100;
            v[ch] = v[ch] < 0 ? 0 : v[ch];
        }
        col = Mx_Rgba(v[0], v[1], v[2], 0x80);
        Mx_Rect(x0 + (f32)n * w, y - 14.0f, x0 + (f32)(n + 1) * w, y + 14.0f, col, col);
    }
    Mx_Frame(x0 - 1.0f, y - 15.0f, x1 + 1.0f, y + 15.0f, 1.5f, Mx_Rgba(250, 205, 90, 0x80));
}

/* The shading screen, made simple: each part's shadow colour (the game's as it is, or one of your own, the shadow falling
   where the game has it), or the same shading as another part; its glow, its outline. Style word: 0 as it is, 7 your
   colour (word 7), 8 the same as another part (word 4 that part's entry). The made styles of before (1..6) are gone. */
enum { SH2_COLOUR, SH2_SAME, SH2_GLOW, SH2_LINE, SH2_ROWS };

static s32 Sh_Same(s32 e) { /* the part whose shading it has, -1 its own */
    s32 i = Sh_Edit(e, 0);

    return i >= 0 && Sh_Style(e) == 8 ? sZw->w.edit[i][4] : -1;
}

static void Sh_Clean(void) { /* (the styles of before, cel and the like: back to the game's) */
    s32 i;

    for (i = 0; i < sZw->w.edits; i++) {
        s32 st = (sZw->w.edit[i][2] >> 16) & 0xFF;

        if (sZw->w.edit[i][0] == ZW_SHADE && st >= 1 && st <= 6) {
            s32 k;

            sZw->w.edit[i][2] &= 0xFFFF;
            for (k = 4; k < 12; k++) {
                sZw->w.edit[i][k] = 0;
            }
        }
    }
}

static void Sh_SetMode(s32 e, s32 st, s32 word4) {
    s32 i = Sh_Edit(e, 1), k;

    if (i < 0) {
        return;
    }
    sZw->w.edit[i][2] = (sZw->w.edit[i][2] & 0xFFFF) | (st << 16);
    if (st != 7) {
        for (k = 4; k < 12; k++) {
            sZw->w.edit[i][k] = 0;
        }
    }
    if (st == 8) {
        sZw->w.edit[i][4] = word4;
    }
}

/* ---- shading by texture: each texture's shading ramps as the model draws it, and a shadow colour of its own (shade_tex=,
   PortWarrior.texShade: every batch drawing that picture, whatever part it is on) ---- */
static void Sh_Changed(void);

static u32 St_Hash(s32 z) {
    return ((u32)sZw->reg[z * 4] >> 5) & 0xFFFFFF;
}

static s32 St_Find(u32 hash) {
    s32 i;

    for (i = 0; i < 16; i++) {
        if (sZw->w.texShade[i][0] != 0 && ((u32)sZw->w.texShade[i][0] & 0xFFFFFF) == hash) {
            return i;
        }
    }
    return -1;
}

/* Its shadow colour now: 0x1000000 | 0xBBGGRR, 0 the game's. */
static s32 St_Colour(s32 z) {
    s32 i = St_Find(St_Hash(z));

    return i >= 0 ? sZw->w.texShade[i][1] : 0;
}

static void St_Set(s32 z, s32 rgb) {
    u32 h = St_Hash(z);
    s32 i = St_Find(h);

    if (rgb != 0 && (rgb & 0x1E000000) == 0) { /* (a new colour keeps the gradient it had) */
        rgb |= St_Colour(z) & 0x1E000000;
    }

    if (rgb == 0) {
        if (i >= 0) {
            sZw->w.texShade[i][0] = 0;
            sZw->w.texShade[i][1] = 0;
        }
        return;
    }
    if (i < 0) {
        for (i = 0; i < 16 && sZw->w.texShade[i][0] != 0; i++) {
        }
        if (i == 16) {
            Zw_Note("Sixteen textures with shading of their own is the most!");
            return;
        }
    }
    sZw->w.texShade[i][0] = (s32)h;
    sZw->w.texShade[i][1] = rgb;
}

static void St_Draw(void) {
    f32 pop = Zw_In(sZw->modeT, 8);
    s32 a = (s32)(128.0f * pop), i, r, z = -1, ramps[1 + 4 * 4];
    char line[80];

    Zw_Host(1250.0f, 330.0f, 300.0f, 1);
    Zw_EditStage(900.0f, 540.0f, 400.0f);
    Zw_Title("SHADING");
    Mx_Bar(ZW_PILL_STEEL, 300.0f, 96.0f, 440.0f, 124.0f, 0x80);
    Mx_Text(370.0f, 110.0f, "BY PART", 15.0f, 120.0f, MX_WHITE, 1);
    Mx_Bar(ZW_PILL, 450.0f, 96.0f, 610.0f, 124.0f, 0x80);
    Mx_Text(530.0f, 110.0f, "BY TEXTURE", 15.0f, 140.0f, MX_DARK, 1);
    Mx_Box(ZW_WINDOW, 30.0f, 132.0f, 620.0f, 628.0f, 22.0f, 0x80);
    for (i = 0; i < 12 && sZw->shTexTop + i < sZw->coRows; i++) { /* the textures (the colours screen's list) */
        s32 row = sZw->shTexTop + i, zz = sZw->coList[row];
        f32 y = 160.0f + (f32)i * 36.0f;
        s32 lit = row == sZw->shTexRow;

        if (zz < 0) {
            static char *heads[3] = {"HEAD", "BODY", "DETAILS"};

            Mx_Bar(ZW_SECTION, 44.0f, y - 13.0f, 240.0f, y + 13.0f, a);
            Mx_TextA(58.0f, y, heads[-1 - zz], 16.0f, 170.0f, MX_DARK, 0, a);
            continue;
        }
        if (lit) {
            z = zz;
            Zw_SelBar(40.0f, y - 16.0f, 610.0f, y + 16.0f);
        }
        Zw_Swatch((u32)sZw->reg[zz * 4 + 1], 62.0f, y, 24.0f, 0);
        Mx_TextA(84.0f, y, Co_RegionName(zz), 16.0f, 400.0f, lit ? MX_DARK : MX_WHITE, 0, a);
        if (St_Colour(zz) != 0) { /* (shaded its own way) */
            Zw_Swatch((u32)St_Colour(zz) & 0xFFFFFF, 590.0f, y, 20.0f, 0);
        }
    }
    if (sZw->shTexTop > 0) {
        Mx_Fit(ZW_UP, 325.0f, 144.0f + 3.0f * Zw_Beat(36), 22.0f, a);
    }
    if (sZw->shTexTop + 12 < sZw->coRows) {
        Mx_Fit(ZW_DOWN, 325.0f, 616.0f - 3.0f * Zw_Beat(36), 22.0f, a);
    }
    /* the texture's shaders */
    Mx_Box(ZW_WINDOW, 1000.0f, 470.0f, 1346.0f, 632.0f, 18.0f, a);
    if (z >= 0) {
        s32 own = St_Colour(z);

        Mx_Text(1016.0f, 490.0f, "ITS SHADERS", 15.0f, 0.0f, MX_GOLD, 0);
        Port_WarriorTexShade((s32)St_Hash(z), ramps);
        for (r = 0; r < ramps[0] && r < 4; r++) { /* (each ramp: lit to dark) */
            f32 y = 516.0f + (f32)r * 22.0f;
            s32 c;

            for (c = 0; c < 4; c++) {
                u32 rgb = (u32)ramps[1 + r * 4 + c];
                u32 col = Mx_Rgba((s32)(rgb & 0xFF), (s32)((rgb >> 8) & 0xFF), (s32)((rgb >> 16) & 0xFF), a);

                Mx_Rect(1016.0f + (f32)c * 78.0f, y - 8.0f, 1092.0f + (f32)c * 78.0f, y + 8.0f, col, col);
            }
        }
        if (ramps[0] == 0) {
            Mx_Text(1016.0f, 530.0f, "(seen once the warrior is made)", 14.0f, 300.0f, MX_LAV, 0);
        }
        Mx_Text(1016.0f, 612.0f, "Shadow colour", 15.0f, 0.0f, MX_LAV, 0);
        if (own != 0) { /* (its gradient) */
            char gl[40];
            s32 g = (own >> 25) & 15;

            sprintf(gl, "Gradient %d  (L2 / R2)", (int)(g == 0 ? 5 : g));
            Mx_Text(1016.0f, 588.0f, gl, 14.0f, 0.0f, MX_GOLD, 0);
        }
        if (own != 0) {
            Zw_Swatch((u32)own & 0xFFFFFF, 1180.0f, 612.0f, 22.0f, 0);
            Mx_Text(1200.0f, 612.0f, "yours", 15.0f, 0.0f, MX_WHITE, 0);
        } else {
            Mx_Text(1170.0f, 612.0f, "the game's", 15.0f, 0.0f, MX_WHITE, 0);
        }
    }
    if (sZw->dirty) {
        Mx_Bar(ZW_PILL, 1150.0f, 110.0f, 1340.0f, 146.0f, 0x80);
        Mx_Text(1245.0f, 128.0f, "NOT SAVED", 17.0f, 170.0f, MX_DARK, 1);
    }
    Zw_Prompt(15, "Texture / shadow colour");
    Zw_Prompt(0, "Any colour");
    Zw_Prompt(10, "Gradient");
    Zw_Prompt(14, "By part");
    Zw_Prompt(1, "The game's");
    Zw_Prompt(7, "Save");
    Zw_Prompt(2, "Done");
    Zw_Talk("Pick a texture: its shaders show on the right. Left and right give its shadow a colour of your own!");
}

static void St_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 z, k, now;

    if (!live || sZw->coRows <= 0) {
        return;
    }
    if (pad->gameRepeat & (PADG_UP | PADG_UP2 | PADG_DOWN | PADG_DOWN2)) {
        s32 d = pad->gameRepeat & (PADG_UP | PADG_UP2) ? -1 : 1, guard;

        for (guard = 0; guard < sZw->coRows; guard++) { /* (over the headings) */
            sZw->shTexRow = (sZw->shTexRow + d + sZw->coRows) % sZw->coRows;
            if (sZw->coList[sZw->shTexRow] >= 0) {
                break;
            }
        }
        Snd_PlaySe(1, 0);
    }
    if (sZw->shTexRow < sZw->shTexTop) {
        sZw->shTexTop = sZw->shTexRow;
    }
    if (sZw->shTexRow >= sZw->shTexTop + 12) {
        sZw->shTexTop = sZw->shTexRow - 11;
    }
    z = sZw->coList[sZw->shTexRow];
    if (z < 0) {
        return;
    }
    if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) { /* through the swatches, the game's first */
        s32 d = pad->gameRepeat & (PADG_LEFT | PADG_LEFT2) ? -1 : 1;

        now = St_Colour(z);
        for (k = 0; k < 30 && (now == 0 || (u32)(now & 0xFFFFFF) != kSwatch[k]); k++) {
        }
        k = now == 0 ? -1 : k;
        k += d;
        k = k < -1 ? 29 : k > 29 ? -1 : k;
        St_Set(z, k < 0 ? 0 : (s32)(0x1000000u | kSwatch[k]));
        Sh_Changed();
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_L2 | PADG_R2)) { /* its gradient: how soft or hard its shadow falls */
        now = St_Colour(z);
        if (now == 0) {
            Zw_Note("Give its shadow a colour first (left / right or Cross), then its gradient!");
            Snd_PlaySe(1, 2);
        } else {
            s32 g = (now >> 25) & 15;

            g = g == 0 ? 5 : g;
            g += (pad->gameRepeat & PADG_R2) ? 1 : -1;
            g = g < 1 ? 1 : g > 9 ? 9 : g;
            St_Set(z, (now & 0x1FFFFFF) | (g << 25));
            Sh_Changed();
            Snd_PlaySe(1, 0);
        }
    } else if (pad->gamePressed & PADG_CROSS) { /* any colour: the colour picker */
        now = St_Colour(z);
        sZw->shTexZ = z;
        sZw->fxTarget = 5;
        sZw->fxBack = now;
        Co_ToHsv(now != 0 ? (u32)now & 0xFFFFFF : 0x6A3A50, &sZw->pkH, &sZw->pkS, &sZw->pkV);
        sZw->pkRow = 0;
        sZw->pkSwatch = 0;
        Zw_SetMode(ZW_FXCOL);
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_SQUARE) {
        St_Set(z, 0);
        Sh_Changed();
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & PADG_START) {
        sZw->saving = 2;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        sZw->shByTex = 0;
        Zw_SetMode(ZW_EDIT);
        Snd_PlaySe(1, 2);
    }
}

static void Sh_Draw(void) {
    f32 pop = Zw_In(sZw->modeT, 8);
    s32 a = (s32)(128.0f * pop), i, r, top = sZw->shRow > 10 ? sZw->shRow - 10 : 0, e;
    char line[64], name[48];

    if (sZw->shByTex) {
        St_Draw();
        return;
    }
    Sh_Load();
    Zw_Host(1250.0f, 330.0f, 300.0f, 1);
    Zw_EditStage(900.0f, 540.0f, 400.0f);
    Ws_ShapeTick(); /* (the part under the cursor blinks on the model) */
    Zw_Title("SHADING");
    Mx_Box(ZW_WINDOW, 30.0f, 122.0f, 620.0f, 628.0f, 22.0f, 0x80);
    for (i = top; i < sShRows && i < top + 13; i++) { /* the parts */
        f32 y = 156.0f + (f32)(i - top) * 36.0f;
        s32 lit = i == sZw->shRow;

        e = sShList[i][0];
        if (lit && sZw->shFocus == 0) {
            Zw_SelBar(40.0f, y - 16.0f, 262.0f, y + 16.0f);
        } else if (lit) {
            Mx_Frame(40.0f, y - 16.0f, 262.0f, y + 16.0f, 2.0f, Mx_Rgba(250, 205, 90, 0x80));
        }
        if (e == SH_ALL) {
            Ps_Copy(name, "All parts", sizeof(name));
        } else {
            Sh_Name(e, name);
        }
        Mx_TextA(52.0f, y, name, 16.0f, 150.0f, lit && sZw->shFocus == 0 ? MX_DARK : e == SH_ALL ? MX_GOLD : MX_WHITE, 0, a);
        if (Sh_Style(e) == 7) { /* (its own colour: a swatch) */
            Zw_Swatch((u32)Sh_Pick(e, 3) & 0xFFFFFF, 246.0f, y, 18.0f, 0);
        } else if (Sh_Style(e) == 8) {
            u32 dot = Mx_Rgba(0x90, 0xD0, 0xFF, 0x80);

            Mx_Disc(246.0f, y, 9.0f, dot, dot);
        }
    }
    e = sShRows > 0 ? sShList[sZw->shRow][0] : SH_ALL;
    if (e == SH_ALL) {
        Ps_Copy(name, "All parts", sizeof(name));
    } else {
        Sh_Name(e, name);
    }
    Mx_TextA(440.0f, 156.0f, name, 19.0f, 330.0f, MX_GOLD, 1, a);
    for (r = 0; r < SH2_ROWS; r++) { /* how */
        f32 y = 156.0f + (f32)(r + 1) * 46.0f;
        s32 lit = sZw->shFocus == 1 && r == sZw->shSet;

        if (r == SH2_SAME && e == SH_ALL) {
            continue;
        }
        if (lit) {
            Zw_SelBar(272.0f, y - 18.0f, 608.0f, y + 18.0f);
        }
        if (r == SH2_COLOUR) {
            Mx_TextA(284.0f, y, "Shadow colour", 16.0f, 130.0f, lit ? MX_DARK : MX_LAV, 0, a);
            if (Sh_Style(e) == 7) {
                Zw_Swatch((u32)Sh_Pick(e, 3) & 0xFFFFFF, 440.0f, y, 26.0f, 0);
                Mx_TextA(462.0f, y, "your colour", 16.0f, 140.0f, lit ? MX_DARK : MX_WHITE, 0, a);
            } else if (Sh_Style(e) == 8) {
                Mx_TextA(422.0f, y, "as the part it copies", 16.0f, 180.0f, lit ? MX_DARK : MX_DIM, 0, a);
            } else {
                Mx_TextA(422.0f, y, e == SH_ALL ? "each part the game's" : "the game's", 16.0f, 180.0f, lit ? MX_DARK : MX_WHITE, 0, a);
            }
        } else if (r == SH2_SAME) {
            s32 src = Sh_Same(e);

            Mx_TextA(284.0f, y, "Same shading as", 16.0f, 130.0f, lit ? MX_DARK : MX_LAV, 0, a);
            if (src >= 0 && src != SH_ALL) {
                Sh_Name(src, line);
            } else {
                Ps_Copy(line, "no: its own", sizeof(line));
            }
            Mx_TextA(422.0f, y, line, 16.0f, 180.0f, lit ? MX_DARK : MX_WHITE, 0, a);
        } else if (r == SH2_GLOW) {
            Mx_TextA(284.0f, y, "Glow", 16.0f, 130.0f, lit ? MX_DARK : MX_LAV, 0, a);
            if (Sh_Glow(e) == 0 && e == SH_ALL) {
                Ps_Copy(line, "each its own", sizeof(line));
            } else {
                Ps_Copy(line, kShGlow[Sh_Glow(e)], sizeof(line));
            }
            Mx_TextA(422.0f, y, line, 16.0f, 180.0f, lit ? MX_DARK : MX_WHITE, 0, a);
        } else if (e != SH_ALL) {
            Mx_TextA(284.0f, y, "Outline", 16.0f, 130.0f, lit ? MX_DARK : MX_LAV, 0, a);
            if (Sh_Line(e) >= 2) {
                sprintf(line, "group %d", (int)(Sh_Line(e) - 1));
            } else if (Sh_Line(e) == 1) {
                Ps_Copy(line, "its own", sizeof(line));
            } else {
                Ps_Copy(line, "with the body", sizeof(line));
            }
            Mx_TextA(422.0f, y, line, 16.0f, 180.0f, lit ? MX_DARK : MX_WHITE, 0, a);
        }
    }
    if (sZw->shFocus == 0) {
        Zw_Prompt(15, "Part");
        Zw_Prompt(0, "Change it");
        Zw_Prompt(1, "As it was");
        Zw_Prompt(2, "Done");
        Zw_Talk("Pick a part, then give its shadow a colour of your own, or the same shading as another part!");
    } else {
        Zw_Prompt(15, "Choose");
        if (sZw->shSet == SH2_COLOUR) {
            Zw_Prompt(0, "Pick the colour");
        }
        Zw_Prompt(1, "As it was");
        Zw_Prompt(2, "Back");
        Zw_Talk(sZw->shSet == SH2_COLOUR ? "Cross: the colour the shadow shows. It falls where the game puts it."
                : sZw->shSet == SH2_SAME ? "Left and right: shaded just like another part. All the way round: its own again."
                : sZw->shSet == SH2_GLOW ? "The glow of a transformed warrior: none, soft, bright, blazing."
                                         : "Parts in one group are one surface; a line is drawn where groups meet.");
    }
}

static void Sh_Changed(void) {
    sZw->dirty = 1;
    sZw->previewDue = ZW_PREVIEW_REST;
}

static void Sh_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 e, i, last;

    if (live && (pad->gamePressed & (PADG_L1 | PADG_R1)) && (sZw->shByTex || sZw->shFocus == 0)) { /* by part / by texture */
        sZw->shByTex = !sZw->shByTex;
        if (sZw->shByTex) {
            Co_Load();
            sZw->shTexRow = sZw->coRows > 1 ? 1 : 0; /* (past the first heading) */
            sZw->shTexTop = 0;
        }
        Snd_PlaySe(1, 0);
        return;
    }
    if (Zw_SaveTick()) {
        return;
    }
    if (sZw->shByTex) {
        St_Update(live);
        return;
    }
    Sh_Load();
    Sh_Clean();
    if (!live || sShRows <= 0) {
        return;
    }
    e = sShList[sZw->shRow][0];
    last = e == SH_ALL ? SH2_GLOW : SH2_LINE;
    if (sZw->shFocus == 0) {
        if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            sZw->shRow = (sZw->shRow + sShRows - 1) % sShRows;
            Snd_PlaySe(1, 0);
        } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            sZw->shRow = (sZw->shRow + 1) % sShRows;
            Snd_PlaySe(1, 0);
        } else if (pad->gamePressed & (PADG_CROSS | PADG_RIGHT | PADG_RIGHT2)) {
            sZw->shFocus = 1;
            sZw->shSet = SH2_COLOUR;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_SQUARE) { /* this part's shading all as it was */
            i = Sh_Edit(e, 0);
            if (i >= 0) {
                memmove(sZw->w.edit[i], sZw->w.edit[i + 1], sizeof(sZw->w.edit[0]) * (u32)(sZw->w.edits - i - 1));
                sZw->w.edits--;
                Sh_Changed();
            }
            Zw_Note("Shaded as the game has it again.");
            Snd_PlaySe(1, 2);
        } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
            Zw_SetMode(ZW_EDIT);
            Snd_PlaySe(1, 2);
        }
        return;
    }
    if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        do {
            sZw->shSet = sZw->shSet > 0 ? sZw->shSet - 1 : last;
        } while (e == SH_ALL && sZw->shSet == SH2_SAME);
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        do {
            sZw->shSet = sZw->shSet < last ? sZw->shSet + 1 : 0;
        } while (e == SH_ALL && sZw->shSet == SH2_SAME);
        Snd_PlaySe(1, 0);
    }
    if (sZw->shSet > last) {
        sZw->shSet = SH2_COLOUR;
    }
    if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2)) {
        s32 step = (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) ? 1 : -1;

        if (sZw->shSet == SH2_SAME) { /* the parts round, then its own */
            s32 at = -1, k, n = sShRows, src = Sh_Same(e);

            for (k = 0; k < n; k++) {
                at = sShList[k][0] == src ? k : at;
            }
            for (k = 0; k < n + 1; k++) {
                at = at + step;
                at = at < -1 ? n - 1 : at >= n ? -1 : at;
                if (at < 0 || (sShList[at][0] != e && sShList[at][0] != SH_ALL && Sh_Style(sShList[at][0]) != 8)) {
                    break;
                }
            }
            Sh_SetMode(e, at < 0 ? 0 : 8, at < 0 ? 0 : sShList[at][0]);
            Sh_Changed();
        } else if (sZw->shSet == SH2_GLOW && (i = Sh_Edit(e, 1)) >= 0) {
            sZw->w.edit[i][2] = (sZw->w.edit[i][2] & ~0xFF) | (Sh_Glow(e) + step + 5) % 5;
            Sh_Changed();
        } else if (sZw->shSet == SH2_LINE && (i = Sh_Edit(e, 1)) >= 0) {
            sZw->w.edit[i][2] = (sZw->w.edit[i][2] & ~0xFF00) | ((Sh_Line(e) + step + 18) % 18) << 8;
            Sh_Changed();
        }
        Snd_PlaySe(1, 0);
    }
    if ((pad->gamePressed & PADG_CROSS) && sZw->shSet == SH2_COLOUR) { /* the shadow's colour */
        s32 v = Sh_Style(e) == 7 ? Sh_Pick(e, 3) : 0;

        sZw->shE = e;
        sZw->shT = 3; /* (word 7; Fx_Set makes it this part's colour) */
        sZw->fxTarget = 4;
        sZw->fxBack = v;
        Co_ToHsv(v >= 0x1000000 ? (u32)v & 0xFFFFFF : 0x6A3A50, &sZw->pkH, &sZw->pkS, &sZw->pkV);
        sZw->pkRow = 0;
        sZw->pkSwatch = 0;
        Zw_SetMode(ZW_FXCOL);
        Snd_PlaySe(1, 1);
        return;
    }
    if ((pad->gamePressed & PADG_SQUARE) && (i = Sh_Edit(e, 0)) >= 0) { /* this one as it was */
        if (sZw->shSet == SH2_COLOUR || sZw->shSet == SH2_SAME) {
            Sh_SetMode(e, 0, 0);
        } else if (sZw->shSet == SH2_GLOW) {
            sZw->w.edit[i][2] &= ~0xFF;
        } else {
            sZw->w.edit[i][2] &= ~0xFF00;
        }
        Sh_Changed();
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
        sZw->shFocus = 0;
        Snd_PlaySe(1, 2);
    }
}

/* ---------------------------------------------------------------------------------------------- a technique's or
 * the aura's colour: any colour, from hue / strength / lightness or the swatches (Select on its row). */
enum { FX_HUE, FX_SAT, FX_VAL, FX_SW1, FX_SW2, FX_OWN, FX_ROWS };

static void Fx_Set(s32 c) {
    if (sZw->fxTarget >= 9 && sZw->fxTarget <= 13) { /* (a technique's body glow) */
        sZw->w.glow[sZw->fxTarget - 9] = c;
        sZw->dirty = 1;
        return;
    }
    if (sZw->fxTarget == 8) { /* (the paint screen's new colour: that slot of the texture's colours) */
        if (sTxNewSlot >= 0 && sTxNewSlot < 16 && sTxN > 0) {
            u32 rgb = c > 0 ? (u32)c & 0xFFFFFF : sZw->picPal[16 + sTxNewSlot] & 0xFFFFFF;

            sZw->picPal[sTxNewSlot] = (sZw->picPal[sTxNewSlot] & 0xFF000000u) | rgb;
            if (Port_PaintSave(sTxKey[sTxCur], sZw->pic, sZw->picW, sZw->picH, sZw->picPal)) {
                sZw->picDirty = 0;
                sZw->dirty = 1;
                sZw->paintDue = ZW_PAINT_REST;
            }
            sZw->picGen++;
        }
        return;
    }
    if (sZw->fxTarget >= 6) { /* (a Blast 1 skill's effects) */
        sZw->w.skillColour[sZw->fxTarget - 6] = c;
    } else if (sZw->fxTarget == 5) { /* (a texture's shadow colour: the shading screen by texture) */
        St_Set(sZw->shTexZ, c);
        Sh_Changed();
    } else if (sZw->fxTarget == 4) { /* (a shade coloured by hand: 0 back to the shade it takes) */
        s32 i = Sh_Edit(sZw->shE, 1);

        if (i >= 0) {
            sZw->w.edit[i][4 + sZw->shT] = c;
            if (sZw->shT == 3) { /* (the shading screen's shadow colour: the part shaded in it; none: the game's again) */
                sZw->w.edit[i][2] = (sZw->w.edit[i][2] & 0xFFFF) | ((c != 0 ? 7 : 0) << 16);
                sZw->w.edit[i][6] = 0;
            }
        }
        sZw->previewDue = ZW_PREVIEW_REST;
    } else if (sZw->fxTarget == 3) {
        sZw->w.auraColour = c;
    } else {
        sZw->w.techColour[sZw->fxTarget] = c;
    }
    sZw->dirty = 1; /* (the effects, not the model: no new preview) */
}

static void Fx_Draw(void) {
    static char *labels[FX_ROWS] = {"Hue", "Strength", "Light", "", "", "Its own colours"};
    f32 pop = Zw_In(sZw->modeT, 8);
    s32 now = sZw->fxTarget >= 9 && sZw->fxTarget <= 13 ? sZw->w.glow[sZw->fxTarget - 9] : sZw->fxTarget == 8 ? (s32)(0x1000000 | (sZw->picPal[sTxNewSlot & 15] & 0xFFFFFF)) : sZw->fxTarget >= 6 ? sZw->w.skillColour[sZw->fxTarget - 6] : sZw->fxTarget == 5 ? St_Colour(sZw->shTexZ) : sZw->fxTarget == 4 ? Sh_Pick(sZw->shE, sZw->shT) : sZw->fxTarget == 3 ? sZw->w.auraColour : sZw->w.techColour[sZw->fxTarget];
    s32 i, j;
    char title[64];

    Zw_Host(1250.0f, 330.0f, 300.0f, 1);
    Zw_EditStage(900.0f, 540.0f, 400.0f);
    Zw_Title(sZw->fxTarget >= 9 && sZw->fxTarget <= 13 ? "BODY GLOW" : sZw->fxTarget == 8 ? "NEW COLOUR" : sZw->fxTarget >= 6 ? "BLAST 1 COLOUR" : sZw->fxTarget >= 4 ? "SHADE COLOUR" : sZw->fxTarget == 3 ? "AURA COLOUR" : "TECHNIQUE COLOUR");
    Mx_Box(ZW_WINDOW, 30.0f, 122.0f, 620.0f, 628.0f, 22.0f, 0x80);
    if (sZw->fxTarget >= 9 && sZw->fxTarget <= 13) {
        Ps_Copy(title, "Glow while doing it (brighter: stronger)", sizeof(title));
    } else if (sZw->fxTarget == 8) {
        Ps_Copy(title, "A colour for this texture", sizeof(title));
    } else if (sZw->fxTarget >= 6) {
        Ps_Copy(title, Zw_MoveName(&sZw->w, ROW_SKILL0 + sZw->fxTarget - 6), sizeof(title));
    } else if (sZw->fxTarget == 5) {
        Ps_Copy(title, Co_RegionName(sZw->shTexZ), sizeof(title));
    } else if (sZw->fxTarget == 4) { /* (copied, not picked with ?: from a table: a 4-byte pointer read as 8 there) */
        if (sZw->shE == SH_ALL) {
            Ps_Copy(title, "Whole warrior: shadow colour", sizeof(title));
        } else {
            Ps_Copy(title, kShPart[(u32)sZw->shE / 2 % 12], sizeof(title));
        }
    } else if (sZw->fxTarget == 3) {
        Ps_Copy(title, "Aura, lightning and charge", sizeof(title));
    } else {
        Ps_Copy(title, Zw_MoveName(&sZw->w, ROW_TECH0 + sZw->fxTarget), sizeof(title));
    }
    Mx_TextA(325.0f, 150.0f, title, 22.0f, 540.0f, MX_GOLD, 1, (s32)(128.0f * pop));
    if (now > 0) {
        Zw_Swatch((u32)now & 0xFFFFFF, 90.0f, 208.0f, 64.0f * pop, 1);
        Mx_TextA(140.0f, 208.0f, "Your colour", 18.0f, 200.0f, MX_LAV, 0, (s32)(128.0f * pop));
    } else {
        Mx_TextA(90.0f, 208.0f, "Its own colours", 20.0f, 300.0f, MX_LAV, 0, (s32)(128.0f * pop));
    }
    for (i = 0; i < FX_ROWS; i++) {
        f32 y = 266.0f + (f32)i * 50.0f;
        s32 lit = i == sZw->pkRow;

        if (labels[i][0]) {
            Mx_TextA(50.0f, y, labels[i], 18.0f, i == FX_OWN ? 300.0f : 110.0f, lit ? MX_GOLD : MX_LAV, 0, (s32)(128.0f * pop));
        }
        if (i <= FX_VAL) {
            Co_Bar(170.0f, y, 410.0f, i, i == FX_HUE ? sZw->pkH : i == FX_SAT ? sZw->pkS : sZw->pkV, lit);
        } else if (i == FX_SW1 || i == FX_SW2) {
            for (j = 0; j < 15; j++) {
                Zw_Swatch(kSwatch[(i - FX_SW1) * 15 + j], 70.0f + (f32)j * 36.0f, y, lit && j == sZw->pkSwatch ? 32.0f : 26.0f, lit && j == sZw->pkSwatch);
            }
        } else if (lit) {
            Zw_SelBar(42.0f, y - 18.0f, 608.0f, y + 18.0f);
            Mx_TextA(50.0f, y, labels[i], 18.0f, 300.0f, MX_DARK, 0, 0x80);
        }
    }
    Zw_Prompt(15, "Choose");
    Zw_Prompt(0, "OK");
    Zw_Prompt(1, "Its own");
    Zw_Prompt(2, "Back");
    Zw_Talk(sZw->fxTarget == 3 ? "Any colour you like for the aura! The lightning and the charge glow take it too."
                               : "Any colour you like! Fire it in a fight to see it.");
}

static void Fx_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 hl = pad->gameHeld & (PADG_LEFT | PADG_LEFT2), hr = pad->gameHeld & (PADG_RIGHT | PADG_RIGHT2);
    s32 left = pad->gameRepeat & (PADG_LEFT | PADG_LEFT2), right = pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2);

    if (!live) {
        return;
    }
    if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sZw->pkRow = (sZw->pkRow + FX_ROWS - 1) % FX_ROWS;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sZw->pkRow = (sZw->pkRow + 1) % FX_ROWS;
        Snd_PlaySe(1, 0);
    }
    if (sZw->pkRow <= FX_VAL && (hl || hr)) { /* sliders: held, they glide */
        f32 *v = sZw->pkRow == FX_HUE ? &sZw->pkH : sZw->pkRow == FX_SAT ? &sZw->pkS : &sZw->pkV;

        *v += (hl ? -1.0f : 1.0f) * (sZw->pkRow == FX_HUE ? 0.006f : 0.008f);
        if (sZw->pkRow == FX_HUE) {
            *v = *v < 0.0f ? *v + 1.0f : *v >= 1.0f ? *v - 1.0f : *v;
        } else {
            *v = *v < 0.0f ? 0.0f : *v > 1.0f ? 1.0f : *v;
        }
        Fx_Set((s32)(0x1000000 | Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV)));
    } else if ((sZw->pkRow == FX_SW1 || sZw->pkRow == FX_SW2) && (left || right)) {
        sZw->pkSwatch = (sZw->pkSwatch + (left ? 14 : 1)) % 15;
        Co_ToHsv(kSwatch[(sZw->pkRow - FX_SW1) * 15 + sZw->pkSwatch], &sZw->pkH, &sZw->pkS, &sZw->pkV);
        Fx_Set((s32)(0x1000000 | Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV)));
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & PADG_CROSS) {
        if (sZw->pkRow == FX_OWN) {
            Fx_Set(0);
        }
        Zw_SetMode(sZw->fxTarget == 8 ? ZW_TEXTURE : sZw->fxTarget == 4 || sZw->fxTarget == 5 ? ZW_SHADEPK : ZW_EDIT);
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_SQUARE) {
        Fx_Set(0);
        Zw_Note("Its own colours again!");
        Snd_PlaySe(1, 2);
    } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) { /* as it was */
        Fx_Set(sZw->fxBack);
        Zw_SetMode(sZw->fxTarget == 8 ? ZW_TEXTURE : sZw->fxTarget == 4 || sZw->fxTarget == 5 ? ZW_SHADEPK : ZW_EDIT);
        Snd_PlaySe(1, 2);
    }
}

/* A test without hands (BT3_ZW_AUTO=<rrggbb>): the first warrior opened, its colours
   screen, its eyebrows painted that colour; the log says what was done (the screenshot is the caller's:
   BT3_CAPTURE_TEST). */
extern char *getenv(const char *name);
extern void Port_ZwLogLine(char *line); /* (plat_mods.c: a line in the log) */

static void Zw_Auto(void) {
    static s32 step = -1, wait, gap = 90;
    static u32 rgb;
    char *e = getenv("BT3_ZW_AUTO");
    char line[160];
    s32 k;

    if (e == NULL) {
        return;
    }
    if (step < 0) {
        u32 v = 0;
        s32 i;

        for (i = 0; i < 6 && e[i] != 0; i++) { /* (rrggbb) */
            char c = e[i];

            v = v * 16 + (u32)(c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0);
        }
        rgb = ((v >> 16) & 0xFF) | (v & 0xFF00) | ((v & 0xFF) << 16);
        step = 0;
        wait = gap;
    }
    if (step >= 3) {
        sZw->rest = 0; /* (as if buttons were being pressed all the while: the model must still be read again) */
    }
    if (--wait > 0) {
        return;
    }
    wait = gap;
    switch (step++) {
    case 0:
        if (sZw->count > 0) {
            Zw_Edit(0);
            sprintf(line, "zw auto: editing warrior 1 (%s)", sZw->w.name);
        } else {
            sprintf(line, "zw auto: no warrior");
        }
        Port_ZwLogLine(line);
        break;
    case 1:
        sZw->coCur = 0;
        sZw->coTop = 0;
        Co_Load();
        sZw->zoneY = CO_Y;
        sZw->colourStage = 0;
        Zw_SetMode(ZW_COLOUR);
        sprintf(line, "zw auto: colours (%d regions)", (int)sZw->regs);
        Port_ZwLogLine(line);
        break;
    case 2: /* the face texture's most yellow colour (Goku's eyebrows) painted with the eyedropper */
        for (k = 0; k < sZw->regs && ((sZw->reg[k * 4 + 2] >> 24) & 0xFF) != 1; k++) {
        }
        if (k < sZw->regs) {
            s32 e, best = 0, score = -1000;

            Co_PicLoad(k);
            for (e = 0; e < 16; e++) {
                u32 v = sZw->picPal[16 + e];
                s32 r = (s32)(v & 0xFF), g = (s32)((v >> 8) & 0xFF), b = (s32)((v >> 16) & 0xFF), sc = r + g - 2 * b;

                if (sc > score) {
                    score = sc;
                    best = e;
                }
            }
            Co_Set(Co_PicTarget(k, best), rgb);
            sZw->colourStage = 3;
            sprintf(line, "zw auto: face texture (row %d) colour %d (was %06x) painted %06x", (int)k, (int)best, (unsigned)(sZw->picPal[16 + best] & 0xFFFFFF),
                    (unsigned)rgb);
        } else {
            sprintf(line, "zw auto: no face texture");
        }
        Port_ZwLogLine(line);
        break;
    default:
        break;
    }
}

s32 PortZw_Run(void) {
    Zw_Init();
    Port_EscQuit(0); /* Esc is "back" in here */
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        s32 live;

        Job_Run();
        Gfx_BeginFrame();
        Pad_Update();
        if (Port_EscTake() && sZw->mode != ZW_NAME) {
            gPad[0].gamePressed |= PADG_TRIANGLE;
        }
        Snd_Update();
        ColorFade_Update();
        live = sZw->leaving != 1 && ColorFade_IsInDone();
        Mm_PromptReset();
        sPromptCount = 0;
        if (live) {
            Zw_Auto();
        }
        if (live) {
            Zw_Touch(); /* (the taps become presses, before whatever reads them) */
        }
        if (sZw->ask != ASK_NONE) {
            if (live) {
                Ask_Update();
            }
        } else {
            if (live) {
                Zw_History();
            }
            if (sZw->mode != ZW_POSE) { /* (the stance maker's live pose only while it is open) */
                gPortLivePoseObj = NULL;
            }
            gPortFreezeObj = (sZw->mode == ZW_POSE && sPoSimple) || (sZw->mode == ZW_PHOTO && (sZw->phShot > 0 || sZw->phHold)) ? Mm_ViewObjPtr() : NULL; /* (held still while
                                                                                                                          dots are dragged, while photographed) */
            switch (sZw->mode) {
            case ZW_LIBRARY: Lib_Update(live); break;
            case ZW_EDIT: Ed_Update(live); break;
            case ZW_FXCOL: Fx_Update(live); break;
            case ZW_SHADEPK: Sh_Update(live); break;
            case ZW_TEXTURE: Tx_Update(live); break;
            case ZW_POSE: Po_Update(live); break;
            case ZW_PICK: Pk_Update(live); break;
            case ZW_LAB: Lb_Update(live); break;
            case ZW_NAME: Kb_Update(live); break;
            case ZW_CROP: Cr_Update(live); break;
            case ZW_COLOUR: Co_Update(live); break;
            case ZW_WORK: Ws_Update(live); break;
            case ZW_PHOTO: Ph_Update(live); break;
            case ZW_WSHOP: Wk_Update(live); break;
            case ZW_WSDETAIL: Wk_DetailUpdate(live); break;
            case ZW_WSUPLOAD: Wk_UploadUpdate(live); break;
            }
        }
        if (sZw->mode == ZW_EDIT || sZw->mode == ZW_COLOUR || sZw->mode == ZW_WORK || sZw->mode == ZW_FXCOL || sZw->mode == ZW_SHADEPK ||
            sZw->mode == ZW_TEXTURE || sZw->mode == ZW_POSE) {
            Zw_PreviewTick();
        }
        if (sZw->dirty && sZw->mode != ZW_LIBRARY && --sZw->draftT <= 0) { /* (the unsaved warrior, kept) */
            sZw->draftT = ZW_DRAFT_EVERY;
            Port_WarriorDraft(&sZw->w, 0);
        }
        if (sZw->ask == ASK_NONE && (sZw->mode == ZW_LIBRARY || sZw->mode == ZW_EDIT || sZw->mode == ZW_COLOUR || sZw->mode == ZW_WORK ||
                                     sZw->mode == ZW_POSE)) {
            Zw_CameraInput(live);
        }
        Mx_Begin();
        gMxTapCount = 0;
        Mx_Pic(ZW_BG, 0.0f, 0.0f, MX_W, MX_H, 0x80);
        switch (sZw->mode) {
        case ZW_LIBRARY: Lib_Draw(); break;
        case ZW_EDIT: Ed_Draw(); break;
        case ZW_FXCOL: Fx_Draw(); break;
        case ZW_SHADEPK: Sh_Draw(); break;
        case ZW_TEXTURE: Tx_Draw(); break;
        case ZW_POSE: Po_Draw(); break;
        case ZW_PICK:
            sZwEdBehind = 1;
            Ed_Draw(); /* (the editor behind it: its model, not its dialog box, prompts or info window) */
            sZwEdBehind = 0;
            sPromptCount = 0;
            Pk_Draw();
            break;
        case ZW_NAME: Kb_Draw(); break;
        case ZW_LAB: Lb_Draw(); break;
        case ZW_CROP: Cr_Draw(); break;
        case ZW_COLOUR: Co_Draw(); break;
        case ZW_WORK: Ws_Draw(); break;
        case ZW_PHOTO: Ph_Draw(); break;
        case ZW_WSHOP: Wk_Draw(); break;
        case ZW_WSDETAIL: Wk_DetailDraw(); break;
        case ZW_WSUPLOAD: Wk_UploadDraw(); break;
        }
        if (sZw->ask != ASK_NONE) {
            gMxTapCount = 0; /* (only the question answers) */
            Ask_Draw();
        }
        ColorFade_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
        sZw->frames++;
        sZw->modeT++;
        sZw->tabT++;
        if (sZw->rest < 1000) {
            sZw->rest++;
        }
        if (sZw->noteT > 0) {
            sZw->noteT--;
        }
        if (sZw->leaving == 1 && ColorFade_IsOutDone()) {
            break;
        }
    }
    sceGsSyncPath(0, 0);
    Port_EscQuit(1);
    {
        s32 to = sZw->exitTo, mod = sZw->w.mod;

        if (to == 2) {
            gPortZwSelPreview = Zw_SkinOf(mod) + 1;
        }
        Zw_Term();
        Dma_ResetBuffers();
        if (to == 1) { /* a training fight, as the Mod Manager's test: the warrior against the manager's opponent */
            static u8 noItems[0x10];
            extern s32 gPortModTest;
            extern s32 gMmTestFoe;
            extern void Battle_ClearWork(void);
            extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
            extern void BattleSetup_SetSide(s32 sideNo, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, void *a7);
            extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health, void *items);
            extern void BattleSetup_Finish(void);
            extern s32 Port_ModTestStage(void);
            PortModInfo info;

            Port_ModGetInfo(mod, &info);
            gPortModTest = 2; /* (its pause menu goes back to Z Warriors) */
            Battle_ClearWork();
            BattleSetup_SetRule(0, 6, 8, 0, 1, Port_ModTestStage(), 0);
            BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, 1, 1, 1, 0, NULL);
            BattleSetup_SetMember(0, 0, info.chara, info.isNew ? 8 + info.skin * 4 : 0, 0, -1, 100.0f, noItems);
            BattleSetup_SetMember(1, 0, gMmTestFoe, 0, 0, -1, 100.0f, noItems);
            BattleSetup_Finish();
        }
        return to;
    }
}
#endif
