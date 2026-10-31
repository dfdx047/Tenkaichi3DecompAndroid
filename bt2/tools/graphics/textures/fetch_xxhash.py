"""Check out the pinned xxHash revision (XXH3, the hash of PCSX2's texture names; docs/plans/graphics/HD_TEXTURES_PLAN.md)."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "ps2recomp"))
from build_tools import checkout  # noqa: E402


def main():
    lock = json.loads((ROOT / "config" / "xxhash.lock.json").read_text(encoding="utf-8"))
    print(checkout("xxhash", lock["xxhash"]).relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
