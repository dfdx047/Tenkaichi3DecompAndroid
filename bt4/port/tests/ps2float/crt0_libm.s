# Entry of the libm test: stack, the game's $gp, then main.
    .set noreorder
    .globl _start
    .ent _start
_start:
    lui   $sp, 0x01F0
    lui   $gp, 0x0030
    ori   $gp, $gp, 0x4270
    jal   main
    nop
1:  b 1b
    nop
    .end _start
    .globl __main
    .ent __main
__main:
    jr $31
    nop
    .end __main
