#ifndef SMB_CLIENT_H
#define SMB_CLIENT_H

#include "servers.h"
#include "browse.h"

// Thin wrapper around libsmb2. smb_connect/smb_disconnect/smb_list/
// smb_list_files_recursive exist so far (enough for écran 1bis's "Tester la
// connexion", écran 2b's remote browser and écran 3bis's sync preview) --
// smb_open_read/smb_read_chunk (see docs/ARCHITECTURE.md) will be added
// when sync_engine.c needs to actually copy files, not just diff them.

typedef struct SmbSession SmbSession;

typedef enum {
	SMB_OK = 0,
	SMB_ERR_FAILED, // couldn't connect/authenticate/tree-connect/list
} SmbError;

// Connects to server->host:server->port and tree-connects server->share,
// authenticating with server->username/password/domain (NTLM only, per
// docs/ARCHITECTURE.md -- Kerberos is disabled in this build). Bounded by
// timeout_seconds. On failure returns NULL and sets *out_error; on success
// the returned session must be released with smb_disconnect().
SmbSession *smb_connect(const Server *server, int timeout_seconds, SmbError *out_error);
void smb_disconnect(SmbSession *session);

// Lists the subfolders directly under remote_path (share-relative, ""
// meaning the share root) into out[0..max_entries). Files are skipped --
// écran 2b only ever picks a folder, never a file. Returns the number of
// entries listed (0..max_entries) on success, or -1 on failure (*out_error
// set); silently truncates past max_entries.
int smb_list(SmbSession *session, const char *remote_path, BrowseEntry *out, int max_entries, SmbError *out_error);

// Recursively lists every file (not directory) under remote_path, walking
// the full subtree -- used by sync_engine.c to diff against a local tree
// for écran 3bis. Each out[].rel_path is relative to remote_path itself,
// matching local_fs.c's local_list_files_recursive() output shape so the
// two can be compared by path. Blocking: one smb2_opendir/readdir pass per
// subfolder found, run synchronously (see sync_engine.c's top comment for
// why this isn't the tick-based incremental design docs/ARCHITECTURE.md
// sketches). Returns the number of files found (may be 0) on success, or -1
// if any directory in the subtree fails to list (*out_error set); silently
// truncates past max_entries.
int smb_list_files_recursive(SmbSession *session, const char *remote_path, BrowseFileEntry *out, int max_entries, SmbError *out_error);

#endif
