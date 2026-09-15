#ifndef SCREENS_HOME_H
#define SCREENS_HOME_H

#include "api.h"

// Écran 0 (accueil simplifié) -- see SPEC.md. One highlighted action
// ("Tout synchroniser", inactive when no job is configured yet) plus a
// status line and navigation to the jobs list.

typedef enum {
	HOME_ACTION_NONE,
	HOME_ACTION_SYNC_ALL,
	HOME_ACTION_MANAGE_JOBS,
	HOME_ACTION_QUIT,
} HomeAction;

HomeAction Home_input(void);
void Home_render(SDL_Surface *screen, int show_setting);

#endif
