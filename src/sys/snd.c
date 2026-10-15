/*
 * Sound effects: EE side of the game's own IOP driver SOUNDS.IRX.   0x123F48..0x125568
 *
 * Three layers, in address order:
 *   0x123F48..0x1245F0  the RPC stubs (Snd_Rpc*, Snd_Queue*, Snd_SpuTransfer*)
 *   0x1245F0..0x1253F8  the bank manager and the calls the game uses (Snd_*)
 *   0x1253F8..0x125568  readers for the Sony HD header inside a bank file (SndHd_*)
 *
 * What a port has to replace
 * --------------------------
 * RPC link. One client bound to server id 0x2000004 (Snd_RpcInit retries the bind until SOUNDS.IRX answers). Every
 * command goes through Snd_RpcCall(fno, gSndRpcBuf, size): sceSifCallRpc with mode 0, i.e. the caller is blocked until
 * the IOP has run the command; 0x40 bytes come back and the first word is the result. No command is sent
 * asynchronously and none has an end callback. Commands (payload = little-endian s32 words unless noted):
 *     0x0  INIT        4 bytes, unused             once, from Snd_RpcInit
 *     0x1  TERM        4 bytes, unused             Snd_Term (never called)
 *     0x2  RESET       4 bytes, unused             Snd_Reset: stop everything
 *     0x3  SET_BANK    mask, spuAddr, hdIopAddr, seqIopAddr      a bank's data is in place
 *     0x4  FREE_BANK   mask
 *     0x5  SET_MONO    0 = stereo, 1 = mono
 *     0x6  SET_PAUSE   mask, on                    pause / resume the voices of those banks
 *     0x7  SET_VOLUME  mask, volume 0..0x7F        only ever sent as (0xFF, 0x7F) at start-up
 *     0x8  ?           one word                    never sent
 *     0x9  TICK        4 bytes, unused             every frame, before the queue
 *     0xA  STOP_BANK   mask                        stop the voices of those banks
 *     0xB  ?           two words                   never sent
 *     0xC  ?           one word                    never sent
 *     0xD  QUEUE       SndQueue, 0x184 bytes       every frame that queued something
 *     0xE  ?           4 bytes, unused             never sent
 *     0xF  FIGHTERS    s16 count, s16 id[count], 0x18 bytes sent      every battle frame
 * "mask" is a bank bit mask; the driver takes several bits at once in 6, 7 and 0xA (0x3C, 0xFF).
 *
 * Per-frame queue. Playing and stopping a sound never talks to the IOP directly: Snd_PlaySeEx / Snd_StopSe /
 * Snd_StopHandle append a 12-byte SndCmd to gSndRpc->queue (32 at most; a 33rd is dropped and the play returns -1).
 * Snd_Update sends TICK, then the whole queue (count + 32 entries) as one QUEUE command, then clears it. So a sound
 * starts on the next Snd_Update and at most 32 voice commands fit in a frame. A PLAY entry carries the bank mask, the
 * sample number, volume 0..0x7F, pan 0..0x7F (0x40 centre), pitch in cents (-1200..1200), the sample's loop flag and
 * a 16-bit handle. The handle is made up here (gSndSerial, +1 per PLAY, wraps at 0x10000) and is what Snd_PlaySeEx
 * returns; STOP_HANDLE names a voice by it.
 *
 * Banks. Eight slots; a bank is always named by the mask 1 << slot (Snd_BankIndex gives the slot of the lowest set
 * bit). A bank file is { u32 ?, bodyOfs, hdOfs, seqOfs, endOfs } followed by three sections:
 *     body  the ADPCM samples; uploaded to SPU2 RAM (SPU heap SpuHeap_Alloc, addresses 0x5010 up)
 *     hd    a Sony HD header (SCEIVers / SCEIHead ...); copied to an IOP heap block
 *     seq   a third table, contents not looked at here (the name is a guess); copied to an IOP heap block
 * Snd_LoadBank allocates the three blocks, copies hd and seq with sceSifSetDma (Dma_SendToIop), then either uploads
 * the body at once (sync: EE -> the 0xA4800-byte IOP staging buffer -> sceSdVoiceTrans to the SPU, wait) or marks the
 * bank pending, and finally sends SET_BANK. A pending bank refuses to play; Snd_Update uploads pending bodies one bank
 * at a time and polls sceSdVoiceTransStatus each frame. Slots 3, 4 and 5 (masks 8, 0x10, 0x20) get fixed-size blocks
 * (SPU 0x13800 / 0x8F000 / 0x8F000 bytes) so that Snd_ReloadBank can swap their contents without reallocating.
 * Slot 0 (mask 1) is the common bank, file 0x14B, loaded at boot.
 * The only thing the EE reads from the header is the vag-info chunk: the highest sample number and each sample's
 * loop flag. So a sound "id" is the sample (VAG) index inside its bank, 0..255.
 *
 * Snd_PlaySe(mask, id) = Snd_PlaySeEx(mask, id, 0x40, 0, 0).
 * Snd_PlaySeEx(mask, id, volume, pan, pitch):
 *     volume  0..0x7F; sent as volume * gSndBankVolume[slot] * save->seVolume / 0x480 clamped to 0..0x7F
 *             (gSndBankVolume = { 0xD8, 0xE0, 0x80, 0x80, 0x80, 0x80, 0x80, 0 }; seVolume 0..9, so 0x480 = 128 * 9)
 *     pan     -0x40..0x3F, 0 = centre; sent as pan + 0x40 clamped to 0..0x7F
 *     pitch   1/64 octave steps; sent as (s32)(pitch * 18.75f) cents clamped to -1200..1200
 *     returns the handle, or -1: bank not loaded, bank still pending, bank 0x10 / 0x20 while BtlScript_IsSlotBusy(0 / 1, 0)
 *             is set (per-side mute), queue full, or id / volume / pan / pitch out of range.
 *
 * Verified by the matching C: everything above about what the EE does. Inferred: the meaning of each command on the
 * IOP side (from its arguments and callers; SOUNDS.IRX was not read), the Sony HD layout (the offsets used match it)
 * and the names sceSdRemote / sceSdRemoteCallbackInit / sceSifCheckStatRpc / sceSifAllocIopHeap for the callees.
 */
