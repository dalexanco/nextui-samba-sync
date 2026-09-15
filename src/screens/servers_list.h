#ifndef SCREENS_SERVERS_LIST_H
#define SCREENS_SERVERS_LIST_H

#include "api.h"

// Écran 1bis (Serveurs, lecture seule) -- see SPEC.md. Diagnostic view over
// servers declared in Samba Servers/ ; no creation/editing here.
//
// SPEC.md reaches this screen from écran 6 (Réglages), which doesn't exist
// yet -- for now it's wired directly from MENU on écran 1 (see main.c),
// as a temporary shortcut until écran 6 becomes the real hub.

typedef enum {
	SERVERS_LIST_ACTION_NONE,
	SERVERS_LIST_ACTION_BACK,
} ServersListAction;

// Clamps/resets selection and per-server test status; call when entering
// this screen.
void ServersList_reset(void);

// *dirty is set to 1 if the selection moved or a connection test finished
// (caller must redraw), left untouched otherwise.
ServersListAction ServersList_input(int *dirty);
void ServersList_render(SDL_Surface *screen, int show_setting);

#endif
