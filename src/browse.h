#ifndef BROWSE_H
#define BROWSE_H

#include <stdbool.h>

// Shared by both sides of a link: the file entry produced by a recursive
// listing (smb_client.c's smb_list_files_recursive() / local_fs.c's
// local_list_files_recursive()) and small path helpers.

#define BROWSE_STR_MAX 256

// Maximum files per side of a link. Listing more fails the link with an
// explicit error rather than truncating (see smb_client.h).
#define BROWSE_MAX_FILES 4096

// One file found anywhere in the recursed subtree, with rel_path relative
// to the recursion root (e.g. "covers/foo.png") so a remote listing and a
// local listing can be compared by path regardless of each root's absolute
// path.
typedef struct {
	char rel_path[BROWSE_STR_MAX];
	long long size;
} BrowseFileEntry;

// Appends name as a new path segment onto base ("" meaning root) into out,
// inserting a "/" separator unless base is empty. out must be a distinct
// buffer of at least BROWSE_STR_MAX bytes.
void browse_path_push(char *out, const char *base, const char *name);

// Truncates path to its parent directory in place (removes the last "/"
// segment). Returns false and leaves path untouched if it has no "/".
bool browse_path_pop(char *path);

#endif
