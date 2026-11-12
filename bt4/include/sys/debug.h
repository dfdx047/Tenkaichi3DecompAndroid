#ifndef SYS_DEBUG_H
#define SYS_DEBUG_H

#include "types.h"

/* Debug hooks whose bodies were stripped from the retail build (0x263098..0x2630E8). */
void Dbg_Init(void);
void Dbg_EndFrame(void);
void Dbg_Stub2630A8(void);
void Dbg_BeginFrame(void);
void Dbg_Stub2630B8(void);
void Dbg_Stub2630C0(void);
s32 Dbg_Stub2630C8(void);
s32 Dbg_Stub2630D0(void);
void Dbg_AfterStoreImage(void);
void Dbg_Stub2630E0(void);

#endif
