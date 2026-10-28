#!/usr/bin/env python3
"""The key that release archives are signed with, for the game's automatic update (port/src/gs/net_update.c).

Ed25519 (RFC 8032), written out below in plain Python (nothing to install): the game checks the signatures with
Monocypher (port/third_party/monocypher), the same algorithm.

    python3 port/tools/update_key.py generate [--out update_signing_key.txt]
        ONCE, by the repository's keeper. Makes a new key pair: the PRIVATE half goes into the file (never on the
        screen); the PUBLIC half is printed, with where it goes (port/src/update_key.h). Then: the file's one line is
        pasted into the repository secret UPDATE_SIGNING_KEY, and the file is deleted or kept somewhere safe.

    python3 port/tools/update_key.py sign <zip>...      (the release workflow does this)
        Writes <zip>.sig beside each: the signature, 128 hex digits and a line's end. The private key comes from the
        environment variable UPDATE_SIGNING_KEY (--key-file <file> instead). Refuses when the key's public half is
        not the one in port/src/update_key.h: games could never check those signatures (--any-key: sign anyway,
        for tests).

    python3 port/tools/update_key.py public [--key-file <file>]       the public half of a private key
    python3 port/tools/update_key.py verify <zip> [--sig <file>] [--public <hex>]
        Checks a signature as the game does (with the key of port/src/update_key.h unless --public is given).

The private key is the 32-byte Ed25519 seed, base64 (one line of 44 characters).
Signing here is plain Python: fine on a build machine that holds the key for a minute; not hardened against
someone measuring that machine's timing.
"""
import argparse, base64, hashlib, os, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
KEY_HEADER = ROOT / "port/src/update_key.h"
ENV = "UPDATE_SIGNING_KEY"

