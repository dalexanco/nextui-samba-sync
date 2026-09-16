#ifndef SCREENS_PROGRESS_H
#define SCREENS_PROGRESS_H

#include "api.h"
#include "jobs.h"

// Écran 4 (Progression) -- see SPEC.md. Single-job only for now, reached
// from écran 3bis's "A Lancer la synchronisation" once sync_engine_confirm()
// has already been called (see screens/preview.c).
//
// Deviates from the SPEC.md mockup in two ways, both because the screens it
// would otherwise hand off to don't exist yet (same graceful-degradation
// pattern écran 3bis already uses for its own missing downstream screen):
// the connexion/analyse/comparaison step lines aren't shown here since they
// already happened during écran 3bis, and a blocking error stays inline
// instead of routing to the not-yet-built écran 5 (Erreur). A successful
// completion, on the other hand, does now auto-route to écran 5bis (Résumé,
// see screens/summary.h) via PROGRESS_ACTION_DONE, matching SPEC.md.

typedef enum {
	PROGRESS_ACTION_NONE,
	PROGRESS_ACTION_BACK,
	PROGRESS_ACTION_DONE, // sync finished successfully -- caller should Summary_enter() and switch to écran 5bis
} ProgressAction;

// Call once, right after Preview_input() has returned PREVIEW_ACTION_START_SYNC
// (and therefore already called sync_engine_confirm()) for this job. `job`
// must stay valid for as long as the screen is active, same borrowed-pointer
// convention as screens/preview.h.
void Progress_enter(const Job *job);

// Ticks the sync engine forward by one bounded unit of work each call (see
// sync_engine.h) and handles B (cancel while running, back once terminal).
// Returns PROGRESS_ACTION_DONE as soon as the engine reaches SYNC_STATE_DONE
// -- including immediately, on the very first call, if sync_engine_confirm()
// already finished synchronously (nothing to copy or delete) -- so this
// screen never renders a stale "done" frame of its own; screens/summary.h
// owns that display. *dirty is always set to 1 -- this screen animates every
// frame while a sync is in progress.
ProgressAction Progress_input(int *dirty);
void Progress_render(SDL_Surface *screen, int show_setting);

#endif
