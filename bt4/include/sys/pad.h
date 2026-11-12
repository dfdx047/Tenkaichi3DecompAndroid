#ifndef SYS_PAD_H
#define SYS_PAD_H

#include "types.h"

#define PAD_COUNT 2 /* controller ports 0 and 1, slot 0 (no multitap) */

/* Pad.held / pressed / repeat bits.
   Bits 0..15 are the controller's two digital button bytes, inverted so that 1 = down.
   Bits 16..23 are derived from the analog sticks by Pad_Update (threshold 0.5, dominant axis only). */
#define PAD_SELECT 0x00000001
#define PAD_L3 0x00000002
#define PAD_R3 0x00000004
#define PAD_START 0x00000008
#define PAD_UP 0x00000010
#define PAD_RIGHT 0x00000020
#define PAD_DOWN 0x00000040
#define PAD_LEFT 0x00000080
#define PAD_L2 0x00000100
#define PAD_R2 0x00000200
#define PAD_L1 0x00000400
#define PAD_R1 0x00000800
#define PAD_TRIANGLE 0x00001000
#define PAD_CIRCLE 0x00002000
#define PAD_CROSS 0x00004000
#define PAD_SQUARE 0x00008000
#define PAD_LSTICK_LEFT 0x00010000
#define PAD_LSTICK_RIGHT 0x00020000
#define PAD_LSTICK_DOWN 0x00040000
#define PAD_LSTICK_UP 0x00080000
#define PAD_RSTICK_LEFT 0x00100000
#define PAD_RSTICK_RIGHT 0x00200000
#define PAD_RSTICK_DOWN 0x00400000
#define PAD_RSTICK_UP 0x00800000

/* Pad.gameHeld / gamePressed / gameRepeat bits, built from Pad.held by Pad_UpdateGameButtons (0x2574F0,
   not part of pad.c). The left stick's digital bits are not used; the game reads gameLeft instead. */
#define PADG_LEFT 0x00000001 /* d-pad */
#define PADG_RIGHT 0x00000002
#define PADG_DOWN 0x00000004
#define PADG_UP 0x00000008
#define PADG_RSTICK_LEFT 0x00000010
#define PADG_RSTICK_RIGHT 0x00000020
#define PADG_RSTICK_DOWN 0x00000040
#define PADG_RSTICK_UP 0x00000080
#define PADG_CIRCLE 0x00000100
#define PADG_CROSS 0x00000200
#define PADG_TRIANGLE 0x00000400
#define PADG_SQUARE 0x00000800
#define PADG_START 0x00001000
#define PADG_SELECT 0x00002000
#define PADG_L1 0x00004000
#define PADG_L2 0x00008000
#define PADG_R1 0x00010000
#define PADG_R2 0x00020000
#define PADG_L3 0x00040000
#define PADG_R3 0x00080000
/* 0x00100000 is tested with an empty mask and never set */
#define PADG_LEFT2 0x00200000 /* d-pad again */
#define PADG_RIGHT2 0x00400000
#define PADG_DOWN2 0x00800000
#define PADG_UP2 0x01000000

#define PAD_STICK_DEADZONE 50 /* raw units out of 127.5 */

/* Pad.phase */
#define PAD_PHASE_VIB_PROFILE 0 /* just connected: ask for the vibration profile */
#define PAD_PHASE_BTN_PROFILE 1 /* ask for the button profile until it arrives */
#define PAD_PHASE_READ 2        /* normal operation */

#define PAD_STATE_STABLE 1 /* scePad2GetState: controller connected and ready */

#define PAD_STATUS_NONE 0xFF /* Pad.status when nothing usable is connected */

/* Indices into Pad.data (raw controller report). */
#define PAD_DATA_BTN_LO 0 /* SELECT L3 R3 START UP RIGHT DOWN LEFT, 0 = down */
#define PAD_DATA_BTN_HI 1 /* L2 R2 L1 R1 TRIANGLE CIRCLE CROSS SQUARE, 0 = down */
#define PAD_DATA_RX 2
#define PAD_DATA_RY 3
#define PAD_DATA_LX 4
#define PAD_DATA_LY 5

/* Argument of scePad2CreateSocket (0x20 bytes). */
typedef struct Pad2SocketParam {
    /* 0x00 */ u32 option; /* 2 = use the given port/slot */
    /* 0x04 */ s32 port;
    /* 0x08 */ s32 slot;
    /* 0x0C */ s32 number;
    /* 0x10 */ u8 name[16];
} Pad2SocketParam;

