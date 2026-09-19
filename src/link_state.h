#ifndef LINK_STATE_H
#define LINK_STATE_H

#include <stdbool.h>

#include "browse.h"
#include "sync_config.h"

// Result of a link's last sync, persisted by the pak in
// $SHARED_USERDATA_PATH "/samba-sync/state/<slug>.txt" (key=value, one file
// per link) so the main screen's failure marker and the detail screen
// survive a restart -- see SPEC.md, screen 1. Keyed by link name:
// renaming a link in Samba Sync.toml loses its history.

#define LINK_STATE_MAX_ERRORS 50
#define LINK_ERROR_REASON_MAX 96

typedef enum {
	LINK_STATUS_NEVER,     // never synced (no state file)
	LINK_STATUS_OK,        // every file copied/deleted
	LINK_STATUS_PARTIAL,   // finished, but some files failed
	LINK_STATUS_ERROR,     // stopped by a link-wide error (connection, auth, ...)
	LINK_STATUS_CANCELLED, // stopped by the user
} LinkStatus;

typedef struct {
	char path[BROWSE_STR_MAX]; // link-relative file path, "" for a link-wide error
	char reason[LINK_ERROR_REASON_MAX]; // short reason, shown as-is
} LinkError;

typedef struct {
	LinkStatus status;
	int time; // unix epoch of the end of the sync, 0 if never
	int files_copied;
	long long bytes_copied;
	int files_deleted;
	int error_count; // errors[0..error_count), capped at LINK_STATE_MAX_ERRORS
	int error_total; // every error that occurred, may exceed error_count
	LinkError errors[LINK_STATE_MAX_ERRORS];
} LinkState;

// Loads link's persisted state into *out. Missing/unreadable file, or one
// that belongs to another link whose name slugifies the same way, yields
// LINK_STATUS_NEVER.
void link_state_load(const Link *link, LinkState *out);

// Rewrites link's state file. Failures are logged and otherwise ignored:
// losing the history of one sync must never break the sync itself.
void link_state_save(const Link *link, const LinkState *state);

// Counts an error in error_total, and keeps it in errors[] unless already at
// LINK_STATE_MAX_ERRORS.
void link_state_add_error(LinkState *state, const char *path, const char *reason);

// True for the statuses that earn a "!" marker on the main screen.
bool link_state_failed(const LinkState *state);

#endif
