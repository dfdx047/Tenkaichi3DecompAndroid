#!/usr/bin/env python3
"""Fixes LLVM IR that clang produces for 4-byte pointers (address space 271, `__ptr32 __uptr`) and that the x86-64
code generator of LLVM 22 cannot select:

  - memcpy / memmove / memset intrinsics on such pointers ("cannot lower memory intrinsic in address space 271")
  - calls through such a pointer ("Cannot select: X86ISD::CALL")
  - a `bitcast` between the two kinds of pointer, which clang emits for some conditional expressions and LLVM rejects

Each operand is first cast to an ordinary pointer (`addrspacecast`, a zero extension), which is what the source
means anyway. Text in, text out; used by undefined.py between `clang -emit-llvm` and `llc`."""
import re, sys

ARG = re.compile(r'ptr addrspace\(271\)((?: (?:noundef|nonnull|readonly|writeonly|noalias|nocapture|captures\(\w+\)|align \d+|dereferenceable(?:_or_null)?\(\d+\)))*) (%[\w.$-]+|@[\w.$-]+|%"[^"]*"|@"[^"]*")')
MEM = re.compile(r'@llvm\.mem(cpy|move|set)((?:\.inline)?)((?:\.p\d+)+)\.(i\d+)')
CALL = re.compile(r'^(\s*(?:%[\w.$-]+ = )?(?:tail |musttail |notail )?call (?:\w+ )*?)addrspace\(271\) (.*)$')

CASTED = "ptr addrspace(271) addrspacecast (ptr "

def data_line(line):
    """A global's definition or a type definition: 4-byte pointers written as i32, so that the address of a symbol in
    static data becomes `.long symbol` (the assembly printer rejects the address-space cast there)."""
    out, pos = [], 0
    while True:
        k = line.find(CASTED, pos)
        if k < 0:
            break
        j, depth = k + len(CASTED), 0
        while not (depth == 0 and line.startswith(" to ptr addrspace(271))", j)):
            depth += line[j] == "("
            depth -= line[j] == ")"
            j += 1
        out.append(line[pos:k] + "i32 ptrtoint (ptr " + line[k + len(CASTED):j] + " to i32)")
        pos = j + len(" to ptr addrspace(271))")
    line = "".join(out) + line[pos:]
    line = re.sub(r'ptr addrspace\(271\) inttoptr \(i(?:32|64) (-?\d+) to ptr addrspace\(271\)\)', r'i32 \1', line)
    line = line.replace("ptr addrspace(271) null", "i32 0").replace("ptr addrspace(271) undef", "i32 undef").replace("ptr addrspace(271) poison", "i32 poison")
    return line.replace("ptr addrspace(271)", "i32")

BITCAST = re.compile(r'^(\s*%[\w.$-]+ = )bitcast (ptr(?: addrspace\(271\))? \S+) to (ptr(?: addrspace\(271\))?)(?=,|$)')
GLOBAL = re.compile(r'^@[^=]+ = .*\b(global|constant)\b')
TYPEDEF = re.compile(r'^%[^=]+ = type ')

STORE = re.compile(r'^(\s*)store ')

def store_line(line, n):
    """`store ..., ptr addrspace(27x) P` -> a cast of P to an ordinary pointer, then the store through that.
    The AArch64 code generator of LLVM 21 drops the truncation of a store through a 32-bit pointer: `store i8` becomes
    a 4-byte `str w`, and a `store i32` of a truncated i64 an 8-byte `str x`, so every byte and halfword the game wrote
    through a pointer also cleared the bytes after it. Through an ordinary pointer the store is selected correctly.
    Returns (pre-line or None, line, n)."""
    depth, k, i = 0, -1, 0
    while i < len(line):
        ch = line[i]
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        elif depth == 0 and line.startswith(", ptr addrspace(27", i):
            k = i
        i += 1
    if k < 0:
        return None, line, n
    start = k + 2
    sp = line.index(")", start) + 2          # after "ptr addrspace(27x) "
    j, depth = sp, 0
    while j < len(line) and not (depth == 0 and line[j] == ","):
        depth += line[j] in "([{<"
        depth -= line[j] in ")]}>"
        j += 1
    n += 1
    pre = f"  %p32st.{n} = addrspacecast {line[start:j]} to ptr"
    return pre, line[:start] + f"ptr %p32st.{n}" + line[j:], n

