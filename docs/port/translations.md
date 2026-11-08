# Translations

The port uses one selected language for three kinds of text. Each kind has its
own JSON catalog under `gamedata/game-text/translations/`:

| Folder | Text covered | Where the English text comes from |
| --- | --- | --- |
| `labels/` | Menu and other text painted into artwork textures | Bundled `port/translations/labels/en.json` |
| `ui/` | Original game's menu, mode and story dialogue text tables | Extracted from the USA game data at startup into `ui/en.json`, unless an English UI catalog is bundled |
| `port/` | Text added by the PC port: the quit confirmation, the lines at the start of an online match (`{name}` is the rival's name), and the automatic update's lines (`update/...`; `{version}`, `{percent}`) | Bundled `port/translations/port/en.json` |

An entry has a stable ID, the English `original`, and editable `text`. For
example, `port/en.json` contains:

```json
{
  "quit/confirm": {
    "original": "Close the game?\nUnsaved progress will be lost.",
    "text": "Close the game?\nUnsaved progress will be lost."
  }
}
```

Keep IDs and `original` unchanged when translating; change `text`. Use `\n`
inside a JSON string for a line break. The catalog's `locale` must match its
filename, such as `es` in `es.json`. Set `name` to the name shown in the
language selector. `order` controls its position there; smaller numbers come
first. `schema_version` identifies the JSON format. `labels_version`,
`translation_version`, and `labels_count` document the source and translation
revision and entry count; they do not select the language or alter rendering.

## Source files and game-data files

The source tree bundles JSON catalogs from `port/translations/labels/`,
`port/translations/ui/`, and `port/translations/port/`. A release puts these
under its corresponding `translations/` folders. At startup, the game
copies missing files into `gamedata/game-text/translations/`. It **never
overwrites an existing game-data translation**, so the game-data copy is the
one to edit after the first launch. Editing a bundled source file will affect
an existing installation only if its game-data copy is also updated or removed.
Packaging includes locale JSON files with valid locale filenames.

The port extracts `ui/en.json` from the installed USA game data at startup
when no English UI catalog exists. It preserves an existing game-data copy.
Developers may bundle reviewed UI catalogs in `port/translations/ui/`;
the game copies those before starting extraction. Translators can also place
UI catalogs directly in `gamedata/game-text/translations/ui/`.

To add a language, copy an English catalog to
`gamedata/game-text/translations/<kind>/<locale>.json`, set `locale` and
`name`, and translate the `text` values. A language may have files in only one
or two of the three folders. Missing menu and battle labels use the English
label catalog; missing disc UI translations leave the game's original text;
missing port entries use the English port catalog or its built-in message.
For artwork labels, select a Menu or Battle font instead of **Original** to
display translated words in place of the original artwork.

## Selecting and loading a language

The game discovers locale files in all three folders and shows each language
once in **F1 > UI > Language**. The startup selector appears only when at least
two languages are available; it is skipped when there is just one. The selected
locale is saved as `menu_lang` in `bt3_settings.txt`. The optional **Show language
selector at startup** setting displays the selector on later launches too.
`BT3_MENU_LANG=es` overrides the saved choice for one launch.

The catalogs are loaded once for a run. Restart the game after choosing a
different language or editing a JSON file. The selected language applies
across the available label, UI, and port catalogs; each kind falls back
independently when it has no matching translation.

## What each catalog can translate

**Labels:** `labels.tsv` identifies texture positions and label IDs, while
`labels/<locale>.json` supplies words. Changing words does not require
rebuilding `labels.tsv`. Layout and style changes are described in
[Menu fonts](menu_font.md).

**Game UI:** `ui/en.json` lists text found in the extracted game tables. Copy
it to `ui/<locale>.json`, keep each ID and `original`, and translate `text`.
Repeated English phrases can have separate IDs because they occur in
different tables. Extraction covers the battle HUD (USA archive entry 5), all
31 menu packs (`pzs3us1/00448.bin` through `00478.bin`), and the 50 story
dialogue tables (`00560.bin` through `00609.bin`). This includes tournament
brackets, saga/episode descriptions and ending narration, Sim Dragon events,
and Ultimate Battle rankings and course descriptions. Whole compressed packs
and compressed sections are decoded before their UTF-16 tables are scanned.
The extractor reads the loose `.bin` files under `gamedata/pzs3us1/` created
by disc setup. The standard USA data yields 47,533 entries, including blank
and placeholder lines in the original tables.

Generated English catalogs from earlier extractor versions are refreshed at
startup. A reviewed catalog with a custom `source` is preserved; merge new IDs
from a freshly generated English catalog into existing manual translations.
The IDs of previously extracted lines stay the same. Only recognized text
tables are covered: painted artwork and hardcoded strings still require label
mappings, replacement artwork, or translation calls in code.

**Port text:** An entry in `port/<locale>.json` works only after port code has
registered its ID and looks it up. To add one, define the ID in
`port/src/gs/menu_translation.h`, add its English wording to `kPortText` in
`port/src/gs/menu_translation.c`, increase `TR_PORT_TEXT_COUNT`, and add the
same ID, `original`, and English `text` to `port/translations/port/en.json`.
Update `labels_count` and increment `labels_version` when changing the source
set of IDs or wording. At the display site, call
`MenuTranslation_PortText(id)` and keep a built-in English fallback for a
missing or invalid catalog. The returned text is UTF-8; ImGui can display it
directly. Native game dialogs need conversion to UTF-16 and a buffer large
enough for translated text, as in `port/src/quit_dialog.c`.

**The online menus (Tenkaichi Budokai Online):** their lines are entries of
`port/<locale>.json` too, with ids `tbo/<8 hex digits>`, looked up by their
English `original` where the menu draws or measures a line
(`port/src/gs/tbo_text.c`), where it joins pieces (`Ps_Copy`/`Ps_Cat`, which
also feed the screen reader) and where it formats a number (`Tb_Fmt` around a
`sprintf` format). The talk box at the foot of the screen takes UTF-8. To make
or bring up to date a file for translators:

    python3 port/tools/tbo_strings.py es --name "Español" --out es.json      # --bt4: without BT3's fighter names

A new line's `text` is its English, as a placeholder (a line left so is shown in
English); running the tool again on the same file keeps what is translated,
adds new lines and drops the ones the menus no longer have. A `note` says where
a line is shown. A line with a player's name in it (the lobby's "%s joined
the lobby", "Spectating %s vs %s") has `%s` for each name. A translation has to keep the line's `%d`/`%02d`/`%s` parts in
their order (and `%%` for a percent sign in such a line) and its `{name}`
parts; one that does not is not used, and the console says so. The file goes in
`translations/port/` beside the program; its language is then offered in the
first-run language screen and F1 even without menu labels of its own.

