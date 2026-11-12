#include "common.h"
#include "menu/menu_k.h"
#include "sys/pad.h"

/*
 * Bracket, 0x3660A0..0x366F58: the guide's speech and the result sequence of the bracket screen. A source file of
 * its own: "fl_guide_out" exists here and in Bracket_Update, "fl_guide_in" here and in Bracket_Load.
 */

#define BRK_SAY(b) Voice_PlayWithSubtitle((b)->subtitles, TOUR_VOICE_BASE, (b)->voiceLine)

/* Confirm skips the speech before a battle once the two large pictures are loaded (BRK_IMAGES). */
#define BRK_SKIP_INTRO(b) \
    if ((gPad[0].gamePressed & 0x200) && ((b)->flags & BRK_IMAGES)) { \
        (b)->seq = 0xB; \
        Snd_PlaySe(1, 1); \
    }

/*
 * Picks the guide's pictures for subtitle line `line`: the Cell Games and Big Tournament guides have two picture
 * sets (pose), the Yamcha Game has two guides (talker) of which the second has two sets.
 */
void Bracket_SetTalker(Bracket *b, s32 line) {
    switch (line) {
    /* Cell Games */
    case 0x4D:
    case 0x4E:
    case 0x4F:
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x56:
    case 0x57:
    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B:
    case 0x5C:
    case 0x5F:
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B:
    case 0x6C:
    case 0x6D:
    case 0x6E:
    case 0x6F:
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77:
    case 0x78:
    case 0x79:
        b->pose = 0;
        b->texGuide[0] = MTEX(b->guideTex[0], 0);
        b->texGuide[2] = MTEX(b->guideTex[0], 1);
        b->texGuide[1] = MTEX(b->guideTex[0], 3);
        b->texGuide[3] = NULL;
        b->texGuide[5] = NULL;
        b->texGuide[4] = NULL;
        break;
    case 0x55:
    case 0x5D:
    case 0x5E:
        b->pose = 1;
        b->texGuide[0] = NULL;
        b->texGuide[2] = NULL;
        b->texGuide[1] = NULL;
        b->texGuide[3] = MTEX(b->guideTex[1], 0);
        b->texGuide[5] = MTEX(b->guideTex[1], 1);
        b->texGuide[4] = MTEX(b->guideTex[1], 3);
        break;
    /* Big Tournament */
    case 0x7D:
    case 0x91:
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x96:
    case 0x97:
    case 0x98:
    case 0x9B:
    case 0x9D:
    case 0x9E:
        b->pose = 0;
        b->texGuide[0] = MTEX(b->guideTex[0], 0);
        b->texGuide[2] = MTEX(b->guideTex[0], 1);
        b->texGuide[1] = MTEX(b->guideTex[0], 3);
        b->texGuide[3] = NULL;
        b->texGuide[5] = NULL;
        b->texGuide[4] = NULL;
        break;
    case 0x99:
    case 0x9A:
    case 0x9C:
        b->pose = 1;
        b->texGuide[0] = NULL;
        b->texGuide[2] = NULL;
        b->texGuide[1] = NULL;
        b->texGuide[3] = MTEX(b->guideTex[1], 0);
        b->texGuide[5] = MTEX(b->guideTex[1], 1);
        b->texGuide[4] = MTEX(b->guideTex[1], 3);
        break;
    /* Yamcha Game */
    case 0xD1:
    case 0xD7:
        b->talker = 0;
        break;
    case 0xCD:
    case 0xCE:
    case 0xCF:
    case 0xD2:
    case 0xD3:
    case 0xD5:
    case 0xD6:
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
    case 0xDC:
    case 0xDD:
    case 0xDE:
    case 0xDF:
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE5:
    case 0xE6:
        b->pose = 0;
        b->talker = 1;
        b->texGuide[0] = MTEX(b->guideTex[0], 0);
        b->texGuide[2] = MTEX(b->guideTex[0], 1);
        b->texGuide[1] = MTEX(b->guideTex[0], 3);
        b->texGuide[3] = NULL;
        b->texGuide[5] = NULL;
        b->texGuide[4] = NULL;
        break;
    case 0xD0:
    case 0xD4:
    case 0xF5:
        b->talker = 1;
        b->pose = 1;
        b->texGuide[0] = NULL;
        b->texGuide[2] = NULL;
        b->texGuide[1] = NULL;
        b->texGuide[3] = MTEX(b->guideTex[1], 0);
        b->texGuide[5] = MTEX(b->guideTex[1], 1);
        b->texGuide[4] = MTEX(b->guideTex[1], 3);
        break;
    }
}

