#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

// Global settings persisted in $SHARED_USERDATA_PATH "/samba-sync/settings.txt"
// (key=value, single file) -- see docs/ARCHITECTURE.md. Loaded once at
// startup via settings_load(); each setter below persists immediately, no
// separate "save" step -- unlike job_wizard's recap-then-Enregistrer flow,
// écran 6's toggles/values take effect as soon as A is pressed (see
// screens/settings.h).
//
// Not yet consumed anywhere else in the app: overwrite_existing,
// preview_before_sync and network_timeout are stored and displayed
// correctly but don't yet change sync_engine/écran 3bis/smb_client
// behavior -- see screens/settings.h for why that's deliberately deferred.

typedef struct {
	bool overwrite_existing;  // Non (default): skip files already present by path+size. Oui: always re-copy.
	bool preview_before_sync; // Oui (default): écran 3bis shows before a sync starts. Non: sync starts immediately.
	int network_timeout;      // Seconds before giving up on an unresponsive connection. Default 10.
} Settings;

// Loads settings.txt into memory, creating it with defaults if missing or
// malformed. Call once at startup (see main.c).
void settings_load(void);

const Settings *settings_get(void);

// Each setter updates the in-memory value and immediately rewrites
// settings.txt -- small enough file that a whole-file rewrite per change is
// simpler than tracking a dirty flag.
void settings_set_overwrite_existing(bool value);
void settings_set_preview_before_sync(bool value);
void settings_set_network_timeout(int value);

#endif
