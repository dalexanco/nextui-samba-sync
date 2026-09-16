#ifndef BROWSE_H
#define BROWSE_H

#include <stdbool.h>

// Folder-navigation entry, shared by écran 2b (browse the remote SMB share,
// screens/job_wizard.c, backed by smb_list() in smb_client.c) and écran 3
// (browse the local SD, backed by local_list() in local_fs.c). Only the
// entry type and the path helpers below are factored out for now: a full
// backend-interface abstraction (per docs/ARCHITECTURE.md) isn't worth the
// indirection until a second caller actually needs it -- same reasoning as
// ui.h's helpers, factored out once two screens need the exact same code.

#define BROWSE_STR_MAX 256
#define BROWSE_MAX_ENTRIES 256

typedef struct {
	char name[BROWSE_STR_MAX];
	bool is_dir;
} BrowseEntry;

// One file found by a recursive listing (smb_client.c's
// smb_list_files_recursive() / local_fs.c's local_list_files_recursive()),
// used by sync_engine.c to diff a remote tree against a local one for écran
// 3bis. Unlike BrowseEntry above (one directory level, dirs only, meant for
// on-screen navigation) this is one file found anywhere in the recursed
// subtree, with rel_path relative to the recursion root (e.g.
// "covers/foo.png") so a remote listing and a local listing can be compared
// by path regardless of what each root's absolute path happens to be.
#define BROWSE_MAX_FILES 4096

typedef struct {
	char rel_path[BROWSE_STR_MAX];
	long long size;
} BrowseFileEntry;

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
