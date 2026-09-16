#ifndef SCREENS_PREVIEW_H
#define SCREENS_PREVIEW_H

#include "api.h"
#include "jobs.h"

// Écran 3bis (Aperçu avant sync) -- see SPEC.md. Single-job only for now:
// reached from écran 1's "Lancer" (A) on a job (see screens/jobs_list.c
// and main.c). The "Tout synchroniser" multi-job variant (home.c) stays a
// no-op until a job queue exists on top of sync_engine.c.
//
// "A Lancer la synchronisation" from the SPEC.md mockup is withheld:
// sync_engine.c only computes the diff so far (WAITING_CONFIRM in
// docs/ARCHITECTURE.md's state machine) -- actually copying/deleting is
// écran 4, not built yet. Same withheld-action pattern used for écran
// 2b/3's Y until their target screens existed (see job_wizard.c).

typedef enum {
	PREVIEW_ACTION_NONE,
	PREVIEW_ACTION_BACK,
} PreviewAction;

// Computes the sync diff for `job` (blocking -- see sync_engine.c); call
// when entering this screen. `job` must stay valid for as long as the
// screen is active (jobs_list.c's array outlives it, same as job_wizard.c
// borrowing servers_get() results).
void Preview_enter(const Job *job);

PreviewAction Preview_input(int *dirty);
void Preview_render(SDL_Surface *screen, int show_setting);

#endif
