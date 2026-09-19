#ifndef LINK_DETAIL_H
#define LINK_DETAIL_H

#include "api.h"

// Écran 2 (SPEC.md): read-only detail of one link -- its configuration, its
// latest check, and its persisted last sync with the list of errors.

typedef enum {
	LINK_DETAIL_ACTION_NONE,
	LINK_DETAIL_ACTION_BACK,
} LinkDetailAction;

// Builds the text for link_index; call when opening the screen.
void LinkDetail_enter(int link_index);
LinkDetailAction LinkDetail_input(int *dirty);
void LinkDetail_render(SDL_Surface *screen, int show_setting);

#endif
