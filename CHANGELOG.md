# Dragon Rage 1.0.0

**Budokai Tenkaichi 2 with only the ISO, Dragon Net Online and the lobbies of PC 0.1.39, the Tag Team Mod as an option, and BT4's far
scenery fixed.** Tested on an RG Cube (Mali-G57) and a Retroid Pocket 5 (Adreno 650), with a recorded fight and a pixel-by-pixel
picture comparison against 0.9.0.

## Budokai Tenkaichi 2
- **Just the ISO**: the engine comes inside the app. There is no engine package to import any more; if you had one, it is deleted
  after a notice and the game uses the app's.
- **The ISO stays where it is**: it is read in place, not copied. Only small files stay in the app's folder.
- **A wrong ISO is refused** with a message that says what was found and which tab installs it.
- **BT2 online**: a match between two devices, with its own screen in the game's menu.
- A game error now shows on the screen, not as a black screen. An output scale below 100% fills the screen.

## Budokai Tenkaichi 4
- **Far scenery on the Dragon Rage renderer**: the mountains no longer come out as blocks of coloured noise. The game's distance blur
  now works.

## Weak devices
- **BT4**: the default now goes by device. A weak device or Lite mode uses the Dragon Rage renderer; if ARMSX2 stays below 27 updates a
  second for 5 s in a fight, the next start uses Dragon Rage. Your choice in Settings always counts. On an RG Cube held down to a chip like the
  G52 MC1, ARMSX2 gave 8 to 10 pictures a second and Dragon Rage 10 to 11; the game still does not reach full speed on that chip.
- **BT3**: the same performance as 0.9.0 on that held-down device.

## Tag Team Mod
- Up to 10 fighters, with the modes and Tag Team Online of the mod by tehmufti and its contributors, adapted to phones.
- **Off by default**: turn it on in Settings. Off, it does not appear in normal fights.

## Online
- **Dragon Net Online**: new screens in place of Dragon Net Battle, with Ranked, lobbies, spectators, quick match by region and joining
  by address (from PC 0.1.39).
- **Fewer requests to the servers**: nothing in the background, a growing wait when a server refuses, and the message "Server busy, try
  again later" in place of an unexplained failure.
- Random music no longer sets the two players of a match apart.

## Fixes from players' logs
- **Menu font and labels**: they no longer ask for game data that is already in the ISO.
- **False texture pack warning** with an empty folder.
- **A file that fails to read**: the game says why, tries again, and the fight goes on.
- **A modified disc**: a negative size is a clear warning, not "out of memory".
- `.7z` and `.rar` are refused with a clear message. Without internet, checking for updates no longer shows as a failure.

## App and controls
- **Controllers navigate the launcher** again, on the RG Cube and the Retroid Pocket 5.
- **Team and licences** on the About screen.
- The controller mapping is read again when the game opens.

## Team
Regulus CiberSec, z3xox (PC), Androidxxt01, jllacchua, victorgonl, tehmufti and the Tag Team Mod contributors, Team BT4. Artists:
MarshallPunk and pluantoine.

## Update notes
- Installs over 0.9.0 and keeps saves, settings and the imported ISOs.
- Online needs the same version on both devices.
