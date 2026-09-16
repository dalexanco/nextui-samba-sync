#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "defines.h"
#include "servers.h"
#include "smb_client.h"
#include "local_fs.h"
#include "browse.h"
#include "sync_engine.h"

#define SYNC_TIMEOUT_SECONDS 10

SyncPreview sync_engine_preview(const Job *job)
{
	SyncPreview preview = {0};

	const Server *server = servers_find(job->server);
	if (!server) return preview;

	SmbError error;
	SmbSession *session = smb_connect(server, SYNC_TIMEOUT_SECONDS, &error);
	if (!session) return preview;

	static BrowseFileEntry remote_files[BROWSE_MAX_FILES];
	static BrowseFileEntry local_files[BROWSE_MAX_FILES];

	int remote_count = smb_list_files_recursive(session, job->remote_path, remote_files, BROWSE_MAX_FILES, &error);
	smb_disconnect(session);
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
		if (!present) {
			preview.to_copy_count++;
			preview.to_copy_bytes += remote_files[i].size;
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
			if (!present) preview.to_delete_count++;
		}
	}

	preview.ok = true;
	return preview;
}
