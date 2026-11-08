/* Optional editable menu labels rendered from a vector font. */
#ifndef GS_MENU_TEXT_H
#define GS_MENU_TEXT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original GS RGBA pixels (128 = opaque) of a texture; `indexed`: it has a palette (the labels all do). Returns
   an allocated texture `scale` times the size with the labels drawn from the font, or NULL when no row was
   replaced. Caller owns the result. Original pixels are never modified; output dimensions are set only on success. */
uint8_t *MenuText_Render(const uint8_t *rgba, int width, int height, int indexed, int scale, int *outWidth, int *outHeight);
/* Whether the original indexed texture contains a Battle font label, also when Battle font is on Original. */
int MenuText_IsBattleTexture(const uint8_t *rgba, int width, int height, int indexed);
/* Whether a texture's dimensions match any Battle label, for deciding if a texture-pack source needs decoding. */
int MenuText_MayHaveBattleTexture(int width, int height, int indexed);
/* The size text is drawn at for an internal resolution: that resolution, at least 2 and at most 4 times the PS2's. */
int MenuText_RasterScale(int internalScale);
/* For the renderer's texture cache: changes with font settings, raster scale, or a newly made label catalog.
   The catalog change refreshes Battle texture classification even when all fonts are Original. */
uint32_t MenuText_TexKey(int width, int height, int indexed, int tbp, int cbp, int scale);
/* 0: Original game sprites (default); other choices come from the font list (fonts.tsv next to the fonts).
   A font is read and checked when it is chosen; one that cannot be used leaves the choice on Original. */
int MenuText_GetFont(void);
void MenuText_SetFont(int choice);
int MenuText_FontCount(void);
enum { MENU_TEXT_MENU = 0, MENU_TEXT_DIALOGUE = 1, MENU_TEXT_BATTLE = 2 };
int MenuText_FontAvailable(int choice, int setting);
const char *MenuText_FontName(int choice);
int MenuText_FontId(int choice);
int MenuText_FirstFontId(int setting); /* first listed font for a setting, even before labels.tsv exists */
/* For drawing in a listed font elsewhere (the fight panels): its file once checked (0: not usable); the choice a
   saved id names (0: none). */
int MenuText_FontFile(int choice, char *out, int size);
int MenuText_FontChoice(int id);
void MenuText_SetFontId(int id);
/* The Menu font to write into the settings file: the one in use, or the one the settings asked for at start while
   the list of labels it needs is still being made from the disc. */
int MenuText_FontIdToSave(void);
/* Battle text has a separate choice: 0 original artwork, other choices use battle entries in fonts.tsv. */
int MenuText_GetBattleFont(void);
void MenuText_SetBattleFont(int choice);
void MenuText_SetBattleFontId(int id);
int MenuText_BattleFontIdToSave(void);
/* The list of labels is made in a thread when it is not there (menu_labels.c). Called by the renderer between two
   frames: when the list has been made, the fonts for the menus are offered and the one asked for is taken into use.
   Costs one read of a variable otherwise. */
void MenuText_Poll(void);
/* Finish text catalog initialization after the first-run language choice, in this process. */
void MenuText_BootLanguageChosen(void);
/* Shared Font_Print glyphs have a separate choice; atlas replacements retain game metrics and styling. */
int MenuText_GetSharedFont(void);
void MenuText_SetSharedFont(int choice);
void MenuText_SetSharedFontId(int id);
void MenuText_RegisterGlyphs(int tbp, int cbp, const void *glyphs, int count, int lineHeight);
uint8_t *MenuText_RenderGlyphs(const uint8_t *rgba, int width, int height, int tbp, int cbp,
                             int scale, int *outWidth, int *outHeight);
/* Packed glyph x/y/width/height, top/bottom padding, atlas width/height, in logical pixels. */
int MenuText_MapGlyph(int tbp, int cbp, int width, int height,
                     float u0, float v0, float u1, float v1, float *rect);
/* External song name in the disc's 512x32 row, using the selected vector font.
   Skips missing glyphs. Returns allocated ordinary RGBA (255 opaque), or NULL for Original/no usable text. */
uint8_t *MenuText_RenderSong(const char *text, int selected, int scale, int *outWidth, int *outHeight);
/* Added stage name centered in the disc's 512x64 row, with ordinary RGBA alpha. */
uint8_t *MenuText_RenderStage(const char *text, int scale, int *outWidth, int *outHeight);
/* A line of UTF-8 text for the PC's own screens (gs/tbo_text.c): `kind` 0 in the Menu font, 1 in the Dialogue / UI
   font, each on "Original" the first font of fonts.tsv for that use. Capitals `cap` pixels high, `pad` pixels of
   room round it; the picture is *w x *h coverage bytes with the baseline *base rows down and the pen `pad` in.
   Measure returns how far the pen goes (0: no font or no text); Render returns the picture (free() it) or NULL.
   The generation changes when the font of either kind changes. */
uint32_t MenuText_LineGeneration(void);
int MenuText_LineMeasure(int kind, const char *text, int cap, int pad, int *w, int *h, int *base);
uint8_t *MenuText_LineRender(int kind, const char *text, int cap, int pad, int *w, int *h, int *base);
/* A heading of those screens in the main menu's heading style (the colours of the replaced headings), in the
   Menu font of kind 0. Measure gives the capitals' height *cap, the picture's *w x *h, the baseline *base rows down
   and the letters' left edge *pad in, and returns how far the pen goes (pixels; 0: no font or no text). Render
   returns the picture, RGBA with the GS's alpha (0x80 opaque), or NULL; free() it. */
int MenuText_HeadingMeasure(const char *text, int *cap, int *w, int *h, int *base, int *pad);
uint8_t *MenuText_HeadingRender(const char *text, int *w, int *h);
#ifdef __cplusplus
}
#endif
#endif
