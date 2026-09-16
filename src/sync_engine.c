#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "defines.h"
#include "servers.h"
#include "smb_client.h"
#include "local_fs.h"
#include "browse.h"
#include "sync_engine.h"

#define SYNC_TIMEOUT_SECONDS 10
#define COPY_CHUNK_SIZE (64 * 1024)

// The diff computed by the most recent sync_engine_preview() call, kept
// around so sync_engine_confirm() (called right after, from écran 3bis's
// BTN_A) doesn't need to recompute or re-list anything.
static BrowseFileEntry to_copy[BROWSE_MAX_FILES];
static int to_copy_count = 0;
static long long to_copy_bytes = 0;
static BrowseFileEntry to_delete[BROWSE_MAX_FILES];
static int to_delete_count = 0;
static int already_present_count = 0;

static SyncState state = SYNC_STATE_IDLE;
static const Job *active_job = NULL;
static SmbSession *session = NULL;
static SmbFile *copy_file = NULL;
static FILE *local_file = NULL;
static int copy_index = 0;
static int delete_index = 0;
static SyncProgress progress;

SyncPreview sync_engine_preview(const Job *job)
{
	SyncPreview preview = {0};
	to_copy_count = 0;
	to_copy_bytes = 0;
	to_delete_count = 0;
	already_present_count = 0;

	const Server *server = servers_find(job->server);
	if (!server) return preview;

	SmbError error;
	SmbSession *preview_session = smb_connect(server, SYNC_TIMEOUT_SECONDS, &error);
	if (!preview_session) return preview;

	static BrowseFileEntry remote_files[BROWSE_MAX_FILES];
	static BrowseFileEntry local_files[BROWSE_MAX_FILES];

	int remote_count = smb_list_files_recursive(preview_session, job->remote_path, remote_files, BROWSE_MAX_FILES, &error);
	smb_disconnect(preview_session);
	if (remote_count < 0) return preview;

	// A local_path that doesn't exist yet just means "never synced" (écran
	// 2c always saves a job with a real remote folder but the local
	// destination might not have been created by anything else yet) --
	// nothing to diff against, not a failure. Only ask local_fs.c to
	// recurse once we know the root itself is there, so a real read error
	// partway through the tree isn't silently mistaken for "doesn't exist".
	char local_full[MAX_PATH];
	snprintf(local_full, sizeof(local_full), "%s/%s", SDCARD_PATH, job->local_path);
	struct stat local_root_st;
	int local_count = 0;
	if (stat(local_full, &local_root_st) == 0) {
		local_count = local_list_files_recursive(job->local_path, local_files, BROWSE_MAX_FILES);
		if (local_count < 0) return preview;
	}

	for (int i = 0; i < remote_count; i++) {
		bool present = false;
		for (int j = 0; j < local_count; j++) {
			if (remote_files[i].size == local_files[j].size &&
			    strcmp(remote_files[i].rel_path, local_files[j].rel_path) == 0) {
				present = true;
				break;
			}
		}
		if (!present && to_copy_count < BROWSE_MAX_FILES) {
			to_copy[to_copy_count++] = remote_files[i];
			to_copy_bytes += remote_files[i].size;
		}
		else if (present) {
			already_present_count++;
		}
	}

	if (job->mirror) {
		for (int i = 0; i < local_count; i++) {
			bool present = false;
			for (int j = 0; j < remote_count; j++) {
				if (strcmp(local_files[i].rel_path, remote_files[j].rel_path) == 0) {
					present = true;
					break;
				}
			}
			if (!present && to_delete_count < BROWSE_MAX_FILES) {
				to_delete[to_delete_count++] = local_files[i];
			}
		}
	}

	preview.ok = true;
	preview.to_copy_count = to_copy_count;
	preview.to_copy_bytes = to_copy_bytes;
	preview.to_delete_count = to_delete_count;
	preview.already_present_count = already_present_count;
	return preview;
}

// Tears down whatever's currently open (file handles, SMB connection),
// lands on end_state and -- if status is non-NULL -- persists it as the
// job's last_sync_status/time. Shared terminal path for every way a sync
// can stop: finishing normally, erroring out, or being cancelled.
static void endSync(SyncState end_state, const char *status)
{
	if (local_file) { fclose(local_file); local_file = NULL; }
	if (copy_file) { smb_close_read(copy_file); copy_file = NULL; }
	if (session) { smb_disconnect(session); session = NULL; }
	state = end_state;
	progress.current_file[0] = '\0';
	if (status) jobs_set_sync_result(active_job, status, (int)time(NULL));
}

