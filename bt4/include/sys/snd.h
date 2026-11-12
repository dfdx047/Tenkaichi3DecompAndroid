#ifndef SYS_SND_H
#define SYS_SND_H

#include "types.h"

/*
 * Sound effects: the EE side of the game's own IOP driver SOUNDS.IRX. src/sys/snd.c = 0x123F48..0x125568.
 * See the comment at the top of snd.c for the protocol and what a port has to provide.
 */

#define SND_RPC_ID 0x2000004 /* sceSifBindRpc server id of SOUNDS.IRX */

#define SND_BANK_COUNT 8  /* a bank is named by a one-bit mask (1 << slot); slot = Snd_BankIndex(mask) */
#define SND_QUEUE_MAX 32  /* queued voice commands per frame */
#define SND_VAG_MAX 0x80  /* samples per bank the loop-flag scratch buffer has room for */

/* Bank masks with a special meaning in this file. */
#define SND_BANK_COMMON 0x01 /* always loaded (Common_LoadBoot, file 0x14B) */
#define SND_BANK_FIXED_8 0x08  /* fixed-size slot: 0x13800 bytes of SPU RAM, reloadable in place */
#define SND_BANK_SIDE_0 0x10   /* fixed-size slot: 0x8F000 bytes of SPU RAM, muted by BtlScript_IsSlotBusy(0, 0) */
#define SND_BANK_SIDE_1 0x20   /* fixed-size slot: 0x8F000 bytes of SPU RAM, muted by BtlScript_IsSlotBusy(1, 0) */
#define SND_BANK_ALL 0xFF

/* Function numbers of the SOUNDS.IRX RPC server. Every call is blocking (sceSifCallRpc mode 0); the payload is
   the shared buffer gSndRpcBuf, `size` bytes of it; the 0x40-byte reply lands in SndRpc.reply. */
enum {
    SND_RPC_INIT = 0x0,        /* size 4, no arguments */
    SND_RPC_TERM = 0x1,        /* size 4, no arguments */
    SND_RPC_RESET = 0x2,       /* size 4, no arguments (the queue is dropped first) */
    SND_RPC_SET_BANK = 0x3,    /* size 0x10: s32 mask, spuAddr, hdIopAddr, seqIopAddr */
    SND_RPC_FREE_BANK = 0x4,   /* size 4: s32 mask */
    SND_RPC_SET_MONO = 0x5,    /* size 4: s32 mono (0 stereo, 1 mono) */
    SND_RPC_SET_PAUSE = 0x6,   /* size 8: s32 mask, s32 on */
    SND_RPC_SET_VOLUME = 0x7,  /* size 8: s32 mask, s32 volume 0..0x7F */
    SND_RPC_CMD_8 = 0x8,       /* size 4: s32 */
    SND_RPC_TICK = 0x9,        /* size 4, no arguments; once per frame */
    SND_RPC_STOP_BANK = 0xA,   /* size 4: s32 mask */
    SND_RPC_CMD_B = 0xB,       /* size 8: s32, s32 */
    SND_RPC_CMD_C = 0xC,       /* size 4: s32 */
    SND_RPC_QUEUE = 0xD,       /* size 0x184: SndQueue */
    SND_RPC_CMD_E = 0xE,       /* size 4, no arguments */
    SND_RPC_FIGHTERS = 0xF     /* size 0x18: s16 count, s16 id[count] (both sides' fighters) */
};

/* SndCmd.type */
enum {
    SND_CMD_PLAY = 0,        /* bank, id, volume, pan, pitch, loop, handle */
    SND_CMD_STOP_ID = 1,     /* bank, id */
    SND_CMD_STOP_HANDLE = 2  /* handle */
};

/* One queued voice command, 0xC bytes. */
typedef struct SndCmd {
    /* 0x00 */ u8 type;    /* SND_CMD_* */
    /* 0x01 */ u8 loop;    /* PLAY: the sample's loop flag from the bank header */
    /* 0x02 */ u16 handle; /* PLAY: serial number given to the voice; STOP_HANDLE: the one to stop */
    /* 0x04 */ u8 bank;    /* bank mask */
    /* 0x05 */ u8 id;      /* sample number inside the bank */
    /* 0x06 */ u8 volume;  /* 0..0x7F, already scaled */
    /* 0x07 */ u8 pan;     /* 0..0x7F, 0x40 = centre */
    /* 0x08 */ s32 pitch;  /* -1200..1200 cents */
} SndCmd;

