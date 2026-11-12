#!/usr/bin/env python3
"""Sets up (and optionally runs) decomp-permuter on one function of this project.

Usage: scripts/permute.py <src/path/file.c> <FunctionName> [--run] [-jN] [options]

Creates build/permuter/work/<FunctionName>/ with
  base.c         the source file, preprocessed (ee-gcc -E -P -Iinclude) with the `#if 0` attempt of the
                 function enabled and every INCLUDE_ASM dropped, then reduced (see below);
  target.o       the original function: asm/nonmatchings/<file>/<Func>.s assembled with the prelude, or, for
                 a function that is live C (no .s), the project's own object, after checking its bytes
                 against the original image exactly as fdiff does;
  compile.sh     `compile.sh in.c -o out.o`: ee-gcc -O2 -fno-strict-aliasing -G<n> -S, then the project's
                 assembler with include/gcc_prelude.inc, with the -G flags fdiff.py uses for that path;
  settings.toml  func_name, compiler_type = "gcc", weights, and objdump_command = objdump.sh;
  objdump.sh     `objdump -drz` of an object cut down to the listing of the target function, so the score
                 is on that function alone although base.c may hold other functions;
  full.c         the unreduced preprocessed source (reference for the self-check).

Reduction of base.c (the permuter's C parser reads the file whole, and every candidate is the whole file):
  - functions BELOW the target lose their bodies (prototypes remain);
  - functions ABOVE the target keep their bodies only when the target refers to them, directly or through
    other kept functions (a callee the compiler has already seen changes its callers' code); the rest
    become prototypes. `--keep-all` keeps every body above the target. The permuter itself turns every
    definition but the target into a prototype, so a kept body is written as
    `PERM_PRETEND(prototype;) PERM_IGNORE(definition)`: the compiler gets the definition, the permuter's
    parser only the prototype (it therefore never permutes or re-types the callees);
  - top-level `__asm__(...)` blocks (labels.inc, INCLUDE_RODATA, LIT4_WORD, RODATA_ALIGN16, ASM_STUB_*)
    stay, wrapped in PERM_IGNORE so that the parser does not see them but the compiler does.
  The script then compiles full.c and base.c and requires the target function to disassemble identically
  from both (if not, it retries with --keep-all, then gives up), and prints the permuter's own base score.

Options:
  --run            start the permuter on the work folder (foreground; Ctrl-C stops it)
  -jN              permuter threads for --run (default 4)
  --keep-all       keep the bodies of all functions above the target
  --text FILE      take the C text from FILE instead of the source path (the path still selects flags,
                   includes and the target); used to test an older version of a file
  --name NAME      work folder name (default: the function name)
  extra arguments after `--` go to permuter.py (e.g. `-- --stop-on-zero --best-only`)

Results: the permuter writes build/permuter/work/<Func>/output-<score>-<n>/source.c. A score of 0 means
the objdump of the function is equal modulo what the scorer ignores (stack offsets, branch targets,
names of file-local symbols); confirm any candidate with scripts/fdiff.py before believing it.
Nothing under src/, include/ or config/ is modified by this script.
"""

import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import fdiff

ROOT = fdiff.ROOT
PERMUTER = ROOT / "build" / "permuter" / "decomp-permuter"
WORK = ROOT / "build" / "permuter" / "work"
CC = ROOT / fdiff.CC
AS = ROOT / (fdiff.BINUTILS + "as")
OBJDUMP = ROOT / (fdiff.BINUTILS + "objdump")

WEIGHTS = """\
# The remaining differences are register allocation and instruction order: favour temporaries,
# statement / declaration order and expression shape. Passes that change extern or function types are
# off: their results could not be applied without editing shared headers.
perm_temp_for_expr = 100
perm_expand_expr = 30
perm_reorder_stmts = 25
perm_reorder_decls = 25
perm_split_assignment = 15
perm_chain_assignment = 10
perm_commutative = 10
perm_ins_block = 10
perm_struct_ref = 10
perm_cast_simple = 10
perm_randomize_internal_type = 10
perm_randomize_external_type = 0
perm_randomize_function_type = 0
perm_float_literal = 0
"""


def die(msg):
    sys.exit("permute.py: " + msg)


# ---------------------------------------------------------------------------------------------
# Step 1: enable the attempt

