#ifndef SCREENS_JOB_WIZARD_H
#define SCREENS_JOB_WIZARD_H

#include "api.h"

// Écran 2a (Nouveau job — Choisir un serveur), étape 1/3 de l'assistant de
// création de job -- see SPEC.md. Only this step exists so far: écran 2b
// (parcourir le partage distant) doesn't exist yet, so on a successful
// connection this screen shows the result inline instead of advancing (same
// withheld-transition pattern as jobs_list.h's MENU shortcut) -- wire the
// real transition once smb_client gains smb_list() and this screen grows a
// browse-remote step.

typedef enum {
	JOB_WIZARD_ACTION_NONE,
	JOB_WIZARD_ACTION_CANCEL,
} JobWizardAction;

// Clamps/resets the selected row and any connection status; call when
// entering this screen.
void JobWizard_reset(void);

// *dirty is set to 1 if the selection moved or a connection attempt just
// finished (caller must redraw), left untouched otherwise.
JobWizardAction JobWizard_input(int *dirty);
void JobWizard_render(SDL_Surface *screen, int show_setting);

#endif
