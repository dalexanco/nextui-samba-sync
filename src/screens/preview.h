#ifndef SCREENS_PREVIEW_H
#define SCREENS_PREVIEW_H

#include "api.h"
#include "jobs.h"

// Écran 3bis (Aperçu avant sync) -- see SPEC.md. Reached either from écran
// 1's "Lancer" (A) on a single job, or from écran 0's "Tout synchroniser"
// (see screens/jobs_list.c, screens/home.c and main.c). Both entry points
// always show this screen unconditionally -- per an explicit product
// decision, `preview_before_sync` isn't wired into either flow yet (see
// screens/settings.h), so both keep behaving as if it were always Oui.

typedef enum {
	PREVIEW_ACTION_NONE,
	PREVIEW_ACTION_BACK,
	// A on a successful preview -- sync_engine_confirm()/sync_queue_confirm()
	// has already been called by the time this is returned, so main.c only
	// needs to switch to écran 4 (screens/progress.c), picking Progress_enter()
	// vs Progress_enterAll() based on Preview_isMultiMode().
	PREVIEW_ACTION_START_SYNC,
} PreviewAction;

// Computes the sync diff for `job` (blocking -- see sync_engine.c); call
// when entering this screen. `job` must stay valid for as long as the
// screen is active (jobs_list.c's array outlives it, same as job_wizard.c
// borrowing servers_get() results).
void Preview_enter(const Job *job);

// Multi-job variant for "Tout synchroniser": snapshots every configured job
// into sync_queue.c and computes the grouped diff (blocking, one preview
// per job -- see sync_queue_preview()).
void Preview_enterAll(void);

// True after Preview_enterAll(), false after Preview_enter() -- main.c uses
// this after PREVIEW_ACTION_START_SYNC to pick which écran 4 entry point to
// call.
bool Preview_isMultiMode(void);

PreviewAction Preview_input(int *dirty);
void Preview_render(SDL_Surface *screen, int show_setting);

#endif