def enable_attempt(text, func):
    """Turns the `#if 0` around the definition of `func` into `#if 1`. Returns (text, note)."""
    lines = text.split("\n")
    defn = None
    pat = re.compile(r"^[A-Za-z_][^;=]*\b%s\s*\(" % re.escape(func))
    for i, line in enumerate(lines):
        if pat.match(line):
            # a definition, not a prototype: the declarator is followed by `{` before any `;`
            rest = "\n".join(lines[i:i + 40])
            brace, semi = rest.find("{"), rest.find(";")
            if brace != -1 and (semi == -1 or brace < semi):
                defn = i
                break
    if defn is None:
        die(f"no C definition of {func} in the file (no attempt to permute)")
    stack = []  # (line of the opening directive, text of the directive, seen #else)
    for i in range(defn):
        s = lines[i].lstrip()
        if not s.startswith("#"):
            continue
        d = s[1:].lstrip()
        if re.match(r"(if|ifdef|ifndef)\b", d):
            stack.append([i, d, False])
        elif re.match(r"(else|elif)\b", d) and stack:
            stack[-1][2] = True
        elif re.match(r"endif\b", d) and stack:
            stack.pop()
    note = "live C"
    for i, d, in_else in stack:
        if re.match(r"if\s+0\b", d):
            if in_else:
                die(f"{func} is defined in the #else of an `#if 0` (line {i + 1}): nothing to enable")
            lines[i] = re.sub(r"if\s+0\b", "if 1", lines[i], count=1)
            note = f"enabled the `#if 0` at line {i + 1}"
    # An attempt between ASM_STUB_BEGIN() and ASM_STUB_END() is compiled but skipped by the assembler:
    # drop the pair around the target so that its code reaches the object.
    begin = next((i for i in range(defn, -1, -1) if re.match(r"ASM_STUB_(BEGIN|END)\(\)", lines[i])), None)
    if begin is not None and lines[begin].startswith("ASM_STUB_BEGIN"):
        end = next(i for i in range(defn, len(lines)) if lines[i].startswith("ASM_STUB_END"))
        lines[begin] = lines[end] = ""
        note = f"removed the ASM_STUB pair at lines {begin + 1}/{end + 1}"
    return "\n".join(lines), note


# ---------------------------------------------------------------------------------------------
# Step 2: top-level scan of preprocessed C

def scan_toplevel(text):
    """Splits preprocessed C into top-level items: dicts with kind ('func', 'asm', 'decl'), start, end,
    and for functions name, body (offset of its `{`)."""
    items = []
    i, n = 0, len(text)
    start = None
    depth = 0
    body = None
    last = ""  # last significant character at depth 0
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            if start is None:
                start = i
            i = j + 1
            last = c
            continue
        if c.isspace():
            i += 1
            continue
        if start is None:
            start = i
        if c == "{":
            if depth == 0 and last == ")" and body is None and not text[start:i].lstrip().startswith("__asm__"):
                body = i
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0 and body is not None:
                items.append({"kind": "func", "start": start, "end": i + 1, "body": body,
                              "name": func_name(text[start:body])})
                start, body = None, None
                last = ""
                i += 1
                continue
        elif c == ";" and depth == 0:
            head = text[start:i + 1]
            kind = "asm" if re.match(r"__asm__\s*\(", head) else "decl"
            items.append({"kind": kind, "start": start, "end": i + 1})
            start = None
            last = ""
            i += 1
            continue
        if depth == 0:
            last = c
        i += 1
    if depth != 0:
        die("unbalanced braces in the preprocessed source")
    return items


def func_name(header):
    """Name of the function a definition header (text up to the `{`) declares."""
    h = header.rstrip()
    depth = 0
    for i in range(len(h) - 1, -1, -1):
        if h[i] == ")":
            depth += 1
        elif h[i] == "(":
            depth -= 1
            if depth == 0:
                m = re.search(r"(\w+)\s*$", h[:i])
                return m.group(1) if m else None
    return None