/* The per-frame command queue, sent whole as RPC 0xD. 0x184 bytes. */
typedef struct SndQueue {
    /* 0x000 */ u32 count;
    /* 0x004 */ SndCmd cmd[SND_QUEUE_MAX];
} SndQueue;

/* Sony sceSifClientData, 0x28 bytes. */
typedef struct SndSifClient {
    /* 0x00 */ void *pktAddr;
    /* 0x04 */ u32 rpcId;
    /* 0x08 */ s32 semaId;
    /* 0x0C */ u32 mode;
    /* 0x10 */ u32 command;
    /* 0x14 */ void *buff;
    /* 0x18 */ void *cbuff;
    /* 0x1C */ void *endFunc;
    /* 0x20 */ void *endParam;
    /* 0x24 */ void *serve; /* non-NULL once the bind was answered */
} SndSifClient;

/* gSndRpc, 0x1EC bytes. */
typedef struct SndRpc {
    /* 0x00 */ s32 reply[16]; /* receive buffer; reply[0] is what Snd_RpcCall returns */
    /* 0x40 */ SndSifClient client;
    /* 0x68 */ SndQueue queue;
} SndRpc;

/* The shared send buffer of every RPC call (0x188 bytes of .bss). */
typedef union SndRpcBuf {
    s32 w[0x62];
    s16 h[0xC4];
    SndQueue queue;
} SndRpcBuf;

/* A bank file as loaded from the archive: five words, then three sections. Offsets are from the file start. */
typedef struct SndBankFile {
    /* 0x00 */ u32 unk00;
    /* 0x04 */ u32 bodyOfs; /* ADPCM sample data, goes to SPU RAM */
    /* 0x08 */ u32 hdOfs;   /* Sony "SCEIVers/SCEIHead" header */
    /* 0x0C */ u32 seqOfs;  /* third section, kept in IOP RAM (contents unknown; the name is a guess) */
    /* 0x10 */ u32 endOfs;
} SndBankFile;

/* One bank slot, 0x4C bytes. */
typedef struct SndBank {
    /* 0x00 */ s32 spuAddr;    /* SPU RAM address of the sample data; non-zero = the bank is loaded */
    /* 0x04 */ void *seqIop;   /* IOP heap copy of the third section */
    /* 0x08 */ void *hdIop;    /* IOP heap copy of the header section */
    /* 0x0C */ void *body;     /* EE pointers into the bank file (only valid while the caller keeps it) */
    /* 0x10 */ void *hd;
    /* 0x14 */ void *seq;
    /* 0x18 */ s32 bodySize;
    /* 0x1C */ s32 hdSize;
    /* 0x20 */ s32 seqSize;
    /* 0x24 */ s32 spuSize;    /* allocated sizes: the section size, or the fixed slot size */
    /* 0x28 */ s32 hdIopSize;
    /* 0x2C */ s32 seqIopSize;
    /* 0x30 */ s32 pending;    /* sample data still has to go to the SPU; the bank cannot play */
    /* 0x34 */ s32 uploading;  /* its transfer is the one in flight */
    /* 0x38 */ u8 vagMax;      /* highest sample number (header's maxVagInfoNumber) */
    /* 0x39 */ u8 loopBits[0x13]; /* bit n = sample n loops */
} SndBank;

/* gSndMgr, 0x2A8 bytes. */
typedef struct SndMgr {
    /* 0x000 */ SndBank bank[SND_BANK_COUNT];
    /* 0x260 */ void *iopBuf;     /* 0xA4800-byte IOP staging buffer for SPU uploads */
    /* 0x264 */ s32 spuMarkCount;
    /* 0x268 */ s32 spuMark[16];  /* stack of SPU-heap usage figures (debug, no callers) */
} SndMgr;

/* Sony HD "Vagi" chunk (vag info) and its entries. */
typedef struct SndVagParam {
    /* 0x00 */ u32 vagOffset;
    /* 0x04 */ u16 sampleRate;
    /* 0x06 */ u8 loop;
    /* 0x07 */ u8 pad;
} SndVagParam;

