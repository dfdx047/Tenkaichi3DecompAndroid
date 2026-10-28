#!/usr/bin/env python3
"""Synthetic extraction -> EU catalogs -> runtime lookup regression (no disc data).

    python3 port/tests/menu_font/ui_text_extract.py
Set CC to select a compiler. Builds with AddressSanitizer and UndefinedBehaviorSanitizer.
"""
import importlib.util
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("extract_eu", ROOT / "port/tools/extract_eu.py")
EU = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EU)
FILES = [5, *range(448, 479), *range(560, 610)]
LINES = 400  # 32,800 total: exceeds the former 30,000-row loader limit.


def table(usa, bank=0):
    parts = []
    for line in range(LINES):
        text = f"{'ES ' if bank else ''}{usa}:{line}"
        blob = b"\xff\xfe" + text.encode("utf-16le") + b"\0\0"
        parts.append(blob + b"\0" * (-len(blob) % 4))
    offsets = [4 * (LINES + 2)]
    for part in parts:
        offsets.append(offsets[-1] + len(part))
    return struct.pack(f"<{LINES + 2}I", LINES, *offsets) + b"".join(parts)


def packed(blob):
    # Identity byte-pair dictionary, then literal blocks, matching Sprite_Unpack's format.
    blocks = []
    for pos in range(0, len(blob), 60000):
        chunk = blob[pos:pos + 60000]
        blocks.append(b"\xff\x80\xfe" + struct.pack(">H", len(chunk)) + chunk)
    payload = b"".join(blocks)
    return struct.pack("<II", len(blob), len(payload)) + payload


def loose_files(path, entries):
    path.mkdir()
    for index, blob in entries.items():
        (path / f"{index:05d}.bin").write_bytes(blob)


def main():
    with tempfile.TemporaryDirectory(prefix="bt3-text-test-") as temp:
        root = Path(temp)
        usa = {i: table(i) if i >= 560 else packed(table(i)) for i in FILES}
        loose_files(root / "pzs3us1", usa)
        eu = {}
        for bank in range(5):
            for i in FILES:
                entry = (5 + bank if i == 5 else
                         656 + i - 560 + bank * 50 if i >= 560 else
                         451 + i - 448 + bank * 31)
                eu[entry] = table(i, bank) if i >= 560 else packed(table(i, bank))
        assert sorted(eu) == EU.entries() and len(eu) == 410
        loose_files(root / "pzs3eu1", eu)
        out = root / "game-text"
        (out / "translations/ui").mkdir(parents=True)
        # Include internals to run the normally detached worker synchronously and check the full row count.
        source = root / "test.c"
        source.write_text(r'''
#include "ui_text_extract.c"
#include "ui_text_translate.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "failed at line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv) {
    UtJob *job;
    char path[1400], why[1800];
    uint8_t *bytes;
    size_t n;
    const uint16_t *original, *translated;
    FILE *fp;
    CHECK(argc == 3);
    setenv("BT3_DATA", argv[1], 1);
    job = calloc(1, sizeof(*job)); CHECK(job);
    snprintf(job->data, sizeof(job->data), "%s", argv[1]);
    snprintf(job->output, sizeof(job->output), "%s/translations/ui/en.json", argv[2]);
    ut_worker(job);
    CHECK(UiText_EuropeanOnStartup(argv[2], why, sizeof(why)) == 4);
    CHECK(UiText_EuropeanOnStartup(argv[2], why, sizeof(why)) == 0);
    CHECK(UiText_Load(argv[2], "es"));
    CHECK(sRowCount == 32800);
    snprintf(path, sizeof(path), "%s/pzs3us1/00560.bin", argv[1]);
    bytes = ut_read(path, &n); CHECK(bytes);
    original = (const uint16_t *)(bytes + ut_u32(bytes + 4));
    translated = UiText_TranslateLine((const uint32_t *)bytes, 0, original);
    CHECK(translated != original && translated[1] == 'E' && translated[2] == 'S');
    CHECK((uintptr_t)translated <= UINT32_MAX);
    free(bytes); ui_clear();
    /* Refresh old generated catalogs; preserve current ones and catalogs with a custom source. */
    snprintf(path, sizeof(path), "%s/stale.json", argv[1]);
    fp = fopen(path, "wb"); CHECK(fp);
    fputs("{\"source\": \"extracted USA game text, version 2\"}", fp); fclose(fp);
    CHECK(ut_stale(path));
    fp = fopen(path, "wb"); CHECK(fp);
    fputs("{\"source\": \"" UT_SOURCE "\"}", fp); fclose(fp); CHECK(!ut_stale(path));
    fp = fopen(path, "wb"); CHECK(fp);
    fputs("{\"source\": \"human reviewed\"}", fp); fclose(fp); CHECK(!ut_stale(path));
    CHECK(!ut_eu_wanted(path));
    /* A truncated loose file and a missing file must be rejected. */
    snprintf(path, sizeof(path), "%s/broken.bin", argv[1]);
    fp = fopen(path, "wb"); CHECK(fp);
    fputs("short", fp); fclose(fp);
    CHECK(ut_read(path, &n) == NULL);
    snprintf(path, sizeof(path), "%s/missing.bin", argv[1]);
    CHECK(ut_read(path, &n) == NULL);
    return 0;
}
''')
        executable = root / "test"
        command = shlex.split(os.environ.get("CC", "cc"))
        subprocess.run(command + ["-std=gnu99", "-Wall", "-Wextra", "-Werror", "-g",
                                 "-fsanitize=address,undefined", "-no-pie", "-pthread",
                                 f"-I{ROOT / 'port/src/gs'}", str(source), "-o", str(executable)], check=True)
        subprocess.run([str(executable), str(root), str(out)], check=True)
        en = json.loads((out / "translations/ui/en.json").read_text())
        es = json.loads((out / "translations/ui/es.json").read_text())
        assert len(en["labels"]) == len(FILES) * LINES
        for entry in FILES:
            key = f"pzs3us1/{entry:05d}/0"
            assert en["labels"][key]["original"] == f"{entry}:0"
            assert es["labels"][key]["text"] == f"ES {entry}:0"
        print("Text extraction, EU mappings, catalog refresh and large-catalog lookup passed")


if __name__ == "__main__":
    main()
