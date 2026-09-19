#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "defines.h"
#include "smb_client.h"
#include "local_fs.h"
#include "sync_engine.h"

#define COPY_CHUNK_SIZE (64 * 1024)
#define TICK_BUDGET_MS 30

// Listings scratch space and the diff being executed. Static rather than
// stack: several MB at BROWSE_MAX_FILES.
static BrowseFileEntry remote_files[BROWSE_MAX_FILES];
static BrowseFileEntry local_files[BROWSE_MAX_FILES];
static BrowseFileEntry to_copy[BROWSE_MAX_FILES];
static int to_copy_count = 0;
static long long to_copy_bytes = 0;
static BrowseFileEntry to_delete[BROWSE_MAX_FILES];
static int to_delete_count = 0;

static SyncState state = SYNC_STATE_IDLE;
static const Link *active_link = NULL;
static SmbSession *session = NULL;
static SmbFile *copy_file = NULL;
static FILE *local_file = NULL;
static char temp_path[MAX_PATH]; // .part file being written, "" if none
static int copy_index = 0;
static int delete_index = 0;
static long long current_file_bytes = 0;
static long long bytes_processed = 0; // completed or failed files only
static bool link_failed = false; // a link-wide error ended the sync
static SyncProgress progress;
static LinkState result;

static void joinPath(char *out, size_t out_size, const char *a, const char *b)
{
	if (a[0] && b[0]) snprintf(out, out_size, "%s/%s", a, b);
	else snprintf(out, out_size, "%s", a[0] ? a : b);
}

// SDCARD_PATH/<link local>/<rel>. False if it doesn't fit: a truncated path
// would point at another file.
static bool localFullPath(char *out, size_t out_size, const char *rel)
{
	char rel_to_sd[MAX_PATH];
	joinPath(rel_to_sd, sizeof(rel_to_sd), active_link->local, rel);
	return (size_t)snprintf(out, out_size, "%s/%s", SDCARD_PATH, rel_to_sd) < out_size;
}

static const char *localErrorLabel(int err)
{
	switch (err) {
	case ENOSPC: return "Espace disque insuffisant";
	case EACCES:
	case EPERM:
	case EROFS: return "Écriture refusée";
	case ENAMETOOLONG: return "Nom de fichier trop long";
	}
	return "Écriture impossible";
}

static int compareEntries(const void *a, const void *b)
{
	return strcmp(((const BrowseFileEntry *)a)->rel_path, ((const BrowseFileEntry *)b)->rel_path);
}

// Connects, lists both sides and fills to_copy[]/to_delete[]. On success,
// hands the still-open session to *out_session if non-NULL (the sync reuses
// it), else disconnects.
static LinkCheck runCheck(const Link *link, SmbSession **out_session)
{
	LinkCheck check = {0};
	to_copy_count = 0;
	to_copy_bytes = 0;
	to_delete_count = 0;

	SmbError error;
	SmbSession *s = smb_connect(link->server, link->share, sync_config_timeout(), &error);
	if (!s) {
		snprintf(check.message, sizeof(check.message), "%s", smb_error_label(error));
		return check;
	}

	int remote_count = smb_list_files_recursive(s, link->remote, remote_files, BROWSE_MAX_FILES, &error);
	if (remote_count < 0) {
		smb_disconnect(s);
		snprintf(check.message, sizeof(check.message), "%s", smb_error_label(error));
		return check;
	}

	// A local folder that doesn't exist yet just means "never synced":
	// everything is to copy.
	char local_root[MAX_PATH];
	snprintf(local_root, sizeof(local_root), "%s/%s", SDCARD_PATH, link->local);
	struct stat st;
	int local_count = 0;
	if (stat(local_root, &st) == 0) {
		if (!S_ISDIR(st.st_mode)) {
			smb_disconnect(s);
			snprintf(check.message, sizeof(check.message), "Le chemin local n'est pas un dossier");
			return check;
		}
		local_count = local_list_files_recursive(link->local, local_files, BROWSE_MAX_FILES);
		if (local_count < 0) {
			smb_disconnect(s);
			snprintf(check.message, sizeof(check.message), "Dossier local illisible ou trop gros");
			return check;
		}
	}

	// Both sides sorted by path, then merged in one pass.
	qsort(remote_files, remote_count, sizeof(BrowseFileEntry), compareEntries);
	qsort(local_files, local_count, sizeof(BrowseFileEntry), compareEntries);

	bool mirror = link->mode == LINK_MODE_MIRROR;
	int r = 0, l = 0;
	while (r < remote_count || l < local_count) {
		int cmp = (r >= remote_count) ? 1
			: (l >= local_count) ? -1
			: strcmp(remote_files[r].rel_path, local_files[l].rel_path);

		if (cmp < 0) { // remote only
			to_copy[to_copy_count++] = remote_files[r];
			to_copy_bytes += remote_files[r].size;
			r++;
		}
		else if (cmp > 0) { // local only
			if (mirror) to_delete[to_delete_count++] = local_files[l];
			l++;
		}
		else {
			if (remote_files[r].size != local_files[l].size) {
				to_copy[to_copy_count++] = remote_files[r];
				to_copy_bytes += remote_files[r].size;
			}
			r++;
			l++;
		}
	}

	check.ok = true;
	check.to_copy_count = to_copy_count;
	check.to_copy_bytes = to_copy_bytes;
	check.to_delete_count = to_delete_count;

	if (out_session) *out_session = s;
	else smb_disconnect(s);
	return check;
}

