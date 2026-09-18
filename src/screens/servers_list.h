#ifndef SCREENS_SERVERS_LIST_H
#define SCREENS_SERVERS_LIST_H

#include "api.h"

// Écran 1bis (Serveurs, lecture seule) -- see SPEC.md. Diagnostic view over
// servers declared in Samba Servers/ ; no creation/editing here. Reached
// from écran 6 (Réglages)'s "Voir les serveurs..." row (see
// screens/settings.h); B returns there, not to écran 1 directly.

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
