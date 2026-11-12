#include "common.h"
#include "sys/gfxm_e.h"

/*
 * Texture block pointers inside a model's VIF streams, 0x114860..0x114B18. See include/sys/gfxm_e.h.
 *
 * A model's mesh stream holds, for every material, a block "unpack 5 quadwords at TOPS + 0" (VIF code
 * 0x6C058000) whose words 5 / 6 and 9 / 10 are two TEX0 values. The file stores them with block pointers
 * counted from wherever the exporter put the textures; binding a model moves them to where the battle
 * uploads its textures.
 */

#define MDLTEX_VIF_END 0x70000000
#define MDLTEX_VIF_TEX 0x6C058000

/* Word offsets inside a texture block, counted from its VIF code. */
#define MDLTEX_TEX0A_LO 5 /* TBP0 in bits 0..13 */
#define MDLTEX_TEX0A_HI 6 /* CBP in bits 5..18 */
#define MDLTEX_TEX0B_LO 9
#define MDLTEX_TEX0B_HI 10

/* Rewrites the texture references of one VIF stream (single != 0) or of every enabled record of a mesh list
   (single == 0). First pass: the lowest TBP0 / CBP of the first TEX0 values, starting from minTbp / minCbp.
   Second pass: first TEX0 = (old - lowest) + tbp / cbp; second TEX0: TBP0 = tbp2, CBP = old + cbp2. */
void MdlTex_RebaseChain(s32 single, void *chain, s32 tbp, s32 cbp, s32 tbp2, s32 cbp2, u32 minTbp, u32 minCbp) {
    u32 *p;
    ObjMdlMesh *mesh;
    u32 v;

    if (single != 0) {
        for (p = chain; *p != MDLTEX_VIF_END; p++) {
            if (*p == MDLTEX_VIF_TEX) {
                if ((p[MDLTEX_TEX0A_LO] & 0x3FFF) < minTbp) {
                    minTbp = p[MDLTEX_TEX0A_LO] & 0x3FFF;
                }
                if (((p[MDLTEX_TEX0A_HI] >> 5) & 0x3FFF) < minCbp) {
                    minCbp = (p[MDLTEX_TEX0A_HI] >> 5) & 0x3FFF;
                }
            }
        }
        for (p = chain; *p != MDLTEX_VIF_END; p++) {
            if (*p == MDLTEX_VIF_TEX) {
                v = (p[MDLTEX_TEX0A_LO] & 0x3FFF) - minTbp + tbp;
                p[MDLTEX_TEX0A_LO] = (p[MDLTEX_TEX0A_LO] & ~0x3FFF) | v;
                v = ((p[MDLTEX_TEX0A_HI] >> 5) & 0x3FFF) - minCbp + cbp;
                p[MDLTEX_TEX0A_HI] = (p[MDLTEX_TEX0A_HI] & 0xFFF8001F) | (v << 5);
                v = ((p[MDLTEX_TEX0B_HI] >> 5) & 0x3FFF) + cbp2;
                p[MDLTEX_TEX0B_LO] = (p[MDLTEX_TEX0B_LO] & ~0x3FFF) | tbp2;
                p[MDLTEX_TEX0B_HI] = (p[MDLTEX_TEX0B_HI] & 0xFFF8001F) | (v << 5);
            }
        }
    } else {
        mesh = chain;
        for (;;) {
            if (mesh->enabled != 0) {
                for (p = mesh->vif; *p != MDLTEX_VIF_END; p++) {
                    if (*p == MDLTEX_VIF_TEX) {
                        if ((p[MDLTEX_TEX0A_LO] & 0x3FFF) < minTbp) {
                            minTbp = p[MDLTEX_TEX0A_LO] & 0x3FFF;
                        }
                        if (((p[MDLTEX_TEX0A_HI] >> 5) & 0x3FFF) < minCbp) {
                            minCbp = (p[MDLTEX_TEX0A_HI] >> 5) & 0x3FFF;
                        }
                    }
                }
            }
            if (mesh->last != 0) {
                break;
            }
            mesh = (ObjMdlMesh *)((u8 *)mesh + mesh->next);
        }
        mesh = chain;
        for (;;) {
            if (mesh->enabled != 0) {
                for (p = mesh->vif; *p != MDLTEX_VIF_END; p++) {
                    if (*p == MDLTEX_VIF_TEX) {
                        v = (p[MDLTEX_TEX0A_LO] & 0x3FFF) - minTbp + tbp;
                        p[MDLTEX_TEX0A_LO] = (p[MDLTEX_TEX0A_LO] & ~0x3FFF) | v;
                        v = ((p[MDLTEX_TEX0A_HI] >> 5) & 0x3FFF) - minCbp + cbp;
                        p[MDLTEX_TEX0A_HI] = (p[MDLTEX_TEX0A_HI] & 0xFFF8001F) | (v << 5);
                        v = ((p[MDLTEX_TEX0B_HI] >> 5) & 0x3FFF) + cbp2;
                        p[MDLTEX_TEX0B_LO] = (p[MDLTEX_TEX0B_LO] & ~0x3FFF) | tbp2;
                        p[MDLTEX_TEX0B_HI] = (p[MDLTEX_TEX0B_HI] & 0xFFF8001F) | (v << 5);
                    }
                }
            }
            if (mesh->last != 0) {
                break;
            }
            mesh = (ObjMdlMesh *)((u8 *)mesh + mesh->next);
        }
    }
}
