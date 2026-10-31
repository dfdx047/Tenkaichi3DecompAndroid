#!/usr/bin/env python3
"""Makes (or brings up to date) a translation file of the online menus for translators.

Usage:
    python3 port/tools/tbo_strings.py <locale> [--out FILE] [--name NAME] [--bt4]

Collects the English lines of the Tenkaichi Budokai Online screens (src/menu/port_tbo.c) and of the port's own
text (port/translations/port/en.json) into translations/port/<locale>.json's shape: one entry per line, "original"
the English, "text" what is shown in that language. A new entry's text is its English, as a placeholder: a line
left as its English is shown in English. When FILE exists already, the lines it has translated are kept, so the
tool can be run again after the menus change (lines no longer in the menus are dropped; new ones come in English).

What a translator has to keep: %d, %02d, %% and the like (the numbers the game puts in; in the same order) and
{name} (moved anywhere in the line). A translation without them is not used; the game says so on its console.

The game reads the file from translations/port/<locale>.json beside the program, for the language chosen in the
first-run language screen or F1 (docs/port/translations.md). Only the lines are looked at: "id" and "note" are for
the translator.
"""
import argparse
import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCES = [ROOT / "src/menu/port_tbo.c", ROOT / "port/src/spec_menu.c"]

# Calls whose strings are not shown: settings' names, file names, comparisons, logs.
NOT_SHOWN = {"Port_Setting", "Port_SettingSave", "Port_SettingText", "Port_SettingTextSave", "strncmp", "strcmp",
             "getenv", "fopen", "fprintf", "printf", "Port_LagMark", "SDL_SetClipboardText"}
# Tables whose strings are not words.
NOT_SHOWN_TABLES = {"sTbKeys", "sTbNameKeys", "sTbRegionArea"}
# Where a line is shown, for the translator's note (the function it is in).
NOTES = {
    "Tb_Help": "talk box at the foot of the screen (two lines at most)",
    "Tb_Note": "message on the search screen",
    "Tb_DrawHub": "main list of the online menu",
    "Tb_Fighter": "a fighter's name",
    "Tb_Speak": "read aloud by the screen reader (a piece may be joined to a name or a code after it)",
    "PortTbo_SparSign": "sign shown during Train While Waiting's sparring fight",
    "Tb_DrawLobbyText": "the lobby's page (its places, code, rules and buttons)",
    "Tb_DrawLobbyWin": "a window over the lobby's page",
    "Tb_LobbyActName": "a button of the lobby's page",
    "Tb_LobbyPopItem": "a choice in a window over the lobby's page",
    "Tb_LobbyLine": "a line of what happened in the lobby (shown in the talk box)",
    "Tb_SpecLine": "the line over a match being watched (spectator)",
    "item_line": "an item of a spectator's own pause menu (the battle's pause menu box)",
    "leave_name": "an item of a spectator's own pause menu (the battle's pause menu box)",
    "view_name": "a spectator's view of the match, in its pause menu (after \"View: \")",
}
# Not words: the name keypad's case key (its rows are the table sTbKeys).
NOT_WORDS = re.compile(r"Aa")
# A number the game puts in: %d, %02d, %s... (not "100% Power")
SLOT = re.compile(r"%[-+#0-9.]*l?[dsuxX]")


def strip_comments(src):
    """The source with its comments and preprocessor lines blanked out (same length, so positions stay)."""
    out = []
    i, n = 0, len(src)
    while i < n:
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j]))
            i = j
        elif src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif src[i] == '"':
            j = i + 1
            while j < n and src[j] != '"':
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src[i] == "'":
            j = i + 1
            while j < n and src[j] != "'":
                j += 2 if src[j] == "\\" else 1
            out.append(" " * (j + 1 - i))
            i = j + 1
        else:
            out.append(src[i])
            i += 1
    text = "".join(out)
    return re.sub(r"(?m)^[ \t]*#.*$", lambda m: " " * len(m.group(0)), text)


def c_unescape(s):
    return re.sub(r'\\(x[0-9a-fA-F]{2}|.)', lambda m: {"n": "\n", "t": "\t", '"': '"', "\\": "\\", "'": "'"}.get(
        m.group(1), chr(int(m.group(1)[1:], 16)) if m.group(1)[0] == "x" else m.group(1)), s)


def function_spans(src):
    """(start, end, name) of each function's body, by its braces (src without comments; strings have no braces
    that matter here: a brace in a string would be counted, and the menu's strings have none)."""
    spans = []
    for m in re.finditer(r"(?m)^[A-Za-z][\w \*]*?\b(\w+)\([^;{]*\)\s*\{", src):
        depth, i = 1, m.end()
        while i < len(src) and depth:
            depth += {"{": 1, "}": -1}.get(src[i], 0)
            i += 1
        spans.append((m.end(), i, m.group(1)))
    return spans


