"""Compare two state hash logs of probe runs (BT2_STATE_HASH=<file>; WHOLE_GAME_COVERAGE_PLAN.md, step 2).

    python tools/ps2recomp/compare_state_hash.py <a> <b>
        the first field at which the two runs differ (or that they agree on every common field)
    python tools/ps2recomp/compare_state_hash.py <a>.pages <b>.pages
        the 4 KB pages that differ at the field the runs wrote their pages at (BT2_STATE_HASH_PAGES=<field>)

Exit status 1 when the runs differ, 0 when they agree.
"""
import sys
from pathlib import Path


def read_fields(path):
    """{field: (flips, hash)} of a state hash log."""
    fields = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        field, flips, digest = line.split()
        fields[int(field)] = (int(flips), digest)
    return fields


def first_difference(a, b):
    """(field, a's entry, b's entry) of the first common field that differs, or None; and the number
    of common fields."""
    common = sorted(set(a) & set(b))
    for field in common:
        if a[field] != b[field]:
            return (field, a[field], b[field]), len(common)
    return None, len(common)


def read_pages(path):
    pages = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        region, page, digest = line.split()
        pages[(region, int(page, 16))] = digest
    return pages


def differing_pages(a, b):
    """Sorted (region, page) keys whose hashes differ or that only one run has."""
    return sorted(key for key in set(a) | set(b) if a.get(key) != b.get(key))


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    a, b = sys.argv[1:]
    if a.endswith('.pages'):
        pages = differing_pages(read_pages(a), read_pages(b))
        for region, page in pages:
            print(f'{region} page 0x{page:X} (offset 0x{page * 4096:X})')
        print(f'{len(pages)} pages differ.')
        return 1 if pages else 0
    difference, common = first_difference(read_fields(a), read_fields(b))
    if difference is None:
        print(f'The runs agree on all {common} common fields.')
        return 0
    field, (flips_a, hash_a), (flips_b, hash_b) = difference
    print(f'First difference at field {field}: flips {flips_a} / {flips_b}, hash {hash_a} / {hash_b} '
          f'({common} common fields). Rerun both with BT2_STATE_HASH_PAGES={field} and compare the .pages files.')
    return 1


if __name__ == '__main__':
    sys.exit(main())
