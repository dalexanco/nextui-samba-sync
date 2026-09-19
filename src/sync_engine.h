#ifndef SYNC_ENGINE_H
#define SYNC_ENGINE_H

#include <stdbool.h>

#include "sync_config.h"
#include "link_state.h"
#include "browse.h"

// Checks and syncs one link (see SPEC.md "Concept" and "Flux Tout
// synchroniser"). A file is to copy if it's missing locally or its size
// differs; in mirror mode, a local file absent remotely is to delete.
//
// Checking is blocking (connect + recursive listings + diff). Syncing is
// incremental: sync_engine_start() checks again (so mirror deletions rely on
// a fresh diff) then sync_engine_tick() advances by a bounded time slice so
// main.c's render loop keeps running.

typedef struct {
	bool ok;
	char message[LINK_ERROR_REASON_MAX]; // why the check failed, if !ok
	int to_copy_count;
	long long to_copy_bytes;
	int to_delete_count; // always 0 unless the link is in mirror mode
} LinkCheck;

// Blocking. Never called on a link with a config error.
LinkCheck sync_engine_check(const Link *link);

typedef enum {
	SYNC_STATE_IDLE,      // nothing started yet
	SYNC_STATE_COPYING,
	SYNC_STATE_DELETING,  // mirror only, only reached once every copy has succeeded
	SYNC_STATE_DONE,      // finished: ok, partial, or stopped by a link-wide error
	SYNC_STATE_CANCELLED,
} SyncState;

typedef struct {
	char current_file[BROWSE_STR_MAX]; // "" when not copying
	int files_done;            // files processed so far (copied or failed)
	long long bytes_done;      // bytes processed so far, including the current file's partial bytes
	int files_deleted;
	int to_copy_count;         // totals of the diff being executed
	long long to_copy_bytes;
	int to_delete_count;
} SyncProgress;

// Starts syncing link: checks it again (blocking), then moves to COPYING
// (or straight to DELETING/DONE). A failed check ends immediately in
// SYNC_STATE_DONE with a link-wide error in sync_engine_result().
void sync_engine_start(const Link *link);

// Advances the sync by a short time slice (a few 64 KB chunks and/or
// deletions) and returns the resulting state. No-op once terminal.
SyncState sync_engine_tick(void);

// Stops a sync in progress: removes the partially downloaded file, keeps
// whatever was already copied/deleted, performs nothing further. Ends in
// SYNC_STATE_CANCELLED. No-op if not copying/deleting.
void sync_engine_cancel(void);

SyncState sync_engine_state(void);
SyncProgress sync_engine_progress(void);

// Outcome of the last sync once DONE/CANCELLED, ready for link_state_save().
const LinkState *sync_engine_result(void);

#endif
