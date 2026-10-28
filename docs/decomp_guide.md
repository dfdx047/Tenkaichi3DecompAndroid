# Decompiling a module (working notes)

The goal is C that compiles to the same bytes as the original with Sony ee-gcc 2.96 at `-O2`
(`-G8` for game code, `-G0` for `src/cri/`) with `-fno-strict-aliasing`. Every function gets a real name.

## Loop
1. Read the functions: slice `asm/cod/*.s` (one huge file per range; never print it whole). Each
   function starts with `nonmatching NAME, 0xSIZE` / `glabel NAME` and ends with `endlabel NAME`.
2. Write `src/<area>/<module>.c` (+ a header in `include/<area>/`), `#include "common.h"` first.
   Types are in `include/types.h` (`s8 u8 s16 u16 s32 u32 s64 u64 f32`), `NULL` in `common.h`.
3. Record every new name in a symbol file under `config/symbols/` (format below).
4. Run `python3 scripts/fdiff.py src/<area>/<module>.c [function ...]`. It compiles the file and
   compares each function with the original, printing the differing instructions side by side
   (original on the left). `OK` means all non-relocated bits match.
5. Iterate until every function is `OK`.

`src/sys/heap.c` + `include/sys/heap.h` are a finished example.

## Symbol file format
    Name = 0x00123456; // type:func
    gGlobal = 0x002FF080; // size:0x8
Put a `//` comment line above each group saying what the function does / why the name fits.
If a name is a best guess, end the line with `// guess`. Names must be unique across all files in
`config/` (grep before adding). Functions: `Module_VerbNoun`. Globals: `gName`.

## Rules that matter for matching
- A C file must cover one contiguous address range, functions in address order, nothing skipped.
- Globals: declare `extern` with the right type and reference them by their symbol name
  (`D_XXXXXXXX` names from the disassembly work as-is; rename them in your symbol file when you
  know what they are). Do not define data in the C file yet.
- Small globals (size <= 8 bytes) are reached through `$gp` (`%gp_rel`); to reproduce that the
  `extern` must have a complete type of <= 8 bytes. An array like `extern s32 tbl[2];` qualifies;
  `extern s32 tbl[];` does not (it compiles to lui/addiu).
- A function that will not match after real effort: replace its body with
  `INCLUDE_ASM("asm/nonmatchings/<area>/<module>", FunctionName);` and keep your best C attempt
  directly above it inside `#if 0 ... #endif` with a one-line note on what differs.

## Things ee-gcc 2.96 is sensitive to
- Statement order of stores, and the order locals are declared/initialised (register allocation).
- `switch` vs `if/else` chains: a `beq 1 / slti <2 / beq 2 / beq 3` ladder was a `switch` with an
  explicit `case 0: break;`.
- `if (a != X && a != Y)` vs nested ifs produce different branch shapes; try both.
- A stray register copy (`move v1,s1`) usually means the original recomputed an expression the
  compiler had already seen, e.g. `(u8 *)free + free->size` instead of reusing a local.
- Loops: `for (i = 0; i < N; i++)` over a `$gp` array becomes a down-counter plus a walking
  pointer; write the plain `for` and let the compiler do it.
- Signed vs unsigned decides `slt`/`sltu`, `div`/`divu`, `lb`/`lbu`, `lh`/`lhu`.
- A `return x;` in the middle vs a result variable assigned and returned at the end changes
  branch-likely (`beql`/`bnel`) usage; try both shapes.
- Tail calls (`j func` after restoring `$ra`) come from `return f(...)` / a call as the last statement.
- `long` is 64-bit on this compiler: a `UL` constant in a multiply gives the
  `multu`/`mfhi`/`dsll32` widening sequence, and a `long` return value gives `dsll32`/`dsra32`.
- A bit mask tested with `1 << n` must be unsigned to get `sllv`/`and` (signed gives `srav`/`andi`).
- If the original reloads a global pointer after a store through it, access the member through
  `(*&ptr->member)` so the compiler cannot assume the pointer is unchanged.
- A list `count` that the compiler reorders around pointer stores matched only when declared in
  an anonymous union with a pointer (`union { s32 count; ListNode *countAlias; }`).
- A `jal` + branch where a tail call was expected means the function is non-void.
- Duplicated statements in both arms of an `if/else` are sometimes required; simplifying them
  collapses the branch.
- `buf = dst; return buf;` (copy the parameter, return the copy) can change which register a
  final test uses.
- `ei`/`di` and `sync.l` go in `__asm__ volatile("...")`.

## Linking a finished file
Add a `c` subsegment for its range in `config/SLUS_216.78.yaml` (and a `.rodata` subsegment if it
emits jump tables or strings), list any new symbol file in both yamls, then
`.venv/bin/python configure.py && ninja`. The build only counts if ninja itself succeeds.
The original padded each object's `.rodata` to 16 bytes and ours pads to 8: when a C file's
rodata does not end on a 16-byte boundary, start the following assembly rodata chunk 8 bytes early.
An assembly rodata chunk is only as aligned as its contents: one that holds no jump table (a
plain table such as `gBtlStatCurve`) is placed right behind the previous object's data, so if
that ends 4 bytes short (a C file ending in a string) start the chunk 4 bytes early
(`[0x1EE7BC, rodata, cod/1EE7BC]` for data at 0x2EE7C0).
A link error `undefined reference to D_XXXXXXXX` / `Name` after linking a file means a name used
in C has no entry in a symbol file LISTED IN THE YAMLS (fdiff reads every file under
`config/symbols/`, the linker only the listed ones, and fdiff masks call targets, so an invented
or stale name still prints OK). Check the call target in the original and use the listed name.

## -fno-strict-aliasing
The build uses it. The original reloads struct fields and global pointers after any store, which
is what this flag produces. Older files carry workarounds from before it was adopted (the
`(*&ptr->member)` form, anonymous unions around `count` or a whole struct); new code should be
written plainly, and the workarounds can be removed where the plain form still matches.

## Assembler prelude (include/gcc_prelude.inc)
Prepended to all compiler output so the modern gas encodes it as Sony's assembler did: `move`,
`break`, FPU hazard nops (compares, `mtc1`/`mfc1`, the `lui`/`mtc1` form of `li.s`), `cvt.w.s`,
`sqrt.s`, and which instruction in front of an unfilled branch moves into its delay slot.
Do not add per-file `__asm__` macro blocks; extend the prelude.
What Sony's assembler did with an unfilled `jal` / `j $31` / branch, and the prelude now does:
- The instruction in front moves into the delay slot, including a `li.s` that is a `.lit4` load
  (`li.s $f12,1.5707963 / jal f` becomes `jal f / lwc1 $f12,...`; a `return 0.28f;` leaf comes
  out as `jr $ra / lwc1 $f0,...` by the same mechanism, though the binary has no example yet).
- `mtc1`, `mfc1`, `ctc1`, `cfc1`, FPU compares and the `lui`/`mtc1` form of `li.s` (constants
  with a zero half, such as 21.0f) never move: the branch gets a nop.
- Nothing moves when the instruction before the candidate was a delay slot the compiler filled
  itself (`jal f / li $4,1 / li.s $f12,0.9 / jal g / nop`), provided the candidate is a single
  machine instruction; a macro that expands to several (`la $4,69132($17)`) still gives up its
  last one.
- A load or store whose address is `symbol(reg)` or `symbol` (`lw $2,gTable($4)`, which expands
  to lui / addu / lw) gives up its last instruction to the branch ONLY when the assembler already
  knows the symbol is not small data, which in practice means the table is DEFINED earlier in the
  same file (a `const` table above the function, or an `INCLUDE_RODATA` at the top of the file).
  For an `extern` (the compiler writes its `.extern name, size` at the END of the output) the
  load stays in front of the branch, which gets a nop. Both assemblers agree on this, so it
  needs no prelude rule, but it is a test for where data was defined:
  `BtlCharSnd_GetBankMask` (`jr ra` / `lw` in the slot: `gBtlSndBankMask` is defined in
  `btl_char_flag_snd.c`), `AiThink_FindWeightColumn` (the column tables at the top of
  `btl_ai_cond.c`), against `BtlObj_Get` and `Pad_GetStatus` (`lw` / `jr ra` / `nop`: bss).
  If a function is off by exactly such a swapped pair, define the table in the file (or move the
  function into the file that has it) instead of touching the prelude.
