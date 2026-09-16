#ifndef BROWSE_H
#define BROWSE_H

#include <stdbool.h>

// Folder-navigation entry, shared by écran 2b (browse the remote SMB share,
// screens/job_wizard.c, backed by smb_list() in smb_client.c) and, later,
// écran 3 (browse the local SD -- not yet implemented). Only the entry type
// and the path helpers below are factored out for now: a full
// backend-interface abstraction (per docs/ARCHITECTURE.md) isn't worth the
// indirection until a second caller actually needs it -- same reasoning as
// ui.h's helpers, factored out once two screens need the exact same code.

#define BROWSE_STR_MAX 256
#define BROWSE_MAX_ENTRIES 256

typedef struct {
	char name[BROWSE_STR_MAX];
	bool is_dir;
} BrowseEntry;

// Appends name as a new path segment onto base (a backend-relative path,
// "" meaning root) into out, inserting a "/" separator unless base is
// empty. out must be a distinct buffer of at least BROWSE_STR_MAX bytes
// (may alias base's storage only if out != base).
void browse_path_push(char *out, const char *base, const char *name);

// Truncates path to its parent directory in place (removes the last "/"
// segment). Returns false and leaves path untouched if it's already at
// root (no "/" found) -- callers use that to know when "B" should exit the
// browser instead of going up a level.
bool browse_path_pop(char *path);

// Sorts entries directories-first, then alphabetically within each group --
// matches the écran 2b/3 mockups in SPEC.md.
void browse_sort(BrowseEntry *entries, int count);

#endif