LinkCheck sync_engine_check(const Link *link)
{
	return runCheck(link, NULL);
}

static void fileError(const char *rel_path, const char *reason)
{
	link_state_add_error(&result, rel_path, reason);
}

static void linkError(const char *reason)
{
	link_failed = true;
	link_state_add_error(&result, "", reason);
}

// Drops the file being copied: closes both ends and removes its .part.
static void abortCurrentFile(void)
{
	if (local_file) { fclose(local_file); local_file = NULL; }
	if (copy_file) { smb_close_read(copy_file); copy_file = NULL; }
	if (temp_path[0]) { remove(temp_path); temp_path[0] = '\0'; }
	current_file_bytes = 0;
	progress.current_file[0] = '\0';
}

// Shared terminal path: finishing normally, on a link-wide error, or
// cancelled.
static void finish(SyncState end_state)
{
	abortCurrentFile();
	if (session) { smb_disconnect(session); session = NULL; }

	result.time = (int)time(NULL);
	result.files_deleted = progress.files_deleted;
	if (end_state == SYNC_STATE_CANCELLED) result.status = LINK_STATUS_CANCELLED;
	else if (link_failed) result.status = LINK_STATUS_ERROR;
	else if (result.error_total > 0) result.status = LINK_STATUS_PARTIAL;
	else result.status = LINK_STATUS_OK;

	state = end_state;
}

// Moves past a file that's done with (copied or failed).
static void nextFile(void)
{
	bytes_processed += to_copy[copy_index].size;
	current_file_bytes = 0;
	copy_index++;
	progress.files_done = copy_index;
}

static void endCopyPhase(void)
{
	progress.current_file[0] = '\0';
	if (active_link->mode != LINK_MODE_MIRROR || to_delete_count == 0) {
		finish(SYNC_STATE_DONE);
	}
	else if (result.error_total > 0) {
		// SPEC.md: mirror deletions only once every copy has succeeded.
		fileError("", "Suppressions Miroir non effectuées (erreurs de copie)");
		finish(SYNC_STATE_DONE);
	}
	else {
		state = SYNC_STATE_DELETING;
	}
}

// Opens the next file of to_copy[] that can be opened on both ends,
// recording a per-file error for each one that can't. Ends the copy phase
// when none are left.
static void startNextCopyFile(void)
{
	while (copy_index < to_copy_count) {
		const char *rel = to_copy[copy_index].rel_path;

		char remote_path[MAX_PATH];
		joinPath(remote_path, sizeof(remote_path), active_link->remote, rel);

		char local_rel[MAX_PATH];
		joinPath(local_rel, sizeof(local_rel), active_link->local, rel);
		char local_dir[MAX_PATH];
		snprintf(local_dir, sizeof(local_dir), "%s", local_rel);
		const char *base = strrchr(local_dir, '/');
		base = base ? base + 1 : local_dir;
		char base_name[MAX_PATH];
		snprintf(base_name, sizeof(base_name), "%s", base);
		if (!browse_path_pop(local_dir)) local_dir[0] = '\0';

		if (!local_ensure_dir(local_dir)) {
			fileError(rel, localErrorLabel(errno));
			nextFile();
			continue;
		}

		SmbError error;
		copy_file = smb_open_read(session, remote_path, &error);
		if (!copy_file) {
			if (error == SMB_ERR_UNREACHABLE) { // no answer at all: the connection is gone
				linkError("Connexion perdue");
				finish(SYNC_STATE_DONE);
				return;
			}
			fileError(rel, smb_error_label(error));
			nextFile();
			continue;
		}

		// Hidden (dot-prefixed) so NextUI never lists it, and ignored by
		// local listings if a crash ever leaves one behind.
		int len = local_dir[0]
			? snprintf(temp_path, sizeof(temp_path), "%s/%s/.%s.part", SDCARD_PATH, local_dir, base_name)
			: snprintf(temp_path, sizeof(temp_path), "%s/.%s.part", SDCARD_PATH, base_name);
		char final_path[MAX_PATH];
		bool fits = (size_t)len < sizeof(temp_path) && localFullPath(final_path, sizeof(final_path), rel);
		local_file = fits ? fopen(temp_path, "wb") : NULL;
		if (!local_file) {
			fileError(rel, localErrorLabel(fits ? errno : ENAMETOOLONG));
			temp_path[0] = '\0';
			abortCurrentFile();
			nextFile();
			continue;
		}

		snprintf(progress.current_file, sizeof(progress.current_file), "%s", rel);
		return;
	}

	endCopyPhase();
}