#include "common.h"
#include "sys/snd.h"
#include "sys/adx.h"
#include "sys/dma.h"
#include "sys/file.h"
#include "sys/heap.h"
#include "sys/save.h"

extern void *memset(void *dst, s32 value, u32 size);
extern void FlushCache(s32 mode);
extern void sceSifInitRpc(s32 mode);
extern s32 sceSifBindRpc(SndSifClient *client, s32 id, s32 mode);
extern s32 sceSifCallRpc(SndSifClient *client, s32 fno, s32 mode, void *send, s32 sendSize, void *recv, s32 recvSize,
                         void *endFunc, void *endParam);
extern s32 func_002B4AF0(SndSifClient *client); /* sceSifCheckStatRpc */
extern s32 sceSdRemoteInit(void);
extern s32 func_00296B48(s32 priority);          /* sceSdRemoteCallbackInit */
extern s32 func_002967C0(s32 block, s32 cmd, ...); /* sceSdRemote */

extern void *IopHeap_Alloc(s32 size);  /* sceSifAllocIopHeap wrapper */
extern void IopHeap_Free(void *addr); /* sceSifFreeIopHeap wrapper */
extern void SpuHeap_Init(void);       /* SPU RAM heap: init (0x5010..0x1F0FD0 + 0x5010) */
extern s32 SpuHeap_Alloc(s32 size);    /* SPU RAM heap: alloc, returns the SPU address */
extern void SpuHeap_Free(s32 addr);   /* SPU RAM heap: free */
extern s32 SpuHeap_GetFreeSize(void);        /* SPU RAM heap: bytes in use */

extern s32 BtlScript_IsSlotBusy(s32 side, s32 idx);
extern s32 BtlCharApi_GetSoundCount(s32 side);
extern void BtlCharApi_GetSound(s32 side, s32 idx, s32 *id, s32 *a3, s32 *a4);

/* Sends one command to SOUNDS.IRX and waits for the answer; returns the first reply word. */
s32 Snd_RpcCall(s32 cmd, void *data, s32 size) {
    FlushCache(0);
    sceSifCallRpc(&gSndRpc->client, cmd, 0, data, size, gSndRpc, 0x40, NULL, NULL);
    return gSndRpc->reply[0];
}

