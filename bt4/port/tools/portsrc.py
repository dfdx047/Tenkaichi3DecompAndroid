"""Shared by the port's build tools: the list of game sources of the PC build and their preparation.

The matching decompilation keeps the C of a function that does not match yet inside `#if 0`, next to an
INCLUDE_ASM line. The PC build wants that C. prepare() writes a copy of the source to port/build/gen/src with
those `#if 0` turned into `#if 1`; nothing else changes, and sources without such functions are used as they are."""
import pathlib, re

ROOT = pathlib.Path(__file__).resolve().parents[2]
GEN = ROOT / "port/build/gen/src"
# Replaced as a whole by PC code under port/src (hand-written VU0 assembly inside): see docs/port/README.md.
REPLACED = {"src/sys/vu0_a_c.c", "src/sys/vu0_a_c_b.c", "src/sys/vu0_a_c_c.c", "src/sys/vu0_b_c.c"}

def sources():
    # the main executable (engine, battle) and the menu overlay (DBZP.BIN on the PS2; one program here)
    fs = [ROOT / "src/main.c"] + sorted((ROOT / "src/sys").glob("*.c")) + sorted((ROOT / "src/battle").glob("*.c")) + \
         sorted((ROOT / "src/menu").glob("*.c"))
    return [f for f in fs if f.relative_to(ROOT).as_posix() not in REPLACED]

def enable_attempts(text):
    """Returns (text, enabled names, names with no attempt found)."""
    lines = text.split("\n")
    done, missing = [], []
    for name in re.findall(r"^INCLUDE_ASM\([^,]+, (\w+)\);", text, re.M):
        pat = re.compile(r"^[A-Za-z_][\w \*]*\b" + name + r"\(.*[,{(]\s*$|^[A-Za-z_][\w \*]*\b" + name + r"\(.*\) \{")
        defs = [i for i, l in enumerate(lines) if pat.match(l) and not l.rstrip().endswith(";")]
        if not defs:
            missing.append(name)
            continue
        depth, opener = 0, None
        for i in range(defs[-1], -1, -1):  # the innermost conditional that encloses the definition
            l = lines[i]
            if l.startswith("#endif"):
                depth += 1
            elif l.startswith("#if"):
                if depth == 0:
                    opener = i
                    break
                depth -= 1
        if opener is not None and re.match(r"#if 0\b", lines[opener]):
            lines[opener] = "#if 1" + lines[opener][5:]  # keep what follows: a note may start on this line
        done.append(name)  # no `#if 0` around it: already compiled (ASM_STUB form)
    return "\n".join(lines), done, missing

def prepare(f):
    """Path to compile for game source f, plus the names enabled and the names without C."""
    text = f.read_text()
    if "INCLUDE_ASM(" not in text:
        return f, [], []
    new, done, missing = enable_attempts(text)
    out = GEN / f.relative_to(ROOT)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(new)
    return out, done, missing