Adding an ID to a JSON file alone cannot translate an arbitrary hardcoded
string: the code or data that displays it must use the corresponding
translation system.

## The online screens' own language

Tenkaichi Budokai Online (src/menu/port_tbo.c) has a language row at the top of its hub: English, Português, Español,
changed with left and right and kept in the setting `tbo_lang` (until it is chosen: the launcher's `dr_lang`). It
changes only those screens, at once, without a restart: `Port_TboLanguage` (port/src/gs/menu_translation.c) loads
`translations/tbo/pt.json` or `es.json` (`port/translations/tbo/` in the source) into the table of the screens'
lines, which `MenuTranslation_Line` reads; English is the screens' own text. The two files are written by
`python3 port/tools/tbo_translations.py`, which holds every line in the three languages, checks each English line
against the strings of port_tbo.c, and names the screen's lines that have no translation. They live apart from
`translations/port/`, so they do not add languages to the game's language list.

## What happens when a translation does not cover every ID?

Each catalog handles missing entries differently:

| Catalog | Missing ID or untranslated entry |
| --- | --- |
| `ui/` | A matched entry with empty `text` uses the original English. A missing ID normally leaves the original text, though an identical English line elsewhere in the catalog can supply its translation when that match is unambiguous. |
| `port/` | The loader uses the English `port/en.json` entry, then the built-in English string if needed. |
| `labels/` | An entry with empty `text` uses its `original`. An ID absent from the selected label catalog has no per-ID English fallback: its artwork stays in place. Some IDs are required to load the catalog at all; if one is missing, the catalog is rejected. |



## The European disc's languages

The European release (SLES-54945) has the menus, the battle HUD and their text five times, once per language
(English, Spanish, German, French, Italian). The port extracts 410 menu/HUD and story text archive entries, one native
language-selector entry, and the opening soundtrack:

- the setup window takes them from a second, optional disc image (in a release folder: native.cpp `step_eu`;
  the "Ready to play" page has "Add languages..." for a game set up before; without a window
  `Tenkaichi3Decomp-setup --install "" <European image>`);
- in a source checkout `python3 install.py --iso <USA image> --eu-iso <European image>`, or on its own
  `python3 port/tools/extract_eu.py <European image>`.

The translated entries go as they are into `gamedata/pzs3eu1/<entry>.bin` (the battle HUD is entries 5 to 9, one
per language; the USA menu archive 448 + k is entry 451 + k + 31 per language).
Story dialogue occupies entries 656 through 905 (50 episodes per language).
The selector textures are in
`gamedata/pzs3eu0/00001.bin` (entry 1 of `DATA/PZS3EU0.AFS`); the renderer decodes the background and arrows
directly from it, without PNG assets. For an existing EU installation, run
`python3 port/tools/extract_eu.py <European image> --selector-only` to add just this entry. These are the
player's own disc data: never commit them. The game does the rest itself:

- `ui/es.json`, `de.json`, `fr.json`, `it.json`: at the first start after the entries arrive, before the language
  list (`UiText_EuropeanOnStartup`, ui_text_extract.c). The menu, dialog, pause menu and story text,
  keyed by the USA IDs: the walk that makes `ui/en.json`, over both discs. A line is taken where the European
  English is the USA English, or where its table has as many lines on both discs: the European English is then
  the same line reworded (bullets added, a typo fixed, "Fight Again?"). About 30,000 of 47,533 lines change; the
  rest include names, numbers and placeholder lines. A catalog is made again when it was made by another version of this; one with
  another source (written by a person) is left alone.
- The painted lettering of the menus and the battle HUD (headings, menu items, buttons such as Yes and No, the
  title's Press START, Ready and Fight, Hits, the pause menu's title): when a language is chosen, every texture
  file of its archives that differs from the USA one is paired with it (`MenuLabels_Art`, menu_labels.c, in a
  thread of its own at start, under two seconds), by the hash of the decoded USA pixels, the same hash the label
  list uses. The renderer then puts the European picture in place of the USA one when it decodes it
  (`MenuLabels_ArtSwap`, `gs_draw.c`). The game itself is untouched, and so is online play: it is only what a
  texture shows. The two main menu sheets that the PC's own item is written into are paired by their first 96
  rows; the item's row is kept.

A redrawn label (Menu font other than Original) is drawn from `labels/`; on a texture that shows the European
picture there is no USA label to redraw, so the European art is what shows.

Existing European installations that imported only the earlier 60 entries must
import their European image again (setup's **Add languages...**, or
`python3 port/tools/extract_eu.py <European image>`). Until the additional packs
are present, existing European catalogs stay in place and the game reports the
incomplete import when it tries to refresh them. Manually authored catalogs
with a different source are preserved.

The English catalog's source is "extracted USA game text, version 3"; generated
European catalogs use "European disc, version 2". These include the lines that
sit among other sections, as the pause menu's do in file 5, along with the mode
packs and story dialogue. Older generated catalogs are refreshed at startup.

`BT3_ART_LOG=1` lists every pair at start and every swap.
