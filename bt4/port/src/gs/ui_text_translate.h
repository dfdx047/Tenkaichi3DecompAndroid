#ifndef BT3_UI_TEXT_TRANSLATE_H
#define BT3_UI_TEXT_TRANSLATE_H
#include <stdint.h>

/* Load the selected language's UI/dialog catalog. A missing catalog leaves the game's text alone. */
int UiText_Load(const char *gameTextDir, const char *locale);
/* Exact table and line lookup, with an unambiguous string lookup for other font calls. */
const uint16_t *UiText_TranslateLine(const uint32_t *table, int line, const uint16_t *original);
const uint16_t *UiText_TranslateString(const uint16_t *original);

#endif
