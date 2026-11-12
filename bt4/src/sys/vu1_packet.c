/*
 * VU1 draw packets.   0x123130..0x123F48
 *
 * Not sound code: this is the packet layer of the 3D renderer at 0x10AD58..0x116xxx (its only callers), placed between
 * the pad and sound objects. No function here uses $gp. Everything is built in the frame's DMA buffer (Dma_Alloc /
 * Dma_AddRef) as VIF1 source-chain packets for nine VU1 microprograms, which sit right after .text:
 *     prog 0  0x2BF6B0..0x2BFAB0     prog 3  0x2C01C0..0x2C04F0     prog 6  0x2C1F00..0x2C2D30
 *     prog 1  0x2BFAB0..0x2BFEA0     prog 4  0x2C04F0..0x2C1180     prog 7  0x2C2D30..0x2C3080
 *     prog 2  0x2BFEA0..0x2C01C0     prog 5  0x2C1180..0x2C1F00     prog 8  0x2C3080..0x2C3380
 * (the program numbers are only the address order. "prog 3" here is the second variant of program 2, called 2b in
 * the listings: src/vu1/prog*.vsm, described in docs/systems/vu1/README.md.)
 *
 * Each program has a pair of builders:
 *   Vu1Pkt_LoadProgN  a "ref" tag that uploads the microprogram, then a "cnt" packet: FLUSHE, UNPACK V4-32 of the
 *                     program's constant block to VU address 0, BASE / OFFSET for its double buffer.
 *   Vu1Pkt_CallProgN  a "call" packet: FLUSHE, UNPACK of the per-object constants, MSCALF 0, BASE / OFFSET, then
 *                     the DMA chain of the object's vertex data (which ends in "ret").
 * The builders fill in the matrices they are given or the current VU0 ones (vf16-19 local-to-world, vf20-23 and
 * vf24-27 the two camera matrices); the ones that return the packet leave the remaining constants to the caller.
 * A port replaces the whole layer: the packets are meaningless without the VU1 programs.
 *
 * The Vu1Node functions walk a model's node list (see Vu1Node): keyframe posing, per-node draw packets for program 8,
 * and a flag patch in the nodes' VIF streams.
 */
#include "common.h"
#include "sys/vu1_packet.h"
#include "sys/dma.h"

extern void *memset(void *dst, s32 value, u32 size);
extern void Vec4_Set(void *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(void *dst, void *src);
extern void Mtx_StoreIdentity(void *m);
extern void Mtx_Mul(void *dst, void *a, void *b); /* matrix product */
extern void Mtx_Copy(void *dst, void *src);        /* 4x4 matrix copy */
extern void Vu0Cur_Push(void);                        /* VU0 matrix stack: push vf16-19 */
extern void Vu0Cur_PopN(s32 count);                   /* VU0 matrix stack: pop count */
extern void Vu0Cur_ResetStack(void);                        /* VU0 matrix stack: reset (vi15 = 0) */
extern void Vu0Cur_LoadMtx(void *m);                     /* vf16-19 = m */
extern void Vu0Cur_StoreMtx(void *m);                     /* m = vf16-19 (current local-to-world) */
extern void Vu0Cur_TranslateLocal(void *vec);                   /* applies a translation to vf16-19 */
extern void Vu0Cur_RotateYXZ(void *vec);                   /* applies a rotation to vf16-19 */
extern void Vu0Clip_StoreMtx(void *m);                     /* m = vf20-23 */
extern void Vu0Screen_StoreMtx(void *m);                     /* m = vf24-27 */

/* Program 7 draw: current matrix, the two camera matrices and w, then the caller's vertex chain. */
u32 *Vu1Pkt_CallProg7(u32 chain, f32 w) {
    u32 *p = Dma_Alloc(0xF0);

    Vu0Cur_StoreMtx(p + 4);
    Vu0Screen_StoreMtx(p + 0x14);
    Vu0Clip_StoreMtx(p + 0x24);
    ((f32 *)p)[0x37] = w;
    p[0] = VU1_DMA_CALL | 0xE;
    p[1] = chain & VU1_ADDR_MASK;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0xD, 0);
    p[0x38] = VU1_VIF_MSCALF(0);
    p[0x39] = VU1_VIF_BASE(0xD);
    p[0x3A] = VU1_VIF_OFFSET(0x1F9);
    p[0x3B] = VU1_VIF_NOP;
    return p;
}

