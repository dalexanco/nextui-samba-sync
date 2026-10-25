#ifndef LINK_DETAIL_H
#define LINK_DETAIL_H

#include "api.h"

// Screen 2 (SPEC.md): detail of one link -- its configuration, its latest
// check, and its persisted last sync with the list of errors. Its only action
// is syncing this link alone, which is how a failed sync is retried.

typedef enum {
	LINK_DETAIL_ACTION_NONE,
	LINK_DETAIL_ACTION_BACK,
	LINK_DETAIL_ACTION_SYNC, // sync this link alone; progress shows on screen 1
} LinkDetailAction;

// Builds the text for link_index; call when opening the screen.
void LinkDetail_enter(int link_index);
LinkDetailAction LinkDetail_input(int *dirty);
void LinkDetail_render(SDL_Surface *screen, int show_setting);

#endif
