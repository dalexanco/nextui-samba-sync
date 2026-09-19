#ifndef SMB_CLIENT_H
#define SMB_CLIENT_H

#include <stdint.h>

#include "sync_config.h"
#include "browse.h"

// Thin wrapper around libsmb2: connect, recursively list a remote folder,
// and read files in small chunks -- everything sync_engine.c needs to check
// and sync a link.

typedef struct SmbSession SmbSession;

// An open remote file, read sequentially in small chunks by
// sync_engine_tick() so a single file's transfer never blocks the UI thread
// for more than one chunk at a time (see docs/ARCHITECTURE.md).
typedef struct SmbFile SmbFile;

typedef enum {
	SMB_OK = 0,
	SMB_ERR_UNREACHABLE,     // no answer from host:port (down, wrong IP, other network, timeout)
	SMB_ERR_AUTH,            // server refused the credentials
	SMB_ERR_SHARE_NOT_FOUND, // no such share on this server
	SMB_ERR_PATH_NOT_FOUND,  // remote folder/file doesn't exist
	SMB_ERR_ACCESS_DENIED,   // exists but this user can't read it
	SMB_ERR_TOO_MANY_FILES,  // remote folder has more than max_entries files
	SMB_ERR_FAILED,          // anything else
} SmbError;

// Short label for an error, as shown on the main screen
// ("Server unreachable").
const char *smb_error_label(SmbError error);

// Connects to server->host:server->port and tree-connects `share`,
// authenticating with server->username/password/domain (NTLM or guest --
// Kerberos is disabled in this build). Bounded by timeout_seconds. On
// failure returns NULL and sets *out_error; on success the returned session
// must be released with smb_disconnect().
SmbSession *smb_connect(const Server *server, const char *share, int timeout_seconds, SmbError *out_error);
void smb_disconnect(SmbSession *session);

// Recursively lists every file (not directory) under remote_path
// (share-relative, "" = share root). Each out[].rel_path is relative to
// remote_path itself, matching local_list_files_recursive()'s output so the
// two can be compared by path. Hidden entries (same hide() rule as the local
// side: dotfiles, *.disabled, map.txt) are skipped, otherwise a remote
// .DS_Store would look "new" forever. Blocking. Returns the number of files
// found on success, or -1 with *out_error set -- including
// SMB_ERR_TOO_MANY_FILES rather than silently truncating, since a partial
// listing would make mirror mode delete files that do exist remotely.
int smb_list_files_recursive(SmbSession *session, const char *remote_path, BrowseFileEntry *out, int max_entries, SmbError *out_error);

// Opens remote_path (share-relative) for sequential reading. NULL + *out_error
// on failure; on success the handle must be released with smb_close_read().
SmbFile *smb_open_read(SmbSession *session, const char *remote_path, SmbError *out_error);

// Reads the next up-to-buf_size bytes from file, continuing from wherever
// the previous call left off. Returns the number of bytes read (possibly
// less than buf_size before the end), 0 at end of file, or -1 on error.
int smb_read_chunk(SmbFile *file, uint8_t *buf, uint32_t buf_size);

void smb_close_read(SmbFile *file);

#endif
