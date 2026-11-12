# Tenkaichi Budokai Online interface credits

These assets are stored here in the source tree and in `tbo/ui/` in release
packages.

- **Announcer artwork** (`hosts_*.png`): **MarshallPunk**.
- **Button artwork** (`btn_*.png`): **pluantoine**, licensed under
  [Creative Commons Attribution 4.0 International](https://creativecommons.org/licenses/by/4.0/)
  (CC BY 4.0). Adaptations include extracting individual buttons, removing
  their backgrounds, resizing them and combining the left and right arrows
  into `btn_leftright.png`.
- **Dragon Radar** (`radar.png`): fan art by **chestergreen**. Adapted into a scaled animation sheet for the port.
- **Redesigned online screens** (`td_*.png`): drawn by `port/tools/tbo_redesign.py` (the approved mock-ups' windows,
  bars, keys, buttons, capsules, title band, name plate, footer box and the Time Machine without its moving parts).
- **Time Machine pieces** (`tm_*.png`): drawn by `port/tools/tbo_time_machine.py`.
- **Time tunnel backdrop** (`bg.mpg`): rendered for the port (a looping MPEG-1 film; `bg.png` stays as its stand-in).
- **Future Gohan** (`gohan.mpg`, `gohan.png`): an animation made with Vidu AI from fan art, cut out frame by frame (its
  background and the marks in its corners removed) with a soft blue aura; a film with transparency: colours above, how
  solid each pixel is below.
- **Fonts** (`port/third_party/fonts`): **Bangers** (titles, menus, buttons) and **Chakra Petch Bold** (texts), both
  under the SIL Open Font License 1.1 (their `OFL.txt`).

After replacing an image with one of a different size, run:

```sh
python3 port/tools/tbo_sizes.py
```

The tool updates `include/menu/tbo_sizes.h`, which the screens use to lay out
the artwork.
