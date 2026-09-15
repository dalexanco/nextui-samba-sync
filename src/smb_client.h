#ifndef SMB_CLIENT_H
#define SMB_CLIENT_H

#include "servers.h"

// Thin wrapper around libsmb2. Only smb_connect/smb_disconnect exist for
// now (enough for écran 1bis's "Tester la connexion") -- smb_list/
// smb_open_read/smb_read_chunk (see docs/ARCHITECTURE.md) will be added
// when browse.c/sync_engine.c need them.

typedef struct SmbSession SmbSession;

typedef enum {
	SMB_OK = 0,
	SMB_ERR_FAILED, // couldn't connect/authenticate/tree-connect
} SmbError;

// Connects to server->host:server->port and tree-connects server->share,
// authenticating with server->username/password/domain (NTLM only, per
// docs/ARCHITECTURE.md -- Kerberos is disabled in this build). Bounded by
// timeout_seconds. On failure returns NULL and sets *out_error; on success
// the returned session must be released with smb_disconnect().
SmbSession *smb_connect(const Server *server, int timeout_seconds, SmbError *out_error);
void smb_disconnect(SmbSession *session);

#endif
