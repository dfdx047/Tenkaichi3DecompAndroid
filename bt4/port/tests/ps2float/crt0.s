# Entry of the PS2 float test: stack, then main; never returns.
    .set noreorder
    .globl _start
    .ent _start
_start:
    lui   $sp, 0x01F0
    jal   main
    nop
1:  b 1b
    nop
    .end _start
# the compiler calls __main (static constructors) at the top of main: nothing to do
    .globl __main
    .ent __main
__main:
    jr $31
    nop
    .end __main
