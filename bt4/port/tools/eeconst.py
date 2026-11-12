"""Float constants as the PS2 compiler makes them.

ee-gcc 2.96 converts a decimal float constant by TRUNCATING it (0.1f is 0x3DCCCCCC, not 0x3DCCCCCD; doubles too),
and when it folds constant arithmetic at compile time it truncates each result as well (1.0f / 30.0f is
0x3D088888). A host compiler rounds to nearest in both places, so the same source gives constants that differ in
the last bit about half the time. Measured with the project's own ee-gcc; see port/tests/ps2float.

transform(text) takes PREPROCESSED C and returns it with
  - every float / double constant replaced by a hexadecimal constant holding the truncated value,
  - `(f32)` / `(float)` casts of constants applied,
  - arithmetic between two constants (+ - * /) folded the way ee-gcc folds it, where C's grammar makes the two
    constants operands of the same operator.
Constant arithmetic that only appears after the compiler propagates variables is not covered; port/tools/
undefined.py --check-fold lists where the host compiler still folds inexact float arithmetic.
"""
import os, re
from fractions import Fraction

TOKEN = re.compile(r'''
    (?P<str>"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')
  | (?P<line>^\#[^\n]*$)
  | (?P<flt>(?<![\w.])(?:(?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)(?P<suf>[fFlL]?)(?![\w.]))
  | (?P<hexf>(?<![\w.])0[xX][0-9a-fA-F]+p[+-]?\d+[fF]?(?![\w.]))
  | (?P<int>(?<![\w.])(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*(?![\w.]))
  | (?P<id>[A-Za-z_]\w*)
  | (?P<ws>\s+)
  | (?P<op>->|\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|.)
''', re.X | re.M | re.S)

class K:
    """A constant: exact value as a Fraction, kind 'f' (float), 'd' (double) or 'i' (integer text kept)."""
    def __init__(self, val, kind, text=None):
        self.val, self.kind, self.text = val, kind, text

def trunc(val, bits, emin, emax):
    """Largest-magnitude binary float with `bits` mantissa bits not exceeding |val| (round toward zero)."""
    if val == 0:
        return Fraction(0), 0, 0
    a = abs(val)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    if Fraction(2) ** e > a:
        e -= 1
    e = max(e, emin)
    m = int(a / Fraction(2) ** (e - (bits - 1)))  # floor
    if e > emax:
        e, m = emax, (1 << bits) - 1
    return (Fraction(m) * Fraction(2) ** (e - (bits - 1))) * (1 if val > 0 else -1), m, e - (bits - 1)

def make(val, kind):
    if kind == 'f':
        v, _, _ = trunc(val, 24, -126, 127)
    else:
        v, _, _ = trunc(val, 53, -1022, 1023)
    return K(v, kind)

def render(k):
    if k.kind == 'i':
        return k.text
    bits = 24 if k.kind == 'f' else 53
    v, m, e = trunc(k.val, bits, -126 if k.kind == 'f' else -1022, 127 if k.kind == 'f' else 1023)
    s = "-" if k.val < 0 else ""
    body = f"{s}0x{m:x}p{e}" + ("f" if k.kind == 'f' else "")
    return f"({body})" if s else body

def parse_hexf(t):
    isf = t[-1] in "fF"
    m = re.match(r"0[xX]([0-9a-fA-F]+)p([+-]?\d+)", t)
    return K(Fraction(int(m.group(1), 16)) * Fraction(2) ** int(m.group(2)), 'f' if isf else 'd')

# `introduced=10.8` in Apple's availability attributes (the macOS SDK's headers) is a version, not a number.
# Only the macOS build has such headers: the other builds read their text exactly as before.
VERSION_KEYS = {"introduced", "deprecated", "obsoleted"} if os.environ.get("BT3_CC") == "mac64" else set()

def lex(text):
    out = []
    for m in TOKEN.finditer(text):
        g = m.lastgroup
        t = m.group(g)
        if g == "flt" and VERSION_KEYS and len(out) >= 2 and out[-1] == ("op", "=") and out[-2][0] == "id" and out[-2][1] in VERSION_KEYS:
            out.append(("str", t))
        elif g == "flt":
            suf = m.group("suf")
            body = t[:-1] if suf else t
            out.append(("k", make(Fraction(body), 'f' if suf in ("f", "F") else 'd')))
        elif g == "hexf":
            out.append(("k", parse_hexf(t)))
        elif g == "int":
            digits = re.match(r"0[xX][0-9a-fA-F]+|\d+", t).group(0)
            # a leading 0 is C's octal (0644, 01), which Python's base 0 rejects
            val = int(digits, 8) if len(digits) > 1 and digits[0] == '0' and digits[1] not in 'xX' else int(digits, 0)
            out.append(("k", K(Fraction(val), 'i', t)))
        elif g in ("str", "line", "ws"):
            out.append((g, t))
        elif g == "id":
            out.append(("id", t))
        else:
            out.append(("op", t))
    return out

BIN = {"*": 3, "/": 3, "%": 3, "+": 2, "-": 2}
FLOAT_NAMES = {"f32", "float"}