/* True while an RPC call on the sound client is still in flight (sceSifCheckStatRpc). */
s32 Snd_RpcIsBusy(void) {
    return func_002B4AF0(&gSndRpc->client);
}

/* Allocates the RPC block, binds to SOUNDS.IRX (retrying until it answers) and sends INIT. */
void Snd_RpcInit(void) {
    s32 i;

    gSndRpc = Heap_Alloc(sizeof(SndRpc), 0x20, 0, 2);
    memset(gSndRpc, 0, sizeof(SndRpc));
    sceSifInitRpc(0);
    while (1) {
        sceSifBindRpc(&gSndRpc->client, SND_RPC_ID, 0);
        if (gSndRpc->client.serve != NULL) {
            break;
        }
        i = 10000;
        while (i--) {
        }
    }
    Snd_RpcCall(SND_RPC_INIT, &gSndRpcBuf, 4);
}

/* RPC 1: shuts the driver down. */
void Snd_RpcTerm(void) {
    Snd_RpcCall(SND_RPC_TERM, &gSndRpcBuf, 4);
}

/* Drops the queued commands and sends RPC 2 (stop everything). */
void Snd_RpcReset(void) {
    memset(&gSndRpc->queue, 0, sizeof(SndQueue));
    Snd_RpcCall(SND_RPC_RESET, &gSndRpcBuf, 4);
}

/* RPC 3: tells the driver where a bank's sample data (SPU), header and third section (IOP) are. */
void Snd_RpcSetBank(s32 mask, s32 spuAddr, void *hdIop, void *seqIop) {
    gSndRpcBuf.w[0] = mask;
    gSndRpcBuf.w[1] = spuAddr;
    gSndRpcBuf.w[2] = (s32)hdIop;
    gSndRpcBuf.w[3] = (s32)seqIop;
    Snd_RpcCall(SND_RPC_SET_BANK, &gSndRpcBuf, 0x10);
}

/* RPC 4: the bank is gone. */
void Snd_RpcFreeBank(s32 mask) {
    gSndRpcBuf.w[0] = mask;
    Snd_RpcCall(SND_RPC_FREE_BANK, &gSndRpcBuf, 4);
}

/* Queues a PLAY command; returns the voice handle (a 16-bit serial) or -1 when the queue is full or an argument is out of range. */
s32 Snd_QueuePlay(s32 mask, u32 id, u32 volume, u32 pan, s32 pitch, s32 loop) {
    SndCmd *cmd;

    if (gSndRpc->queue.count >= SND_QUEUE_MAX) {
        return -1;
    }
    if (id >= 0x100) {
        return -1;
    }
    if (volume >= 0x80) {
        return -1;
    }
    if (pan >= 0x80) {
        return -1;
    }
    if (pitch < -1200 || pitch > 1200) {
        return -1;
    }
    cmd = &gSndRpc->queue.cmd[gSndRpc->queue.count];
    cmd->loop = loop;
    cmd->bank = mask;
    cmd->id = id;
    cmd->volume = volume;
    cmd->pan = pan;
    cmd->pitch = pitch;
    cmd->type = SND_CMD_PLAY;
    cmd->handle = gSndSerial;
    gSndRpc->queue.count++;
    gSndSerial++;
    return cmd->handle;
}

/* Queues a STOP for every voice playing sample `id` of the bank. */
void Snd_QueueStopId(s32 mask, u32 id) {
    SndCmd *cmd;

    if (gSndRpc->queue.count < SND_QUEUE_MAX) {
        if (id < 0x100) {
            cmd = &gSndRpc->queue.cmd[gSndRpc->queue.count];
            cmd->type = SND_CMD_STOP_ID;
            cmd->bank = mask;
            cmd->id = id;
            gSndRpc->queue.count++;
        }
    }
}

/* Queues a STOP for the voice a PLAY returned `handle` for. */
void Snd_QueueStopHandle(s32 handle) {
    SndCmd *cmd;

    if (gSndRpc->queue.count < SND_QUEUE_MAX) {
        if (handle >= 0) {
            cmd = &gSndRpc->queue.cmd[gSndRpc->queue.count];
            cmd->type = SND_CMD_STOP_HANDLE;
            cmd->handle = handle;
            gSndRpc->queue.count++;
        }
    }
}

