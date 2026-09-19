#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "utils.h"
#include "smb_client.h"

struct SmbSession {
	struct smb2_context *smb2;
};

// Keeps its own smb2 context pointer (rather than requiring callers to pass
// the owning SmbSession back into smb_read_chunk()/smb_close_read()) since
// smb2_read()/smb2_close() only need the context, not the session wrapper.
struct SmbFile {
	struct smb2_context *smb2;
	struct smb2fh *fh;
};

const char *smb_error_label(SmbError error)
{
	switch (error) {
	case SMB_OK: return "";
	case SMB_ERR_UNREACHABLE: return "Server unreachable";
	case SMB_ERR_AUTH: return "Authentication refused";
	case SMB_ERR_SHARE_NOT_FOUND: return "Share not found";
	case SMB_ERR_PATH_NOT_FOUND: return "Remote folder not found";
	case SMB_ERR_ACCESS_DENIED: return "Access denied";
	case SMB_ERR_TOO_MANY_FILES: return "Too many files in the remote folder";
	case SMB_ERR_FAILED: break;
	}
	return "SMB error";
}

// Maps the NT status of the last failed libsmb2 call. A failure with no NT
// status at all means we never got an answer from the server.
static SmbError lastError(struct smb2_context *smb2)
{
	uint32_t status = (uint32_t)smb2_get_nterror(smb2);
	switch (status) {
	case 0: return SMB_ERR_UNREACHABLE;
	case SMB2_STATUS_LOGON_FAILURE:
	case SMB2_STATUS_ACCOUNT_RESTRICTION:
	case SMB2_STATUS_ACCOUNT_DISABLED:
	case SMB2_STATUS_WRONG_PASSWORD:
	case SMB2_STATUS_PASSWORD_EXPIRED:
		return SMB_ERR_AUTH;
	case SMB2_STATUS_BAD_NETWORK_NAME: return SMB_ERR_SHARE_NOT_FOUND;
	case SMB2_STATUS_OBJECT_NAME_NOT_FOUND:
	case SMB2_STATUS_OBJECT_PATH_NOT_FOUND:
	case SMB2_STATUS_NO_SUCH_FILE:
		return SMB_ERR_PATH_NOT_FOUND;
	case SMB2_STATUS_ACCESS_DENIED: return SMB_ERR_ACCESS_DENIED;
	}
	return SMB_ERR_FAILED;
}

SmbSession *smb_connect(const Server *server, const char *share, int timeout_seconds, SmbError *out_error)
{
	struct smb2_context *smb2 = smb2_init_context();
	if (!smb2) {
		*out_error = SMB_ERR_FAILED;
		return NULL;
	}

	smb2_set_timeout(smb2, timeout_seconds);
	smb2_set_authentication(smb2, SMB2_SEC_NTLMSSP);
	smb2_set_password(smb2, server->password);
	smb2_set_domain(smb2, server->domain);

	char host[CONFIG_STR_MAX + 16];
	snprintf(host, sizeof(host), "%s:%d", server->host, server->port);

	const char *user = server->username[0] ? server->username : NULL;

	if (smb2_connect_share(smb2, host, share, user) != 0) {
		*out_error = lastError(smb2);
		smb2_destroy_context(smb2);
		return NULL;
	}

	SmbSession *session = malloc(sizeof(SmbSession));
	session->smb2 = smb2;
	*out_error = SMB_OK;
	return session;
}

void smb_disconnect(SmbSession *session)
{
	if (!session) return;
	smb2_disconnect_share(session->smb2);
	smb2_destroy_context(session->smb2);
	free(session);
}

// One entry of a directory, copied out of libsmb2's own storage.
typedef struct {
	char name[BROWSE_STR_MAX];
	bool is_dir;
	long long size;
} DirEntry;

