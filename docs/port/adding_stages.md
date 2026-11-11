# Custom stages

Place compatible `.unk` stage models in the `stages` folder beside the game
executable, then restart the game. They appear at the end of the offline selector. Up to 26 added
maps are supported; custom maps are not offered online.

## Thumbnails and previews

Add a PNG with exactly the same filename stem:

```text
stages/Kaio Planet.unk
stages/Kaio Planet.png
```

The game uses it for the circular icon and full preview while retaining the
native cursor, transition and BT4 map-name lettering. Kaio Planet's included
PNG is a clean capture of the actual supplied stage, without HUD or fighters.
Recommended image size: **512 × 448 pixels**. Images must not exceed
**2048 × 2048 pixels** or **8 MiB**.
An absent, damaged or unsupported image uses the game's fallback artwork.

## Names and ordering

An optional `maps.txt` sets filenames, labels and ordering:

```text
Kaio Planet.unk|Kaio Planet
```

Use characters available in the native font. Models not listed in the manifest follow in sorted order.
 `BT3_STAGES` overrides the directory. `maps` is accepted as a legacy
folder alias when `stages` contains no models. Installing a model requires no
external image or font generator.
