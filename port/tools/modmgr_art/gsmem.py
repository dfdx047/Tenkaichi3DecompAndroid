"""GS local memory addressing (the tables of port/src/gs/gs_core.c) with numpy, and the BT3 texture file (.dbt)
built on it: textures are sent as 32-bit rectangles and read back as 4- or 8-bit ones.

  dbt_read(data)  -> list of dicts (raw entry fields, 'image' = PIL RGBA or None, 'idx' = indices, 'pal' = (n,4))
  make_texture(indices (h,w) uint8, palette (16|256, 4) uint8) -> (pix packet bytes, clut packet bytes, fields)
"""
import struct
import numpy as np
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')).replace('\\', '/')  # the repository


kBlock32 = np.array([[0, 1, 4, 5, 16, 17, 20, 21], [2, 3, 6, 7, 18, 19, 22, 23], [8, 9, 12, 13, 24, 25, 28, 29], [10, 11, 14, 15, 26, 27, 30, 31]])
kBlock16 = np.array([[0, 2, 8, 10], [1, 3, 9, 11], [4, 6, 12, 14], [5, 7, 13, 15], [16, 18, 24, 26], [17, 19, 25, 27], [20, 22, 28, 30], [21, 23, 29, 31]])
kCol32 = np.array([[0, 1, 4, 5, 8, 9, 12, 13], [2, 3, 6, 7, 10, 11, 14, 15], [16, 17, 20, 21, 24, 25, 28, 29], [18, 19, 22, 23, 26, 27, 30, 31],
                   [32, 33, 36, 37, 40, 41, 44, 45], [34, 35, 38, 39, 42, 43, 46, 47], [48, 49, 52, 53, 56, 57, 60, 61], [50, 51, 54, 55, 58, 59, 62, 63]])
kCol8First = np.array([[0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54], [8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62],
                       [33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23], [41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31]])
kCol4First = np.array([
    [0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106, 4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110],
    [16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122, 20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126],
    [65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43, 69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47],
    [81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59, 85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63]])
sCol8 = np.zeros((16, 16), dtype=np.int64)
sCol4 = np.zeros((16, 32), dtype=np.int64)
for _y in range(16):
    _c = _y >> 2
    for _x in range(16):
        sCol8[_y, _x] = kCol8First[_y & 3][(_x ^ 4) if _c & 1 else _x] + _c * 64
    for _x in range(32):
        sCol4[_y, _x] = kCol4First[_y & 3][(_x ^ 4) if _c & 1 else _x] + _c * 128


def _grid(w, h):
    y, x = np.mgrid[0:h, 0:w]
    return x.astype(np.int64), y.astype(np.int64)


def addr32(bw, w, h):
    """Index in 32-bit words of every pixel of a 32-bit w x h rectangle at block 0."""
    x, y = _grid(w, h)
    return (((y & ~0x1F) * bw + ((x >> 1) & ~0x1F) + kBlock32[(y >> 3) & 3, (x >> 3) & 7]) << 6) + kCol32[y & 7, x & 7]


def addr8(bw, w, h):
    """Index in bytes of every pixel of an 8-bit buffer."""
    x, y = _grid(w, h)
    return ((((y >> 1) & ~0x1F) * (bw >> 1) + ((x >> 2) & ~0x1F) + kBlock32[(y >> 4) & 3, (x >> 4) & 7]) << 8) + sCol8[y & 15, x & 15]


def addr4(bw, w, h):
    """Index in nibbles of every pixel of a 4-bit buffer."""
    x, y = _grid(w, h)
    return ((((y >> 2) & ~0x1F) * (bw >> 1) + ((x >> 2) & ~0x1F) + kBlock16[(y >> 4) & 7, (x >> 5) & 3]) << 9) + sCol4[y & 15, x & 31]


def csm1(pal):
    """The CLUT's storage order for 256 entries (entries 8..15 and 16..23 of every 32 exchanged); its own inverse."""
    pal = np.array(pal)
    if len(pal) != 256:
        return pal
    idx = np.arange(256)
    sw = (idx & 0x18)
    idx = np.where(sw == 8, idx + 8, np.where(sw == 16, idx - 8, idx))
    return pal[idx]


