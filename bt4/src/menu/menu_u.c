#include "menu/menu_u.h"

/*
 * Menu overlay DBZP.BIN, 0x3911A8..0x395E30 (placeholder stem "menu_u"), eight source files:
 *
 *   menu_u.c    0x3911A8..0x391430  SimEvent 31   } the last six event scripts of the day screen of mode 22
 *   menu_u_b.c  0x391430..0x391B70  SimEvent 32   } (table gSimEvent, 0x3B7388; dispatcher SimEvent_Run in menu_r_b.c;
 *   menu_u_c.c  0x391B70..0x391E28  SimEvent 33   } the scripts before them are in menu_s / menu_t). 32 is the rock
 *   menu_u_d.c  0x391E28..0x392A30  SimEvent 34   } game and 34 the shell game: the two mini games behind trainings
 *   menu_u_e.c  0x392A30..0x392D20  SimEvent 35   } 1 and 2 (gSimTrain1 / gSimTrain2).
 *   menu_u_f.c  0x392D20..0x392F10  SimEvent 36   }
 *   menu_u_g.c  0x392F10..0x393C58  EvoZ: init / term / draw / update / run of the character customising screen
 *                                   (mode 49, called by the handler of modes 48..50, 0x39E940)
 *   menu_u_h.c  0x393C58..0x395E30  EvoZ: the two scissor callbacks and EvoZ_Refresh; this source file goes on
 *                                   in the next chunk with EvoZ_Load (0x395E30..0x396838, menu_v.c)
 *
 * File boundaries: every event's read-only data starts on a 16-byte boundary although jump tables and strings
 * are only 8-byte aligned (0x3BAEB8 -> 0x3BAEC0, 0x3BAFA8 -> 0x3BAFB0, 0x3BB104 -> 0x3BB110), and "mc_lose_text"
 * exists at 0x3BAED0 (event 32) and at 0x3BB020 (event 34): one file per event. (32 / 33 and 34 / 35 could be
 * one file each by the layout alone.) 0x3BB140 is `.data` (gEvoZ, gItemHelp, gShop): a new link group starts
 * there. "mc_ability_limit_up" exists at 0x3BB150 (EvoZ_Update) and at 0x3BB3D0 (EvoZ_Refresh): two files,
 * cut between EvoZ_Run and the callbacks EvoZ_Refresh installs.
 *
 * An event script is called every frame while the day screen is in its script state: `day->seq` is the step,
 * `day->seqTimer` its frame counter; it returns 1 when it is over. msgLine = line of the message window (-1
 * closes it); flags |= 0x80 waits for the player to confirm the message; SimDay_Cmd sends a command to the
 * board's movies (5 / 6 show the pictures faceA / faceB, 7 hides them, 0x11 asks yes / no (answer in unkB8C),
 * 0x19 clears, 0x1A shows the stat changes queued with SimDay_AddChange; 0x2D.. and 0x37.. drive the two mini
 * games' movies: what each does is inferred from where the scripts use them). Stats: 0 attack, 1 defence,
 * 2 health, 3 points. Every script starts with the same three steps (picture 11, message 0x62 with a jingle,
 * clear) and ends by taking Rand_Range(3) (0..2) from attack and from defence with message 0x63, except on the
 * paths noted. All names are guesses.
 */

/*
 * Event 31 (pictures 6 and 3): a yes / no offer. Yes: +1000 points, then the usual loss of attack and defence.
 * No: picture 2 comes in and defence drops by 5 (no random loss on this path).
 */
s32 SimEv31(USimDay *day) {
    switch (day->seq) {
    case 0:
        day->faceA = 11;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x62;
        Snd_PlaySe(2, 0x17);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 2:
        day->msgLine = -1;
        SimDay_Cmd(0x19);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = -1;
        day->faceB = 3;
        day->faceA = 6;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(6);
        day->msgLine = 0xAF;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0xB0;
        SimDay_Cmd(0x11);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = -1;
        if (day->unkB8C != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            day->seq = 11;
            day->seqTimer = 0;
        }
        break;
    case 7:
        day->msgLine = 0xB5;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = 0xB6;
        SimDay_AddChange(3, 1000);
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 9:
        day->msgLine = -1;
        SimDay_Cmd(7);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = 0x63;
        SimDay_AddChange(0, -Rand_Range(3));
        SimDay_AddChange(1, -Rand_Range(3));
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq = 17;
        break;
    case 11:
        day->msgLine = 0xB1;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        SimDay_Cmd(7);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 13:
        day->faceB = 2;
        SimDay_Cmd(6);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0xB2;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0xB3;
        SimDay_AddChange(1, -5);
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 16:
        day->msgLine = 0xB4;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 17:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}