/* Program 7 setup: uploads the microprogram and the two camera matrices. */
u32 *Vu1Pkt_LoadProg7(void) {
    u32 *p;

    Dma_AddRef(D_002C2D30, D_002C3080 - D_002C2D30);
    p = Dma_Alloc(0xF0);
    Vu0Screen_StoreMtx(p + 0x14);
    Vu0Clip_StoreMtx(p + 0x24);
    p[0] = VU1_DMA_CNT | 0xE;
    p[1] = 0;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0xD, 0);
    p[0x38] = VU1_VIF_NOP;
    p[0x39] = VU1_VIF_BASE(0xD);
    p[0x3A] = VU1_VIF_OFFSET(0x1F9);
    p[0x3B] = VU1_VIF_NOP;
    return p;
}

/* Program 0 draw: 0x22 quadwords of constants for the caller to fill, then the caller's chain. */
u32 *Vu1Pkt_CallProg0(u32 chain) {
    u32 *p = Dma_Alloc(0x240);

    p[0] = VU1_DMA_CALL | 0x23;
    p[1] = chain & VU1_ADDR_MASK;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0x22, 0);
    p[0x8C] = VU1_VIF_MSCALF(0);
    p[0x8D] = VU1_VIF_BASE(0x22);
    p[0x8E] = VU1_VIF_OFFSET(0x1EF);
    p[0x8F] = VU1_VIF_NOP;
    return p;
}

/* Program 0 setup: uploads the microprogram and reserves its constant block. */
u32 *Vu1Pkt_LoadProg0(void) {
    u32 *p;

    Dma_AddRef(D_002BF6B0, D_002BFAB0 - D_002BF6B0);
    p = Dma_Alloc(0x240);
    p[0] = VU1_DMA_CNT | 0x23;
    p[1] = 0;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0x22, 0);
    p[0x8C] = VU1_VIF_NOP;
    p[0x8D] = VU1_VIF_BASE(0x22);
    p[0x8E] = VU1_VIF_OFFSET(0x1EF);
    p[0x8F] = VU1_VIF_NOP;
    return p;
}

/* Program 1 draw: as program 0, for an object whose chain starts at +0x60. */
u32 *Vu1Pkt_CallProg1(u32 obj) {
    u32 *p = Dma_Alloc(0x240);

    p[0] = VU1_DMA_CALL | 0x23;
    p[1] = (obj + 0x60) & VU1_ADDR_MASK;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0x22, 0);
    p[0x8C] = VU1_VIF_MSCALF(0);
    p[0x8D] = VU1_VIF_BASE(0x22);
    p[0x8E] = VU1_VIF_OFFSET(0x1EF);
    p[0x8F] = VU1_VIF_NOP;
    return p;
}

/* Program 1 setup. */
u32 *Vu1Pkt_LoadProg1(void) {
    u32 *p;

    Dma_AddRef(D_002BFAB0, D_002BFEA0 - D_002BFAB0);
    p = Dma_Alloc(0x240);
    p[0] = VU1_DMA_CNT | 0x23;
    p[1] = 0;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0x22, 0);
    p[0x8C] = VU1_VIF_NOP;
    p[0x8D] = VU1_VIF_BASE(0x22);
    p[0x8E] = VU1_VIF_OFFSET(0x1EF);
    p[0x8F] = VU1_VIF_NOP;
    return p;
}

