#include "common.h"
#include "menu/menu_r.h"

/*
 * The event scripts (src/menu/menu_s_d.c, menu_t_b.c, menu_u*.c; their headers see the work area through views of
 * their own, TSimDay / USimDay, so they are declared here with this file's type).
 */
extern s32 SimEv00(SimDay *day);
extern s32 SimEv01(SimDay *day);
extern s32 SimEv02(SimDay *day);
extern s32 SimEv03(SimDay *day);
extern s32 SimEv04(SimDay *day);
extern s32 SimEv05(SimDay *day);
extern s32 SimEv06(SimDay *day);
extern s32 SimEv07(SimDay *day);
extern s32 SimEv08(SimDay *day);
extern s32 SimEv09(SimDay *day);
extern s32 SimEv10(SimDay *day);
extern s32 SimEv11(SimDay *day);
extern s32 SimEv12(SimDay *day);
extern s32 SimEv13(SimDay *day);
extern s32 SimEv14(SimDay *day);
extern s32 SimEv15(SimDay *day);
extern s32 SimEv16(SimDay *day);
extern s32 SimEv17(SimDay *day);
extern s32 SimEv18(SimDay *day);
extern s32 SimEv19(SimDay *day);
extern s32 SimEv20(SimDay *day);
extern s32 SimEv21(SimDay *day);
extern s32 SimEv22(SimDay *day);
extern s32 SimEv23(SimDay *day);
extern s32 SimEv24(SimDay *day);
extern s32 SimEv25(SimDay *day);
extern s32 SimEv26(SimDay *day);
extern s32 SimEv27(SimDay *day);
extern s32 SimEv28(SimDay *day);
extern s32 SimEv29(SimDay *day);
extern s32 SimEv30(SimDay *day);
extern s32 SimEv31(SimDay *day);
extern s32 SimEv32(SimDay *day);
extern s32 SimEv33(SimDay *day);
extern s32 SimEv34(SimDay *day);
extern s32 SimEv35(SimDay *day);
extern s32 SimEv36(SimDay *day);

/*
 * The scripts by event number (.data, 0x3B7388..0x3B741C): [0..2] the three trainings (board row 0), [3] board
 * row 2, [4] board row 3, [5..36] the 32 random events of board row 1 (SimDay_PickEvent).
 */
s32 (*gSimEvent[37])(SimDay *day) = {
    SimEv00, SimEv01, SimEv02, SimEv03, SimEv04, SimEv05, SimEv06, SimEv07,
    SimEv08, SimEv09, SimEv10, SimEv11, SimEv12, SimEv13, SimEv14, SimEv15,
    SimEv16, SimEv17, SimEv18, SimEv19, SimEv20, SimEv21, SimEv22, SimEv23,
    SimEv24, SimEv25, SimEv26, SimEv27, SimEv28, SimEv29, SimEv30, SimEv31,
    SimEv32, SimEv33, SimEv34, SimEv35, SimEv36,
};

/*
 * SimEvent, 0x3851B0..0x3851E8: one function between the board screen (SimDay, work pointer 0x3B7384) and the
 * result screen (SimResult, 0x3B741C). Its table gSimEvent (0x3B7388..0x3B741C, 37 function pointers, .data)
 * lies between the two work pointers in the same way; the handlers are the event scripts at 0x38C408..0x392D20
 * (chunks menu_s, menu_t, menu_u). Nothing here emits read-only data and no branch depends on a neighbour, so the
 * file it belongs to is not decided: the last function of the SimDay object (then the table follows that
 * object's work pointer in its .data), or a file of its own in front of SimResult.
 */

/* Runs one step of event script `event` of the turn; non-zero when it is over (also for a number past the table). */
s32 SimEvent_Run(SimDay *day, u32 event) {
    s32 done = 1;

    if (event < 37) {
        done = gSimEvent[event](day);
    }
    return done;
}
