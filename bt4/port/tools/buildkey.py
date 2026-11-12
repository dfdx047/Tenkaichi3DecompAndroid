"""The build key: from the build's environment to the one file that uses it (port/src/gs/net_sign.c).

    BT3_BUILD_KEY=<16 to 128 characters of A-Z a-z 0-9 . _ - + / =>   the key of the release being built

An official release is built with it (the workflow takes it from the repository secret of the same name) and signs
its requests to the matchmaking service; see net_sign.c and port/matchmaking/README.md. Without the variable, or with
an empty one (a fork, a pull request, a developer's build), the program is built without a key and is what it was.

The key is written to no file: it goes to the compiler as two macros on the command line of net_sign.c alone, already
scrambled (which keeps it out of `strings`, no more), and nothing here prints it."""
import hashlib, os, re

def flags():
    """The compiler arguments that carry the key, [] without one. Stops the build on a key that cannot be used."""
    key = os.environ.get("BT3_BUILD_KEY", "").strip()
    if not key:
        return []
    if not re.fullmatch(r"[A-Za-z0-9._+/=-]{16,128}", key):
        raise SystemExit("FAILED build key: BT3_BUILD_KEY must be 16 to 128 characters of A-Z a-z 0-9 . _ - + / = (it is not; its value is not shown)")
    data = key.encode()
    # where the sequence starts: any number but 0 (the same key gives the same program)
    seed = int.from_bytes(hashlib.sha256(b"bt3 build key seed" + data).digest()[:4], "big") or 1
    x, out = seed, []
    for b in data:  # the same sequence as net_sign.c's key_get
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        out.append(b ^ ((x >> 11) & 0xFF))
    return [f"-DBT3_BUILD_KEY_SEED=0x{seed:08x}u", "-DBT3_BUILD_KEY_DATA=" + ",".join(str(b) for b in out)]
