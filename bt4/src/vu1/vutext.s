/* The VU1 microprogram block (.vutext, 0x2BF6B0..0x2C3380): nine VIF packets, each a VIF NOP, one or two MPG
   codes with the microcode, and NOP padding. build/vu1/vutext.bin is assembled from the listings next to this
   file by scripts/vuasm.py (see configure.py). The packets' labels (D_002BFAB0, ...) are given to the linker by
   address; Vu1Pkt_LoadProgN (src/sys/vu1_packet.c) hands each packet to the DMA chain. */
.include "macro.inc"

.section .text, "ax"

glabel D_002BF6B0
.incbin "build/vu1/vutext.bin"
endlabel D_002BF6B0
