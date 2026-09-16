#ifndef SCREENS_SUMMARY_H
#define SCREENS_SUMMARY_H

#include "api.h"
#include "jobs.h"

// Écran 5bis (Résumé), job unique only -- see SPEC.md. Reached automatically
// once screens/progress.c's Progress_input() returns PROGRESS_ACTION_DONE,
// i.e. only on a successful sync; a blocking job-level error stays inline on
// écran 4 instead of routing here (see progress.h). The "Tout synchroniser"
// multi-job variant from SPEC.md isn't built yet either -- this only ever
// shows one job's tally.
//
// sync_engine still aborts the whole job on the first file failure (no
// per-file error recovery yet), so the "erreurs non fatales par fichier"
// line from SPEC.md's mockup is always 0 here -- this screen is only ever
// reached via the success path.

typedef enum {
	SUMMARY_ACTION_NONE,
	SUMMARY_ACTION_BACK,
} SummaryAction;

// Call once, right after Progress_input() has returned PROGRESS_ACTION_DONE
// for this job. `job` must stay valid for as long as the screen is active,
// same borrowed-pointer convention as screens/progress.h. Reads the finished
// run's numbers straight off sync_engine_progress()/sync_engine_totals() at
// render time, so nothing must call sync_engine_confirm() again (which would
// overwrite them) before the user backs out of this screen.
void Summary_enter(const Job *job);

// A or B both return to the jobs list, per SPEC.md's "A/B Retour" -- this
// screen has no other input.
SummaryAction Summary_input(int *dirty);
void Summary_render(SDL_Surface *screen, int show_setting);

#endif