`__gp_forget` uses `.set mips64` (it was `.set mips4`): under mips4 the assembler still kept the HI/LO hazard
nop after `mfhi` / `mflo` when an `mtc1` follows (`x % 360` converted to float, `EftBurst_Update`); under mips64
it does not. Changing it altered no other linked object (all compiler output was reassembled with both).
Known gaps, to check first if a function is off by a swapped or extra instruction next to a
branch: a single-instruction load, store or `la` (`lw $2,8($sp)`) right after a compiler-filled
delay slot and in front of an unfilled branch is still moved (their size is not measured by the
macros); `break` and `sqrt.s` are emitted as data and never move; `li.s` under `-G0` is untested; `sqrt.s` needs `$fN`
operands. The probes the prelude uses leave an unloaded `.gcc_prelude_scratch` section in every
object; the linker script discards it.

## Object boundaries and rodata alignment
Jump tables are 16-byte aligned inside an object's `.rodata`, so where the object *starts*
matters. If a file's rodata only lines up when the object begins earlier than the code you
decompiled, extend the C file backwards and pull the earlier functions in with `INCLUDE_ASM`
(splat moves each one's jump table into its generated .s file). `src/battle/btl_seq.c` is the
example: its code starts at 0x216AC0 but the object starts at 0x215540.
Things that only show when the file is linked (fdiff compares function by function and cannot
see them):
- A function-local table of a function that is still `INCLUDE_ASM` is not in its .s file (only
  jump tables are). Emit it with `INCLUDE_RODATA("asm/nonmatchings/<area>/<module>", D_XXXXXXXX);`
  next to the function; splat writes the .s for every symbol named that way. The same works for
  file-scope tables at the top of an object (`src/battle/btl_ai_cond.c`).
- The jump table in an `INCLUDE_ASM` .s file is only 8-byte aligned. If the object's rodata is
  not on a 16-byte boundary there, put `RODATA_ALIGN16();` in front of the `INCLUDE_ASM`.
- A float constant of an `INCLUDE_ASM` function that lies in the MIDDLE of the file's `.lit4`
  (C functions before and after it have constants) is emitted in place with
  `LIT4_WORD(D_XXXXXXXX, 0x........);` in front of the `INCLUDE_ASM` (`BtlInput_Update` in
  `src/battle/btl_input.c`); the file need not be split. A constant at the start or end of the
  pool can simply stay in the neighbouring assembly `lit4` chunk.
- A non-static `inline` function is emitted at the END of the object by this compiler, whatever
  its place in the source. If a function is both called normally and inlined into a neighbour,
  write the body as a `static inline` helper and call it from both.

## Placeholder names across modules
Integrator only (agents working in parallel must NOT run it: it edits every file under `src/`
and `include/`, including other agents' work in progress). After adding names, run
`python3 scripts/apply_names.py`: it rewrites `func_XXXXXXXX` /
`D_XXXXXXXX` in `src/` and `include/` to the current names, so one module's rename does not
leave another module calling a symbol that no longer exists. It takes path substrings to skip
(files other agents are writing). Caution: it reads EVERY file under `config/symbols/`, also the
ones not yet listed in the yamls; applying those names to linked files breaks the link. While
unlisted symbol files exist, apply only the listed ones (list the file in both yamls first).
The skip arguments do not help with that: they name source files to leave alone, the symbol files are
still all read. Run a copy of the script that ignores the unlisted symbol files instead.
Renaming a function: replace the name on word boundaries over `config/`, `src/` and `include/`
together (aliases of the form `extern T f_(void) __asm__("Name");` carry the name in a string and must
follow), check first that the new name is not used anywhere, then reconfigure and rebuild.

## More matching lessons
- A function whose early exits are all `return 0` and whose last statement is `return 1` keeps
  its separate success blocks; ending on `return 0` makes the compiler merge them.
- Whether a callee has already been compiled in the same file changes delay-slot filling and
  branch-likely choices in its callers. A mismatch of that kind is evidence of an original file
  boundary; as a stopgap, call through an aliased declaration
  (`extern T f2(void) __asm__("f");`).
  It works in both directions, and decides merges: `BtlColl_Update` needed
  `BtlHit_CheckProximityAll` above it (appended to `btl_char_hit.c`), `BtlInput_TestAction` the
  readers of `btl_input.c` (appended there), `BtlMember_Damage` needed `BtlColl_NextPoolMember`
  (moved to the top of `btl_char_member.c`); while two callers of `BtlAi_ScaleByLevel` in
  `btl_ai_cond.c` match only with it OUTSIDE the file, so it became the last function of
  `btl_ai_seq.c`. When two files with different local views of the same structures are merged,
  keep each half's types and put cast macros between the halves
  (`#define gBtlAi ((AiThMgr *)gBtlAi)`, see the middle of `btl_ai_cond.c`); a cast through
  `*(T **)&global` changes register allocation, a plain cast does not.
  A whole-file merge can be done mechanically: append the second file (minus `#include "common.h"`),
  compile, and turn every `extern` of the second part that the compiler reports as `conflicting types`
  into `#define Name ((ret (*)(args))Name)` (a cast of the function's address; the call stays a direct
  `jal` and the code does not change). Point the `INCLUDE_ASM` folders at the merged file's stem,
  remove the second file's `c` / `.rodata` / `.lit4` lines from the yaml (the merged file's sections
  simply run on), re-run fdiff on the whole file. A deliberate non-libc `memset` prototype is reported as
  a conflict with the built-in: that one is a warning, leave the `extern` alone. A header that carries
  prototypes (not only types) cannot follow such macros: a third part needs `#undef`s first.
  An `INCLUDE_ASM` function that ends up in the middle of a merged file needs its constants emitted in
  place with `LIT4_WORD` (take the labels from its generated .s, the values from the original).
- A `beqz` that comes out as `beqzl` (or the reverse) with everything else equal is the usual symptom of
  a missing earlier definition in the file; five functions of the action-handler batch were fixed by
  merges alone (`BtlFx_UpdateGroundFx`, `BtlAct_AttackDashHandler`, `BtlAct_SuperRushDashHandler`,
  `BtlAct_SwitchArriveLand`, `BtlAct_KoSwitchFlyIn`). Try the merge with the neighbouring file before
  giving such a function up, even when no particular callee is suspected.
- A switch whose dispatch flips between `sll / lui / addu / lw table(reg) / jr` and
  `lui / sll / addiu / addu / lw 0(reg) / jr` when nothing in the function changed is decided by what
  the compiler has seen earlier in the file, not by the code (`BtlAct_SuperRushFollowHandler` in
  `btl_act_f.c`): adding or removing one declaration anywhere above it flips it. A redundant prototype
  directly in front of the function is the least intrusive fix; say so in a comment and re-check the
  function after every change to the file or its headers.
- A helper the compiler can see is `const` (static, no side effects) lets callers keep values
  in registers across the call; try `static` on a small helper.
- Float literals: fdiff masks constant relocations, so compare each `.lit4` value with the
  original data. Decimal literals are truncated: use `3.14159265f`, `6.2831853f`,
  `1.41421356f`, `1.1666667f`.
- An object's `.lit4` and `.rodata` are each contiguous. If a function left in assembly owns
  constants in the middle of a file's pool, the file has to be split around it.
- More merge mechanics (effect batch). When the second part's HEADER (not a local `extern`) declares a name the
  first part already declared with another type, hide the header's declaration and cast afterwards:
  `#define gEftBurst gEftBurst_eDecl` / `#include "battle/eft_e.h"` / `#undef gEftBurst` /
  `#define gEftBurst ((EftTransWork *)gEftBurst)`. A plain cast also works on the left of an assignment (this
  compiler accepts a cast as an lvalue); a global that is a structure takes `(*(T *)&name)`; a structure tag
  both parts define takes `#define EftView EftViewE` at the top of the second part. A function that the second
  part DEFINES and the first part declared with other types cannot be a cast macro (it would rewrite the
  definition): give the first part an aliased declaration,
  `extern void f_e(A *) __asm__("f");` / `#define f f_e`, and `#undef f` at the top of the second part
  (`src/battle/eft_e.c`, `src/battle/eft_d_b.c`). A file can also be cut in two and its head appended to the
  previous file; both pieces then carry a copy of the preamble.
- Placing a new file's data without trusting notes: link the `c` subsegment first, build with `ninja -k 0`, and
  search each object's `.rodata` / `.lit4` / `.sdata` bytes in the original image (relocated words masked) in
  address order. Every file of the effect batch was placed that way; the gaps between the hits are the constants
  and tables of INCLUDE_ASM functions and stay assembly chunks. Do it again after the `.rodata` subsegments are
  listed: only then does splat write the jump tables into the INCLUDE_ASM .s files and the `INCLUDE_RODATA` .s
  files at all (before that the file does not assemble, or its `.rodata` is too small).
- VU0 macro code cannot be INCLUDE_ASM: in per-function files splat writes the accumulator operand as `ACC`
  (in the big chunks as `$ACC`), which the assembler rejects. Cut the C file around such functions and leave
  them an `asm` chunk (`stg_a.c` / `cod/140C68` / `stg_a_b.c`).
- A C file may call a function by a name that exists only in a symbol file that is not listed yet (another
  agent's): it links only by the placeholder. Check each undefined reference against the `jal` target in the
  original, never by the name's look.
- The private copy of `apply_names.py` should select the symbol files by "listed in the yaml", and skip the
  SOURCE files of agents that are still working by stem, not by prefix (stems of different waves share prefixes).
- A caller that needs a callee DEFINED above it, when the callee itself is still INCLUDE_ASM (second effects
  wave): compile the callee's C attempt, but let the assembler skip its output.
  `ASM_STUB_BEGIN();` / the attempt / `ASM_STUB_END();` / `INCLUDE_ASM(...)` (macros in include/include_asm.h;
  they emit `.if 0` and `.endif` as top-level assembly, which this compiler writes out in source order around
  the function). The compiler has then seen a definition, the code still comes from the original. It made
  `EftRibbon_Update`, `EftRibbon_Draw`, `EftRibbon_SetEnd` (callees `EftRibbon_PlaceStrip`, `EftRibbon_DrawStrip`,
  `EftRibbon_DrawKind1`) and `EftAura_ChangeType` (callee `EftAura_SetType`) match. Use the real attempt, not an
  empty body: the compiler works out for itself that a function without side effects is `const` and then treats
  its callers differently. Everything the attempt emits inside its function (jump tables, the constants of
  local initialisers, `li.s` literals) is skipped with it; a function-local `static` would not be. It only
  helps callers LATER in the file: wrapping every attempt of the wave this way changed no other function.
- fdiff masks `$gp`-relative offsets as it masks every relocation, so two small globals stored in the wrong
  ORDER print OK and only show when the file is linked (`EftAuraMgr_Init`, `EftGlowMgr_Init`: two pointers
  set to the same value). In `a = b = x;` the store to `a` is emitted FIRST: put the global the original
  stores first on the left.
- A whole-file merge changes the alignment base of the second part's read-only data: jump tables are aligned
  to 16 bytes relative to the START of the object's `.rodata`. If the first part's data starts 8 bytes off a
  16-byte boundary, a table of the second part that the original had on a boundary moves by 8 (eft_y + eft_z:
  0x2ED310 became 0x2ED308). That is evidence that the two are not one object beginning at the first part's
  data; cut the first file where its read-only data ends and merge only the tail (eft_y.c was cut at 0x195038).
- Tools from the second effects integration (build/scratch_integrate5/): `merge5.py` does the mechanical merge
  including conflicts that come from a HEADER the second part includes (it hides the header's declaration with
  `#define N N__p2` around the `#include` and adds the cast macro built from the header's own text);
  `layout.py` + `spec.py` generate the `.rodata` / `.lit4` / `.sdata` subsegments from each object's measured
  section size, so only the START address of each file's data has to be found (`place.py`), and the gaps become
  assembly chunks by themselves. A gap that is only alignment padding in front of a C file's section needs no
  chunk (the next object's own alignment produces it).
- After a re-split that creates or changes the .s files of INCLUDE_ASM / INCLUDE_RODATA (a `.rodata` subsegment
  added or resized), ninja does not rebuild the C object that includes them: delete the object. The symptom
  is an undefined `jtbl_XXXXXXXX` at link, or a section of the old size.
- A global that only unlinked files use may have no entry in any symbol file although every agent "knows" its
  name (`gEftZapMgr`): fdiff does not need the address of a `$gp` global, the linker does. Find it from the
  `$gp` offset in the original (`_gp` = start of `.lit4` + 0x7FF0) and add it to the defining module's file.

- Lessons of the last integration (eft_ae, col_b / col_c, bobj, the AI sequence; build/scratch_integrate6/):
  - Mechanical merge when the SECOND part defines functions that the first part declared with other types and the
    second part's header also declares them: `merge5.py` then hides the header's declarations and adds cast
    macros, which rewrite the definitions (parse errors at each definition). Give it the aliased declarations
    instead: `WORKDIR/sub1.txt` replaces each such `extern` of part 1 by
    `extern T f_a(args) __asm__("f");` / `#define f f_a`, and `WORKDIR/pre2.txt` puts the `#undef f` lines (and
    the `#undef` of any macro constant both headers define, `BOBJ_NODE_MAX`) at the top of part 2
    (`src/battle/bobj_a.c`). The copy of merge5.py in scratch_integrate6 accepts `\n` in the replacement text.
  - Replacing a block of INCLUDE_ASM lines at the top of a linked file with their C (btl_ai_seq_a.c into
    btl_ai_seq.c) moves only the START of the file's `.lit4`: the assembly chunk in front of it was those
    functions' constants. List the file's `.lit4` at the chunk's start and delete the old line; `.rodata` does
    not move (the jump tables were already in the object through the INCLUDE_ASM .s files), and an
    `INCLUDE_RODATA` table becomes the local initialiser of the function that owns it.
  - An empty function (a stripped assert) that callers in ANOTHER file need to be known as side-effect free:
    declare it `__attribute__((const))` in the callers' file, and give the defining file the same prototype in
    front of the definition (`EftTexSet_CheckCount`, eft_ae.c / eft_det_a.c). It replaces a merge.
  - `place.py` cannot place a `.rodata` that is mostly jump tables (every word is a relocation, so it "matches"
    everywhere): take the address from the `jtbl_` labels of the assembly chunk that the file's code range owns,
    link, and check the object's section size against the span. For a file with an `INCLUDE_RODATA` the `.rodata`
    subsegment has to be listed (with a provisional end) before the object can be assembled at all.
  - `enable.py` / `tryall.py` / `stub.py` expect `#if 0` alone on its line; an attempt written as
    `#if 0 /* comment` ... `#else` INCLUDE_ASM `#endif` (eft_ae.c) has to be tried by hand.
  - With every symbol file listed, `scripts/apply_names.py` runs as is. Order that worked: list the new symbol
    files in both yamls, check duplicates over ALL symbol files, run the name pass, pass the gate, and only then
    add the new `c` subsegments. All five new objects then linked byte-identical at the first attempt.

## Scratch files
Each agent uses its own subfolder for scratch scripts. Run Python scratch files with
`python3 file.py`, never as `./file.py` or `sh file.py` (a shell runs `import` as ImageMagick's
screenshot tool, which hangs). Never use `pkill` by name.

## Matching lessons from the near-miss cleanup and the eighth batch (2026-10-04)

Registers and allocation
- "Runs out of saved registers / keeps the index on the stack", or an element pointer swapped
  with another saved register: the original indexed the array at every use (`objs[i].f`,
  `&objs[i]`), no element pointer. Conversely a search loop that reads `0x10(base)` once and
  walks a pointer came from `f = table; f[i].field`, not `f++`.
- A value reloaded from memory at every use through a pointer recomputed once per iteration is
  an inlined helper (`static inline f32 Key_GetFrame(Key *k)` called with `b - 1`, `b`).
- Copies of `&local` in saved registers in each arm: write `&local` at every use, no pointer
  variables.
- A constant kept in a saved register and reloaded as a literal in one place is a variable set
  once to that constant; "reloaded in one arm only" means every arm assigned it. A variable
  set twice is not hoisted or strength-reduced by the loop pass.
- A function-wide temporary reused for one more clamp gets the low register; when the original
  uses a higher one, give that site a block-local variable. Variables declared inside each arm
  of an if / else do not share registers the way function-scope ones do.
- Declaration order of spilled variables = stack-slot order; among equal priorities the lower
  pseudo (declared first) wins.
- Swapping two adjacent independent stores, a dead store (`r = 0.0f;` before the real
  assignment) or reusing a variable for an unrelated value changes which register a neighbour
  gets. A run of plain stores is emitted rotated by one (last source statement first).
- This compiler splits a member offset into a multiple of 16 (added to the index) and a rest
  (added to the base). A base like `(p + 0x20) + 0x90`, or `work + 0x2F1`, is evidence of
  struct nesting (`plan = &ai->plan; plan->total[k][i]`, `w->pair.b[i]`); choose nesting whose
  rests sum to the base constant.

Control flow
- An if / else with identical arms is merged after register allocation: put the whole
  duplicated block (with its own block-local variables) in both arms when a compare result is
  unused or a common `p += 2` / `jal` sits in both paths. Same for switch cases that each end
  in the same stores.
- A shared `return 0` block stops if-conversion to slt / sltiu; two `return t;` statements
  keep a float in `$f1` with one copy to `$f0`; explicit `return;` after each arm can be
  needed for tail calls. A missing tail call: non-void function with no return statement,
  `if (p == NULL) return NULL; return p;`, or a one-iteration loop.
- A constant load placed before a branch although used only after it: wrap the body in
  `do { } while (0)`. A loop entered by a jump into its middle is `for (;;) { step; if (a)
  break; if (b) return; }`.
- `lbu` + `sltiu N` then `sll / sra` + compare on an `s8` field is `x >= 0 && x <= N-1 ||
  x == K` in its own `if`, not a `u8` local. `andi; sltu` on one bit is `if (x & 1) return 1;
  return 0;`. `bgez; move; negu` is `__builtin_abs`.
- A switch with redundant cases (`case A: =2; default: break; case B: =6; case C: =6;`) avoids
  a conditional move; unsimplified `xori reg,0` before `movz` means several statements in the
  `if` (`flags |= 2; flags |= 4;`).
- Bit fields for stores, masks for tests, can coexist on the same word (anonymous union).

Attributes and prototypes
- `__attribute__((const))` on a pure arithmetic helper, or `pure` on a memory-reading one,
  declared locally, stands in for a definition the original compiler had seen (changes
  scheduling around calls and delay-slot annulling).
- A float argument's position in a prototype changes the caller's set-up order: check
  prototypes against every caller (found `StgRigid_Create(pos, radius, user)` this way).

Data
- Decimal float literals are truncated: write enough digits (0.52359875f, not 0.5235988f) and
  ALWAYS compare .lit4 bits; fdiff cannot see them.
- `u32 pkt[N] = {...}` reproduces "build at sp+X, copy to sp"; `memset(&h, 0, 8);
  memcpy(&h, p, 8)` gives `ldl / ldr` after `sd zero`; memcpy of a 16-byte row gives the block
  move with a shared base.
- A dead division (`li v0,32 / beql / break 7`) is `s32 w = 32; s32 n = 512 / w;` with the
  divisor a variable of its own.

Tools
- For near-misses that will not match, a differential interpreter (original bytes against the
  compiled attempt on random memory, comparing final memory and outgoing calls) gives a
  behavioural check: build/scratch_cleanup_eft/emu*.py.
- fdiff cannot assemble an unlinked file that contains INCLUDE_ASM: check a copy with the
  INCLUDE_ASM / INCLUDE_RODATA / LIT4_WORD lines removed. Use `.venv/bin/python configure.py`.

## Linking lessons from the ninth step (2026-10-04)
- To find where an object's `.rodata` / `.lit4` / `.sdata` belongs, search the original image for
  the section's bytes (relocated words masked), restricted to that section's range; a few lines
  of Python over `readelf -S / -r` do it. Ambiguous hits (a 4-byte zero) are settled by order.
- A global pointer that the disassembly shows inside `.sdata` (address below 0x2FF180) must be
  defined `= NULL` in its file; one in `.sbss` stays `extern`. A local array initialiser of 8
  bytes or less (`u8 col[8] = {...}`) is also `.sdata`, 8-byte aligned.
- `INCLUDE_RODATA` .s files are written only for a file that has a `.rodata` subsegment, and
  after renaming the symbol run configure again before building. Give the table its size in the
  symbol file if a subsegment boundary used to fall inside it.
- A read-only table that lands 8 bytes early means the object is larger than the file: the
  section's alignment (16 with a jump table, 8 with only tables and strings) comes from the whole
  object. Merge the files rather than forcing the alignment.
- An INCLUDE_ASM function that uses a string of the C part needs that string as a named object
  (`const char gName[] __attribute__((aligned(8))) = "...";` placed in front of the function
  whose pool emits it first, and used by name everywhere in the file).
- Hand-written VU0 routines inside a C file: top-level `__asm__` blocks with `$ACC` / `$Q`
  operands (build/scratch_integ/asmblocks.py turns INCLUDE_ASM lines into such blocks from the
  split's per-function files); fdiff checks them like C functions.
- Initialiser VALUES (colour tables, packet words, floats) are invisible to fdiff. Compare them
  with the original data before reporting a match; the image compare is the only other check.

## Linking lessons from the tenth step (2026-10-04)
- Before the first link of a new file, compare each of its `.text` relocations with the original image
  (build/scratch_integ10/rc.py): an unlisted or wrong callee name shows up with the address, and the listed name, it
  should have. Faster than reading link errors, and it also catches a name that exists but is the wrong function.
- Private aliases (`#define MyName func_XXXXXXXX`) in an unlinked file become macros over real names once the
  names are applied. Remove them before applying names, replacing all of them in one pass.
- A file whose only `.rodata` comes from INCLUDE_ASM / INCLUDE_RODATA is 8-byte aligned: put `RODATA_ALIGN16();`
  in front of the first one when the original object had a jump table.
- The first function of a file may be an INCLUDE_ASM inside `#else`: take a file's start from its lowest address,
  and check `start + .text size` (rounded to 8) against the next file.
- splat moves a string or table that only ONE function refers to into that function's .s file. When the C file
  defines it as a named object for an INCLUDE_ASM function, add `force_not_migration:True` to its symbol line.
- Symbols outside the image that C refers to (overlay functions and data) go in config/linker_script_extra.ld.
- Objects do not depend on the per-function .s files: delete the object after a re-split changes one.
- When the object after a C file has data aligned to less than the padding the original had, add a small assembly
  chunk for the gap (`[0x1F2168, rodata, ...]`) or start the following assembly chunk early (`0x1FF05C`, `0x1EB354`).


## Linking lessons from the eleventh step: the menu overlay (2026-10-05)
- Second binary, same tree: the overlay's yaml uses `src_path: src` with subsegments named `menu/<file>` and
  `nonmatchings_path: ../nonmatchings`, so INCLUDE_ASM paths look the same for both binaries
  (`asm/nonmatchings/menu/<file>`). configure.py's target has `src_subdir`; the main executable never compiles
  src/menu because only files with a `c` subsegment in a target's own yaml are built.
- Symbols across the two binaries: splat writes `undefined_*_auto` entries only for references made from
  assembly. configure.py therefore writes `build/<target>_extern_syms.ld` with every LISTED symbol that lies
  outside the target's image (overlay C calling the main executable, the wish screen calling `ItemHelp_*`,
  overlay globals that live in the main executable's `.bss` / `.sbss`). A name must be in a listed symbol file.
- The overlay interleaves `.data` and `.rodata` (per link group: the work pointers, then each file's read-only
  data). splat orders a segment by section type, so give each `.rodata` subsegment
  `linker_section_order: .data` (dict form: `{start: 0x.., type: .rodata, name: menu/x, linker_section_order:
  .data}`); everything then links in yaml order. With that there is no need to know the original link groups:
  each file defines its own pointer (`X *gX = NULL;`, `extern` kept in the header) and gets a 4-byte `.data`
  subsegment at the pointer's address.
- `.data` alignment tells what a zero word is: a pointer followed by an initialised table of 8 bytes or more
  (`gMisSel` + `gMisSelFaceTex`, `gSimTop` + `gSimTopFaceTex`, `gPassWin` + `gPassKeys`, `gItemPanel[2]` behind
  `gTeamSel`) has a padding word because the compiler aligns such objects to 8.
- Check a file BEFORE linking with a relocating byte compare (build/scratch_integ11/ov.py: compiles as the build
  does, applies every relocation from config/, finds the `.rodata` in the image by content, compares `.text`,
  `.rodata` and `.data`). All 93 chunk files passed it for `.text` as delivered; what it found were layout
  problems only (below). A yaml generator driven by its output (mk.py) replaces editing subsegments by hand.
- Halves of one source file cannot be linked separately when they share string literals (the tail's code refers
  to strings inside the head's `.rodata`: undefined `D_XXXXXXXX` from the assembly half). Merge first. A merge is
  mostly a struct-view unification (the head's partial view against the tail's complete one); it was farmed out
  to parallel agents with the checker as the acceptance test. Three functions matched only once merged
  (`CharSel_Input`, `Train_Update`, `Train_Input`: a callee defined earlier in the translation unit changes
  `beqz` / `bnel` and delay slots).
- A duplicated string inside what looks like one module means two source files (`"mc_ability_limit_up"` twice:
  `EvoZ_Init..Run` and `EvoZ_Refresh..Load` are separate objects).
- String literals of a `static inline` function are emitted where the inline function is DEFINED, not where it
  is expanded. If literals are out of order in `.rodata`, move the helper's definition (src/menu/menu_z_c.c:
  two helpers sit behind `DcMenu_DrawPlates`).
- Unreferenced development paths ("host:data/...") in an object are arguments of an inline "section" helper
  that ignores them (`Train_Section`, `DcMenu_Section`): one string per call, in call order.
- After an `INCLUDE_ASM` that follows a named read-only object, the compiler still believes it is in `.rodata`
  while the macro ended in `.text`: literals of the next function land in `.text`. Put
  `__asm__(".section .rodata");` behind that INCLUDE_ASM (src/menu/menu_z_c.c).
- Assembler `-G` and delay slots under `-G0`: Sony's assembler left a `symbol(reg)` load of a not-yet-defined
  symbol in front of an unfilled `jal` even with -G0; the modern gas does so only with a non-zero -G. A file
  without float constants can be ASSEMBLED with -G8 while compiled with -G0 (`AS_G_FLAGS` in configure.py and
  the same exception in scripts/fdiff.py; `SimEv28` in src/menu/menu_t_b.c). With floats it cannot: `li.s`
  would become a gp-relative `.lit4` load.
- `li.d` (a `double` constant in an integer register) is in the prelude; the compiler's `litodp` / `dptoli`
  calls are aliased to `__floatsidf` / `__fixdfsi` in config/linker_script_extra_dbzp.ld.
- splat keeps old split files on disk (asm/dbzp/000000.s etc.): scripts/progress.py reads only the assembly
  files the yaml names now.
- An INCLUDE_ASM function whose strings are ALSO used by C functions behind it, in the middle of its own strings
  (`Option_Draw`): name those strings in the symbol file with `force_migration:True` (splat does not move a
  user-named symbol into the function's .s otherwise, and stops migrating there), and let the C refer to them as
  `extern const char name[]`. A string emitted by C in FRONT of the function is the other case: a named
  `static const char x[] __attribute__((aligned(8)))` object with `force_not_migration:True`.
- An object whose only jump table belongs to an INCLUDE_ASM function has an 8-aligned `.rodata`; when the
  original was 16-aligned put `RODATA_ALIGN16();` at the TOP of the file (in front of the first table), not in
  front of the INCLUDE_ASM, if strings of the C part end in between (src/menu/menu_u_d.c).

## Matching lessons from cleanup round 2 (2026-10-05, added as agents report)

- Branch prediction decides delay-slot filling: this compiler tags `==` branches 40 % and `!=`
  60 %, anything else 50 %; at 50 % or above the slot is filled from the target (`beqzl` +
  the target's first instruction), below from the fall-through. A range flag kept in a
  variable and tested later counts as `x == 0`; with the compare next to its branch it is an
  unsigned compare.
- This code base precomputes biased values into locals before earlier tests
  (`s32 cls = (s8)tbl->stateClass[state] - 15;` ... later `if ((u32)cls < 3)`): a subtraction
  in front of an unrelated branch with its `sltiu` in that branch's delay slot is the sign.
- `if (f() == 0) { ...; return 0; }` drops the `move v0,zero` (v0 still holds the call's 0).
  A path that uses v0 as a temporary before a `return 0` reached a SHARED `return 0` by
  control flow.
- `if (phase) X; else X;` with identical arms on an already-loaded register is a block
  boundary until the last jump pass and changes argument-load order before it.
- Allocator priority is `floor_log2(refs) * refs / live_length`; ties go to the lower pseudo.
  The `-da` dumps (`.lreg` "Register N used X times across Y insns", `.greg`) give the exact
  numbers: compute how many references must change before trying forms.
- `move rX,rY` is also what an int-to-long sign extension compiles to (`(s64)u0 << 4`).
- Check for a matched twin first: mask the immediates and search for the same instruction
  sequence in an already matched function (build/scratch_cleanup2_D/twin.py). The GS packet
  header is `K + (ctx << 4)` (int shift, constant first), not `((u64)ctx << 4) + K`.
- List unlink with a copy into a0: ONE variable for both `*head` and `p->next`
  (`a = *head; if (a == NULL) return; ... b = a; a = prev; b->prev = a; a->next = b;`).
- A switch of range checks whose "return 1" is reached by a branch-likely into the shared
  exit: every in-switch exit is `return 0`, each case ends `break`, the function ends
  `return 1`.
- A constant subtraction done at run time with one hoisted 1.0: compare the clamp bound
  through the variable (`a = 1.0f; if (!(a < x)) a = x;`).
- Two saved registers swapped between equal-looking variables: look for a PRIORITY TIE in the
  `-dg` dump (build/scratch_cleanup2_D/lr.py FILE FUNC prints refs, length, priority per
  pseudo); a dead initialiser (`f32 r = 0.0f;`) or one more instruction in the live range
  flips it.
- `!(a & X) && !(a & Y)` compiles to one mask test; the original's `andi / sltiu / movn` is a
  variable set in steps (`f = w->flags & X; f = f == 0; if (w->flags & Y) f = 0;`), which
  also changes spills elsewhere.
- `lhu` on an s32 screen coordinate: `(u16)scr[i].x * 16 + 0x7000`. `addiu sN,sN,48` on a
  parameter: the parameter itself was advanced.
- FAKE MATCHES: `EftLink_DrawBillboard` (eft_w.c) matches only with an empty `__asm__("");`
  that adds one RTL instruction to break an allocator tie. Kept, marked in the source; the
  natural source form is unknown. List such cases in [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md).
- memset arguments loaded a1, a2, a0 mean the clear is compiler-generated: write the argument
  block as a STRUCT INITIALISER with `{0, ...}` members and 16-aligned vectors
  (`EftArg9 arg = { { res0, res1 }, tex, {0,0,0,0}, {0,0,0,0}, size, rate, objId };`), not
  assignments plus `memset`; an element read through a pointer gives the "build in a
  temporary, block copy" form.
- A constant load one step too early among packet stores is a local initialised at the top
  (`u64 gif1 = 0xF42142142160;`). A flag word reloaded at the end of every path is a variable
  assigned once before the block; a pointer set in the delay slot of a test is assigned both
  before and after the `if`.
- slt / movz without `xori` on a validity result:
  `if (a <= K) { if (a > 0) ok = 1; else ok = 0; }`.
- FAKE MATCH: `EftBlast_Init` (eft_j.c) reads `src->def` through a volatile lvalue in its first
  test to stop gcse's PRE merging two loads; the real source cause is unknown.
- Global allocation refs are weighted by loop depth; the log2 factor steps at 16 and 32 refs.
  Equal-priority block-local quantities go to the first-born.
- A constant in a saved register loaded in front of a loop, where a literal stays in place:
  a variable set to the constant before an intervening call (`s32 start = 1;` ... `if (next
  == start)`).
- Two saved registers swapped between two unrelated places: suspect ONE source variable used
  in both (`baseY` in `Option_Draw`). Check `b` targets in "registers only" notes: a wrong
  target (a redundant or missing store in one switch case) hides among register lines.
- Single-use loop constants in the wrong saved registers: ties go to creation order; inline
  parameters create their registers at the call, so try writing the constants out.
- The first scheduling pass only moves instructions between blocks in functions of at most
  10 basic blocks. A hoisted constant-equivalent address (`sp + N`, a string address) has its
  live length doubled for allocation priority.
- A call to a function the compiler knows is const (static pure arithmetic, or
  `__attribute__((const))`) does not flush the scheduler's pending reads: a swapped "field
  load / constant load" pair in front of such a call is this (try `static`). Scheduler
  tie-break (`-da -fsched-verbose=6`): priority, then more dependents, then source order.
- Loop-pass hoisting is count-driven (`-dL` prints it): a one-insn constant is hoisted while
  threshold * savings * lifetime >= loop insn count (threshold 64 with a call in the loop).
  Insns that later vanish still count: an inline predicate's `!= 0`
  (`static inline s32 TestFlag(def, flag) { return (def->flags & flag) != 0; }`), a `trap_if`
  per division. "One constant hoisted, the next one not" = a loop a few insns longer.
- A value in a saved register but computed after the calls was WRITTEN between or before the
  calls and sunk by the second scheduling pass (`mask = 1 << (bit - 1);` between two calls).
- Interblock scheduling has a size limit (about 100 RTL insns per region): `/` and `%` written
  out at each use add `trap_if` insns and can push a function over it.
- Folded member offset in an array-of-struct access:
  `*(s32 *)((u8 *)&scr[0].z + (i << 4))`. `dsll32 24 / dsra32 24` with no `andi` is `(s8)`
  applied to an int; two constants ORed separately means one is a 64-bit local
  (`s64 abe = 1;` ... `prim = (abe << 6) | ((s64)(s8)ctx << 9) | 0x1B`).
- A switch whose cases each have their OWN copy of a body (0..11 plus default) merges only
  after register allocation; the extra insns flip allocation races. A store order that no
  statement order reproduces is an aggregate initialiser with nested aggregates.
- `x = a; if (c) x -= a;` keeps `move / movn x,zero`; an unexplained `move sN,sM` is a
  function-scope copy with a dead initialiser.
- `ColObb_Contact` (col_a.c) is NOT a near-miss: the original is 0x4258 bytes with nine
  hand-written-looking edge cases; the stored attempt is a folded rewrite. Dead code, needs a
  from-scratch decompile.
- CHECK CALLEE RETURN TYPES against the definition: a callee wrongly declared `void` changes
  which instruction lands in the call's delay slot (a store using `$v0` moves out of it).
- "Parameter copied to another register at entry" can be declaration order of two
  initialisers that load through the same pointer (`prm = arg->prm` before `anim =
  arg->anim`). "Two spill slots exchanged" or "invariant computed twice": a local caching an
  index expression; write the expression at every use.
- Loop heads are aligned to 8: "one instruction short plus a missing nop" is ONE missing
  instruction.
- Open family (no source form found yet): a plain pointer copy that survives to register
  allocation in the original but is always removed by cse / gcse here (`EftBlade_GetColor`,
  `EftRibbon_PlaceStrip`, `EftRibbon_PlaceTrail2`, `EftOrbTail_DrawStreaks`,
  `EftOrbTail_InitFrames`, the two ground-dust inits). `-fno-gcse` breaks matched functions,
  so gcse was on.
- ALIGNMENT OF THE ELEMENT TYPE decides how `p->arr[i].m` is addressed: the member offset is
  added to the base first (`addiu s0,s0,4` ... `addu s0,s0,idx`, or the same `base + idx`
  computed several times) only when the element's known alignment equals the member's (4);
  with a 16-aligned element the offset stays in the load / store. Fix with a local view of
  the other alignment (both directions occur). A pointer indexed directly never does this; a
  pointer to a structure holding the array does.
- A load of a global that never moves into the delay slot of the branch in front of it while
  the store behind it does (`bnel` / `beqzl` + `lw gOtCur`): read the global through a
  volatile alias (`extern u32 *volatile gOtCurRead __asm__("gOtCur");`). A stand-in (fake
  match family): the same pattern is in `EftSurf_DrawTriOt`, `EftWater_DrawBillboard`,
  `EftWater_DrawClippedFan` and others.
- Two arms ending in the same call that the original shares (one `jal`): duplicate the
  statements BEHIND the call into both arms. A duplicated block also changes register
  priority (its uses count before the merge).
- An up-counting loop the compiler would reverse: `for (;;) { ...; j++; if (j >= N) break; }`.
  Initialising a variable in the `for` header instead of its declaration keeps it from
  sharing a register with the parameter it copies.
- Between two equal ready instructions the scheduler takes the one in which a register dies:
  the LAST use of a value in source order decides. A scheduler trace is cheap and decisive:
  `-dS -fsched-verbose=4` (build/scratch_cleanup2_C/sv.sh).
- BEHAVIOURAL ERROR found this round: `EftGlow_SpawnPart`'s old attempt passed 1/fadeIn for
  the x, y, z of a colour step where the original passes the OLD value (0.0); an assignment
  inside an argument list was the cause. Never write `f(x, x, x, a * (x = ...))`.
- A DEAD CONDITIONAL survives register allocation: `if (c) { v /= 1024; }` whose body is dead
  leaves an empty branch until after allocation. Symptoms: a saved register saved and never
  used, a missing tail call, a temporary "one register too far". Only bodies the early jump
  pass cannot convert work (a signed division by a power of two). Plausible original source
  (a removed debug print) in `IopHeap_PrintFree`; an INVENTED stand-in in `Dialog_SetCursor`
  (fake match, marked in the source).
- A constant computed at run time in front of a loop (`move v1,zero / sll v1,v1,4`) is a
  second induction variable (`x += 32`), not `i * 32`. `(s64)expr | C` is narrowed to 32 bits
  when C fits an int: put the value through an `s64` local to keep `daddiu`.
- Two variables with the same constant value (`w = 8; tw = 8;`) give two counters their own
  step registers. `&local.member` goes through a temporary that gcse hoists; a plain
  `f32 local` with `&local` goes straight into the argument register.
- `(next - 1)->field` written at every use reloads per block while `next->field` stays in a
  register; a loop bound read from memory in the `for` condition gives a second register and
  a copy (use a local for the count).
- Open: chains of register copies in front of a run of stores (`move a3,v1 / move a2,a3`,
  `Ot_Reset`, `StgPanBlur_UpdateView`): cse making each statement's address temporary the
  canonical one; one link reproduced, not a whole chain.
- ELEMENT TYPE OF A LOCAL ARRAY: `s32 a[N][4]; a[i][2]` compiles to base + (i*16 + 8); a
  4-aligned struct element (`a[i].z`) to (base + 8, own register) + i*16, which also changes
  which induction pointers the loop pass makes. Likewise `s32 t[40]` indexed `i*2` gets a
  walking pointer where an array of pairs does not. Try this FIRST when the original indexes
  and the attempt walks a pointer or the reverse (seven functions fell to it).
- Order of plain stores: among stores ready together the first scheduling pass emits those
  whose source register dies there first, in source order, then the rest in source order. So
  plain field-order source produces the "scrambled" original order; do not hand-place
  statements. For packet headers a hill-climb over statement order works
  (build/scratch_cleanup2_A/search.py).
- Parameter order between float and integer parameters changes the order the incoming
  registers are copied (which saved float register each gets), not the registers.
- A counted loop is reversed in every structured form unless a value computed from the
  counter is used after the increment or the loop has a second exit; a count-up loop with
  separate walking pointers was only reproduced with a backward `goto`
  (`EftWater_DrawSprayQuad`, commented in the source).
- A caller that computes `sp + K` straight into `$a0` twice where hand-written code would
  share a saved register: the body is an inline function (`static inline EftSurf_QueueTri`).
- A late conditional move (`lw / slti / movn / sw`): a second, dead statement in the `if`.
- A `{0, 0, 0, 1}` local initialiser compiles to memset plus stores.
- "Scratch struct" views in drawing code are often NOT structs: a run of 4-byte stack slots
  behind an address-taken pointer, with values sometimes reloaded and sometimes in a
  register, is spilled locals. Stack order: aggregates get their slot at declaration; an
  address-taken scalar or one-pointer struct gets it after ALL locals; spilled locals follow
  in declaration order. Compiling a scratch copy with `-g` and reading the stabs maps every
  local to its slot (build/scratch_cleanup2_blur/vars.sh).
- A `static inline` vertex writer reproduces the register-copy chain in front of a run of
  stores (`addu t2,v1,t4 / move a3,t2 / move v1,a3`): try this on the open copy-chain family.
- `x = (c) ? K : 0` expands to a conditional move with K in a register (`li / move / movn
  zero`); `arr[(c) ? 0 : 1]` in a subscript gives `li 4 / movn zero`. An integer constant
  converted to float through a spilled variable becomes a constant-pool word in `.sdata`.
- Reload registers come round-robin from the set of hard regs picked as spill regs anywhere
  in the function: a whole-function "off by one register" in reload temporaries can be caused
  by a single far-away instruction (see `Using reg` lines in the `.greg` dump).

## Lessons from the first decomp-permuter batch (2026-10-05)

Tool: `.venv/bin/python scripts/permute.py <src/file.c> <Func> [--run] [-jN]`; queue driver
build/permuter/queue.sh; results in build/permuter/results.txt; check a candidate with
build/permuter/verify.py (a score-10 candidate can already be exact: verify before assuming).
Only exact candidates may be applied, and SPLIT the permuter's diff first: in five of nine
functions half of its changes were unnecessary, and in two the rest was a natural form.
Functions starting at a score of 60 or less were nearly all solved in minutes; those above
100 mostly did not move (structural causes).
- Callback argument copied into a typed local (`T *slot = arg;`) declared BEHIND a local that
  a call initialises: a second pseudo that survives cse, so gcse's PRE no longer merges
  repeated `slot->param` loads (`EftVolley_Init`, `EftBlast_Init`: natural, the volatile fake
  is gone).
- Code behind a `while (1)` loop may belong inside the `if (...) { ...; break; }` body
  (`TexChain_Build`: cse knowing a constant on the first-iteration path was the symptom).
  Two loops with counters in the same register are one variable.
- The first scheduling pass issues two instructions per cycle: one instruction more or fewer
  in front of a call decides the order of the NEXT call's argument loads.
- `do { } while (0)` around a run of stores changes their order (reads like a statement
  macro: `Movie_ReadBuf`); around a block with an inlined loop it changes which `return 0`
  block survives cross-jumping.
- FAKE MATCHES kept from this batch (marked in the sources): `Rigid_Init` (`body++; body--;`),
  `BtlAiStep_GuardUntilSafe` (two constant-holding variables), `EftHit_InitMultiHit` (a field
  stored back to itself; possibly a vacuous clamp originally), `EftBlastObj_Init`
  (`(void)&arg;`: an address-taken parameter may alias the frame), `EftOrbTail_Create` and
  `DcPass_Decode` (`do { } while (0)` around a block). `EftBlast_Init` is no longer a fake.
- `DcPass_Input` (src/menu/menu_z_d.c) is PATH-SENSITIVE: an unchanged copy of the file under
  another directory compiles its switch dispatch differently. Scratch copies of that file
  cannot be trusted for that function.
- ARRAY OF TWO-FLOAT STRUCT versus `f32 [N][2]`: the struct member offset joins the base (a
  base of its own per member); the 2-D constant index joins the index (shared base). The
  symptom is NOT local: every shared address in a long run of statements changes register
  ("same operations, 200+ differ, registers only"). Try the 2-D form first whenever a "pair"
  struct is indexed by a variable. Likewise a folded `f32 val[16][3]` view of consecutive
  tracks does not match per-member code: one member per track.
- Base+index shapes are produced by combine, not by source: do not read struct nesting into
  `(def+rest) + (idx4+K16)` versus `K16(idx4 + (def+rest))`; test fragments in isolation
  mislead because sharing decides the form.
- `T *p = &tmp;` used for the memset and all later calls moves a store scheduled in front of
  the first call to behind it (a single `sw zero,N(sp)` on the wrong side of a `jal`).
- A float clamp as a conditional expression (`t = (t < 0.0f) ? 0.0f : (1.0f < t) ? 1.0f : t;`)
  gives `mtc1 zero,f0 / mov.s f1,f0`; the if / else form loads the constant straight.
- Check a sibling's MATCHED function for its struct view before designing one.
- GS PACKET HEADER, the form that keeps matching: stores in the order prim, dmaTag, vif0,
  vif1, gifTag, then `p->regs = K + (ctx << 4);` (int shift, constant first, AFTER gifTag),
  then next. A `dsll` on the context is NOT evidence of a 64-bit operand. One insn more or
  fewer in the header shifts every live range crossing it and flips allocation ties far away
  ("u0 / v0 swapped", "depth in t0"): `EftGfx_DrawSprite`, `EftPrim_DrawBillboard` and
  `EftPrim_DrawTriangle` all fell to this alone. Try it on every remaining packet writer.
- Which saved float register an incoming float parameter gets depends on where the int
  parameters sit in the list: take the parameter order from callers that already match.

## Differential behaviour test (2026-10-05)

build/scratch_cleanup3_S/ (README.txt there): `dt.py` on top of `emu2.py` runs the ORIGINAL
bytes of a function and the compiled C attempt on the same seeded inputs and compares the
return value, every outgoing call with its arguments in order, every byte written outside
the frame and the callee-saved registers; callee stubs return seeded values by call position
on both sides; it reports branch coverage of the original. A new test needs a prototype
table, a `setup()` and `T.run(setup, seeds)`; examples `t_testskill.py`, `t_unk17.py`,
`t_input.py`. USE IT on every function that stays unmatched before the port relies on its C.
Results so far (4000 seeds each, no difference): `AiThink_TestSkill`, `BtlAiStep_FireSkill`
(both since matched), `BtlInput_Update` (8 of 8 branches covered; still 4 of 149 off: one
store position; analysis in the source note).

More lessons (agents S and V):
- The loop pass runs TWICE; a giv "not worth while" in the first run can be reduced in the
  second, where the loop is shorter. A whole-function reload-register shift plus a larger
  frame can be ONE RTL instruction of loop length: try compare forms that differ in insn
  count (`kind != 1 && kind != 2` versus `(u32)((u8)kind - 1) >= 2`). `grep "Loop from\|not
  worth" x.c.09.loop` shows both runs.
- `if (x != K) return 1;` versus nesting the tail under `if (x == K) { ... } return 1;`
  decides which branch gets the first epilogue load in its delay slot (an empty slot where
  the attempt has `ld sN`).
- Scheduling: at most one memory instruction and two instructions per cycle; a store whose
  source register is set again before the next call sinks to the end of the block. Loads in
  one order and stores in another, with float registers following the loads: the values went
  through locals.
- Screen sizes as variables set at the top (`width`, `half`, `srcH`, `h`) are a house style
  of the draw functions; signs: a real `mult` / `div` by a constant, an integer constant
  converted to float from `.sdata`, single-use constants in saved registers.
- Spill-slot order is creation order of the pseudo, temporaries included; a division always
  lands in a fresh temporary. `p += 2` inside each arm versus behind the if / else decides
  the schedule. Copy chains in front of stores come from post-reload cse when each address
  has its constant inside the subscript (`p->arr[i * 12 + k]`).
- A register-masked structural diff (build/scratch_cleanup3_V/sdiff.py) is the useful metric
  for large functions.

## Toolchain check (2026-10-05): we use the game's compiler

Investigation in build/scratch_compiler/ (summary.txt; driver drv.py). Our compiler is
`gcc version 2.96-ee-001003-1` (tools/ee-gcc2.96; its cc1 is byte-identical to the one in
`ee-gcc2.96.tar.xz` of the GitHub decompme/compilers release); the game ELF carries Sony
library tags 3000 / 3020. Measured over ALL source files (6,953 matching compiled functions
at the time):
- Every other public ee-gcc (2.9-ee-99xxxx, 2.95.2 / 2.95.3 SN, 3.2-ee) keeps at most 16 % of
  the matched functions and makes every stubborn function worse. Compiling as C++ breaks
  matches too: the game code is C.
- 118 flag sets plus 361 combinations: none makes any stubborn function match; every flag
  that changes code breaks matched functions in the same file. `-fno-strict-aliasing` is
  firmly original (1,060 functions break without it); the game was built without `-g` (84
  functions break with it; `-g1` is nearly neutral).
- So the "surviving copy" family (`EftBlade_GetColor`, `EftRibbon_PlaceStrip`,
  `EftRibbon_PlaceTrail2`, `EftOrbTail_DrawStreaks`, `EftOrbTail_InitFrames`, the ground-dust
  inits, `Ot_Reset`, `StgPanBlur_UpdateView`) and the exact ties (`EftEmit_SpawnType16` / `5`,
  `PadWatch_GetMissing`, `ScrXfade_StoreHalf`, `Shen_BuildList`) are SOURCE-FORM problems, not
  toolchain ones. Do not spend more effort on compiler versions or flags.
- Leads it turned up: `ChrCam_CalcCut` (btl_char_cam_cut.c) drops from 248 to 17 differing
  with `-fssa` (which renumbers pseudos: a declaration / statement ORDER clue, not a flag);
  `Sprite_DrawPicture` improves with `-fforce-addr` (an address-through-a-variable clue).

## Lessons from cleanup round 3 (2026-10-05, agents X2, W2, X1, W1)

Wrong declarations (each of these was behind a "registers / load order only" near-miss):
- CHECK EVERY CALLEE'S RETURN TYPE AND PARAMETER ORDER against its definition.
  `Vu0Cur_ProjectPoint` returns a value (declared `void`, the three off-screen tests come
  out with x / limit / reload register permuted). A local prototype listing ints before
  floats when the definition has floats first (or the reverse) changes only the ORDER the
  argument registers are loaded (`EftGndDust_SpawnPieceEx`, `EftSpr_DrawFlat`,
  `EftPrim_DrawQuadDepthScaled`).
- Check branch TARGETS before believing "registers only": `BtlText_DrawScrollBar` hid a
  behavioural error (a thumb part drawn only conditionally) among 29 register lines for
  three rounds. An aligned diff normalises targets away; fdiff's raw output shows them.

Source forms:
- Typed copy of a callback argument (`T *arg = param;`) must stand BEHIND another initialised
  declaration (`w = task->work` is enough); first in the list it is folded away. This is the
  natural form of the "surviving copy" (ground-dust inits). The orb-tail members were
  element alignment instead (`f32 uv[16][4]`, not 16-aligned vectors).
- Exact local-alloc ties: an insn in which more registers die than are born is scheduled
  first, so `res = base + x` (two deaths) always precedes a pending load while `res += x`
  (one death) does not. Building a pointer in two statements with another read in between
  solved `EftEmit_SpawnType16 / 5 / 14`.
- A value masked or biased before two uses: write the expression INLINE at each use when the
  original keeps the unmasked value in its own register (`node & MASK` in `ChrCam_CalcCut`:
  248 differing down to 0). A `-fssa` improvement can mean "one more pseudo is needed".
- Int-to-float through a spilled variable: a lone 4-byte `.sdata` word used by one function
  (`lui / addiu / lwc1 / cvt.s.w`) is `s32 k = N;` ... `(f32)k`.
- "Each call loads its own copy of a constant while other constants are shared": a variable
  assigned before each call (`sz = 9.45943f;` three times).
- A pointer variable assigned twice has an unknown alias base: stores through it force
  reloads. One walking variable shared by two loops makes its giv "not replaceable"; give
  each loop its own.
- List walk `lw v0,next(sN) / bnez v0 / move sN,v0`:
  `for (link = &head; *link != NULL; link = &f->next) { f = *link; ...`.
- Inline predicates `if (x & bit) return 1; return 0;` versus `return (x & bit) != 0;` give
  different branch prediction at the call site; both lengthen the loop for the hoisting
  threshold. A four-term sum `a + b + c + d` compiles to `(a + d) + (c + b)`: do not copy the
  asm pairing into the source.
- A store behind an if / else in front of a loop may belong inside BOTH arms (each packet
  header arm is complete, including `next = NULL`).
- `x = a - b` behind an `if` that duplicates one in front of it gets a gcse copy; routing an
  operand through a variable set twice (`h = y0; h = y1 - h;`) stops it (possibly a stand-in).
- local-alloc doubles the live length of any register whose first set is `reg = constant`,
  even if it is set again: a variable initialised to 0 at the top competes at half priority.
- Block-scoped locals are allocated before function-scope ones live at the same time.
- Tool caution: build/scratch_cleanup3_V/sdiff.py reads the object of the last SUCCESSFUL
  compile; after a compile error it silently reports the previous result.
FAKE MATCHES added this round (marked in the sources): `EftPtcl_DrawAxisQuads` (volatile
alias of gOtCur), `StgPanBlur_UpdateView` (a pointer to a local plus `do { } while (0)`),
`EftSmoke_Draw` (two dead assignments in the loop), `EftBound_BuildWall` (index pointer
starting at element 1, merged local struct, dead initialiser), `BtlText_DrawScrollBar`
(two-step height, possibly).

## Behaviour test sweep over all unmatched functions (2026-10-05)

build/scratch_difftest/ (README.md, results.md; driver gdt.py on interpreter emu3.py; 17
function-specific setups in setups/). 59 unmatched functions tested, 300 generic seeds each
plus 500 with a setup where one exists; every function reached at least 60 % of the
original's branches both ways; nine deliberate mutations were all detected. Result: 58
IDENTICAL; ONE real difference, `EftChain_BlendKeys` (a row index, fixed the same day, now
IDENTICAL on 200 seeds); `ColObb_Contact` not run (no faithful attempt). Usage:
`python3 build/scratch_difftest/gdt.py src/<file>.c <Func> <seeds> [--nosetup] [--seed N]`.
Re-run it on any attempt that is rewritten and stays unmatched.

## Lessons from the second permuter batch (2026-10-05)

- A PERMUTER SCORE OF 0 IS NOT PROOF: its scorer ignores stack offsets and it reuses a live
  variable's slot. 2 of 13 "solved" candidates were not exact, and one (`EftRibbon_DrawKind1`)
  was behaviourally WRONG. Run build/permuter/verify.py on every candidate, then fdiff on the
  real file, then the differential test if the form looks odd.
- Each constant set at the head of a variable's insn chain doubles its live length in
  local-alloc; swapping the arms of an if / else (which arm assigns the constant first)
  rotates saved registers without changing the code (`Num_DrawEx`).
- `ids[i]` versus `*ids` with `ids++` in the `for` header: same instructions, different live
  range for `i`. `T *c = NULL;` plus a later assignment keeps its own pseudo
  (`EftBlade_GetColor`). A field pointer assigned AFTER a store to that field
  (`flash->flags &= ~END; flags = &flash->flags; if (*flags & PLAY)`) keeps a second
  register (`Flash_Advance`). A 64-bit packet word built in two statements changes store
  order against its neighbour (`Sprite_DrawPicture`).
- An "empty" function that still copies its by-value struct to the frame: a dead float local
  read from the struct, compared and conditionally reassigned (`EftLink_Warp / SetDir`).
- A local initialised to a SYMBOL ADDRESS is still an instruction at the first scheduling
  pass even when propagated away; constants are not. Useful probe for "one instruction more
  in front of a call".
- The "surviving copy" family has no single form: an initialiser plus a later assignment for
  one member; a second `r = w` on a partial path inside the loop for the ribbon placers
  (kept as fakes).
FAKE MATCHES added (marked in the sources): `BtlText_PutSprite` (`y0++; y0--;`),
`EftAnimPart_Draw` (`other++; other--;`), `DcPass_DrawRows` (a second `buf = name`),
`EftRibbon_PlaceStrip`, `EftRibbon_PlaceTrail2` (a second `r = w`), `EftZap_Draw` (a function
pointer local). `EftLink_DrawBillboard` now carries the tag too.

## `BtlInput_Update` (2026-10-05): matched as a FAKE MATCH; allocator facts from the gcc source

`BtlInput_Update` (src/battle/btl_input.c) is live through an empty asm with operands behind
the computation of `pressed`: `__asm__("" : "=r"(pressed) : "0"(pressed));` plus the `pressed`
store moved behind the float stores. It emits no instruction; it is one more register copy
tied to `pressed` (two more references). Behaviour: differential test on the final form,
4000 seeds, 8 of 8 branches, no difference. No natural form was found; the note in the source
has the arithmetic argument why no statement order can work with four references.
- From gcc's local-alloc.c (2.95.3 source in build/scratch_cleanup5_IN/gccsrc/): quantities
  with a hard-register SUGGESTION (a copy to or from a hard register) are allocated first, to
  that register; only the rest go by `floor_log2(refs) * refs * size / length`, ties to the
  first-born; a candidate register is excluded if the hard register is live anywhere in the
  quantity's lifetime.
- TOOL: `__asm__("" : "=r"(v) : "0"(v));` adds two references and one tied copy to `v`,
  emits nothing and is NOT a scheduling barrier (a bare `__asm__("")` is). It is a precise
  probe for "this quantity needs N more references" hypotheses, and a last-resort fake.
- Things removed before allocation (so they cannot add a reference): `x++; x--;` folded by
  combine unless the variable has a use in between; a double store (dead store); a field
  stored to itself or reloaded (cse); a `register ... asm("$2")` variable.
- `do { } while (0)` inside a block is a scheduling barrier; a dead conditional splits the
  block.