// Opens the next not-yet-copied file in to_copy[], creating any missing
// local parent directories first. Once every file's been handled, advances
// state past COPYING (into DELETING, or straight to DONE via endSync()).
// Returns true if a file is now open and ready for sync_engine_tick() to
// read from.
static bool startNextCopyFile(void)
{
	while (copy_index < to_copy_count) {
		const BrowseFileEntry *entry = &to_copy[copy_index];

		char remote_full[MAX_PATH];
		browse_path_push(remote_full, active_job->remote_path, entry->rel_path);

		char local_rel[MAX_PATH];
		browse_path_push(local_rel, active_job->local_path, entry->rel_path);

		char local_dir[MAX_PATH];
		snprintf(local_dir, sizeof(local_dir), "%s", local_rel);
		if (!browse_path_pop(local_dir)) local_dir[0] = '\0';
		local_ensure_dir(local_dir);

		char local_full[MAX_PATH];
		snprintf(local_full, sizeof(local_full), "%s/%s", SDCARD_PATH, local_rel);

		SmbError error;
		copy_file = smb_open_read(session, remote_full, &error);
		if (!copy_file) {
			endSync(SYNC_STATE_ERROR, "error");
			return false;
		}

		local_file = fopen(local_full, "wb");
		if (!local_file) {
			smb_close_read(copy_file);
			copy_file = NULL;
			endSync(SYNC_STATE_ERROR, "error");
			return false;
		}

		snprintf(progress.current_file, sizeof(progress.current_file), "%s", entry->rel_path);
		return true;
	}

	progress.current_file[0] = '\0';
	if (active_job->mirror && to_delete_count > 0) state = SYNC_STATE_DELETING;
	else endSync(SYNC_STATE_DONE, "ok");
	return false;
}

void sync_engine_confirm(const Job *job)
{
	active_job = job;
	copy_index = 0;
	delete_index = 0;
	copy_file = NULL;
	local_file = NULL;
	progress = (SyncProgress){0};

	const Server *server = servers_find(job->server);
	SmbError error;
	session = server ? smb_connect(server, SYNC_TIMEOUT_SECONDS, &error) : NULL;
	if (!session) {
		state = SYNC_STATE_ERROR;
		jobs_set_sync_result(job, "error", (int)time(NULL));
		return;
	}

	if (to_copy_count > 0) {
		state = SYNC_STATE_COPYING;
		startNextCopyFile();
	}
	else if (job->mirror && to_delete_count > 0) {
		state = SYNC_STATE_DELETING;
	}
	else {
		endSync(SYNC_STATE_DONE, "ok");
	}
}

SyncState sync_engine_tick(void)
{
	if (state == SYNC_STATE_COPYING) {
		if (!copy_file) return state; // startNextCopyFile() already moved state elsewhere

		uint8_t buf[COPY_CHUNK_SIZE];
		int n = smb_read_chunk(copy_file, buf, sizeof(buf));
		if (n < 0) {
			endSync(SYNC_STATE_ERROR, "error");
			return state;
		}
		if (n > 0) {
			fwrite(buf, 1, (size_t)n, local_file);
			progress.bytes_copied += n;
		}
		if ((size_t)n < sizeof(buf)) { // short read == end of this file
			fclose(local_file);
			local_file = NULL;
			smb_close_read(copy_file);
			copy_file = NULL;
			progress.files_copied++;
			copy_index++;
			startNextCopyFile();
		}
		return state;
	}

	if (state == SYNC_STATE_DELETING) {
		if (delete_index >= to_delete_count) {
			endSync(SYNC_STATE_DONE, "ok");
			return state;
		}

		char local_rel[MAX_PATH];
		browse_path_push(local_rel, active_job->local_path, to_delete[delete_index].rel_path);
		char local_full[MAX_PATH];
		snprintf(local_full, sizeof(local_full), "%s/%s", SDCARD_PATH, local_rel);
		remove(local_full);

		progress.files_deleted++;
		delete_index++;
		if (delete_index >= to_delete_count) endSync(SYNC_STATE_DONE, "ok");
		return state;
	}

	return state; // IDLE or already terminal: nothing to do
}

void sync_engine_cancel(void)
{
	if (state != SYNC_STATE_COPYING && state != SYNC_STATE_DELETING) return;
	endSync(SYNC_STATE_CANCELLED, NULL);
}

SyncState sync_engine_state(void)
{
	return state;
}

SyncProgress sync_engine_progress(void)
{
	return progress;
}

SyncPreview sync_engine_totals(void)
{
	SyncPreview totals = {0};
	totals.ok = true;
	totals.to_copy_count = to_copy_count;
	totals.to_copy_bytes = to_copy_bytes;
	totals.to_delete_count = to_delete_count;
	totals.already_present_count = already_present_count;
	return totals;
}