def reduce_source(text, func, keep_all, wrap=True):
    items = scan_toplevel(text)
    funcs = [it for it in items if it["kind"] == "func"]
    target = [it for it in funcs if it["name"] == func]
    if len(target) != 1:
        die(f"{len(target)} definitions of {func} after preprocessing")
    target = target[0]
    above = {it["name"]: it for it in funcs if it["start"] < target["start"]}
    keep = set()
    if keep_all:
        keep = set(above)
    else:
        todo = [target]
        while todo:
            it = todo.pop()
            for word in set(re.findall(r"[A-Za-z_]\w*", text[it["body"]:it["end"]])):
                if word in above and word not in keep:
                    keep.add(word)
                    todo.append(above[word])
    out = []
    pos = 0
    stripped = 0
    for it in items:
        out.append(text[pos:it["start"]])
        chunk = text[it["start"]:it["end"]]
        if it["kind"] == "asm":
            if chunk.count("(") != chunk.count(")"):
                die("top-level __asm__ block with unbalanced parentheses: " + chunk[:60])
            if wrap:
                chunk = "PERM_IGNORE(" + chunk + ")"
        elif it["kind"] == "func" and it is not target and it["name"] not in keep:
            chunk = text[it["start"]:it["body"]].rstrip() + ";"
            stripped += 1
        elif it["kind"] == "func" and it is not target and wrap:
            # The permuter turns every other definition into a prototype: hide the body from its parser
            # (PERM_IGNORE reaches the compiler untouched) and show the parser a prototype instead.
            if chunk.count("(") != chunk.count(")") or "PERM_" in chunk:
                die(f"cannot wrap {it['name']} in PERM_IGNORE (unbalanced parentheses in a literal)")
            chunk = ("PERM_PRETEND(" + text[it["start"]:it["body"]].rstrip() + ";)\n"
                     "PERM_IGNORE(" + chunk + ")")
        out.append(chunk)
        pos = it["end"]
    out.append(text[pos:])
    kept = [name for name in above if name in keep]
    return "".join(out), kept, stripped


# ---------------------------------------------------------------------------------------------
# Step 3: tools

def run(cmd, **kw):
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, capture_output=True, text=True, **kw)


def as_gflag(src):
    return "-G8" if "src/menu/menu_t_b" in str(src) else fdiff.gflag(src)


def write_compile_sh(path, src):
    path.write_text(f"""#!/bin/bash
# compile.sh in.c -o out.o    (generated by scripts/permute.py for {src})
# Same commands as the build (see compile_c in scripts/fdiff.py); the input is already preprocessed.
ROOT="{ROOT}"
IN="$1"
OUT="$3"
ASM="${{OUT%.o}}.s"
"$ROOT/{fdiff.CC}" -O2 -fno-strict-aliasing -I"$ROOT/include" {fdiff.gflag(src)} -w -S "$IN" -o "$ASM" || {{ rm -f "$ASM"; exit 1; }}
"$ROOT/{fdiff.BINUTILS}as" -EL -march=r5900 -mabi=o64 -I"$ROOT/include" -I"$ROOT" {as_gflag(src)} -mno-pdr \\
    "$ROOT/include/gcc_prelude.inc" "$ASM" -o "$OUT"
RC=$?
rm -f "$ASM"
exit $RC
""")
    path.chmod(0o755)


def compile_with(work, c_path, o_path):
    r = run(["bash", work / "compile.sh", c_path, "-o", o_path])
    if r.returncode:
        die(f"compiling {c_path} failed:\n{r.stdout}{r.stderr}")


def write_objdump_sh(path, func):
    path.write_text(f"""#!/bin/bash
# objdump.sh file.o: disassembly (with relocations) of {func} only, cut out of the whole object's listing.
# (`objdump --disassemble=<sym>` is not used: it prints the relocations of the code in front of the
# function under the function's first instruction.)
"{OBJDUMP}" -drz "$1" | awk '$2 == "<{func}>:" {{ p = 1; print; next }} /^[0-9a-f]+ <.*>:$/ {{ p = 0 }} p'
""")
    path.chmod(0o755)


def dump(obj, func):
    """Disassembly of one function with relocations, addresses and branch targets removed."""
    r = run([OBJDUMP, "-drz", obj])
    if r.returncode:
        die(r.stderr)
    rows = []
    inside = False
    for line in r.stdout.splitlines():
        if re.match(r"[0-9a-f]+ <.*>:$", line):
            inside = line.split()[1] == f"<{func}>:"
            continue
        if not inside or "\t" not in line:
            continue
        p = line.split("\t")
        if "R_MIPS" in line:
            rel = " ".join(line.split()[1:])
            if rel.split()[-1].startswith(".") and rows:
                # against a section (file-local data, literal pool): the addend in the instruction is the
                # position inside this object's section, which depends on what else the file holds
                rows[-1] = re.sub(r"-?\b(0x[0-9a-f]+|\d+)(?=\(|$)", "N", rows[-1])
                rel = rel.split("+")[0]
            rows.append("    " + rel)
        else:
            row = re.sub(r"\s*<[^>]*>$", "", " ".join(p[2:]))
            if re.match(r"(b|j)\w*\s", row) and not row.startswith(("jal", "jr")):
                row = re.sub(r"[0-9a-f]+$", "<target>", row)
            rows.append(row)
    if not rows:
        die(f"{func} not found in {obj}")
    return rows