/* RPC 0xB (two words). No callers reach it. */
void Snd_RpcCmdB(s32 a, s32 b) {
    gSndRpcBuf.w[0] = a;
    gSndRpcBuf.w[1] = b;
    Snd_RpcCall(SND_RPC_CMD_B, &gSndRpcBuf, 8);
}

/* RPC 0xC (one word). No callers reach it. */
void Snd_RpcCmdC(s32 a) {
    gSndRpcBuf.w[0] = a;
    Snd_RpcCall(SND_RPC_CMD_C, &gSndRpcBuf, 4);
}

/* Once per frame: RPC 9 (driver tick), then the queued commands as one RPC 0xD, then clears the queue. */
void Snd_RpcFlush(void) {
    Snd_RpcCall(SND_RPC_TICK, &gSndRpcBuf, 4);
    if (gSndRpc->queue.count != 0) {
        FlushCache(0);
        gSndRpcBuf.queue = gSndRpc->queue;
        Snd_RpcCall(SND_RPC_QUEUE, &gSndRpcBuf, sizeof(SndQueue));
    }
    memset(&gSndRpc->queue, 0, sizeof(SndQueue));
}

/* RPC 5: mono (1) or stereo (0) output. */
void Snd_RpcSetMono(s32 mono) {
    gSndRpcBuf.w[0] = mono;
    Snd_RpcCall(SND_RPC_SET_MONO, &gSndRpcBuf, 4);
}

/* RPC 6: pauses (1) or resumes (0) the voices of the banks in mask. */
void Snd_RpcSetPause(s32 mask, s32 on) {
    gSndRpcBuf.w[0] = mask;
    gSndRpcBuf.w[1] = on;
    Snd_RpcCall(SND_RPC_SET_PAUSE, &gSndRpcBuf, 8);
}

/* RPC 0xA: stops the voices of the banks in mask. */
void Snd_RpcStopBank(s32 mask) {
    gSndRpcBuf.w[0] = mask;
    Snd_RpcCall(SND_RPC_STOP_BANK, &gSndRpcBuf, 4);
}

/* RPC 7: volume (0..0x7F) of the banks in mask. */
void Snd_RpcSetVolume(s32 mask, s32 volume) {
    gSndRpcBuf.w[0] = mask;
    gSndRpcBuf.w[1] = volume;
    Snd_RpcCall(SND_RPC_SET_VOLUME, &gSndRpcBuf, 8);
}

/* RPC 8 (one word). No callers reach it. */
void Snd_RpcCmd8(s32 a) {
    gSndRpcBuf.w[0] = a;
    Snd_RpcCall(SND_RPC_CMD_8, &gSndRpcBuf, 4);
}

/* Starts a DMA upload of IOP memory to SPU RAM: sceSdRemote(1, rSdVoiceTrans, core 1, write, iopAddr, spuAddr, size). */
void Snd_SpuTransfer(void *iopAddr, s32 spuAddr, s32 size) {
    func_002967C0(1, 0x80D0, 1, 0, iopAddr, spuAddr, size);
}

/* Blocks until the SPU upload is over: sceSdRemote(1, rSdVoiceTransStatus, core 1, wait). */
s32 Snd_SpuTransferWait(void) {
    return func_002967C0(1, 0x80F0, 1, 1);
}

/* Polls the SPU upload: sceSdRemote(1, rSdVoiceTransStatus, core 1, check); 1 = finished. */
s32 Snd_SpuTransferCheck(void) {
    return func_002967C0(1, 0x80F0, 1, 0);
}

/* RPC 0xE (no arguments). No callers reach it. */
void Snd_RpcCmdE(void) {
    Snd_RpcCall(SND_RPC_CMD_E, &gSndRpcBuf, 4);
}

/* RPC 0xF: a count and that many 16-bit ids (the fighters on the field). */
void Snd_RpcSetFighters(s32 count, s32 *ids) {
    s32 i;

    gSndRpcBuf.h[0] = count;
    for (i = 0; i < count; i++) {
        gSndRpcBuf.h[i + 1] = ids[i];
    }
    Snd_RpcCall(SND_RPC_FIGHTERS, &gSndRpcBuf, 0x18);
}

/* Bank mask -> slot number: the position of the lowest set bit (0 when none of the low 8 is set). */
s32 Snd_BankIndex(u32 mask) {
    s32 idx = 0;
    s32 i;

    for (i = 0; i < SND_BANK_COUNT; i++) {
        if (mask & (1 << i)) {
            idx = i;
            break;
        }
    }
    return idx;
}