/* Program 2/3 draw: 0xB quadwords of per-object constants, then the object's chain at +0x60. */
u32 *Vu1Pkt_CallProg2(u32 obj) {
    u32 *p = Dma_Alloc(0xD0);

    p[0] = VU1_DMA_CALL | 0xC;
    p[1] = (obj + 0x60) & VU1_ADDR_MASK;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0xB, 0);
    p[0x30] = VU1_VIF_MSCALF(0);
    p[0x31] = VU1_VIF_BASE(0x13);
    p[0x32] = VU1_VIF_OFFSET(0x1F6);
    p[0x33] = VU1_VIF_NOP;
    return p;
}

/* Program 2 (alt = 0) or 3 setup: uploads the microprogram and reserves 0x13 quadwords of constants. */
u32 *Vu1Pkt_LoadProg2(s32 alt) {
    u32 *p;

    if (alt) {
        Dma_AddRef(D_002C01C0, D_002C04F0 - D_002C01C0);
    } else {
        Dma_AddRef(D_002BFEA0, D_002C01C0 - D_002BFEA0);
    }
    p = Dma_Alloc(0x150);
    p[0] = VU1_DMA_CNT | 0x14;
    p[1] = 0;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0x13, 0);
    p[0x50] = VU1_VIF_NOP;
    p[0x51] = VU1_VIF_BASE(0x13);
    p[0x52] = VU1_VIF_OFFSET(0x1F6);
    p[0x53] = VU1_VIF_NOP;
    return p;
}

/* Program 4 draw: runs the caller's chain with the constants already loaded. */
void Vu1Pkt_CallProg4(u32 chain) {
    u32 *p = Dma_Alloc(0x20);

    p[0] = VU1_DMA_CALL | 1;
    p[1] = chain & VU1_ADDR_MASK;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_NOP;
    p[4] = VU1_VIF_MSCALF(0);
    p[5] = VU1_VIF_BASE(0x8D);
    p[6] = VU1_VIF_OFFSET(0x1B4);
    p[7] = VU1_VIF_NOP;
}

/* Program 4 setup: uploads the microprogram, the two camera matrices and the given matrix. */
void Vu1Pkt_LoadProg4(void *mtx) {
    u32 *p;

    Dma_AddRef(D_002C04F0, D_002C1180 - D_002C04F0);
    p = Dma_Alloc(0xE0);
    Mtx_Copy(p + 0x24, mtx);
    Vu0Screen_StoreMtx(p + 4);
    Vu0Clip_StoreMtx(p + 0x14);
    p[0] = VU1_DMA_CNT | 0xD;
    p[1] = 0;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0xC, 0);
    p[0x34] = VU1_VIF_NOP;
    p[0x35] = VU1_VIF_BASE(0x8D);
    p[0x36] = VU1_VIF_OFFSET(0x1B4);
    p[0x37] = VU1_VIF_NOP;
}

/* Program 5 draw: one matrix, then the caller's chain. No callers. */
void Vu1Pkt_CallProg5(u32 chain, void *mtx) {
    u32 *p = Dma_Alloc(0x60);

    Mtx_Copy(p + 4, mtx);
    p[0] = VU1_DMA_CALL | 5;
    p[1] = chain & VU1_ADDR_MASK;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(4, 0);
    p[0x14] = VU1_VIF_MSCALF(0);
    p[0x15] = VU1_VIF_BASE(0x8D);
    p[0x16] = VU1_VIF_OFFSET(0x1B4);
    p[0x17] = VU1_VIF_NOP;
}

/* Program 5 setup: identity, the two camera matrices and the given matrix. No callers. */
void Vu1Pkt_LoadProg5(void *mtx) {
    u32 *p;

    Dma_AddRef(D_002C1180, D_002C1F00 - D_002C1180);
    p = Dma_Alloc(0x120);
    Mtx_StoreIdentity(p + 4);
    Mtx_Copy(p + 0x34, mtx);
    Vu0Screen_StoreMtx(p + 0x14);
    Vu0Clip_StoreMtx(p + 0x24);
    p[0] = VU1_DMA_CNT | 0x11;
    p[1] = 0;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0x10, 0);
    p[0x44] = VU1_VIF_NOP;
    p[0x45] = VU1_VIF_BASE(0x8D);
    p[0x46] = VU1_VIF_OFFSET(0x1B4);
    p[0x47] = VU1_VIF_NOP;
}

