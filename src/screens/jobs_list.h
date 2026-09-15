#ifndef SCREENS_JOBS_LIST_H
#define SCREENS_JOBS_LIST_H

#include "api.h"

// Écran 1 (Gestion des jobs) -- see SPEC.md. For now only navigation and
// B=Retour are wired: A=Lancer, X=Ajouter, Y=Éditer, MENU=Réglages need
// sync_engine/job_wizard/settings screens that don't exist yet, so their
// hints are withheld rather than shown as dead buttons.

typedef enum {
	JOBS_LIST_ACTION_NONE,
	JOBS_LIST_ACTION_BACK,
	// Temporary: SPEC.md reaches écran 1bis (Serveurs) via écran 6
	// (Réglages), which doesn't exist yet -- MENU shortcuts there
	// directly for now (see main.c).
	JOBS_LIST_ACTION_SERVERS,
} JobsListAction;

// Clamps/resets the selected row; call when entering this screen.
void JobsList_reset(void);

// *dirty is set to 1 if the selection moved (caller must redraw), left
// untouched otherwise.
JobsListAction JobsList_input(int *dirty);
void JobsList_render(SDL_Surface *screen, int show_setting);

#endif
