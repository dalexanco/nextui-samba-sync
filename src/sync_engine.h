#ifndef SYNC_ENGINE_H
#define SYNC_ENGINE_H

#include <stdbool.h>

#include "jobs.h"

// Computes what syncing `job` would do, for écran 3bis (Aperçu avant sync)
// -- the diffing half of docs/ARCHITECTURE.md's sync_engine state machine
// (CONNECTING -> LISTING_REMOTE -> LISTING_LOCAL -> DIFFING ->
// WAITING_CONFIRM). The COPYING/DELETING/DONE states (actually moving
// files, écran 4) aren't implemented yet.
//
// Blocking: connects, lists the whole remote subtree then the whole local
// subtree, and diffs them, all synchronously on the calling thread -- same
// as every other smb_client call in this codebase so far (job_wizard's
// browse steps, servers_list's connection test). docs/ARCHITECTURE.md
// sketches a tick-based incremental engine specifically to avoid ever
// blocking the UI thread for a sync; deferred until that's actually shown
// to be a problem on device, since nothing so far has needed it.
//
// "Already present" is decided by relative path + size only, no checksum,
// per docs/ARCHITECTURE.md.
typedef struct {
	bool ok; // false if the server lookup, connect, or either listing failed
	int to_copy_count;
	long long to_copy_bytes;
	int to_delete_count; // always 0 unless job->mirror
} SyncPreview;

SyncPreview sync_engine_preview(const Job *job);

#endif
