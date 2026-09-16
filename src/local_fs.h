#ifndef LOCAL_FS_H
#define LOCAL_FS_H

#include <stdbool.h>

#include "browse.h"

// Thin wrapper around the local filesystem calls écran 3 (parcourir la SD)
// needs -- opendir/readdir/mkdir under SDCARD_PATH. Mirrors smb_client.c's
// shape (a couple of narrow functions, no BrowseBackend interface -- see
// browse.h) so job_wizard.c treats both browse steps the same way. No error
// enum here: every path this operates on is either SDCARD_PATH itself or a
// path this same module just listed, so failure only means "SD card pulled
// mid-browse" or a permissions oddity, and callers just show a generic
// error string.

// Lists the subfolders directly under SDCARD_PATH/path (path=="" meaning
// the SD card root) into out[0..max_entries). Dotfiles/.disabled entries
// are hidden per NextUI's own hide() convention (utils.h) -- same rule the
// stock file browser uses, so e.g. .system/.userdata never show up here.
// Files are skipped -- écran 3 only ever picks a folder. Returns the number
// of entries listed on success, or -1 on failure (path missing/unreadable).
int local_list(const char *path, BrowseEntry *out, int max_entries);

// Creates a new subfolder under SDCARD_PATH/parent_path named base_name,
// or "base_name (2)", "base_name (3)", ... on collision. Writes the actual
// created name into out_name (size BROWSE_STR_MAX) and returns true on
// success; returns false if base_name and all 999 numbered variants are
// already taken, or the create fails for any other reason.
bool local_create_folder(const char *parent_path, const char *base_name, char *out_name);

#endif