/* Volume sent to the driver: request * bank level / 128 * SE option / 9, clamped to 0..0x7F. */
s32 Snd_ScaleVolume(s32 bank, s32 volume) {
    volume *= gSndBankVolume[bank];
    volume *= gSaveData->seVolume;
    volume /= 0x480;
    if (volume > 0x7F) {
        volume = 0x7F;
    }
    if (volume < 0) {
        volume = 0;
    }
    return volume;
}

/* Pan sent to the driver: -0x40..0x3F around the centre -> 0..0x7F. */
s32 Snd_ScalePan(s32 pan) {
    s32 v = pan + 0x40;

    if (v > 0x7F) {
        v = 0x7F;
    }
    if (v < 0) {
        v = 0;
    }
    return v;
}

/* Pitch sent to the driver: 64 units per octave -> cents, clamped to one octave either way. */
s32 Snd_ScalePitch(s32 pitch) {
    s32 v = (f32)pitch * 18.75f;

    if (v > 1200) {
        v = 1200;
    }
    if (v < -1200) {
        v = -1200;
    }
    return v;
}

/* Fills a bank slot from a bank file: section pointers and sizes, the loop flags, and (alloc) SPU and IOP memory. */
void Snd_SetupBank(SndBank *bank, u32 mask, SndBankFile *file, s32 alloc) {
    s32 size;

    bank->bodySize = file->hdOfs - file->bodyOfs;
    bank->body = (u32 *)file + file->bodyOfs / 4;
    bank->hdSize = file->seqOfs - file->hdOfs;
    bank->hd = (u32 *)file + file->hdOfs / 4;
    bank->seqSize = file->endOfs - file->seqOfs;
    bank->seq = (u32 *)file + file->seqOfs / 4;
    if (alloc) {
        size = file->hdOfs - file->bodyOfs;
        switch (mask) {
            case 0x10:
            case 0x20:
                size = 0x8F000;
                break;
            case 8:
                size = 0x13800;
                break;
        }
        bank->spuSize = size;
        bank->spuAddr = SpuHeap_Alloc(size);

        size = file->seqOfs - file->hdOfs;
        switch (mask) {
            case 0x10:
            case 0x20:
                size = 0x1000;
                break;
            case 8:
                size = 0x800;
                break;
        }
        bank->hdIopSize = size;
        bank->hdIop = IopHeap_Alloc(size);

        size = file->endOfs - file->seqOfs;
        switch (mask) {
            case 0x10:
            case 0x20:
                size = 0x800;
                break;
            case 8:
                size = 0x800;
                break;
        }
        bank->seqIopSize = size;
        bank->seqIop = IopHeap_Alloc(size);
    }
    bank->vagMax = SndHd_GetVagMax(file);
    SndHd_GetLoopBits(file, bank->loopBits);
}

/* Copies a bank to the IOP: header and third section at once; the samples now (sync) or later from Snd_Update. */
void Snd_SendBank(SndBank *bank, s32 sync) {
    Dma_SendToIop(bank->hdIop, bank->hd, bank->hdSize);
    Dma_SendToIop(bank->seqIop, bank->seq, bank->seqSize);
    if (sync) {
        Dma_SendToIop(gSndMgr->iopBuf, bank->body, bank->bodySize);
        Snd_SpuTransfer(gSndMgr->iopBuf, bank->spuAddr, bank->bodySize);
        Snd_SpuTransferWait();
    } else {
        bank->uploading = 0;
        bank->pending = 1;
    }
}

/* Allocates the manager and the IOP staging buffer, starts libsdr and the RPC link, sets full volume. */
void Snd_Init(void) {
    gSndMgr = Heap_Alloc(sizeof(SndMgr), 0x20, 0, 2);
    memset(gSndMgr, 0, sizeof(SndMgr));
    gSndMgr->iopBuf = IopHeap_Alloc(0xA4800);
    SpuHeap_Init();
    sceSdRemoteInit();
    func_00296B48(5);
    Snd_RpcInit();
    Snd_SetMasterVolume(0x7F);
}