def make_target(work, src, func):
    """Writes target.o; returns a description of where it came from."""
    stem = Path(src).with_suffix("")
    s_file = ROOT / "asm" / "nonmatchings" / stem.relative_to("src") / (func + ".s")
    target = work / "target.o"
    if s_file.exists():
        wrapper = work / "target.s"
        wrapper.write_text(
            '.include "labels.inc"\n.section .text\n.set noat\n.set noreorder\n'
            f'.include "{s_file.relative_to(ROOT)}"\n.set reorder\n.set at\n')
        r = run([AS, "-EL", "-march=r5900", "-mabi=o64", "-Iinclude", "-I.", as_gflag(src), "-mno-pdr",
                 "include/gcc_prelude.inc", wrapper, "-o", target])
        if r.returncode:
            die(f"assembling {s_file} failed:\n{r.stderr}")
        # the object must be the original function, byte for byte (relocated fields masked)
        check_against_image(target, func, work)
        return f"{s_file.relative_to(ROOT)} (bytes checked against the original image)"
    # Live C: the project's object for this file, if the function in it equals the original image.
    live = work / "live.c"
    r = run([CC, "-E", "-P", "-Iinclude", f"-I{Path(src).parent}", src, "-o", live])
    if r.returncode:
        die(r.stderr)
    compile_with(work, live, target)
    check_against_image(target, func, work)
    live.unlink()
    return "the live C object (function bytes checked against the original image)"


def check_against_image(obj, func, work):
    funcs = fdiff.functions(obj)
    if func not in funcs:
        die(f"{func} is not a function of {obj}")
    known = fdiff.targets()
    if func not in known:
        die(f"{func} has no address in the symbol files")
    off, size = funcs[func]
    masks = fdiff.reloc_masks(obj)
    text = fdiff.text_bytes(obj)
    (obj.parent / "text.bin").unlink()
    addr, elf = known[func]
    mine = struct.unpack(f"<{size // 4}I", text[off:off + size])
    orig = fdiff.original_words(work, elf, addr, size)
    bad = [i for i, (m, o) in enumerate(zip(mine, orig)) if (m ^ o) & ~masks.get(off + i * 4, 0) & 0xFFFFFFFF]
    if bad or len(orig) != len(mine):
        die(f"target object for {func} differs from the original image in {len(bad)} instructions "
            "(stale asm/nonmatchings file, or the live C does not match)")


def base_score(work, func):
    """Runs the permuter's own front end and scorer on base.c: (score, error text)."""
    sys.path.insert(0, str(PERMUTER))
    from src import ast_util
    from src.compiler import Compiler
    from src.perm.eval import perm_evaluate_one
    from src.perm.parse import perm_parse
    from src.preprocess import preprocess
    from src.scorer import Scorer
    source = preprocess(str(work / "base.c"))
    source, _ = perm_evaluate_one(perm_parse(source))
    ast = ast_util.parse_c(source)
    ast_util.extract_fn(ast, func)
    out = ast_util.to_c(ast)
    compiler = Compiler(str(work / "compile.sh"), show_errors=True, debug_mode=False)
    obj = compiler.compile(out)
    if obj is None:
        die("the permuter's rendering of base.c does not compile (see the errors above)")
    scorer = Scorer(str(work / "target.o"), stack_differences=False, algorithm="difflib", debug_mode=False,
                    ign_branch_targets=True,
                    objdump_command=str(work / "objdump.sh"))
    score, _ = scorer.score(obj)
    rendered = dump(obj, func)
    os.unlink(obj)
    return score, rendered


