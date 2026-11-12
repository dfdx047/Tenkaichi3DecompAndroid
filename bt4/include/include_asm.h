#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

#if !defined(M2CTX) && !defined(PERMUTER)

#ifndef INCLUDE_ASM
#define INCLUDE_ASM(FOLDER, NAME) \
    __asm__( \
        ".section .text\n" \
        "    .set noat\n" \
        "    .set noreorder\n" \
        "    .include \"" FOLDER "/" #NAME ".s\"\n" \
        "    .set reorder\n" \
        "    .set at\n" \
    )
#endif
#ifndef INCLUDE_RODATA
#define INCLUDE_RODATA(FOLDER, NAME) \
    __asm__( \
        ".section .rodata\n" \
        "    .include \"" FOLDER "/" #NAME ".s\"\n" \
        ".section .text" \
    )
#endif

/* Put in front of the INCLUDE_ASM of a function that has a jump table when the object's .rodata is not on a
 * 16-byte boundary at that point: the compiler aligned jump tables to 16 bytes, the generated .s only to 8. */
#ifndef RODATA_ALIGN16
#define RODATA_ALIGN16() \
    __asm__( \
        ".section .rodata\n" \
        "    .balign 16\n" \
        ".section .text" \
    )
#endif

/* Put in front of the INCLUDE_ASM of a function that owns a float constant in the MIDDLE of the file's .lit4 pool
 * (constants of C functions before and after it): emits the constant in place, so the pool stays contiguous and
 * the file need not be split around the function. NAME is the D_XXXXXXXX label the function's .s uses. */
#ifndef LIT4_WORD
#define LIT4_WORD(NAME, HEX) \
    __asm__( \
        ".section .lit4\n" \
        #NAME ":\n" \
        "    .word " #HEX "\n" \
        ".section .text" \
    )
#endif

/* Around the C attempt of a function that stays INCLUDE_ASM, when functions LATER in the file only match if the
 * compiler has seen its definition (a call to a function already defined in the translation unit is treated
 * differently: delay slots, branch-likely). The attempt is compiled, so the compiler knows the function, but the
 * assembler skips its output (`.if 0`); the code comes from the INCLUDE_ASM that follows.
 *     ASM_STUB_BEGIN();
 *     void f(void) { ... }
 *     ASM_STUB_END();
 *     INCLUDE_ASM("asm/nonmatchings/...", f);
 * The attempt must not define function-local `static` data (it would be emitted outside the skipped block). */
#ifndef ASM_STUB_BEGIN
#define ASM_STUB_BEGIN() __asm__(".if 0")
#define ASM_STUB_END() __asm__(".endif")
#endif

#if INCLUDE_ASM_USE_MACRO_INC
__asm__(".include \"macro.inc\"\n");
#else
__asm__(".include \"labels.inc\"\n");
#endif

#else

#ifndef INCLUDE_ASM
#define INCLUDE_ASM(FOLDER, NAME)
#endif
#ifndef INCLUDE_RODATA
#define INCLUDE_RODATA(FOLDER, NAME)
#endif
#ifndef RODATA_ALIGN16
#define RODATA_ALIGN16()
#endif
#ifndef LIT4_WORD
#define LIT4_WORD(NAME, HEX)
#endif
#ifndef ASM_STUB_BEGIN
#define ASM_STUB_BEGIN() extern int asm_stub_begin_
#define ASM_STUB_END() extern int asm_stub_end_
#endif

#endif /* !defined(M2CTX) && !defined(PERMUTER) */

#endif /* INCLUDE_ASM_H */