# ---- Ed25519 (RFC 8032, section 5.1; the reference code of section 6) ----
P = 2 ** 255 - 19
L = 2 ** 252 + 27742317777372353535851937790883648493
D = -121665 * pow(121666, P - 2, P) % P
SQRT_M1 = pow(2, (P - 1) // 4, P)

def _sha512(*parts):
    h = hashlib.sha512()
    for p in parts:
        h.update(p)
    return h.digest()

def _add(a, b):
    x1, y1, z1, t1 = a
    x2, y2, z2, t2 = b
    A = (y1 - x1) * (y2 - x2) % P
    B = (y1 + x1) * (y2 + x2) % P
    C = 2 * t1 * t2 * D % P
    Dd = 2 * z1 * z2 % P
    E, F, G, H = B - A, Dd - C, Dd + C, B + A
    return (E * F % P, G * H % P, F * G % P, E * H % P)

def _mul(s, pt):
    q = (0, 1, 1, 0)
    while s > 0:
        if s & 1:
            q = _add(q, pt)
        pt = _add(pt, pt)
        s >>= 1
    return q

def _equal(a, b):
    x1, y1, z1, _ = a
    x2, y2, z2, _ = b
    return (x1 * z2 - x2 * z1) % P == 0 and (y1 * z2 - y2 * z1) % P == 0

def _recover_x(y, sign):
    if y >= P:
        return None
    x2 = (y * y - 1) * pow(D * y * y + 1, P - 2, P)
    if x2 == 0:
        return None if sign else 0
    x = pow(x2, (P + 3) // 8, P)
    if (x * x - x2) % P != 0:
        x = x * SQRT_M1 % P
    if (x * x - x2) % P != 0:
        return None
    if (x & 1) != sign:
        x = P - x
    return x

G_Y = 4 * pow(5, P - 2, P) % P
G = (_recover_x(G_Y, 0), G_Y, 1, _recover_x(G_Y, 0) * G_Y % P)

def _compress(pt):
    x, y, z, _ = pt
    zi = pow(z, P - 2, P)
    x, y = x * zi % P, y * zi % P
    return int.to_bytes(y | ((x & 1) << 255), 32, "little")

def _decompress(b):
    if len(b) != 32:
        return None
    y = int.from_bytes(b, "little")
    sign = y >> 255
    y &= (1 << 255) - 1
    x = _recover_x(y, sign)
    return None if x is None else (x, y, 1, x * y % P)

def _expand(seed):
    h = _sha512(seed)
    a = int.from_bytes(h[:32], "little")
    a &= (1 << 254) - 8
    a |= 1 << 254
    return a, h[32:]

def public_key(seed):
    a, _ = _expand(seed)
    return _compress(_mul(a, G))

def sign(seed, msg):
    a, prefix = _expand(seed)
    A = _compress(_mul(a, G))
    r = int.from_bytes(_sha512(prefix, msg), "little") % L
    R = _compress(_mul(r, G))
    h = int.from_bytes(_sha512(R, A, msg), "little") % L
    s = (r + h * a) % L
    return R + int.to_bytes(s, 32, "little")

def verify(public, msg, sig):
    if len(public) != 32 or len(sig) != 64:
        return False
    A = _decompress(public)
    R = _decompress(sig[:32])
    if A is None or R is None:
        return False
    s = int.from_bytes(sig[32:], "little")
    if s >= L:
        return False
    h = int.from_bytes(_sha512(sig[:32], public, msg), "little") % L
    return _equal(_mul(s, G), _add(R, _mul(h, A)))

# ---- the files ----
def header_key():
    m = re.search(r'#define\s+PORT_UPDATE_KEY_RELEASE\s+"([0-9a-fA-F]{64})"', KEY_HEADER.read_text())
    if not m:
        sys.exit(f"{KEY_HEADER.relative_to(ROOT)}: PORT_UPDATE_KEY_RELEASE not found")
    return bytes.fromhex(m.group(1))

def load_seed(args):
    if getattr(args, "key_file", None):
        text = pathlib.Path(args.key_file).read_text()
    else:
        text = os.environ.get(ENV, "")
        if not text.strip():
            sys.exit(f"no private key: the environment variable {ENV} is empty (or give --key-file)")
    try:
        seed = base64.b64decode(text.strip(), validate=True)
    except Exception:
        seed = b""
    if len(seed) != 32:
        sys.exit("the private key is not 32 bytes of base64 (the line update_key.py generate wrote); its value is not shown")
    return seed

def cmd_generate(args):
    out = pathlib.Path(args.out)
    if out.exists():
        sys.exit(f"{out} exists already: not overwritten (a key in use would be lost). Choose another --out.")
    seed = os.urandom(32)
    fd = os.open(out, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w") as f:
        f.write(base64.b64encode(seed).decode() + "\n")
    pub = public_key(seed).hex()
    print(f"""A new update signing key was made.

PRIVATE key: written to {out.resolve()}  (not shown here)
  1. On GitHub: the repository > Settings > Secrets and variables > Actions > New repository secret.
     Name: {ENV}
     Secret: the one line of that file (open it in a text editor and copy it).
  2. Then delete the file, or keep it somewhere safe and offline. Whoever has it can sign updates that
     every game will install. Never commit it.

PUBLIC key: put it into port/src/update_key.h, replacing the zeros:
#define PORT_UPDATE_KEY_RELEASE "{pub}"
and commit that. Releases built from then on install signed updates; the next release that is signed is the
first one they can update to.""")

def cmd_public(args):
    print(public_key(load_seed(args)).hex())

def cmd_sign(args):
    seed = load_seed(args)
    pub = public_key(seed)
    want = header_key()
    if pub != want and not args.any_key:
        if want == bytes(32):
            sys.exit(f"{KEY_HEADER.relative_to(ROOT)} has no public key yet (all zeros): put the public half of {ENV} there first "
                     "(update_key.py public prints it). Not signed.")
        sys.exit(f"the key in {ENV} does not match the public key in {KEY_HEADER.relative_to(ROOT)}: games could not check these "
                 "signatures. Not signed.")
    for name in args.files:
        path = pathlib.Path(name)
        sig = sign(seed, path.read_bytes())
        pathlib.Path(str(path) + ".sig").write_text(sig.hex() + "\n")
        print(f"signed {path.name} -> {path.name}.sig")

def cmd_verify(args):
    public = bytes.fromhex(args.public) if args.public else header_key()
    path = pathlib.Path(args.file)
    text = pathlib.Path(args.sig or str(path) + ".sig").read_text().strip()
    ok = re.fullmatch(r"[0-9a-fA-F]{128}", text) is not None and verify(public, path.read_bytes(), bytes.fromhex(text))
    print("signature good" if ok else "signature BAD")
    sys.exit(0 if ok else 1)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("generate")
    g.add_argument("--out", default="update_signing_key.txt")
    p = sub.add_parser("public")
    p.add_argument("--key-file")
    s = sub.add_parser("sign")
    s.add_argument("files", nargs="+")
    s.add_argument("--key-file")
    s.add_argument("--any-key", action="store_true")
    v = sub.add_parser("verify")
    v.add_argument("file")
    v.add_argument("--sig")
    v.add_argument("--public")
    args = ap.parse_args()
    {"generate": cmd_generate, "public": cmd_public, "sign": cmd_sign, "verify": cmd_verify}[args.cmd](args)

if __name__ == "__main__":
    main()
