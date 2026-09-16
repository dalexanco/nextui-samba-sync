#ifndef SCREENS_JOBS_LIST_H
#define SCREENS_JOBS_LIST_H

#include "api.h"
#include "jobs.h"

// Écran 1 (Gestion des jobs) -- see SPEC.md. For now navigation, B=Retour,
// X=Ajouter and A=Lancer are wired: Y=Éditer needs a pre-fillable
// job_wizard that doesn't exist yet, and MENU=Réglages needs écran 6, so
// their hints are withheld rather than shown as dead buttons.

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
	// A -> écran 3bis (aperçu avant sync) for JobsList_selectedJob(). See
	// screens/preview.h.
	JOBS_LIST_ACTION_SYNC_JOB,
} JobsListAction;

// Clamps/resets the selected row; call when entering this screen.
void JobsList_reset(void);

// *dirty is set to 1 if the selection moved (caller must redraw), left
// untouched otherwise.
JobsListAction JobsList_input(int *dirty);
void JobsList_render(SDL_Surface *screen, int show_setting);

// The job the current selection points at when JOBS_LIST_ACTION_SYNC_JOB
// (or, later, an edit action) is returned. NULL if there are no jobs.
const Job *JobsList_selectedJob(void);

#endif
