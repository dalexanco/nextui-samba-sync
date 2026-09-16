#ifndef SCREENS_JOBS_LIST_H
#define SCREENS_JOBS_LIST_H

#include "api.h"

// Écran 1 (Gestion des jobs) -- see SPEC.md. For now navigation, B=Retour
// and X=Ajouter are wired: A=Lancer and Y=Éditer need sync_engine/a
// pre-fillable job_wizard that don't exist yet, and MENU=Réglages needs
// écran 6, so their hints are withheld rather than shown as dead buttons.

typedef enum {
	JOBS_LIST_ACTION_NONE,
	JOBS_LIST_ACTION_BACK,
	// Temporary: SPEC.md reaches écran 1bis (Serveurs) via écran 6
	// (Réglages), which doesn't exist yet -- MENU shortcuts there
	// directly for now (see main.c).
	JOBS_LIST_ACTION_SERVERS,
	// X -> écran 2a (assistant de création, étape 1/3). See
	// screens/job_wizard.h.
	JOBS_LIST_ACTION_NEW_JOB,
} JobsListAction;

// Clamps/resets the selected row; call when entering this screen.
void JobsList_reset(void);

// *dirty is set to 1 if the selection moved (caller must redraw), left
// untouched otherwise.
JobsListAction JobsList_input(int *dirty);
void JobsList_render(SDL_Surface *screen, int show_setting);

#endif
