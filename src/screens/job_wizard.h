#ifndef SCREENS_JOB_WIZARD_H
#define SCREENS_JOB_WIZARD_H

#include "api.h"

// The full job-creation assistant -- écran 2a (choix serveur), 2b (parcours
// distant), 3 (parcours SD locale) and 2c (récapitulatif), étapes 1 to 3/3
// of SPEC.md's flow -- as one state machine. See screens/job_wizard.c for
// the per-step breakdown.

typedef enum {
	JOB_WIZARD_ACTION_NONE,
	JOB_WIZARD_ACTION_CANCEL,
	JOB_WIZARD_ACTION_SAVED, // job written to disk (écran 2c "A Enregistrer")
} JobWizardAction;

// Clamps/resets the selected row and any connection status; call when
// entering this screen.
void JobWizard_reset(void);

// *dirty is set to 1 if the selection moved or a connection attempt just
// finished (caller must redraw), left untouched otherwise.
JobWizardAction JobWizard_input(int *dirty);
void JobWizard_render(SDL_Surface *screen, int show_setting);

#endif