/* One controller port (gPad, 2 entries of 0x1C0). */
typedef struct Pad {
    /* 0x000 */ u8 work[0x100];  /* libpad2 socket work area (must be 64-byte aligned) */
    /* 0x100 */ s32 socket;      /* scePad2CreateSocket result */
    /* 0x104 */ s32 state;       /* scePad2GetState result of this frame */
    /* 0x108 */ s32 phase;       /* PAD_PHASE_* */
    /* 0x10C */ u8 profile[4];   /* button profile (scePad2GetButtonProfile): one bit per input the pad has */
    /* 0x110 */ s32 profileSize; /* its length, < 0 while not available */
    /* 0x114 */ u8 data[18];     /* raw report: 2 button bytes, RX RY LX LY, 12 pressure bytes */
    /* 0x128 */ s32 dataSize;    /* scePad2Read result */
    /* 0x12C */ u16 vibData;     /* bit 0 = small motor on, bits 1..7 = large motor power */
    /* 0x12E */ u8 vibProfile[2]; /* actuator profile (sceVibGetProfile) */
    /* 0x130 */ f32 left[2];     /* left stick x, y: -1..1, + = right / down, inside the unit circle */
    /* 0x138 */ f32 right[2];    /* right stick x, y */
    /* 0x140 */ s32 unk140;      /* only ever cleared */
    /* 0x144 */ s32 unk144;      /* only ever cleared */
    /* 0x148 */ u32 held;        /* PAD_* bits down this frame */
    /* 0x14C */ u32 prev;        /* held of the previous frame (equal to held after Pad_Update) */
    /* 0x150 */ u32 pressed;     /* went down this frame */
    /* 0x154 */ u32 repeat;      /* pressed, plus held again each time the repeat timer runs out */
    /* 0x158 */ s32 repeatTimer;
    /* 0x15C */ u32 repeatLast;  /* held value the timer is running for */
    /* 0x160 */ f32 vibPower;    /* large motor request accumulated this frame, 0..1 */
    /* 0x164 */ s32 vibSmall;    /* small motor request accumulated this frame, 0/1 */
    /* 0x168 */ u8 unk168[0x18];
    /* 0x180 */ s32 status;      /* 0 = connected, PAD_STATUS_NONE = not usable (read by 0x257830) */
    /* 0x184 */ s32 lastStatus;  /* written by Pad_UpdateGameButtons: last status < 4 (read by 0x257850) */
    /* 0x188 */ u32 gameHeld;    /* game button word built by Pad_UpdateGameButtons */
    /* 0x18C */ u32 gamePressed;
    /* 0x190 */ u32 gameRepeat;
    /* 0x194 */ s32 gameRepeatTimer;
    /* 0x198 */ u32 gameRepeatLast;
    /* 0x19C */ s32 repeatDelay;    /* frames before the first repeat (20) */
    /* 0x1A0 */ s32 repeatInterval; /* frames between repeats (1) */
    /* 0x1A4 */ f32 gameLeft[2];    /* copies of left/right made by Pad_UpdateGameButtons */
    /* 0x1AC */ f32 gameRight[2];
    /* 0x1B4 */ u8 unk1B4[0xC];
} __attribute__((aligned(16))) Pad; /* 0x1C0; the 16-byte alignment is needed for matching code */

extern Pad gPad[PAD_COUNT];

void Pad_Init(void);
void Pad_Stub0(void);
void Pad_Stub1(void);
void Pad_Stub2(void);
void Pad_Stub3(void);
void Pad_Update(void);
s32 Pad_IsHeld(s32 pad, u32 mask);
s32 Pad_IsPressed(s32 pad, u32 mask);
s32 Pad_IsRepeat(s32 pad, u32 mask);
void Pad_GetSticks(s32 pad, f32 *left, f32 *right);
void Pad_AddVibration(s32 pad, s32 small, f32 power);
s32 Pad_GetVibProfile(s32 pad);
void Pad_SendActuator(s32 pad);
void Pad_Reset(s32 pad);
s32 Pad_IsDigitalOnly(s32 pad);
s32 Pad_TestMask(s32 pad, u32 bits, u32 mask);
f32 Pad_StickToFloat(u8 raw, s32 deadzone);

#endif
