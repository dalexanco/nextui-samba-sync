#ifndef UI_H
#define UI_H

#include <stddef.h>

#include "api.h"

// Small text-rendering helpers shared across screens/*.c, following the
// same primitives every screen in docs/ARCHITECTURE.md needs (title bar,
// centered/aligned text) -- factored out once two screens needed the exact
// same code (home.c, jobs_list.c), not speculatively.

int UI_renderText(SDL_Surface *screen, const char *text, TTF_Font *font, SDL_Color color, int x, int y);
int UI_textWidth(const char *text, TTF_Font *font);
int UI_renderTextCentered(SDL_Surface *screen, const char *text, TTF_Font *font, SDL_Color color, int y);

// Standard pak title bar: name in a black pill, top-left, with the
// hardware (volume/brightness) icon group to its right.
void UI_renderTitle(SDL_Surface *screen, const char *name, int show_setting);

// Copies text into out (out_size bytes), shortened with a trailing "…" if
// needed so it renders no wider than max_width. Cuts on UTF-8 character
// boundaries, unlike GFX_truncateText() which can split an accented letter.
// Returns the rendered width.
int UI_fitText(TTF_Font *font, const char *text, char *out, size_t out_size, int max_width);

// Splits text into at most max_lines chunks, each rendering no wider than
// max_width, breaking on spaces (and inside a word when a single word is too
// long). Each out[i] holds line_size bytes. Returns the number of lines
// written, at least 1.
int UI_wrapText(TTF_Font *font, const char *text, char *out, size_t line_size, int max_lines, int max_width);

// Formats a byte count as a human-readable string ("340 Mo", "12 Ko",
// "3 o") -- factored out of screens/preview.c once screens/progress.c
// needed the exact same code.
void UI_formatBytes(long long bytes, char *out, size_t out_size);

#endif
