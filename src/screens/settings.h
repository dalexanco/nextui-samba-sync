#ifndef SCREENS_SETTINGS_H
#define SCREENS_SETTINGS_H

#include "api.h"

// Écran 6 (Réglages globaux) -- see SPEC.md. Reached via MENU from écran 1
// (see screens/jobs_list.h), and is now the real hub for écran 1bis
// (Serveurs) per SPEC.md, retiring the direct-from-écran-1 shortcut that
// used to live there (see screens/servers_list.h).
//
// Toggles/cycles the three values persisted by settings.h -- each A press
// commits immediately, there's no separate "save" step. Deviates from
// SPEC.md in one respect: overwrite_existing, preview_before_sync and
// network_timeout are stored and displayed correctly but don't yet change
// any other screen's behavior (écran 3bis still always shows before a sync,
// "already present" is still always skipped, and sync_engine/smb_client's
// connection timeout is still each call site's own hardcoded constant) --
// wiring that up was deliberately deferred to a later increment.

typedef enum {
	SETTINGS_ACTION_NONE,
	SETTINGS_ACTION_BACK,
	SETTINGS_ACTION_VIEW_SERVERS, // A on "Voir les serveurs..." -> écran 1bis
} SettingsAction;

// Resets the selected row; call when entering this screen.
void Settings_reset(void);

// *dirty is set to 1 if the selection moved or a value changed (caller must
// redraw), left untouched otherwise.
SettingsAction Settings_input(int *dirty);
void Settings_render(SDL_Surface *screen, int show_setting);

#endif
