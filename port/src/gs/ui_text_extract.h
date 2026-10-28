#ifndef BT3_UI_TEXT_EXTRACT_H
#define BT3_UI_TEXT_EXTRACT_H

/* Generate the English UI, mode and story dialogue catalog from the extracted disc files.
   Work runs in a detached thread so it does not hold up the first menu. */
void UiText_ExtractOnStartup(const char *gameTextDir);

/* The Spanish, German, French and Italian catalogs (ui/<locale>.json) from the European disc's archives the setup
   took (gamedata/pzs3eu1), when one is missing or was made by another version of this. At once, in the caller's
   thread (only when catalogs need to be generated). Returns how many were made, 0 when none had to be, -1 when they could
   not be (`why` says why). */
#include <stddef.h>
int UiText_EuropeanOnStartup(const char *gameTextDir, char *why, size_t whyLen);

#endif