/* Shuts the driver down and frees everything. No callers. (The loop unloads only empty slots, i.e. nothing.) */
void Snd_Term(void) {
    SndBank *bank;
    s32 i;

    Snd_RpcTerm();
    for (i = 0, bank = gSndMgr->bank; i < SND_BANK_COUNT; i++, bank++) {
        if (bank->spuAddr == 0) {
            Snd_UnloadBank(1 << i);
        }
    }
    IopHeap_Free(gSndMgr->iopBuf);
    gSndMgr->iopBuf = NULL;
    Heap_Free(gSndMgr);
    gSndMgr = NULL;
}

/* Stops everything: drops the queue, resets the driver, waits out the SPU upload and forgets the pending ones. */
void Snd_Reset(void) {
    SndBank *bank;
    s32 i;

    Snd_RpcReset();
    Snd_SpuTransferWait();
    for (i = 0, bank = gSndMgr->bank; i < SND_BANK_COUNT; i++, bank++) {
        bank->pending = 0;
        bank->uploading = 0;
    }
}

/* Per frame: sends the tick and the queued commands, then moves the deferred sample uploads along, one bank at a time. */
void Snd_Update(void) {
    SndBank *bank;
    s32 busy = 0;
    s32 i;

    Snd_RpcFlush();
    for (i = 0, bank = gSndMgr->bank; i < SND_BANK_COUNT; i++, bank++) {
        if (bank->uploading) {
            if (Snd_SpuTransferCheck() == 1) {
                bank->pending = 0;
                bank->uploading = 0;
            } else {
                busy = 1;
            }
            break;
        }
    }
    if (!busy) {
        for (i = 0, bank = gSndMgr->bank; i < SND_BANK_COUNT; i++, bank++) {
            if (bank->pending) {
                void *buf;

                bank->uploading = 1;
                buf = gSndMgr->iopBuf;
                Dma_SendToIop(buf, bank->body, bank->bodySize);
                Snd_SpuTransfer(buf, bank->spuAddr, bank->bodySize);
                break;
            }
        }
    }
}

/* Loads a bank file into the slot named by mask (nothing happens when the slot is in use). sync = upload the samples before returning. */
void Snd_LoadBank(u32 mask, SndBankFile *file, s32 sync) {
    s32 idx = Snd_BankIndex(mask);
    SndBank *bank;

    if (!Snd_IsBankLoaded(mask)) {
        bank = &gSndMgr->bank[idx];
        if (bank->pending) {
            if (bank->uploading) {
                Snd_SpuTransferWait();
            }
            bank->pending = 0;
            bank->uploading = 0;
        }
        Snd_SetupBank(bank, mask, file, 1);
        Snd_SendBank(bank, sync);
        Snd_RpcSetBank(mask, bank->spuAddr, bank->hdIop, bank->seqIop);
    }
}

/* Replaces the contents of a loaded fixed-size slot (masks 8, 0x10, 0x20) without reallocating it. */
void Snd_ReloadBank(u32 mask, SndBankFile *file, s32 sync) {
    s32 fixed;
    s32 idx;
    SndBank *bank;

    switch (mask) {
        case 8:
        case 0x10:
        case 0x20:
            fixed = 1;
            break;
        default:
            fixed = 0;
            break;
    }
    idx = Snd_BankIndex(mask);
    if (fixed && Snd_IsBankLoaded(mask)) {
        bank = &gSndMgr->bank[idx];
        if (bank->pending) {
            if (bank->uploading) {
                Snd_SpuTransferWait();
            }
            bank->pending = 0;
            bank->uploading = 0;
        }
        Snd_StopBank(mask);
        Snd_SetupBank(bank, mask, file, 0);
        Snd_SendBank(bank, sync);
        Snd_RpcSetBank(mask, bank->spuAddr, bank->hdIop, bank->seqIop);
    }
}

/* Reads bank file `fileId` from the archive, loads it synchronously and frees the file. */
void Snd_LoadBankFile(u32 mask, s32 fileId) {
    void *data = File_LoadSync(fileId, NULL, 0);

    Snd_LoadBank(mask, data, 1);
    Heap_Free(data);
}

