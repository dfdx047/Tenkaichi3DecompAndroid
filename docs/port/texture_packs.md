# Texture packs

Copy a compatible PCSX2 texture pack ZIP to `textures` next to `BT4decomp`.
The game reads individual textures from the archive as needed. Large packs do
not need to be extracted or copied into memory in full.

Extracted `.png` and `.dds` files also work, including nested folders such as
`SLUS-21978/replacements`. Keep the pack's original hash-based file names.
DDS supports DXT1, DXT3, DXT5 and uncompressed 32-bit images.

Restart after adding or removing a pack. **F1 > Video > Texture pack** controls
whether the detected replacements are used. The preference is also respected
by the DirectX 9 performance renderer.

`BT3_TEXTURES` selects a different texture directory. Packs affect local
rendering; they do not replace battle logic or require opponents to install
the same artwork.
