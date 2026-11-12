#include "common.h"
#include "menu/menu_c.h"
#include "sys/pad.h"
#include "port_variant.h"

/*
 * HistGuide, 0x33F700..0x3405A0: the scripted dialogues of the history menu (HistSel). An object of its own:
 * it repeats strings of the HistSel object ("mc_guide_%02d", "fl_menu_in") and reaches the work area through
 * a parameter instead of gHistSel.
 *
 * HistSel.script is the step; 0 means no script (the menu reads the pad). Odd steps start a voice line, even
 * steps wait for it to end (confirm skips). The sequences:
 *     1..11       first visit: Goku explains the mode (sets unlockFlags 0x80)
 *     51..53      leaving: Goku says goodbye, fade out
 *     101..107    a sub menu whose guide is known was chosen: Goku comments, the movie closes, leave
 *     151, 152    a sub menu chosen for the first time: its guide walks in, then 251 + 50 * sub menu
 *     201..203    the movie closes, leave (clears the sub menu's "new" marks)
 *     251..617    eight introductions, one per sub menu, between Goku (talker 0) and the guide (talker 1)
 *     651         an event visit: the guest is already chosen; continues at 751 + 50 * guest
 *     701         back to the menu after an event
 *     751..1121   eight event dialogues, the guest walking in and out
 *     1151..1159  Goku's explanation after the last sub menu was completed (sets unlockFlags 0x100)
 */