typedef struct SndVagInfoChunk {
    /* 0x00 */ u32 creator;  /* 'SCEI' */
    /* 0x04 */ u32 type;     /* 'Vagi' */
    /* 0x08 */ u32 size;
    /* 0x0C */ u32 maxNumber; /* highest index */
    /* 0x10 */ u32 offset[1]; /* maxNumber + 1 offsets, followed by the SndVagParam entries */
} SndVagInfoChunk;

extern SndRpc *gSndRpc;
extern SndMgr *gSndMgr;
extern SndRpcBuf gSndRpcBuf;
extern u16 gSndSerial;
extern s32 gSndBankVolume[SND_BANK_COUNT];

s32 Snd_RpcCall(s32 cmd, void *data, s32 size);
s32 Snd_RpcIsBusy(void);
void Snd_RpcInit(void);
void Snd_RpcTerm(void);
void Snd_RpcReset(void);
void Snd_RpcSetBank(s32 mask, s32 spuAddr, void *hdIop, void *seqIop);
void Snd_RpcFreeBank(s32 mask);
s32 Snd_QueuePlay(s32 mask, u32 id, u32 volume, u32 pan, s32 pitch, s32 loop);
void Snd_QueueStopId(s32 mask, u32 id);
void Snd_QueueStopHandle(s32 handle);
void Snd_RpcCmdB(s32 a, s32 b);
void Snd_RpcCmdC(s32 a);
void Snd_RpcFlush(void);
void Snd_RpcSetMono(s32 mono);
void Snd_RpcSetPause(s32 mask, s32 on);
void Snd_RpcStopBank(s32 mask);
void Snd_RpcSetVolume(s32 mask, s32 volume);
void Snd_RpcCmd8(s32 a);
void Snd_SpuTransfer(void *iopAddr, s32 spuAddr, s32 size);
s32 Snd_SpuTransferWait(void);
s32 Snd_SpuTransferCheck(void);
void Snd_RpcCmdE(void);
void Snd_RpcSetFighters(s32 count, s32 *ids);
s32 Snd_BankIndex(u32 mask);
s32 Snd_ScaleVolume(s32 bank, s32 volume);
s32 Snd_ScalePan(s32 pan);
s32 Snd_ScalePitch(s32 pitch);
void Snd_SetupBank(SndBank *bank, u32 mask, SndBankFile *file, s32 alloc);
void Snd_SendBank(SndBank *bank, s32 sync);
void Snd_Init(void);
void Snd_Term(void);
void Snd_Reset(void);
void Snd_Update(void);
void Snd_LoadBank(u32 mask, SndBankFile *file, s32 sync);
void Snd_ReloadBank(u32 mask, SndBankFile *file, s32 sync);
void Snd_LoadBankFile(u32 mask, s32 fileId);
void Snd_UnloadBank(u32 mask);
s32 Snd_IsUploadDone(void);
void Snd_PushSpuMark(void);
s32 Snd_PopSpuMark(void);
void Snd_SetStereo(s32 stereo);
void Snd_SetMasterVolume(s32 volume);
s32 Snd_PlaySe(u32 mask, s32 id);
s32 Snd_PlaySeEx(u32 mask, s32 id, s32 volume, s32 pan, s32 pitch);
void Snd_StopSe(u32 mask, s32 id);
void Snd_StopHandle(s32 handle);
void Snd_CmdB(s32 a, s32 b);
void Snd_CmdC(s32 a);
void Snd_SetPause(s32 mask, s32 on);
void Snd_StopBank(s32 mask);
void Snd_StopBankAndResume(s32 mask);
void Snd_Cmd8(s32 a);
s32 Snd_IsLoopSe(u32 mask, s32 id);
s32 Snd_GetVagMax(u32 mask);
s32 Snd_IsBankLoaded(u32 mask);
void Snd_CmdE(void);
void Snd_SendFighters(void);
SndVagInfoChunk *SndHd_GetVagInfo(u32 *head);
s32 SndHd_GetVagMax(SndBankFile *file);
void SndHd_GetLoopFlags(SndBankFile *file, u8 *flags);
void SndHd_GetLoopBits(SndBankFile *file, u8 *bits);

#endif
