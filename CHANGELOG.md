# Dragon Rage 1.0.2

**Budokai Tenkaichi 4 with a new engine: BT4decomp, NauTaro's decompilation, now runs inside the app.**
Installs over 1.0.1 and 1.0.0 without uninstalling: saves, settings, controls and imported games stay.
BT3 and BT2 are the same as in 1.0.1.

You need your own image of each disc:
- **Budokai Tenkaichi 3**: the American disc, SLUS-21678.
- **Budokai Tenkaichi 4**: Team BT4's mod, **Beta 14 Rev2**, in **English or Spanish**.
- **Budokai Tenkaichi 2**: the American disc, SLUS-21441.

## Budokai Tenkaichi 4: BT4decomp
- The BT4 card now opens **BT4decomp** ([DBZ-BT4-Tag-Team-Decomp](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp),
  by **NauTaro**): the decompilation of Team BT4's mod, built on Tenkaichi3Decomp, with Dragon Rage's Android port.
  It replaces the previous BT4 engine (the recompiled mod with the ARMSX2 renderer).
- **English or Spanish**: both versions of Beta 14 Rev2 work, and the game uses the language of the imported image.
- **Tag Team** in BT4: the **Local Room** (against the CPU) works without a network. BT4's online Tag Team depends on
  Dragon Net being enabled for this version.

## New BT4 import
- The game's files are copied from the image (about **5.7 GB**, a few minutes) and **each one is checked**. A damaged
  image, or one of another version, stops the install with a clear message, and nothing is installed.
- After the import the image is no longer needed: you can delete the ISO.
- Before starting, the app checks the free space and says how much is missing.
- The card shows "Beta 14 Rev2 verified", with the number of files and the size.

## If you already had BT4 on 1.0.0 or 1.0.1
- **Import the BT4 ISO again**: the data format changed, and the card asks for the image again.
- The new install removes the old engine's files. **Saves are not touched**.

## BT4 settings
- The in-game menu (mid-fight) and BT4's Settings now have the same pages and options as BT3, including smooth motion.
- The items that only belonged to the old engine are gone: BT4's aspect ratio and 60 fps.

## Important
- To play online, everyone needs the same version of the app.
- To report a problem: Debug → Save log to Downloads, and send the file (on Discord, for example).
