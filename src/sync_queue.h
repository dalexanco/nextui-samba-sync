#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include "jobs.h"
#include "sync_engine.h"

// Drives sync_engine.c across every configured job in sequence, for écran 0's
// "Tout synchroniser" -- see docs/ARCHITECTURE.md ("la file multi-job est
// gérée par une couche au-dessus... directement dans home.c" -- this module
// is that layer, kept separate rather than folded into home.c since three
// screens (3bis/4/5bis) all need to drive/read it).
//
// Per SPEC.md, a job failing doesn't stop the queue -- each job's outcome is
// recorded in sync_queue_result() and the next job starts regardless. This
// is unrelated to (and doesn't reopen) the single-job flow's existing
// all-or-nothing-per-job behavior inside sync_engine.c itself, which this
// module doesn't change.

typedef enum {
	QUEUE_STATE_IDLE, // no sync_queue_confirm() call yet, or reset via sync_queue_build()
	QUEUE_STATE_RUNNING,
	QUEUE_STATE_DONE, // every queued job was attempted (ok or error) -- not cancelled
	QUEUE_STATE_CANCELLED,
} QueueState;

typedef struct {
	const Job *job;
	bool ok; // false if this job's (re)connect or copy/delete failed
	int files_copied;
	long long bytes_copied;
	int files_deleted;
} SyncQueueResult;

// Grouped totals across every queued job, for écran 3bis. Mirror-mode
// per-job delete counts are kept as a separate breakdown (SPEC.md's écran
// 3bis mockup lists deletions per job, not just a single summed count).
typedef struct {
	int job_count;
	int to_copy_count;
	long long to_copy_bytes;
	struct {
		const Job *job;
		int to_delete_count;
	} mirror[MAX_JOBS];
	int mirror_count;
} SyncQueuePreview;

// Snapshots jobs_count()/jobs_get() into this module's own queue; call once
// when entering écran 3bis via the "Tout synchroniser" path.
void sync_queue_build(void);

// Sums sync_engine_preview() across every queued job. Blocking, same as
// sync_engine_preview() itself. Each job's diff is only used here to
// contribute to the grouped totals -- sync_queue_confirm() re-fetches it
// per-job immediately before running, since sync_engine_preview()'s cache is
// module-static and would otherwise still point at whichever job this
// function queried last.
SyncQueuePreview sync_queue_preview(void);

// Starts executing the queue from its first job.
void sync_queue_confirm(void);

// Advances one bounded unit of work (delegates to sync_engine_tick()),
// automatically starting the next queued job once the current one reaches a
// terminal state, recording its result. No-op, returning the current state
// immediately, once in a terminal state or before confirm() is called.
QueueState sync_queue_tick(void);

// Cancels the job currently running and every remaining queued job (per
// SPEC.md, unlike the single-job flow this doesn't leave later jobs to run).
void sync_queue_cancel(void);

QueueState sync_queue_state(void);
int sync_queue_count(void);
int sync_queue_current_index(void); // valid while RUNNING
const Job *sync_queue_currentJob(void); // valid while RUNNING

int sync_queue_resultCount(void);
const SyncQueueResult *sync_queue_result(int index);

#endif