/* Sends the clip of sub menu `guide`'s guide to a label ("fl_in" / "fl_out"). */
void HistGuide_ClipGoto(HistSel *menu, s32 guide, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &menu->flash[0];

    if (guide == 6) {
        menu->flags |= 0x80;
    }
    sprintf(name, "mc_guide_%02d", guide);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* The next line of the current voice set, spoken by Goku (0) or the guest (1). */
#define G_SAY(t) \
    menu->talker = (t); \
    menu->voiceLine++; \
    HistSel_PlayVoice(); \
    menu->script++; \
    break

/* The next line, same speaker. */
#define G_NEXT() \
    menu->voiceLine++; \
    HistSel_PlayVoice(); \
    menu->script++; \
    break

/* Waits for the voice to end; confirm skips it. */
#define G_WAIT() \
    if (Voice_GetStat(0) == 5) { \
        menu->script++; \
    } else if (gPad[0].gamePressed & 0x200) { \
        menu->script++; \
        Snd_PlaySe(1, 1); \
    } \
    break

/* The guest's clip walks in or out. */
#define G_GUEST(label) \
    HistGuide_ClipGoto(menu, menu->guest, label); \
    menu->script++; \
    break

/* Waits for a trigger of the movie's timeline. */
#define G_TRIG(bit) \
    if (menu->flash[0].trig & (bit)) { \
        menu->script++; \
    } \
    break

/* One step of the guide script. Writes *result when a script ends the screen. */
void HistGuide_Update(HistSel *menu, s32 *result) {
    if (menu->script == 0) {
        return;
    }
    switch (menu->script) {
    case 1:
        MSAVE->unlockFlags |= MUNLOCK_HIST_INTRO;
        menu->guide = 8;
        menu->talker = 0;
        menu->voiceLine = 0;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 3:
    case 5:
    case 7:
    case 9:
        G_NEXT();
    case 2:
    case 4:
    case 6:
    case 8:
    case 10:
        G_WAIT();
    case 11:
        menu->script = 0;
        Flash_GotoLabel(&menu->flash[0], "fl_menu_in", 1);
        menu->guide = 8;
        menu->voiceLine = menu->cursor[0] + 5;
        HistSel_PlayVoice();
        Snd_PlaySe(2, 0xE);
        break;
    case 1151:
        MSAVE->unlockFlags |= MUNLOCK_HIST_COMPLETE;
        menu->guide = 8;
        menu->voiceLine = 0x2C;
        menu->talker = 0;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 1152:
    case 1154:
    case 1156:
    case 1158:
        G_WAIT();
    case 1153:
    case 1155:
    case 1157:
        G_NEXT();
    case 1159:
        menu->script = 0;
        Flash_GotoLabel(&menu->flash[0], "fl_menu_in", 1);
        menu->guide = 8;
        menu->voiceLine = menu->cursor[0] + 5;
        HistSel_PlayVoice();
        Snd_PlaySe(2, 0xE);
        break;
    case 51:
        menu->guide = 8;
        menu->voiceLine = 0x2B;
        menu->talker = 0;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 52:
        if (Voice_GetStat(0) == 5) {
            menu->script++;
        } else if (gPad[0].gamePressed & 0x200) {
            menu->script++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            menu->script++;
            Snd_PlaySe(1, 2);
        }
        break;
    case 53:
        ColorFade_StartOut(0, 0, 0, 0x14);
        menu->script = 0;
        break;
    case 101:
        menu->voiceLine = Rand_Range(4) + 0x26;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 102:
        G_WAIT();
    case 103:
        Flash_GotoLabel(&menu->flash[0], "fl_scenario_ok", 1);
        menu->script++;
        break;
    case 104:
        G_TRIG(1);
    case 105:
        Flash_StepFrames(&menu->flash[0], 1);
        Snd_PlaySe(2, 0xC);
        IconWin_Close();
        MsgWin_Close();
        menu->script++;
        break;
    case 106:
        G_TRIG(2);
    case 107:
        *result = 1;
        menu->flags |= HISTSEL_CHOSEN;
        menu->flags |= HISTSEL_LEAVING;
        menu->timer = 15;
        menu->script = 0;
        break;
    case 151:
        Flash_GotoLabel(&menu->flash[0], "fl_scenario_ok", 1);
        menu->guest = PORT_V(menu->items[menu->cursor[1]],6);
        HistGuide_ClipGoto(menu, menu->guest, "fl_in");
        menu->guide = menu->items[menu->cursor[1]];
        menu->script++;
        MSAVE->slot[menu->items[menu->cursor[1]]].flags |= MSLOT_INTRODUCED;
        menu->voiceLine = -1;
        break;
    case 152:
        if (menu->flash[0].trig & 4) {
            menu->script = menu->items[menu->cursor[1]] * 50 + 251;
        }
        break;
    case 201:
        Flash_StepFrames(&menu->flash[0], 1);
        Snd_PlaySe(2, 0xC);
        IconWin_Close();
        MsgWin_Close();
        menu->script++;
        break;
    case 202:
        G_TRIG(2);
    case 203:
        *result = 1;
        menu->flags |= HISTSEL_CHOSEN;
        menu->flags |= HISTSEL_LEAVING;
        menu->timer = 15;
        MSAVE->slot[menu->items[menu->cursor[1]]].flags &= ~MSLOT_NEW;
        MSAVE->slot[menu->items[menu->cursor[1]]].flags &= ~MSLOT_NEW_EPISODE;
        menu->script = 0;
        break;
    case 251:
        menu->voiceLine = 0;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 253:
    case 261:
        G_SAY(0);
    case 255:
    case 257:
    case 259:
    case 263:
    case 265:
        G_SAY(1);
    case 252:
    case 254:
    case 256:
    case 258:
    case 260:
    case 262:
    case 264:
    case 266:
        G_WAIT();
    case 301:
        menu->voiceLine = 0;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 303:
    case 307:
    case 311:
        G_SAY(0);
    case 305:
        G_SAY(1);
    case 309:
        menu->voiceLine = 0x14;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 302:
    case 304:
    case 306:
    case 308:
    case 310:
    case 312:
        G_WAIT();
    case 351:
        menu->voiceLine = 0;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 353:
    case 357:
    case 361:
    case 365:
        G_SAY(0);
    case 355:
    case 359:
    case 363:
    case 367:
        G_SAY(1);
    case 352:
    case 354:
    case 356:
    case 358:
    case 360:
    case 362:
    case 364:
    case 366:
    case 368:
        G_WAIT();
    case 401:
        menu->voiceLine = 0;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 403:
    case 409:
    case 411:
    case 413:
        G_SAY(0);
    case 405:
    case 407:
    case 415:
    case 417:
    case 419:
        G_SAY(1);
    case 402:
    case 404:
    case 406:
    case 408:
    case 410:
    case 412:
    case 414:
    case 416:
    case 418:
    case 420:
        G_WAIT();
    case 451:
        menu->voiceLine = 0;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 453:
    case 463:
        G_SAY(0);
    case 455:
    case 457:
    case 459:
    case 461:
    case 465:
    case 467:
        G_SAY(1);
    case 452:
    case 454:
    case 456:
    case 458:
    case 460:
    case 462:
    case 464:
    case 466:
    case 468:
        G_WAIT();
    case 501:
        menu->voiceLine = 0;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 503:
    case 511:
    case 519:
        G_SAY(0);
    case 505:
    case 507:
    case 509:
    case 513:
    case 515:
    case 517:
        G_SAY(1);
    case 502:
    case 504:
    case 506:
    case 508:
    case 510:
    case 512:
    case 514:
    case 516:
    case 520:
        G_WAIT();
    case 518:
        if (Voice_GetStat(0) == 5) {
            HistGuide_ClipGoto(menu, menu->guest, "fl_out");
            menu->script++;
        } else if (gPad[0].gamePressed & 0x200) {
            HistGuide_ClipGoto(menu, menu->guest, "fl_out");
            menu->script++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 551:
        menu->voiceLine = 0;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 553:
    case 555:
    case 557:
    case 561:
        G_SAY(0);
    case 559:
        G_SAY(1);
    case 552:
    case 554:
    case 556:
    case 558:
    case 560:
    case 562:
        G_WAIT();
    case 601:
        menu->voiceLine = 0;
        menu->talker = 1;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 603:
    case 609:
    case 615:
        G_SAY(0);
    case 605:
    case 607:
    case 611:
    case 613:
        G_SAY(1);
    case 602:
    case 604:
    case 606:
    case 608:
    case 610:
    case 612:
    case 614:
    case 616:
        G_WAIT();
    case 267:
    case 313:
    case 369:
    case 421:
    case 469:
    case 521:
    case 563:
    case 617:
        menu->script = 201;
        break;
    case 651:
        menu->guide = menu->guest;
        MSAVE->slot[menu->guest].flags |= MSLOT_EVENT_DONE;
        menu->script = menu->guest * 50 + 751;
        break;
    case 701:
        menu->script = 0;
        Flash_GotoLabel(&menu->flash[0], "fl_menu_in", 1);
        menu->guest = -1;
        menu->guide = 8;
        menu->voiceLine = menu->cursor[0] + 5;
        menu->talker = 0;
        HistSel_PlayVoice();
        Snd_PlaySe(2, 0xE);
        break;
    case 751:
        menu->voiceLine = 0x1A;
        menu->talker = 0;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 761:
    case 763:
    case 771:
    case 773:
        G_SAY(0);
    case 755:
    case 757:
    case 759:
    case 765:
    case 767:
        G_SAY(1);
    case 752:
    case 756:
    case 758:
    case 760:
    case 762:
    case 764:
    case 766:
    case 768:
    case 772:
    case 774:
        G_WAIT();
    case 753:
        G_GUEST("fl_in");
    case 801:
        menu->voiceLine = 0x14;
        menu->talker = 0;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 809:
    case 811:
    case 819:
    case 821:
        G_SAY(0);
    case 805:
    case 807:
    case 813:
    case 815:
        G_SAY(1);
    case 802:
    case 806:
    case 808:
    case 810:
    case 812:
    case 814:
    case 816:
    case 820:
    case 822:
        G_WAIT();
    case 803:
        G_GUEST("fl_in");
    case 851:
        G_GUEST("fl_in");
    case 853:
        menu->talker = 1;
        menu->voiceLine = 0x1D;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 855:
    case 859:
    case 861:
    case 865:
    case 873:
        G_SAY(0);
    case 857:
    case 863:
    case 867:
    case 869:
        G_SAY(1);
    case 854:
    case 856:
    case 858:
    case 860:
    case 862:
    case 864:
    case 866:
    case 868:
    case 870:
    case 874:
        G_WAIT();
    case 901:
        menu->voiceLine = 0x1E;
        menu->talker = 0;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 909:
    case 913:
    case 917:
    case 919:
    case 921:
        G_SAY(0);
    case 905:
    case 907:
    case 911:
        G_SAY(1);
    case 902:
    case 906:
    case 908:
    case 910:
    case 912:
    case 914:
    case 918:
    case 920:
    case 922:
        G_WAIT();
    case 903:
        G_GUEST("fl_in");
    case 951:
        G_GUEST("fl_in");
    case 953:
        menu->talker = 1;
        menu->voiceLine = 0x27;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 955:
    case 959:
    case 967:
    case 971:
    case 975:
        G_SAY(0);
    case 957:
    case 961:
    case 963:
    case 965:
    case 969:
        G_SAY(1);
    case 954:
    case 956:
    case 958:
    case 960:
    case 962:
    case 964:
    case 966:
    case 968:
    case 970:
    case 972:
    case 976:
        G_WAIT();
    case 1001:
        G_GUEST("fl_in");
    case 1003:
        menu->talker = 1;
        menu->voiceLine = 0x1D;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 1005:
    case 1007:
    case 1013:
    case 1021:
        G_SAY(0);
    case 1009:
    case 1011:
    case 1015:
    case 1017:
        G_SAY(1);
    case 1004:
    case 1006:
    case 1008:
    case 1010:
    case 1012:
    case 1014:
    case 1016:
    case 1018:
    case 1022:
        G_WAIT();
    case 1051:
        G_GUEST("fl_in");
    case 1053:
        menu->talker = 1;
        menu->voiceLine = 0x18;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 1055:
    case 1059:
    case 1065:
    case 1067:
    case 1069:
    case 1071:
        G_SAY(0);
    case 1057:
    case 1061:
        G_SAY(1);
    case 1054:
    case 1056:
    case 1058:
    case 1060:
    case 1062:
    case 1066:
    case 1068:
    case 1070:
    case 1072:
        G_WAIT();
    case 1101:
        G_GUEST("fl_in");
    case 1103:
        menu->talker = 1;
        menu->voiceLine = 0x16;
        HistSel_PlayVoice();
        menu->script++;
        break;
    case 1105:
    case 1109:
    case 1111:
    case 1113:
    case 1119:
        G_SAY(0);
    case 1107:
    case 1115:
        G_SAY(1);
    case 1104:
    case 1106:
    case 1108:
    case 1110:
    case 1112:
    case 1114:
    case 1116:
    case 1120:
        G_WAIT();
    case 769:
    case 817:
    case 871:
    case 915:
    case 973:
    case 1019:
    case 1063:
    case 1117:
        G_GUEST("fl_out");
    case 754:
    case 770:
    case 804:
    case 818:
    case 852:
    case 872:
    case 904:
    case 916:
    case 952:
    case 974:
    case 1002:
    case 1020:
    case 1052:
    case 1064:
    case 1102:
    case 1118:
        G_TRIG(4);
    case 775:
    case 823:
    case 875:
    case 923:
    case 977:
    case 1023:
    case 1073:
    case 1121:
        menu->script = 701;
        break;
    default:
        Voice_FadeOutStep(0);
        break;
    }
}