// dir_path is share-relative (what smb2_opendir needs); rel_prefix is
// relative to the recursion root (what ends up in out[].rel_path). They
// diverge as soon as the recursion root isn't the share root, which is the
// common case (link->remote is normally a subfolder).
//
// A directory is read completely, and closed, BEFORE descending into its
// subfolders: opening another directory on the same smb2 context while a
// readdir loop is in flight makes that loop drop the entries it hadn't
// returned yet (observed against a real NAS -- the same share listed 272
// files once, then 110, silently). A short listing is not a cosmetic bug
// here: mirror mode deletes whatever it doesn't see remotely.
static void smbListRecurse(struct smb2_context *smb2, const char *dir_path, const char *rel_prefix,
                            BrowseFileEntry *out, int max_entries, int *count, SmbError *error)
{
	struct smb2dir *dir = smb2_opendir(smb2, dir_path);
	if (!dir) {
		*error = lastError(smb2);
		return;
	}

	int capacity = 64;
	int entry_count = 0;
	DirEntry *entries = malloc(capacity * sizeof(*entries));
	if (!entries) {
		smb2_closedir(smb2, dir);
		*error = SMB_ERR_FAILED;
		return;
	}

	struct smb2dirent *entry;
	while ((entry = smb2_readdir(smb2, dir))) {
		if (hide((char *)entry->name)) continue; // also covers "." and ".."
		if (entry->st.smb2_type != SMB2_TYPE_DIRECTORY && entry->st.smb2_type != SMB2_TYPE_FILE) continue;

		if (entry_count == capacity) {
			DirEntry *grown = realloc(entries, capacity * 2 * sizeof(*entries));
			if (!grown) {
				free(entries);
				smb2_closedir(smb2, dir);
				*error = SMB_ERR_FAILED;
				return;
			}
			entries = grown;
			capacity *= 2;
		}

		snprintf(entries[entry_count].name, BROWSE_STR_MAX, "%s", entry->name);
		entries[entry_count].is_dir = entry->st.smb2_type == SMB2_TYPE_DIRECTORY;
		entries[entry_count].size = (long long)entry->st.smb2_size;
		entry_count++;
	}
	smb2_closedir(smb2, dir);

	for (int i = 0; i < entry_count && *error == SMB_OK; i++) {
		char child_path[BROWSE_STR_MAX];
		browse_path_push(child_path, dir_path, entries[i].name);
		char child_rel[BROWSE_STR_MAX];
		browse_path_push(child_rel, rel_prefix, entries[i].name);

		if (entries[i].is_dir) {
			smbListRecurse(smb2, child_path, child_rel, out, max_entries, count, error);
		}
		else if (*count >= max_entries) {
			*error = SMB_ERR_TOO_MANY_FILES;
		}
		else {
			snprintf(out[*count].rel_path, BROWSE_STR_MAX, "%s", child_rel);
			out[*count].size = entries[i].size;
			(*count)++;
		}
	}

	free(entries);
}

int smb_list_files_recursive(SmbSession *session, const char *remote_path, BrowseFileEntry *out, int max_entries, SmbError *out_error)
{
	int count = 0;
	*out_error = SMB_OK;
	smbListRecurse(session->smb2, remote_path, "", out, max_entries, &count, out_error);
	return *out_error == SMB_OK ? count : -1;
}

SmbFile *smb_open_read(SmbSession *session, const char *remote_path, SmbError *out_error)
{
	struct smb2fh *fh = smb2_open(session->smb2, remote_path, O_RDONLY);
	if (!fh) {
		*out_error = lastError(session->smb2);
		return NULL;
	}

	SmbFile *file = malloc(sizeof(SmbFile));
	file->smb2 = session->smb2;
	file->fh = fh;
	*out_error = SMB_OK;
	return file;
}

int smb_read_chunk(SmbFile *file, uint8_t *buf, uint32_t buf_size)
{
	int n = smb2_read(file->smb2, file->fh, buf, buf_size);
	return n < 0 ? -1 : n;
}

void smb_close_read(SmbFile *file)
{
	if (!file) return;
	smb2_close(file->smb2, file->fh);
	free(file);
}
