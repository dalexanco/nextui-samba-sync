#ifndef SYNC_ENGINE_H
#define SYNC_ENGINE_H

#include <stdbool.h>

#include "jobs.h"
#include "browse.h"

// Computes/executes a job's sync, driving docs/ARCHITECTURE.md's state
// machine: CONNECTING -> LISTING_REMOTE -> LISTING_LOCAL -> DIFFING ->
// WAITING_CONFIRM (sync_engine_preview(), écran 3bis) -> COPYING ->
// DELETING -> DONE (sync_engine_confirm()/sync_engine_tick(), écran 4).
//
// "Already present" is decided by relative path + size only, no checksum,
// per docs/ARCHITECTURE.md. Mirror mode never deletes before every copy has
// succeeded (COPYING always finishes, or errors out, before DELETING
// starts).

typedef struct {
	bool ok; // false if the server lookup, connect, or either listing failed
	int to_copy_count;
	long long to_copy_bytes;
	int to_delete_count; // always 0 unless job->mirror
	int already_present_count; // remote files matched by path+size, skipped -- écran 5bis's "ignorés"
} SyncPreview;

// Blocking: connects, lists the whole remote subtree then the whole local
// subtree, and diffs them, all synchronously on the calling thread -- same
// as every other smb_client call in this codebase so far (job_wizard's
// browse steps, servers_list's connection test). A single directory-listing
// pass is cheap and has no meaningful incremental progress to show, unlike
// the copy/delete phase below, so this doesn't need the tick-based design.
//
// The diff is kept in module-static state for sync_engine_confirm() to
// reuse -- callers must call this immediately before confirm()'ing the same
// job (écran 3bis does, via Preview_enter()/BTN_A in screens/preview.c).
SyncPreview sync_engine_preview(const Job *job);

typedef enum {
	SYNC_STATE_IDLE, // no sync_engine_confirm() call yet, or a previous run finished/reset
	SYNC_STATE_COPYING,
	SYNC_STATE_DELETING, // mirror only; only reached once every copy has succeeded
	SYNC_STATE_DONE,
	SYNC_STATE_ERROR,
	SYNC_STATE_CANCELLED,
} SyncState;

typedef struct {
	char current_file[BROWSE_STR_MAX]; // "" between files, or once terminal
	int files_copied;
	long long bytes_copied;
	int files_deleted;
} SyncProgress;

// Starts executing the diff computed by the most recent sync_engine_preview
// call on this same job: (re)connects and moves to SYNC_STATE_COPYING (or
// straight to DELETING/DONE if there's nothing to copy). Sets
// SYNC_STATE_ERROR (and persists it via jobs_set_sync_result()) if the
// (re)connect fails.
void sync_engine_confirm(const Job *job);

// Advances one bounded unit of work -- one 64KB chunk of the file currently
// being copied, or one file during the delete phase -- and returns the
// resulting state, so main.c's dirty-gated render loop never blocks for a
// whole file/job (see docs/ARCHITECTURE.md). No-op, returning the current
// state immediately, once in a terminal state or before confirm() is called.
SyncState sync_engine_tick(void);

// Cancels a sync in progress: closes the SMB connection, leaves whatever's
// already been copied/deleted in place, performs no further copies or
// deletions. No-op if not currently copying/deleting. Does not update the
// job's persisted last_sync_status/time -- an interrupted attempt isn't a
// completed one.
void sync_engine_cancel(void);

SyncState sync_engine_state(void);
SyncProgress sync_engine_progress(void);

// The diff totals sync_engine_confirm() is executing against -- the
// denominators écran 4 needs for "X / Y fichiers" and percentage displays.
// Same numbers sync_engine_preview() returned; kept accessible here too so
// screens/progress.c doesn't have to hold onto écran 3bis's SyncPreview.
SyncPreview sync_engine_totals(void);

#endif