void sync_engine_start(const Link *link)
{
	active_link = link;
	copy_index = 0;
	delete_index = 0;
	current_file_bytes = 0;
	bytes_processed = 0;
	link_failed = false;
	temp_path[0] = '\0';
	memset(&progress, 0, sizeof(progress));
	memset(&result, 0, sizeof(result));

	LinkCheck check = runCheck(link, &session);
	if (!check.ok) {
		linkError(check.message);
		finish(SYNC_STATE_DONE);
		return;
	}
	progress.to_copy_count = check.to_copy_count;
	progress.to_copy_bytes = check.to_copy_bytes;
	progress.to_delete_count = check.to_delete_count;

	if (!local_ensure_dir(link->local)) {
		linkError("Dossier local impossible à créer");
		finish(SYNC_STATE_DONE);
		return;
	}

	state = SYNC_STATE_COPYING;
	startNextCopyFile();
}

static void copyStep(void)
{
	static uint8_t buf[COPY_CHUNK_SIZE];
	const char *rel = to_copy[copy_index].rel_path;

	int n = smb_read_chunk(copy_file, buf, sizeof(buf));
	if (n < 0) {
		linkError("Connexion perdue");
		finish(SYNC_STATE_DONE);
		return;
	}
	if (n > 0) {
		if (fwrite(buf, 1, (size_t)n, local_file) != (size_t)n) {
			fileError(rel, localErrorLabel(errno));
			abortCurrentFile();
			nextFile();
			startNextCopyFile();
			return;
		}
		current_file_bytes += n;
		return;
	}

	// n == 0: end of the remote file.
	int close_rc = fclose(local_file);
	int close_errno = errno;
	local_file = NULL;
	smb_close_read(copy_file);
	copy_file = NULL;

	char final_path[MAX_PATH];
	localFullPath(final_path, sizeof(final_path), rel); // already checked when the file was opened
	if (close_rc != 0) {
		fileError(rel, localErrorLabel(close_errno));
		abortCurrentFile();
	}
	else if (rename(temp_path, final_path) != 0) {
		fileError(rel, localErrorLabel(errno));
		abortCurrentFile();
	}
	else {
		temp_path[0] = '\0';
		result.files_copied++;
		result.bytes_copied += current_file_bytes;
	}
	nextFile();
	startNextCopyFile();
}

// Removes the now-empty parent folders of a deleted file, up to (not
// including) the link's local root. Stops at the first non-empty one.
static void removeEmptyParents(const char *rel_path)
{
	char dir[MAX_PATH];
	snprintf(dir, sizeof(dir), "%s", rel_path);
	while (browse_path_pop(dir)) {
		char full[MAX_PATH];
		if (!localFullPath(full, sizeof(full), dir) || rmdir(full) != 0) break;
	}
}

static void deleteStep(void)
{
	const char *rel = to_delete[delete_index].rel_path;
	char full[MAX_PATH];
	if (!localFullPath(full, sizeof(full), rel)) {
		fileError(rel, localErrorLabel(ENAMETOOLONG));
	}
	else if (remove(full) != 0 && errno != ENOENT) {
		fileError(rel, "Suppression impossible");
	}
	else {
		progress.files_deleted++;
		removeEmptyParents(rel);
	}

	delete_index++;
	if (delete_index >= to_delete_count) finish(SYNC_STATE_DONE);
}

static long long nowMs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

SyncState sync_engine_tick(void)
{
	long long start = nowMs();
	do {
		if (state == SYNC_STATE_COPYING) copyStep();
		else if (state == SYNC_STATE_DELETING) deleteStep();
		else break;
	} while (nowMs() - start < TICK_BUDGET_MS);

	progress.bytes_done = bytes_processed + current_file_bytes;
	return state;
}

void sync_engine_cancel(void)
{
	if (state != SYNC_STATE_COPYING && state != SYNC_STATE_DELETING) return;
	finish(SYNC_STATE_CANCELLED);
}

SyncState sync_engine_state(void)
{
	return state;
}

SyncProgress sync_engine_progress(void)
{
	return progress;
}

const LinkState *sync_engine_result(void)
{
	return &result;
}