def enclosing_function(spans, pos):
    """The function `pos` is in, or None in a table outside any."""
    for start, end, name in spans:
        if start <= pos < end:
            return name
    return None


def collect(path, bt4=False):
    raw = path.read_text(encoding="utf-8")
    src = strip_comments(raw)
    spans = function_spans(src)
    lines = []
    # adjacent literals ("a" "b") are one string
    for m in re.finditer(r'"((?:[^"\\\n]|\\.)*)"(?:\s*"((?:[^"\\\n]|\\.)*)")*', src):
        whole = "".join(re.findall(r'"((?:[^"\\\n]|\\.)*)"', m.group(0)))
        text = c_unescape(whole)
        if not re.search(r"[A-Za-z]{2}", text):
            continue
        if re.fullmatch(r"[a-z0-9_./]+", text) or NOT_WORDS.fullmatch(text):  # a setting's or a file's name
            continue
        before = src[max(0, m.start() - 300):m.start()]
        call = re.findall(r"(\w+)\s*\((?![^()]*\))", before)
        if call and call[-1] in NOT_SHOWN:
            continue
        table = re.findall(r"(\w+)\s*\[[^\]]*\]\s*(?:\[[^\]]*\]\s*)*=\s*\{[^;]*$", before)
        if table and table[-1] in NOT_SHOWN_TABLES:
            continue
        where = enclosing_function(spans, m.start())
        note = NOTES.get(where, f"in {where}" if where else "a table of the online menu")
        if where is None:  # in a table outside the functions: which one
            tables = re.findall(r"(\w+)\s*(?:\[[^\]]*\]\s*)+=\s*\{", src[:m.start()])
            table_name = tables[-1] if tables else ""
            if table_name in NOT_SHOWN_TABLES or (bt4 and table_name == "sTbNames"):
                continue  # (BT4's fighters are named in tbo/fighters.txt, not by the original's table)
            note = {"sTbNames": NOTES["Tb_Fighter"], "sTbHubName": "main list of the online menu",
                    "sTbLobbyLines": "a line of the lobby: what happened, or why it closed (talk box or a window)",
                    "sTbLobbyNamed": "a line of the lobby with a player's name in it (%s: the name; keep it)",
                    "sTbBattleName": "a battle type (the lobby's rules)", "sTbRuleName": "a rule of the lobby",
                    "sTbViewName": "a spectator's view of the match",
                    "sTbHubSay": "talk box at the foot of the screen (two lines at most), for the main list's entries",
                    "sTbTime": "a time limit's name"}.get(table_name, note)
        if SLOT.search(text):
            note += "; keep the %d / %02d parts, in this order (and %% for a % sign)"
        lines.append((text, note))
    return lines


def line_id(text):
    h = 2166136261
    for b in text.encode("utf-8"):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return f"tbo/{h:08x}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("locale", help="the language's code: es, de, fr, it, pt...")
    ap.add_argument("--out", help="the file to make or bring up to date (default: translations/port/<locale>.json here)")
    ap.add_argument("--name", help="the language's own name, as the language screen shows it (Español...)")
    ap.add_argument("--bt4", action="store_true", help="for the mod Budokai Tenkaichi 4: without the original's fighter names")
    a = ap.parse_args()
    out = pathlib.Path(a.out or f"translations/port/{a.locale}.json")

    old = {}
    if out.exists():
        data = json.loads(out.read_text(encoding="utf-8"))
        for row in data.get("labels", {}).values():
            if row.get("text") and row.get("text") != row.get("original"):
                old[row["original"]] = row["text"]
        a.name = a.name or data.get("name")

    labels = {}
    # the port's own lines (quit question, the start of an online match), with their fixed ids
    en = json.loads((ROOT / "port/translations/port/en.json").read_text(encoding="utf-8"))
    for key, row in en["labels"].items():
        labels[key] = {"original": row["original"], "text": old.get(row["original"], row["original"]),
                       "note": "shown by the PC port"}
    seen = set()
    for path in SOURCES:
        for text, note in collect(path, a.bt4):
            if text in seen:
                continue
            seen.add(text)
            labels[line_id(text)] = {"original": text, "text": old.get(text, text), "note": note}

    doc = {"schema_version": 1, "labels_version": 1, "translation_version": 1, "labels_count": len(labels),
           "locale": a.locale, "name": a.name or a.locale, "order": 2, "labels": labels}
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    done = sum(1 for r in labels.values() if r["text"] != r["original"])
    print(f"{out}: {len(labels)} lines, {done} translated, {len(labels) - done} still in English")


if __name__ == "__main__":
    main()