def main():
    args = sys.argv[1:]
    extra = []
    if "--" in args:
        k = args.index("--")
        args, extra = args[:k], args[k + 1:]
    opts = [a for a in args if a.startswith("-")]
    pos = [a for a in args if not a.startswith("-")]
    text_file = name = None
    for flag in ("--text", "--name"):
        if flag in args:
            value = args[args.index(flag) + 1]
            pos.remove(value)
            if flag == "--text":
                text_file = value
            else:
                name = value
    if len(pos) != 2:
        sys.exit(__doc__)
    src, func = pos
    src = str(Path(src))
    jobs = next((a[2:] for a in opts if re.fullmatch(r"-j\d+", a)), "4")
    keep_all = "--keep-all" in opts
    if not (ROOT / src).exists():
        die(f"no such file: {src}")

    work = WORK / (name or func)
    if work.exists():
        for old in work.iterdir():
            if not old.name.startswith("output-") and old.name != "run.log":
                old.unlink() if old.is_file() else shutil.rmtree(old)
    work.mkdir(parents=True, exist_ok=True)
    write_compile_sh(work / "compile.sh", src)
    write_objdump_sh(work / "objdump.sh", func)

    text, note = enable_attempt((ROOT / (text_file or src)).read_text(), func)
    enabled = work / "enabled.c"
    enabled.write_text(text)
    # INCLUDE_ASM is dropped; the other macros of include_asm.h keep their real expansions.
    r = run([CC, "-E", "-P", "-Iinclude", f"-I{Path(src).parent}", "-DINCLUDE_ASM(f,n)=", enabled,
             "-o", work / "full.c"])
    if r.returncode:
        die("preprocessing failed:\n" + r.stderr)
    enabled.unlink()
    full = (work / "full.c").read_text()

    compile_with(work, work / "full.c", work / "full.o")
    want = dump(work / "full.o", func)
    for attempt_keep_all in ([keep_all] if keep_all else [False, True]):
        base, kept, stripped = reduce_source(full, func, attempt_keep_all)
        (work / "base.c").write_text(base)
        # base.c holds PERM_IGNORE(...): compile what the permuter would, minus its parser
        plain = work / "base_plain.c"
        plain.write_text(reduce_source(full, func, attempt_keep_all, wrap=False)[0])
        compile_with(work, plain, work / "base_plain.o")
        same = dump(work / "base_plain.o", func) == want
        plain.unlink()
        (work / "base_plain.o").unlink()
        if same:
            keep_all = attempt_keep_all
            break
        print(f"  reduced source ({'all' if attempt_keep_all else 'referenced'} bodies above kept) compiles "
              f"{func} differently from the full file")
    else:
        die("the reduced source does not reproduce the full file's code for the function")

    origin = make_target(work, src, func)
    (work / "settings.toml").write_text(
        f'func_name = "{func}"\ncompiler_type = "gcc"\n'
        f'objdump_command = "{work / "objdump.sh"}"\n\n[weight_overrides]\n{WEIGHTS}')

    score, rendered = base_score(work, func)
    tgt = dump(work / "target.o", func)
    n_tgt = sum(1 for r_ in tgt if not r_.startswith("    "))
    print(f"{func}: work folder {work.relative_to(ROOT)}")
    print(f"  source: {text_file or src} ({note}); {as_gflag(src)}/{fdiff.gflag(src)}")
    print(f"  bodies kept above the target: {len(kept)}{' (all)' if keep_all else ''}"
          f"{' [' + ', '.join(kept[:12]) + (', ...' if len(kept) > 12 else '') + ']' if kept else ''}; "
          f"stripped to prototypes: {stripped}")
    print(f"  target: {origin}; {n_tgt} instructions")
    if rendered != want:
        print("  WARNING: the permuter's rendering of base.c compiles the function differently from the "
              "original text (parser round trip changes code); scores are still relative to the target")
    # symbols the target relocates against that the candidate never names: a floor on the score
    def syms(rows):
        return {r_.split()[-1].split("+")[0] for r_ in rows if r_.startswith("    ")}
    missing = sorted(s for s in syms(tgt) - syms(rendered) if not s.startswith("."))
    cand_named = sorted(s for s in syms(rendered) - syms(tgt) if not s.startswith(".") and not s.startswith("$"))
    if missing and cand_named:
        print(f"  note: target-only symbols {missing[:6]} vs candidate-only {cand_named[:6]}: "
              "if these are two names for one address, the permuter cannot remove the difference "
              "(score floor)")
    print(f"  base score: {score}")

    if "--run" in opts:
        cmd = [sys.executable, str(PERMUTER / "permuter.py"), str(work), f"-j{jobs}"] + extra
        print("  running:", " ".join(cmd), flush=True)
        # A shell that started us in the background (queue.sh under nohup) left SIGINT ignored, and Python
        # then installs no handler: restore the default so that `timeout -s INT` / Ctrl-C stop the run.
        import signal
        signal.signal(signal.SIGINT, signal.SIG_DFL)
        os.execv(sys.executable, cmd)


if __name__ == "__main__":
    main()
