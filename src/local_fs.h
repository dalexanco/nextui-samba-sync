#ifndef LOCAL_FS_H
#define LOCAL_FS_H

#include <stdbool.h>

#include "browse.h"

// Thin wrapper around the local filesystem calls sync_engine.c needs, all
// relative to SDCARD_PATH. Mirrors smb_client.c's shape so both sides of a
// link are listed the same way.

// Recursively lists every file under SDCARD_PATH/path, walking the full
// subtree (dotfiles/.disabled entries hidden per NextUI's hide() rule,
// utils.h). Each out[].rel_path is relative to path itself, matching
// smb_client.c's smb_list_files_recursive() output shape so the two can be
// compared by path. Returns the number of files found (may be 0) on
// success, or -1 on failure (path unreadable, or more than max_entries
// files -- never silently truncated).
int local_list_files_recursive(const char *path, BrowseFileEntry *out, int max_entries);

// Creates SDCARD_PATH/relative_path and any missing parent directories
// (mkdir -p). relative_path=="" is a no-op (SDCARD_PATH itself always
// exists). Returns false only if a directory create fails for a reason other
// than already existing.
bool local_ensure_dir(const char *relative_path);

#endif
