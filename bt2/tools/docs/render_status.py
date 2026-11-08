"""Render BT2's authored component scopes as a compact status map."""
import argparse
import html
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STATES = {"verified": ("#16835d", "Verified"), "working": ("#2b78b8", "Operational"), "incomplete": ("#b77912", "Incomplete"),
          "blocked": ("#c2410c", "Blocking"), "not_reached": ("#8793a5", "Not Reached")}
FILLS = {"verified": "#ecfdf5", "working": "#eff6ff", "incomplete": "#fff7e6", "blocked": "#fff1eb", "not_reached": "#fff"}


FLOW_TOP = 84     # Game-flow track: how far the port gets through the game's own sequence.
GROUPS_TOP = 196  # Component map below it.


def render_flow(stages, esc):
    if not 4 <= len(stages) <= 8:
        raise ValueError("Game flow expects four to eight stages")
    gap, width = 12, 912
    chip = (width - gap * (len(stages) - 1)) / len(stages)
    y = FLOW_TOP + 22
    parts = [f'<text x="24" y="{FLOW_TOP + 8}" font-size="15" font-weight="600" fill="#334155">Game flow</text>',
             f'<line x1="{24 + chip / 2:.1f}" y1="{y + 22}" x2="{24 + width - chip / 2:.1f}" y2="{y + 22}" stroke="#cbd5e1" stroke-width="2"/>']
    for i, stage in enumerate(stages):
        if len(stage["name"]) > 16 or len(stage.get("note", "")) > 18:
            raise ValueError("Game flow label too long; shorten it for readability")
        color, label = STATES[stage["status"]]
        x = 24 + i * (chip + gap)
        fill = FILLS[stage["status"]]
        parts += [f'<g><title>{esc(stage["name"])}: {esc(label)}. {esc(stage.get("note", ""))}</title>',
                  f'<rect x="{x:.1f}" y="{y}" width="{chip:.1f}" height="44" rx="22" fill="{fill}" stroke="{color}" stroke-width="{2 if stage.get("current") else 1.2}"/>',
                  f'<text x="{x + chip / 2:.1f}" y="{y + 27}" text-anchor="middle" font-size="12.5" font-weight="600" fill="#1e293b">{esc(stage["name"])}</text>']
        if stage.get("note"):
            parts.append(f'<text x="{x + chip / 2:.1f}" y="{y + 62}" text-anchor="middle" font-size="11" fill="{color if stage.get("current") else "#64748b"}">{esc(stage["note"])}</text>')
        parts.append('</g>')
    return parts


def render(data):
    groups = data["groups"]
    if len(groups) not in (3, 4) or any(len(g["items"]) != 5 for g in groups):
        raise ValueError("Status map expects three or four groups of five bounded scopes")
    gap = 18
    column = (912 - gap * (len(groups) - 1)) / len(groups)
    # Label limits that fit the column (13 px names, 11.5 px notes).
    max_name, max_note = (28, 36) if len(groups) == 3 else (26, 32)
    esc = html.escape
    height = GROUPS_TOP + 14 + 5 * 62 + 16
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="960" height="{height}" viewBox="0 0 960 {height}" role="img" aria-labelledby="title desc">',
             '<title id="title">BT2 port status</title>',
             '<desc id="desc">How far the port gets through the game, then foundation, runtime and game component status. Verified scopes are bounded.</desc>',
             f'<rect x="1" y="1" width="958" height="{height - 2}" rx="12" fill="#f8fafc" stroke="#dce3eb"/>',
             '<g font-family="Segoe UI,Arial,sans-serif">',
             '<text x="24" y="34" font-size="21" font-weight="600" fill="#1e293b">BT2 • Port status</text>',
             f'<text x="936" y="33" text-anchor="end" font-size="12" fill="#64748b">{esc(data["updated"])} · last measured checkpoint</text>']
    for i, (color, label) in enumerate(STATES.values()):
        x = 24 + i * 170
        parts += [f'<circle cx="{x+5}" cy="59" r="4" fill="{color}"/>',
                  f'<text x="{x+17}" y="63" font-size="12" fill="#475569">{label}</text>']
    parts += render_flow(data["flow"], esc)
    for col, group in enumerate(groups):
        x = 24 + col * (column + gap)
        parts += [f'<text x="{x:.1f}" y="{GROUPS_TOP}" font-size="15" font-weight="600" fill="#334155">{esc(group["name"])}</text>']
        for row, item in enumerate(group["items"]):
            y = GROUPS_TOP + 14 + row * 62
            color, label = STATES[item["status"]]
            if len(item["name"]) > max_name or len(item["note"]) > max_note:
                raise ValueError("Status map label too long; shorten it for readability")
            parts += [f'<g><title>{esc(item["name"])}: {esc(label)}. {esc(item["note"])}</title>',
                      f'<rect x="{x:.1f}" y="{y}" width="{column:.1f}" height="54" rx="6" fill="#fff" stroke="#e2e8f0"/>',
                      f'<rect x="{x:.1f}" y="{y+9}" width="3" height="36" rx="1.5" fill="{color}"/>',
                      f'<circle cx="{x+17:.1f}" cy="{y+20}" r="3.5" fill="{color}"/>',
                      f'<text x="{x+28:.1f}" y="{y+24}" font-size="13" font-weight="600" fill="#1e293b">{esc(item["name"])}</text>',
                      f'<text x="{x+13:.1f}" y="{y+42}" font-size="11.5" fill="#64748b">{esc(item["note"])}</text></g>']
    parts += ['</g></svg>']
    return "\n".join(parts) + "\n"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    source = ROOT / "docs/progress/components.json"
    path = ROOT / "docs/progress/dashboard.svg"
    content = render(json.loads(source.read_text(encoding="utf-8")))
    if args.check:
        if path.read_text(encoding="utf-8") != content:
            raise SystemExit("Component map is stale; run python tools/docs/render_status.py")
    else:
        with path.open("w", encoding="utf-8", newline="\n") as output:
            output.write(content)
    print("Component map checked." if args.check else "Component map rendered.")


if __name__ == "__main__":
    main()