def decode(pix, clut, tex0, upw, uph, dbw):
    """pix: the uploaded 32-bit rectangle's bytes; clut: uploaded CLUT bytes (or None)."""
    psm = (tex0 >> 20) & 0x3F
    tbw = (tex0 >> 14) & 0x3F
    w = 1 << ((tex0 >> 26) & 0xF)
    h = 1 << ((tex0 >> 30) & 0xF)
    mem = np.zeros(1 << 22, dtype=np.uint8)
    words = np.frombuffer(pix[:upw * uph * 4], dtype=np.uint32)
    m32 = mem.view(np.uint32)
    m32[addr32(max(dbw, 1), upw, uph).ravel()] = words
    if psm == 0x14:
        a = addr4(tbw, w, h)
        idx = (mem[a >> 1] >> ((a & 1) * 4)) & 15
    elif psm == 0x13:
        idx = mem[addr8(tbw, w, h)]
    else:
        return m32[addr32(max(tbw, 1), w, h)].view(np.uint8).reshape(h, w, 4), None
    return idx.astype(np.uint8), psm


def encode(idx, bits):
    """indices (h, w) -> (bytes of the 32-bit rectangle to upload, its width, height, DBW, TBW)."""
    h, w = idx.shape
    tbw = w // 64
    mem = np.zeros(1 << 22, dtype=np.uint8)
    if bits == 4:
        upw, uph = w // 2, h // 4
        a = addr4(tbw, w, h).ravel()
        np.bitwise_or.at(mem, a >> 1, ((idx.ravel().astype(np.uint8) & 15) << ((a & 1) * 4).astype(np.uint8)).astype(np.uint8))
    else:
        upw, uph = w // 2, h // 2
        mem[addr8(tbw, w, h).ravel()] = idx.ravel()
    dbw = upw // 64
    out = mem.view(np.uint32)[addr32(max(dbw, 1), upw, uph)].astype('<u4').tobytes()
    return out, upw, uph, dbw, tbw


def image_packet(data, w, h):
    """The game's upload packet without BITBLTBUF: VIF direct, TRXPOS / TRXREG / TRXDIR, the image, TEXFLUSH."""
    qw = len(data) // 16
    p = struct.pack('<4I', 0, 0, 0, 0x50000000 | (qw + 7))
    p += struct.pack('<4I', 0x8003, 0x10000000, 0xE, 0)
    p += struct.pack('<QQ', 0, 0x51) + struct.pack('<QQ', w | (h << 32), 0x52) + struct.pack('<QQ', 0, 0x53)
    p += struct.pack('<4I', 0x8000 | qw, 0x08000000, 0, 0)
    return p + data + struct.pack('<4I', 0x8001, 0x10000000, 0xE, 0) + struct.pack('<QQ', 0, 0x3F)


def dbt_read(d):
    from PIL import Image
    count = struct.unpack_from('<I', d, 0)[0]
    out = []
    for i in range(count):
        e = d[0x20 + i * 0x40:0x60 + i * 0x40]
        f = struct.unpack_from('<10I', e, 0)
        tex0 = struct.unpack_from('<Q', e, 0x30)[0]
        po, co = f[0] * 4, f[1] * 4
        pix = d[po:po + f[2]]
        clut = d[co:co + f[3]] if f[3] else None
        upw, uph = struct.unpack_from('<II', pix, 0x30)
        r = {'i': i, 'raw': e, 'pixpkt': pix, 'clutpkt': clut, 'tex0': tex0, 'tstep': f[4], 'cstep': f[5], 'pblt': f[6], 'cblt': f[7],
             'tofs': f[8], 'cofs': f[9], 'slot': e[0x2C], 'alpha': e[0x2D], 'up': (upw, uph)}
        idx, psm = decode(pix[0x60:], clut, tex0, upw, uph, f[6])
        if psm is None:
            r['image'] = Image.fromarray(idx.copy(), 'RGBA'); r['idx'] = None
        else:
            n = 16 if psm == 0x14 else 256
            pal = np.frombuffer(clut[0x60:0x60 + n * 4], dtype=np.uint8).reshape(n, 4)
            pal = csm1(pal)
            r['idx'] = idx; r['pal'] = pal
            rgba = pal[idx].copy()
            rgba[..., 3] = 255
            r['image'] = Image.fromarray(rgba, 'RGBA')
        out.append(r)
    return out