/* Frees a bank's SPU and IOP memory and tells the driver. */
void Snd_UnloadBank(u32 mask) {
    s32 idx = Snd_BankIndex(mask);
    SndBank *bank;

    if (Snd_IsBankLoaded(mask)) {
        bank = &gSndMgr->bank[idx];
        SpuHeap_Free(bank->spuAddr);
        bank->spuAddr = 0;
        IopHeap_Free(bank->hdIop);
        bank->hdIop = NULL;
        IopHeap_Free(bank->seqIop);
        bank->seqIop = NULL;
        Snd_RpcFreeBank(mask);
    }
}

/* True when no bank still waits for its sample upload. */
s32 Snd_IsUploadDone(void) {
    SndBank *bank = gSndMgr->bank;
    s32 done = 1;
    s32 i;

    for (i = 0; i < SND_BANK_COUNT; i++, bank++) {
        if (bank->pending == 0) {
            continue;
        }
        done = 0;
        break;
    }
    return done;
}

/* Pushes the SPU heap's current usage on a small stack. No callers. */
void Snd_PushSpuMark(void) {
    gSndMgr->spuMark[gSndMgr->spuMarkCount] = SpuHeap_GetFreeSize();
    gSndMgr->spuMarkCount++;
}

/* Pops that stack and returns the current usage. No callers. */
s32 Snd_PopSpuMark(void) {
    gSndMgr->spuMarkCount--;
    return SpuHeap_GetFreeSize();
}

/* Options: stereo (non-zero) or mono (0) for the driver and for the ADX streams. */
void Snd_SetStereo(s32 stereo) {
    if (stereo) {
        Snd_RpcSetMono(0);
        Adx_SetMono(0);
    } else {
        Snd_RpcSetMono(1);
        Adx_SetMono(1);
    }
}

/* Volume of all eight banks at once. */
void Snd_SetMasterVolume(s32 volume) {
    Snd_RpcSetVolume(SND_BANK_ALL, volume);
}

/* Plays sample `id` of a bank at volume 0x40, centre, unshifted. */
#ifdef PORT
/* Set while the online pause menu of the OTHER player runs (btl_seq.c BtlSeqFight_Update): its sounds are asked for
   as on that player's copy, so that both copies do the same, but with no volume: the menu is that player's alone. */
s32 gPortSeSilent;
#endif

s32 Snd_PlaySe(u32 mask, s32 id) {
    return Snd_PlaySeEx(mask, id, 0x40, 0, 0);
}

/* Plays sample `id` of a bank: volume 0..0x7F (before the bank and option scaling), pan -0x40..0x3F, pitch in 1/64 octave.
   Returns the voice handle, or -1 when the bank is missing, still uploading or muted, or the queue is full. */
s32 Snd_PlaySeEx(u32 mask, s32 id, s32 volume, s32 pan, s32 pitch) {
    s32 idx = Snd_BankIndex(mask);
    s32 side;

    if (!Snd_IsBankLoaded(mask)) {
        return -1;
    }
    if (gSndMgr->bank[idx].pending) {
        return -1;
    }
    /* Banks 0x10 / 0x20 belong to side 0 / 1 and can be muted per side. (This shape is what matches.) */
    if (mask != SND_BANK_SIDE_0) {
        side = 1;
        if (mask != SND_BANK_SIDE_1) {
            goto play;
        }
    } else {
        side = 0;
    }
    if (BtlScript_IsSlotBusy(side, 0)) {
        return -1;
    }
play:
    volume = Snd_ScaleVolume(idx, volume);
    pan = Snd_ScalePan(pan);
    pitch = Snd_ScalePitch(pitch);
#ifdef PORT
    if (gPortSeSilent) { /* (the same request on both copies of an online match, heard on one: see the variable) */
        volume = 0;
    }
#endif
    return Snd_QueuePlay(mask, id, volume, pan, pitch, Snd_IsLoopSe(mask, id));
}

/* Stops every voice playing sample `id` of a loaded bank. No callers. */
void Snd_StopSe(u32 mask, s32 id) {
    if (Snd_IsBankLoaded(mask)) {
        Snd_QueueStopId(mask, id);
    }
}

/* Stops the voice a Snd_PlaySe call returned `handle` for. */
void Snd_StopHandle(s32 handle) {
    Snd_QueueStopHandle(handle);
}

/* Public name of RPC 0xB. No callers. */
void Snd_CmdB(s32 a, s32 b) {
    Snd_RpcCmdB(a, b);
}

