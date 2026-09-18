#ifndef SCREENS_ERROR_H
#define SCREENS_ERROR_H

#include "api.h"

// Écran 5 (Erreur) -- see SPEC.md. Generic full-screen error display.
//
// SPEC.md routes three distinct failure points here: the job_wizard's
// connection test (écran 2a) and remote-browse errors (écran 2b), and a
// blocking failure during the sync itself (écran 4). Only the écran 4 case
// is wired for now -- job_wizard.c keeps its own established inline
// remote_list_error/local_list_error pattern, out of scope for this
// increment.
//
// sync_engine.c/smb_client.c's error model is success/failure only (no
// reason code distinguishing timeout vs auth vs missing share vs local
// disk full, the specific cases SPEC.md's mockup enumerates), so the
// message shown here is necessarily generic rather than per-cause --
// narrowing that would mean threading a real reason through
// smb_client.c/sync_engine.c first, a separate increment.

typedef enum {
	ERROR_ACTION_NONE,
	ERROR_ACTION_BACK,
} ErrorAction;

// Copies message into this screen's own buffer (safe even if the caller's
// string was built on the stack) ; call when entering this screen. Kept
// short by callers (e.g. "Échec de synchronisation : <job>") since
// Error_render() has no text-wrapping -- see its own comment.
void Error_enter(const char *message);

// A or B both return ERROR_ACTION_BACK, per SPEC.md's "A/B Retour" (same
// convention as screens/summary.c).
ErrorAction Error_input(int *dirty);
void Error_render(SDL_Surface *screen, int show_setting);

#endif
