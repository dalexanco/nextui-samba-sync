#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>

// Sync jobs persisted as flat files in
// $SHARED_USERDATA_PATH "/samba-sync/jobs/<slug>.txt" (key=value, one file
// per job). The referenced server is resolved dynamically by name via
// servers_find() -- a job is never deleted automatically if its server goes
// missing (renamed/removed), see SPEC.md écran 1.
//
// Creation happens through the assistant screens (écran 2a/2b/3/2c) via
// jobs_unique_name()/jobs_create() below; editing an existing job isn't
// implemented yet.

#define MAX_JOBS 64
#define JOB_STR_MAX 256

typedef struct {
	char name[JOB_STR_MAX];
	char server[JOB_STR_MAX];
	char remote_path[JOB_STR_MAX];
	char local_path[JOB_STR_MAX];
	bool mirror;
	char last_sync_status[JOB_STR_MAX]; // "", "ok", or "error"
	int last_sync_time; // unix epoch, 0 = never synced
} Job;

// Rescans SHARED_USERDATA_PATH "/samba-sync/jobs" from scratch. Malformed
// entries
// (missing name/server/remote_path/local_path) are skipped silently. Safe
// to call again later to pick up jobs created/edited elsewhere.
void jobs_rescan(void);

int jobs_count(void);
const Job *jobs_get(int index);

// Resolves base_name to a name that doesn't collide with any currently
// loaded job, by appending " (2)", " (3)", ... on collision -- same
// numeric-suffix convention as local_fs.c's local_create_folder(). Pure
// computation, no I/O: écran 2c calls this once to show the user the name
// that A "Enregistrer" will actually save.
void jobs_unique_name(const char *base_name, char *out_name);

// Persists a new job under JOBS_PATH and reloads the in-memory list (so
// jobs_count()/jobs_get() reflect it immediately). `name` is expected to
// already be collision-free (see jobs_unique_name()) -- this only guards
// against the separate, much rarer case of two distinct names slugifying
// to the same filename. Returns false if the write fails.
bool jobs_create(const char *name, const char *server, const char *remote_path, const char *local_path, bool mirror);

// Persists status ("ok" or "error") and sync_time (unix epoch) as job's
// last_sync_status/last_sync_time, rewriting its on-disk file and reloading
// the in-memory list so écran 1's status badge reflects it immediately.
// job must be a pointer returned by jobs_get() (or anything that passed
// through it, e.g. JobsList_selectedJob()) -- its on-disk file is recovered
// by pointer arithmetic against this module's internal jobs[] array, so a
// pointer obtained any other way is undefined. No-op if job doesn't
// currently point into that array (shouldn't happen: nothing calls
// jobs_rescan() while a sync using this is in flight).
void jobs_set_sync_result(const Job *job, const char *status, int sync_time);

#endif