/* Public name of RPC 0xC. No callers. */
void Snd_CmdC(s32 a) {
    Snd_RpcCmdC(a);
}

/* Pauses (1) or resumes (0) the banks in mask; the battle pause uses (4, 1) / (4, 0). */
void Snd_SetPause(s32 mask, s32 on) {
    Snd_RpcSetPause(mask, on);
}

/* Stops the voices of the banks in mask. */
void Snd_StopBank(s32 mask) {
    Snd_RpcStopBank(mask);
}

/* Stops the voices of the banks in mask and clears their pause; the battle restart uses mask 0x3C. */
void Snd_StopBankAndResume(s32 mask) {
    Snd_StopBank(mask);
    Snd_SetPause(mask, 0);
}

/* Public name of RPC 8. No callers. */
void Snd_Cmd8(s32 a) {
    Snd_RpcCmd8(a);
}

/* The loop flag of sample `id` (0 when the bank is not loaded). */
s32 Snd_IsLoopSe(u32 mask, s32 id) {
    s32 idx = Snd_BankIndex(mask);
    SndBank *bank;

    if (Snd_IsBankLoaded(mask)) {
        bank = &gSndMgr->bank[idx];
        return (bank->loopBits[id / 8] >> (id % 8)) & 1;
    }
    /* no return value here in the original: the caller gets Snd_IsBankLoaded's 0 */
}

/* The highest sample number of a bank (0 when it is not loaded). No callers. */
s32 Snd_GetVagMax(u32 mask) {
    s32 idx = Snd_BankIndex(mask);
    SndBank *bank;

    if (Snd_IsBankLoaded(mask)) {
        bank = &gSndMgr->bank[idx];
        return bank->vagMax;
    }
    /* no return value here in the original: the caller gets Snd_IsBankLoaded's 0 */
}

/* True when the slot named by mask holds a bank. */
s32 Snd_IsBankLoaded(u32 mask) {
    return gSndMgr->bank[Snd_BankIndex(mask)].spuAddr != 0;
}

/* Public name of RPC 0xE. No callers. */
void Snd_CmdE(void) {
    Snd_RpcCmdE();
}

/* Battle, per frame: sends the ids of both sides' fighters to the driver (RPC 0xF). */
void Snd_SendFighters(void) {
    s32 ids[12];
    s32 count = 0;
    s32 i;

    for (i = 0; i < BtlCharApi_GetSoundCount(0); i++) {
        BtlCharApi_GetSound(0, i, &ids[count], NULL, NULL);
        count++;
    }
    for (i = 0; i < BtlCharApi_GetSoundCount(1); i++) {
        BtlCharApi_GetSound(1, i, &ids[count], NULL, NULL);
        count++;
    }
    Snd_RpcSetFighters(count, ids);
}

/* Sony HD header chunk -> its vag-info chunk (offset at +0x30 of the header section, word aligned). */
SndVagInfoChunk *SndHd_GetVagInfo(u32 *head) {
    return (SndVagInfoChunk *)(head + head[0xC] / 4);
}

/* The highest sample number in a bank file's header. */
s32 SndHd_GetVagMax(SndBankFile *file) {
    return SndHd_GetVagInfo((u32 *)file + file->hdOfs / 4)->maxNumber;
}

/* Writes one byte per sample: 1 when it loops. */
void SndHd_GetLoopFlags(SndBankFile *file, u8 *flags) {
    SndVagInfoChunk *chunk = SndHd_GetVagInfo((u32 *)file + file->hdOfs / 4);
    SndVagParam *param = (SndVagParam *)&chunk->offset[chunk->maxNumber + 1];
    u32 i;

    for (i = 0; i <= chunk->maxNumber; i++) {
        if (param->loop) {
            *flags = 1;
        } else {
            *flags = 0;
        }
        param++;
        flags++;
    }
}

/* Packs the loop flags into a bit array (bits are only ever set, so the caller clears it first). */
void SndHd_GetLoopBits(SndBankFile *file, u8 *bits) {
    u8 flags[SND_VAG_MAX];
    s32 count = SndHd_GetVagMax(file);
    s32 i;

    SndHd_GetLoopFlags(file, flags);
    for (i = 0; i < count; i++) {
        if (flags[i]) {
            bits[i / 8] |= 1 << (i % 8);
        }
    }
}