/* ORs bits into words 4 and 5 (the second shifted left 5) of every node-header block in a node list's VIF streams. */
void Vu1Node_OrFlags(Vu1Node *node, u32 bits0, u32 bits1) {
    u32 *p;

    while (1) {
        if (node->hasMesh) {
            for (p = node->vif; *p != VU1_DMA_END; p++) {
                if (*p == VU1_VIF_NODE_HEADER) {
                    p[5] |= bits0;
                    p[6] |= bits1 << 5;
                }
            }
        }
        if (node->last) {
            break;
        }
        node = (Vu1Node *)((u8 *)node + node->size);
    }
}

/* Poses a node list at `frame`: per node, position and rotation from its key track (the pair of keys around the frame,
   interpolated linearly), then the node matrices through the VU0 matrix stack.
   The shape the code needs: the later key is a walking pointer stepped at the top of the body, the earlier key is
   written `(next - 1)` at every use (so its frame number is reloaded in each block while the later key's stays in a
   register), the key count is a local, and the frame word is read as the fourth float of `pos` when it is copied. */
void Vu1Node_Animate(Vu1Node *node, Vu1Track *track, void *mtx, f32 frame) {
    Vu1Key *next;
    s32 i;
    s32 count;
    f32 t;

    Vu0Cur_ResetStack();
    Vu0Cur_LoadMtx(mtx);
    while (1) {
        Vu0Cur_Push();
        if (track != NULL) {
            count = track->keyCount;
            next = track->key;
            for (i = 1; i < count; i++) {
                next++;
                if ((f32)(next - 1)->frame == frame) {
                    Vec4_Set(node->pos, (next - 1)->pos[0], (next - 1)->pos[1], (next - 1)->pos[2], (next - 1)->pos[3]);
                    Vec4_Set(node->rot, (next - 1)->rot[0], (next - 1)->rot[1], (next - 1)->rot[2], (next - 1)->rot[3]);
                    break;
                }
                if ((f32)(next - 1)->frame < frame && frame < (f32)next->frame) {
                    t = (frame - (f32)(next - 1)->frame) / (f32)(next->frame - (next - 1)->frame);
                    node->pos[0] = (next->pos[0] - (next - 1)->pos[0]) * t + (next - 1)->pos[0];
                    node->pos[1] = (next->pos[1] - (next - 1)->pos[1]) * t + (next - 1)->pos[1];
                    node->pos[2] = (next->pos[2] - (next - 1)->pos[2]) * t + (next - 1)->pos[2];
                    node->rot[0] = (next->rot[0] - (next - 1)->rot[0]) * t + (next - 1)->rot[0];
                    node->rot[1] = (next->rot[1] - (next - 1)->rot[1]) * t + (next - 1)->rot[1];
                    node->rot[2] = (next->rot[2] - (next - 1)->rot[2]) * t + (next - 1)->rot[2];
                    break;
                }
                if ((f32)next->frame == frame) {
                    Vec4_Set(node->pos, next->pos[0], next->pos[1], next->pos[2], next->pos[3]);
                    Vec4_Set(node->rot, next->rot[0], next->rot[1], next->rot[2], next->rot[3]);
                    break;
                }
            }
        }
        node->pos[3] = 1.0f;
        node->rot[3] = 1.0f;
        Vu0Cur_StoreMtx(node->parent);
        Vu0Cur_TranslateLocal(node->pos);
        Vu0Cur_RotateYXZ(node->rot);
        Vu0Cur_StoreMtx(node->world);
        Vec4_Copy(node->unkD0, node->unk30);
        Vec4_Copy(node->unkE0, node->unk40);
        if (node->popCount) {
            Vu0Cur_PopN(node->popCount);
        }
        if (node->last) {
            break;
        }
        node = (Vu1Node *)((u8 *)node + node->size);
        if (track != NULL) {
            track = (Vu1Track *)&track->key[track->keyCount];
        }
    }
}

