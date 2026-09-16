#ifndef SMB_CLIENT_H
#define SMB_CLIENT_H

#include "servers.h"
#include "browse.h"

// Thin wrapper around libsmb2. smb_connect/smb_disconnect/smb_list exist
// so far (enough for écran 1bis's "Tester la connexion" and écran 2b's
// remote browser) -- smb_open_read/smb_read_chunk (see
// docs/ARCHITECTURE.md) will be added when sync_engine.c needs them.

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

#endif
