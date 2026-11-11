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

After replacing an image with one of a different size, run:

```sh
python3 port/tools/tbo_sizes.py
```

The tool updates `include/menu/tbo_sizes.h`, which the screens use to lay out
the artwork.