/* Queues one program 8 draw per node that has a mesh, with the node's matrices and vectors. */
void Vu1Node_Draw(Vu1Node *node) {
    u32 *p;

    while (1) {
        if (node->hasMesh) {
            p = Vu1Pkt_CallProg8(node);
            Mtx_Copy(p + 0x14, node->parent);
            Mtx_Copy(p + 4, node->world);
            Vec4_Copy(p + 0x24, node->unkD0);
            Vec4_Copy(p + 0x28, node->unkE0);
        }
        if (node->last) {
            break;
        }
        node = (Vu1Node *)((u8 *)node + node->size);
    }
}

/* Program 8 draw: 0xA quadwords of per-node constants, then the node's VIF stream. */
u32 *Vu1Pkt_CallProg8(Vu1Node *node) {
    u32 *p = Dma_Alloc(0xC0);

    p[0] = VU1_DMA_CALL | 0xB;
    p[1] = (u32)node->vif & VU1_ADDR_MASK;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0xA, 0);
    p[0x2C] = VU1_VIF_MSCALF(0);
    p[0x2D] = VU1_VIF_BASE(0x12);
    p[0x2E] = VU1_VIF_OFFSET(0x1F7);
    p[0x2F] = VU1_VIF_NOP;
    return p;
}

/* Program 8 setup: uploads the microprogram and reserves 0x12 quadwords of constants. */
u32 *Vu1Pkt_LoadProg8(void) {
    u32 *p;

#ifdef PORT
    /* D_002C3380 is the first symbol of the next data file: on PC the linker puts it elsewhere, and the length
       came out as nonsense (the chain then ran off into unrelated memory on every stage with animated objects). */
    Dma_AddRef(D_002C3080, 0x300 / sizeof(D_002C3080[0]));
#else
    Dma_AddRef(D_002C3080, D_002C3380 - D_002C3080);
#endif
    p = Dma_Alloc(0x140);
    p[0] = VU1_DMA_CNT | 0x13;
    p[1] = 0;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0x12, 0);
    p[0x4C] = VU1_VIF_NOP;
    p[0x4D] = VU1_VIF_BASE(0x12);
    p[0x4E] = VU1_VIF_OFFSET(0x1F7);
    p[0x4F] = VU1_VIF_NOP;
    return p;
}

/* Program 6 draw: runs the caller's chain with the constants already loaded. */
void Vu1Pkt_CallProg6(u32 chain) {
    u32 *p = Dma_Alloc(0x20);

    p[0] = VU1_DMA_CALL | 1;
    p[1] = chain & VU1_ADDR_MASK;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_NOP;
    p[4] = VU1_VIF_MSCALF(0);
    p[5] = VU1_VIF_BASE(0x92);
    p[6] = VU1_VIF_OFFSET(0x1B2);
    p[7] = VU1_VIF_NOP;
}

/* Program 6 setup: uploads the microprogram, obj's matrices (+0x100, +0x80 and +0xC0 each times +0x40), a matrix and a vector. */
void Vu1Pkt_LoadProg6(u8 *obj, void *mtx, f32 *vec) {
    u32 *p;

    Dma_AddRef(D_002C1F00, D_002C2D30 - D_002C1F00);
    p = Dma_Alloc(0x130);
    memset(p, 0, 0x130);
    Mtx_Copy(p + 0x24, obj + 0x100);
    Mtx_Mul(p + 4, obj + 0x80, obj + 0x40);
    Mtx_Mul(p + 0x14, obj + 0xC0, obj + 0x40);
    Mtx_Copy(p + 0x34, mtx);
    Vec4_Copy(p + 0x44, vec);
    p[0] = VU1_DMA_CNT | 0x12;
    p[1] = 0;
    p[2] = VU1_VIF_FLUSHE;
    p[3] = VU1_VIF_UNPACK_V4_32(0x11, 0);
    p[0x48] = VU1_VIF_NOP;
    p[0x49] = VU1_VIF_BASE(0x92);
    p[0x4A] = VU1_VIF_OFFSET(0x1B2);
    p[0x4B] = VU1_VIF_NOP;
}
