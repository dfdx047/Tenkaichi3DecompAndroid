"""The game's packs offline: Bpe_Decode (sys/bpe.c: Philip Gage's byte-pair "expand") and the texture lists inside."""
import struct
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')).replace('\\', '/')  # the repository



def decode(data, off=0):
    size, pack = struct.unpack_from('<II', data, off)
    p = off + 8
    end = p + pack
    out = bytearray()
    while p < end:
        left = list(range(256)); right = [0] * 256
        count = data[p]; p += 1
        c = 0
        while True:
            if count > 127:
                c += count - 127; count = 0
            if c == 256:
                break
            for _ in range(count + 1):
                left[c] = data[p]; p += 1
                if c != left[c]:
                    right[c] = data[p]; p += 1
                c += 1
            if c == 256:
                break
            count = data[p]; p += 1
        n = data[p] * 256 + data[p + 1]; p += 2
        stack = []
        while True:
            if stack:
                ch = stack.pop()
            else:
                if n == 0:
                    break
                n -= 1
                ch = data[p]; p += 1
            if ch == left[ch]:
                out.append(ch)
            else:
                stack.append(right[ch]); stack.append(left[ch])
    return bytes(out[:size])


def section(pack, n):
    return (struct.unpack_from('<I', pack, n * 4)[0] >> 2) << 2


def textures(pack, n):
    """The textures of texture list `n` of an unpacked pack, each as mmtex's dump format (entry, pixels, palette)."""
    base = section(pack, n)
    count = struct.unpack_from('<i', pack, base)[0]
    tex = base + 0x20  # the entries follow the 0x20-byte header (the pointer at +0x10 is filled in by the game)
    out = []
    if not 0 < count <= 64 or tex + count * 0x40 > len(pack):
        return out
    for i in range(count):
        e = pack[tex + i * 0x40:tex + i * 0x40 + 0x40]
        isz, csz = struct.unpack_from('<II', e, 8)
        io, co = (4 * v for v in struct.unpack_from('<II', e, 0))  # in words from the list's start; +0x38 / +0x3C are the game's pointers
        if base + io + isz > len(pack) or base + co + csz > len(pack) or isz > 0x100000:
            return out
        out.append(e + pack[base + io:base + io + isz] + pack[base + co:base + co + csz])
    return out


def image(tex):
    """A texture from textures() as a PIL image (the entry's own size fields are only filled in by the game: the
    size is read from the upload packet in front of the pixels)."""
    from PIL import Image
    import mmtex
    isz, csz = struct.unpack_from('<II', tex, 8)
    img, clut = tex[0x40:0x40 + isz], tex[0x40 + isz:0x40 + isz + csz]
    w = h = 0
    for o in range(0x10, 0x60, 0x10):
        if img[o + 8] == 0x52:
            w, h = struct.unpack_from('<II', img, o)
    n = isz - 0x80
    if csz >= 0x400 + 0x80:  # 256 colours
        W, H = (w * 2, h * 2) if w * h * 4 == n else (w, h)
        if W * H != n:
            return None
        idx = mmtex.unswizzle8(img[0x60:0x60 + n], W, H) if w * h * 4 == n else list(img[0x60:0x60 + n])
        cl = clut[0x60:0x60 + 1024]
        pal = [tuple(cl[i * 4:i * 4 + 4]) for i in range(256)]
        pal = [pal[(i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)] if (i & 0x18) in (8, 16) else pal[i] for i in range(256)]
    else:
        return None
    im = Image.new('RGBA', (W, H))
    im.putdata([(pal[i][0], pal[i][1], pal[i][2], min(255, pal[i][3] * 2)) for i in idx])
    return im
