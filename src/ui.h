#ifndef UI_H
#define UI_H

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

#endif