/* Steps the guide's speech and the result sequences: `seq` is the step, 0 = idle. */
void Bracket_UpdateSeq(Bracket *b) {
    if (b->seq == 0) {
        return;
    }
    switch (b->seq) {
    /* 1..0xB: before a battle, in front of the versus panel */
    case 1:
        b->flags |= BRK_NO_TEXT;
        if (gProgress->flags & MPROG_TOUR_GREETED) {
            b->seq = 9;
            break;
        }
        switch (TOUR_PROG2->tour) {
        case TOUR_WORLD:
            b->voiceLine = b->round * 2 + 0x32;
            break;
        case TOUR_BIG:
            b->voiceLine = b->round * 2 + 0x7E;
            break;
        case TOUR_CELL:
            b->voiceLine = b->round * 2 + 0x5F;
            break;
        case TOUR_OTHERWORLD:
            b->voiceLine = b->round * 2 + 0xAF;
            break;
        case TOUR_YAMCHA:
            b->voiceLine = b->round * 2 + 0xDB;
            break;
        }
        BRK_SAY(b);
        b->seq++;
        Bracket_SetTalker(b, b->voiceLine);
        BRK_SKIP_INTRO(b);
        break;
    case 2:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            b->seq++;
        } else BRK_SKIP_INTRO(b);
        break;
    case 3:
        b->voiceLine++;
        BRK_SAY(b);
        Bracket_SetTalker(b, b->voiceLine);
        b->seq++;
        BRK_SKIP_INTRO(b);
        break;
    case 4:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            if (b->round == 4) {
                b->seq++;
            } else {
                b->seq += 5;
            }
        } else BRK_SKIP_INTRO(b);
        break;
    case 5:
        b->voiceLine++;
        BRK_SAY(b);
        Bracket_SetTalker(b, b->voiceLine);
        b->seq++;
        BRK_SKIP_INTRO(b);
        break;
    case 6:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            b->seq++;
        } else BRK_SKIP_INTRO(b);
        break;
    case 7:
        switch (TOUR_PROG2->tour) {
        case TOUR_WORLD:
            b->voiceLine = 0x3D;
            break;
        case TOUR_BIG:
            b->voiceLine = 0x89;
            break;
        case TOUR_CELL:
            b->voiceLine = 0x6A;
            break;
        case TOUR_OTHERWORLD:
            b->voiceLine = 0xBA;
            break;
        case TOUR_YAMCHA:
            b->voiceLine = 0xE6;
            break;
        }
        BRK_SAY(b);
        Bracket_SetTalker(b, b->voiceLine);
        b->seq++;
        BRK_SKIP_INTRO(b);
        break;
    case 8:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            b->seq++;
        } else BRK_SKIP_INTRO(b);
        break;
    case 9:
        /* a random cheer */
        switch (TOUR_PROG2->tour) {
        case TOUR_WORLD:
            b->voiceLine = Rand_Range(14) + 0x3E;
            break;
        case TOUR_BIG:
            b->voiceLine = Rand_Range(7) + 0x8A;
            break;
        case TOUR_CELL:
            b->voiceLine = Rand_Range(14) + 0x6B;
            break;
        case TOUR_OTHERWORLD:
            b->voiceLine = Rand_Range(14) + 0xBB;
            break;
        case TOUR_YAMCHA:
            b->voiceLine = Rand_Range(14) + 0xE7;
            break;
        }
        BRK_SAY(b);
        Bracket_SetTalker(b, b->voiceLine);
        b->seq++;
        break;
    case 0xA:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            b->seq++;
        } else BRK_SKIP_INTRO(b);
        break;
    case 0xB:
        /* leave for the battle */
        b->timer = 0xC;
        b->voiceLine = -1;
        b->flags &= ~BRK_NO_TEXT;
        b->flags |= BRK_LEAVING;
        b->seq = 0;
        gProgress->flags |= MPROG_TOUR_GREETED;
        break;
    /* 0x33..0x39: a new tournament opens, in front of the title */
    case 0x33:
        b->seq++;
    case 0x34:
        if (b->flash[BRK_FL_TITLE].flags & MFLASH_PAD) {
            b->seq++;
        }
        break;
    case 0x35:
        b->flags &= ~BRK_NO_TEXT;
        switch (TOUR_PROG2->tour) {
        case TOUR_WORLD:
            b->voiceLine = 0x24;
            b->seq = 0x38;
            break;
        case TOUR_BIG:
            b->voiceLine = 0x7D;
            b->seq = 0x38;
            break;
        case TOUR_CELL:
            b->voiceLine = 0x50;
            b->seq++;
            break;
        case TOUR_OTHERWORLD:
            b->voiceLine = 0xA1;
            b->seq = 0x38;
            break;
        case TOUR_YAMCHA:
            b->voiceLine = 0xCD;
            b->seq = 0x38;
            break;
        }
        BRK_SAY(b);
        Bracket_SetTalker(b, b->voiceLine);
        if (gPad[0].gamePressed & 0x200) {
            b->seq = 0x39;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x36:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            b->seq++;
        }
        if (gPad[0].gamePressed & 0x200) {
            b->seq = 0x39;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x37:
        b->voiceLine++;
        BRK_SAY(b);
        b->seq++;
        Bracket_SetTalker(b, b->voiceLine);
        if (gPad[0].gamePressed & 0x200) {
            b->seq = 0x39;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x38:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            b->seq++;
        }
        if (gPad[0].gamePressed & 0x200) {
            b->seq = 0x39;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x39:
        Flash_GotoLabel(&b->flash[BRK_FL_TITLE], "fl_direct_out", 1);
        Flash_Play(&b->flash[BRK_FL_TREE], 1);
        b->seq = 0x65;
        break;
    /* 0x65..0x69: the round is announced over the tree */
    case 0x65:
        b->flags &= ~BRK_NO_TEXT;
        b->flags |= BRK_RESULT0;
        if (b->round == 0) {
            switch (TOUR_PROG2->tour) {
            case TOUR_WORLD:
                b->voiceLine = 0x25;
                break;
            case TOUR_BIG:
                b->voiceLine = 0x91;
                break;
            case TOUR_CELL:
                b->voiceLine = 0x52;
                break;
            case TOUR_OTHERWORLD:
                b->voiceLine = 0xA2;
                break;
            case TOUR_YAMCHA:
                b->voiceLine = 0xCE;
                break;
            }
            BRK_SAY(b);
            Bracket_SetTalker(b, b->voiceLine);
        } else {
            switch (TOUR_PROG2->tour) {
            case TOUR_WORLD:
                b->voiceLine = b->round * 3 + 0x23;
                break;
            case TOUR_BIG:
                b->voiceLine = b->round * 3 + 0x8F;
                break;
            case TOUR_CELL:
                b->voiceLine = b->round * 3 + 0x50;
                break;
            case TOUR_OTHERWORLD:
                b->voiceLine = b->round * 3 + 0xA0;
                break;
            case TOUR_YAMCHA:
                b->voiceLine = b->round * 3 + 0xCC;
                break;
            }
            BRK_SAY(b);
            Bracket_SetTalker(b, b->voiceLine);
        }
        b->seq++;
        if (gPad[0].gamePressed & 0x200) {
            b->seq = 0x67;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x66:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            b->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            b->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x67:
        if (b->round != 0) {
            /* one of two follow-up lines: libc rand(), not the shared generator */
            b->voiceLine = b->voiceLine + (rand() & 1) + 1;
            BRK_SAY(b);
            b->seq++;
            Bracket_SetTalker(b, b->voiceLine);
        } else {
            b->seq = 0x69;
        }
        break;
    case 0x68:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            b->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            b->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x69:
        b->seq = 0;
        b->flags &= ~BRK_RESULT0;
        Voice_StopWithLip();
        b->voiceLine = -1;
        break;
    /* 0x97..0x99: back from a battle: the versus panel shows the outcome until confirm */
    case 0x97:
        b->seq++;
        b->flags |= BRK_NO_TEXT;
        break;
    case 0x98:
        if (gPad[0].gamePressed & 0x200) {
            b->seq = 0x99;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x99:
        b->voiceLine = -1;
        b->flags &= ~BRK_NO_TEXT;
        if (b->flags & BRK_RESULT) {
            b->flags ^= BRK_RESULT;
        }
        b->seq = 0;
        Flash_GotoLabel(&b->flash[BRK_FL_VS], "fl_out", 1);
        Flash_Play(&b->flash[BRK_FL_TREE], 1);
        Flash_Play(&b->flash[BRK_FL_GUIDE], 1);
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_in", 1);
        MsgWin_Open();
        break;
    /* 0xFB..0x106: the player won the real tournament: banner, speech, rewards, save */
    case 0xFB: {
        s32 frames;

        Flash_GotoLabel(&b->flash[BRK_FL_TITLE], "fl_yusyo_in", 1);
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_out", 1);
        MsgWin_Close();
        b->flags |= BRK_SCROLL_BACK;
        frames = 30;
        b->scrollSpeed = (0x200 - (s32)b->scroll) / frames;
        b->seq++;
        break;
    }
    case 0xFC:
        if (b->flash[BRK_FL_TITLE].flags & 8) {
            b->seq++;
        }
        break;
    case 0xFD:
        b->flags &= ~BRK_NO_TEXT;
        switch (TOUR_PROG2->tour) {
        case TOUR_WORLD:
            b->voiceLine = 0x4C;
            break;
        case TOUR_BIG:
            b->voiceLine = 0x9E;
            break;
        case TOUR_CELL:
            b->voiceLine = 0x79;
            break;
        case TOUR_OTHERWORLD:
            b->voiceLine = 0xC9;
            break;
        case TOUR_YAMCHA:
            b->voiceLine = 0xF5;
            break;
        }
        BRK_SAY(b);
        b->seq++;
        Bracket_SetTalker(b, b->voiceLine);
        b->flags |= BRK_VS_FRONT;
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_in", 1);
        MsgWin_Open();
        break;
    case 0xFE:
        if (gPad[0].gamePressed & 0x200) {
            b->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0xFF:
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_out", 1);
        MsgWin_Close();
        Flash_GotoLabel(&b->flash[BRK_FL_TITLE], "fl_yusyo_out", 1);
        Bracket_GivePrizes(b, 0);
        b->rewardIdx = 0;
        b->seq++;
        break;
    case 0x100:
        if (b->flash[BRK_FL_TITLE].flags & 8) {
            b->seq++;
        }
        break;
    case 0x101:
        GetWin_Setup(b->reward[b->rewardIdx].kind, b->reward[b->rewardIdx].value);
        if (b->rewardIdx != 0) {
            GetWin_Next();
        } else {
            GetWin_Open();
        }
        b->rewardIdx++;
        b->seq++;
        break;
    case 0x102:
        if (GetWin_IsAnimating()) {
            b->seq++;
        }
        break;
    case 0x103:
        if (gPad[0].gamePressed & 0x200) {
            if (b->rewardIdx == b->rewardCount) {
                b->seq++;
            } else {
                b->seq = 0x101;
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x104:
        b->seq++;
        GetWin_Close();
        break;
    case 0x105:
        if (GetWin_IsAnimating()) {
            b->seq++;
        }
        break;
    case 0x106:
        b->seq = 0;
        McFlow_Start(0);
        break;
    /* 0x12D..0x131: the player won a free tournament: banner and speech only */
    case 0x12D: {
        s32 frames;

        Flash_GotoLabel(&b->flash[BRK_FL_TITLE], "fl_yusyo_in", 1);
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_out", 1);
        MsgWin_Close();
        b->flags |= BRK_SCROLL_BACK;
        frames = 30;
        b->scrollSpeed = (0x200 - (s32)b->scroll) / frames;
        b->seq++;
        break;
    }
    case 0x12E:
        if (b->flash[BRK_FL_TITLE].flags & 8) {
            b->seq++;
        }
        break;
    case 0x12F:
        b->flags &= ~BRK_NO_TEXT;
        switch (TOUR_PROG2->tour) {
        case TOUR_WORLD:
            b->voiceLine = 0x4C;
            break;
        case TOUR_BIG:
            b->voiceLine = 0x9E;
            break;
        case TOUR_CELL:
            b->voiceLine = 0x79;
            break;
        case TOUR_OTHERWORLD:
            b->voiceLine = 0xC9;
            break;
        case TOUR_YAMCHA:
            b->voiceLine = 0xF5;
            break;
        }
        BRK_SAY(b);
        b->seq++;
        Bracket_SetTalker(b, b->voiceLine);
        b->flags |= BRK_VS_FRONT;
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_in", 1);
        MsgWin_Open();
        break;
    case 0x130:
        if (gPad[0].gamePressed & 0x200) {
            b->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x131:
        b->seq = 0;
        b->flags |= BRK_DONE;
        b->flags |= BRK_LEAVING;
        b->timer = 0xF;
        b->result = 0;
        break;
    /* 0x15F..0x169: the player lost the final of the real tournament: banner, runner-up rewards, save */
    case 0x15F: {
        s32 frames;

        Flash_GotoLabel(&b->flash[BRK_FL_TITLE], "fl_haiboku_in", 1);
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_out", 1);
        MsgWin_Close();
        b->flags |= BRK_SCROLL_BACK;
        frames = 30;
        b->scrollSpeed = (0x200 - (s32)b->scroll) / frames;
        b->seq++;
        break;
    }
    case 0x160:
        if (b->flash[BRK_FL_TITLE].flags & 8) {
            b->seq++;
        }
        break;
    case 0x161:
        if (gPad[0].gamePressed & 0x200) {
            b->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x162:
        Flash_GotoLabel(&b->flash[BRK_FL_TITLE], "fl_jyunyusyo_in", 1);
        Bracket_GivePrizes(b, 1);
        b->rewardIdx = 0;
        b->seq++;
        break;
    case 0x163:
        if (b->flash[BRK_FL_TITLE].flags & 8) {
            b->seq++;
        }
        break;
    case 0x164:
        GetWin_Setup(b->reward[b->rewardIdx].kind, b->reward[b->rewardIdx].value);
        if (b->rewardIdx != 0) {
            GetWin_Next();
        } else {
            GetWin_Open();
        }
        b->rewardIdx++;
        b->seq++;
        break;
    case 0x165:
        if (GetWin_IsAnimating()) {
            b->seq++;
        }
        break;
    case 0x166:
        if (gPad[0].gamePressed & 0x200) {
            if (b->rewardIdx == b->rewardCount) {
                b->seq++;
            } else {
                b->seq = 0x164;
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x167:
        b->seq++;
        GetWin_Close();
        break;
    case 0x168:
        if (GetWin_IsAnimating()) {
            b->seq++;
        }
        break;
    case 0x169:
        b->seq = 0;
        McFlow_Start(0);
        break;
    /* 0xC9..0xCC: the player lost (written last in the source: its copies of the shared steps are the ones kept) */
    case 0xC9: {
        s32 frames;

        Flash_GotoLabel(&b->flash[BRK_FL_TITLE], "fl_haiboku_in", 1);
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_out", 1);
        MsgWin_Close();
        b->flags |= BRK_SCROLL_BACK;
        frames = 30;
        b->scrollSpeed = (0x200 - (s32)b->scroll) / frames;
        b->seq++;
        break;
    }
    case 0xCA:
        if (b->flash[BRK_FL_TITLE].flags & 8) {
            b->seq++;
        }
        break;
    case 0xCB:
        if (gPad[0].gamePressed & 0x200) {
            b->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0xCC:
        b->seq = 0;
        b->flags |= BRK_DONE;
        b->flags |= BRK_LEAVING;
        b->timer = 0xF;
        b->result = 0;
        break;
    default:
        Voice_FadeOutStep(0);
        break;
    }
}