def fix(text, stores=False):
    out, n, declared, need = [], 0, set(), {}
    for line in text.split("\n"):
        if "addrspace(271)" in line and (GLOBAL.match(line) or TYPEDEF.match(line)):
            out.append(data_line(line))
            continue
        if stores and "addrspace(27" in line and STORE.match(line):
            pre, line, n = store_line(line, n)
            if pre is not None:
                out.append(pre)
        if " bitcast " in line and "addrspace(271)" in line:
            # clang writes a plain bitcast between a 4-byte and an ordinary pointer in a conditional expression that
            # mixes the two (`a ? param : &obj->field`); the conversion that exists for that is addrspacecast
            fixed = BITCAST.sub(r"\1addrspacecast \2 to \3", line)
            if fixed != line:
                n += 1
                line = fixed
        if line.startswith("declare"):
            m = MEM.search(line)
            if m:
                declared.add(m.group(0))
        elif "@llvm.mem" in line and "addrspace(271)" in line and " call " in " " + line:
            m = MEM.search(line)
            if m:
                pre = []
                def cast(a):
                    nonlocal n
                    n += 1
                    pre.append(f"  %p32fix.{n} = addrspacecast ptr addrspace(271) {a.group(2)} to ptr")
                    return f"ptr{a.group(1)} %p32fix.{n}"
                line = ARG.sub(cast, line)
                # operands that are constant expressions: "addrspacecast (...)", "getelementptr inbounds (...)"
                while True:
                    k = re.search(r'ptr addrspace\(271\)((?: (?:noundef|nonnull|readonly|writeonly|noalias|nocapture|captures\(\w+\)|align \d+|dereferenceable(?:_or_null)?\(\d+\)))*) (?=[a-z])', line)
                    if not k:
                        break
                    j = line.index("(", k.end())
                    depth = 0
                    while True:
                        depth += line[j] == "("
                        depth -= line[j] == ")"
                        j += 1
                        if depth == 0:
                            break
                    n += 1
                    pre.append(f"  %p32fix.{n} = addrspacecast ptr addrspace(271) {line[k.end():j]} to ptr")
                    line = line[:k.start()] + f"ptr{k.group(1)} %p32fix.{n}" + line[j:]
                name = f"@llvm.mem{m.group(1)}{m.group(2)}" + ".p0" * m.group(3).count(".p") + "." + m.group(4)
                line = line.replace(m.group(0), name)
                need[name] = (m.group(1), m.group(3).count(".p"), m.group(4))
                out.extend(pre)
        else:
            m = CALL.match(line)
            if m:
                rest = m.group(2)
                k = re.search(r'addrspacecast \(ptr (@[\w.$-]+|@"[^"]*") to ptr addrspace\(271\)\)\(', rest)
                c = re.search(r'(%[\w.$-]+|%"[^"]*")\(', rest)
                if k and (not c or k.start() < c.start()):  # a known function behind a folded cast: call it directly
                    n += 1
                    line = m.group(1) + rest[:k.start()] + k.group(1) + "(" + rest[k.end():]
                elif c:
                    n += 1
                    out.append(f"  %p32fix.{n} = addrspacecast ptr addrspace(271) {c.group(1)} to ptr")
                    line = m.group(1) + rest[:c.start()] + f"%p32fix.{n}" + rest[c.end(1):]
        out.append(line)
    extra = []
    for name, (kind, ptrs, size) in need.items():
        if name not in declared:
            extra.append(f"declare void {name}(ptr, {'ptr' if kind != 'set' else 'i8'}, {size}, i1)")
    text = "\n".join(out)
    if extra:
        text += "\n" + "\n".join(extra) + "\n"
    return text, n

if __name__ == "__main__":
    t, n = fix(open(sys.argv[1]).read())
    open(sys.argv[2], "w").write(t)
    print(n, "fixes", file=sys.stderr)
