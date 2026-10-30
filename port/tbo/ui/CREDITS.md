# Tenkaichi Budokai Online: credits for the pictures

The pictures of the Tenkaichi Budokai Online screens (this folder; `tbo/ui/` in a release).

- **The announcer** (`hosts_*.png`): by **MarshallPunk**. (Kept for the PC's look; the Android screens show Raditz.)
- **Raditz** (`raditz.png`): the picture the project's owner chose for the screens, its background already transparent;
  cut to what is drawn.
- **The backdrop film** (`bg.mpg`): Kame House's island, made by the project's owner with an image-to-video tool; the
  tool's mark cut away (the bottom strip), turned into a 4-second loop whose last second fades into its first, MPEG-1
  for the decoder of `port/third_party/pl_mpeg` (shown in place of `bg.png`, which stays as the fallback).
- **The plates and the window** (`kit_bar.png`, `kit_sel.png`, `kit_window.png`): drawn for the port (the leaning
  plates of the game's menus, blue and gold; a navy window).
- **The buttons** (`btn_*.png`): by **pluantoine**, under the
  [Creative Commons Attribution 4.0 International license](https://creativecommons.org/licenses/by/4.0/) (CC BY 4.0).
  Changed for the port: cut out of the sheet, their background removed and scaled; the left and right directions
  combined into one picture (`btn_leftright.png`).

A picture replaced by one of another size: run `python3 port/tools/tbo_sizes.py` (the screens lay the pictures out
by their sizes, `include/menu/tbo_sizes.h`).
