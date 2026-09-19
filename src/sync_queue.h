#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include <stdbool.h>

#include "sync_config.h"
#include "sync_engine.h"
#include "link_state.h"

// Runs sync_engine.c over every link of the config, one after the other, in
// file order -- either checking them (at startup and on "Check") or syncing
// them ("Sync everything"). Links with a config error are skipped.
// A link failing never stops the queue (SPEC.md). Also keeps, per link,
// everything the main and detail screens show: current phase, latest check
// and persisted last sync.

typedef enum {
	LINK_PHASE_IDLE,          // not checked (config error, or its check was interrupted)
	LINK_PHASE_CHECK_PENDING, // waiting for its turn to be checked
	LINK_PHASE_CHECKING,
	LINK_PHASE_CHECKED,       // sync_queue_check() is valid
	LINK_PHASE_SYNC_PENDING,  // waiting for its turn to be synced
	LINK_PHASE_SYNCING,
	LINK_PHASE_SYNCED,        // sync_queue_last() holds this run's outcome
	LINK_PHASE_SYNC_CANCELLED,// the run was cancelled before reaching this link
} LinkPhase;

typedef enum {
	QUEUE_IDLE,
	QUEUE_CHECKING,
	QUEUE_SYNCING,
} QueueMode;

// Reloads Samba Sync.toml and every link's persisted state. Only call while
// QUEUE_IDLE.
void sync_queue_reload(void);

// Starts checking every valid link. Only call while QUEUE_IDLE.
void sync_queue_check_all(void);

// Starts syncing every valid link. Only call while QUEUE_IDLE.
void sync_queue_sync_all(void);

// Advances the current run. Each call does at most one blocking check or one
// sync_engine_tick() slice; a link is first moved to CHECKING/SYNCING by a
// call of its own so the screen can show it before the blocking part.
// Returns true if anything changed (the screen should redraw).
bool sync_queue_tick(void);

// Interrupts the current run: checking stops after the link in progress;
// syncing cancels the link in progress and every remaining one.
void sync_queue_cancel(void);

QueueMode sync_queue_mode(void);

// Position of the link currently being synced among the links synced by
// this run (1-based) and their count -- for "Syncing 2/4".
int sync_queue_sync_position(void);
int sync_queue_sync_total(void);

// True once a "Sync everything" run has ended (until the next reload or
// check) -- for "Sync finished".
bool sync_queue_sync_finished(void);

LinkPhase sync_queue_phase(int link_index);
const LinkCheck *sync_queue_check(int link_index); // meaningful from LINK_PHASE_CHECKED
const LinkState *sync_queue_last(int link_index);  // persisted last sync (status NEVER if none)

// Can "Sync everything" run: idle, and at least one valid link.
bool sync_queue_can_sync(void);

#endif
