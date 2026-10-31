"""Keep the README checkpoint synchronized with docs/PROG.md."""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
START = "<!-- progress-summary:start -->"
END = "<!-- progress-summary:end -->"


def bounds(text):
    if text.count(START) != 1 or text.count(END) != 1:
        raise ValueError("Expected exactly one progress summary marker pair")
    start = text.index(START)
    end = text.index(END) + len(END)
    if end <= start:
        raise ValueError("Progress summary markers are out of order")
    return start, end


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail if README is stale; do not write")
    args = parser.parse_args()
    source = (ROOT / "docs/PROG.md").read_text(encoding="utf-8")
    path = ROOT / "README.md"
    readme = path.read_text(encoding="utf-8")
    a, b = bounds(source)
    c, d = bounds(readme)
    block = source[a:b].replace("(progress/dashboard.svg)", "(docs/progress/dashboard.svg)")
    updated = readme[:c] + block + readme[d:]
    if updated == readme:
        print("README progress summary is current.")
        return
    if args.check:
        raise SystemExit("README progress summary is stale. Run python tools/docs/sync_progress.py")
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(updated)
    print("Updated README progress summary from docs/PROG.md.")


if __name__ == "__main__":
    main()