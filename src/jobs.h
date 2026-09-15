#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>

// Sync jobs persisted as flat files in
// $SHARED_USERDATA_PATH "/samba-sync/jobs/<slug>.txt" (key=value, one file
// per job). The referenced server is resolved dynamically by name via
// servers_find() -- a job is never deleted automatically if its server goes
// missing (renamed/removed), see SPEC.md écran 1.
//
// This module only reads that config for now: job creation/editing happens
// through the assistant screens (écran 2a/2b/3), not yet implemented.

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

#endif