def fold(toks):
    """One pass of local rewrites over the significant tokens; returns (tokens, changed)."""
    sig = [i for i, (k, _) in enumerate(toks) if k not in ("ws", "line")]
    pos = {i: n for n, i in enumerate(sig)}
    def S(n):
        return toks[sig[n]] if 0 <= n < len(sig) else ("op", "")
    def isk(t, kinds="fd"):
        return t[0] == "k" and t[1].kind in kinds
    def is_op(t, s):
        return t[0] == "op" and t[1] == s
    for n in range(len(sig)):
        t = S(n)
        # ( K )  ->  K      (not a call's argument list and not a cast target: previous token is not a name)
        if is_op(t, "(") and isk(S(n + 1)) and is_op(S(n + 2), ")") and S(n - 1)[0] != "id" and not is_op(S(n - 1), ")"):
            return toks[:sig[n]] + [S(n + 1)] + toks[sig[n + 2] + 1:], True
        # ( f32 ) K  ->  float K
        if is_op(t, "(") and S(n + 1)[0] == "id" and S(n + 1)[1] in FLOAT_NAMES and is_op(S(n + 2), ")") and \
                isk(S(n + 3), "fdi") and not (is_op(S(n + 4), ".") or is_op(S(n + 4), "[")):
            return toks[:sig[n]] + [("k", make(S(n + 3)[1].val, 'f'))] + toks[sig[n + 3] + 1:], True
        # ( f32 ) ( K )  ->  float K
        if is_op(t, "(") and S(n + 1)[0] == "id" and S(n + 1)[1] in FLOAT_NAMES and is_op(S(n + 2), ")") and \
                is_op(S(n + 3), "(") and isk(S(n + 4), "fdi") and is_op(S(n + 5), ")"):
            return toks[:sig[n]] + [("k", make(S(n + 4)[1].val, 'f'))] + toks[sig[n + 5] + 1:], True
        # unary minus on a float constant: previous token is an operator or an opening
        if is_op(t, "-") and isk(S(n + 1)) and (S(n - 1)[0] == "op" and S(n - 1)[1] not in (")", "]") or
                                                 S(n - 1)[0] == "id" and S(n - 1)[1] in ("return", "case")):
            k = S(n + 1)[1]
            if S(n + 2)[0] == "op" and S(n + 2)[1] in ("*", "/", "%"):
                continue  # -a * b: leave the product to the binary rule first (same value, simpler order)
            return toks[:sig[n]] + [("k", K(-k.val, k.kind))] + toks[sig[n + 1] + 1:], True
        # K op K with at least one float / double constant
        if t[0] == "op" and t[1] in ("*", "/", "+", "-") and S(n - 1)[0] == "k" and S(n + 1)[0] == "k":
            a, b = S(n - 1)[1], S(n + 1)[1]
            if a.kind == "i" and b.kind == "i":
                continue
            prev, nxt = S(n - 2), S(n + 2)
            p = BIN[t[1]]
            # the left constant must not belong to an operator on its left, nor the right one to a tighter one
            if prev[0] == "k" or prev[0] == "id" and prev[1] not in ("return", "case") or \
                    prev[0] == "op" and (prev[1] in (")", "]") or prev[1] in BIN and BIN[prev[1]] >= p):
                continue
            if nxt[0] == "op" and (nxt[1] in BIN and BIN[nxt[1]] > p or nxt[1] in (".", "[", "(")):
                continue
            if t[1] == "/" and b.val == 0:
                continue
            kind = "d" if "d" in (a.kind, b.kind) else "f"
            av = make(a.val, kind).val
            bv = make(b.val, kind).val
            v = {"*": av * bv, "/": av / bv if bv else 0, "+": av + bv, "-": av - bv}[t[1]]
            return toks[:sig[n - 1]] + [("k", make(v, kind))] + toks[sig[n + 1] + 1:], True
    return toks, False

def transform(text):
    toks = lex(text)
    if not any(k == "k" and v.kind in "fd" for k, v in toks):
        return text
    # fold statement by statement (bounded work per rewrite): split at ; { }
    out, chunk = [], []
    state = {"float_init": 0}  # brace depth of an initialiser of a float object, 0 = none

    def is_float_decl(c):
        """`[static] [const] float name[...] = ...`: a declaration of float objects (not a function)."""
        head = []
        for k, v in c:
            if k in ("ws", "line"):
                continue
            if k == "op" and v == "=":
                return any(h[0] == "id" and h[1] in FLOAT_NAMES for h in head) and \
                    not any(h[0] == "op" and h[1] == "(" for h in head) and \
                    not any(h[0] == "id" and h[1] == "double" for h in head)
            head.append((k, v))
        return False

    def flush():
        c = chunk[:]
        if any(k == "k" and v.kind in "fd" for k, v in c):
            changed = True
            while changed:
                c, changed = fold(c)
        opens = sum(1 for k, v in c if k == "op" and v == "{")
        closes = sum(1 for k, v in c if k == "op" and v == "}")
        # a double constant that initialises a float object is converted by the compiler: truncated as well
        if state["float_init"] or is_float_decl(c):
            c = [("k", make(v.val, 'f')) if k == "k" and v.kind == "d" else (k, v) for k, v in c]
            if state["float_init"] or opens:
                state["float_init"] += opens - closes
                if state["float_init"] < 0:
                    state["float_init"] = 0
        out.extend(c)
        chunk.clear()
    for t in toks:
        chunk.append(t)
        if t[0] == "op" and t[1] in (";", "{", "}") or t[0] == "line":
            flush()
    flush()
    return "".join(render(v) if k == "k" else v for k, v in out)

if __name__ == "__main__":
    import sys
    sys.stdout.write(transform(sys.stdin.read()))
