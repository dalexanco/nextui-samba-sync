#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include "defines.h"
#include "utils.h"
#include "local_fs.h"

static void fullPath(char *out, size_t out_size, const char *relative_path)
{
	if (relative_path[0]) snprintf(out, out_size, "%s/%s", SDCARD_PATH, relative_path);
	else snprintf(out, out_size, "%s", SDCARD_PATH);
}

// dir_path is SDCARD_PATH-relative (what fullPath() needs); rel_prefix is
// relative to the recursion root (what ends up in out[].rel_path). Mirrors
// smb_client.c's smbListRecurse() -- same split, same reason.
static void localListRecurse(const char *dir_path, const char *rel_prefix,
                              BrowseFileEntry *out, int max_entries, int *count, bool *had_error)
{
	char full[BROWSE_STR_MAX + 64];
	fullPath(full, sizeof(full), dir_path);

	DIR *dir = opendir(full);
	if (!dir) {
		*had_error = true;
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(dir))) {
		if (hide(entry->d_name)) continue;

		char child_path[BROWSE_STR_MAX];
		browse_path_push(child_path, dir_path, entry->d_name);
		char child_rel[BROWSE_STR_MAX];
		browse_path_push(child_rel, rel_prefix, entry->d_name);

		if (entry->d_type == DT_DIR) {
			localListRecurse(child_path, child_rel, out, max_entries, count, had_error);
			if (*had_error) break;
		}
		else if (entry->d_type == DT_REG) {
			if (*count >= max_entries) {
				*had_error = true;
				break;
			}
			char child_full[BROWSE_STR_MAX + 64];
			fullPath(child_full, sizeof(child_full), child_path);
			struct stat st;
			out[*count].size = (stat(child_full, &st) == 0) ? (long long)st.st_size : 0;
			snprintf(out[*count].rel_path, BROWSE_STR_MAX, "%s", child_rel);
			(*count)++;
		}
	}

	closedir(dir);
}

int local_list_files_recursive(const char *path, BrowseFileEntry *out, int max_entries)
{
	int count = 0;
	bool had_error = false;
	localListRecurse(path, "", out, max_entries, &count, &had_error);
	if (had_error) return -1;
	return count;
}

bool local_ensure_dir(const char *relative_path)
{
	if (!relative_path[0]) return true;

	char path[BROWSE_STR_MAX + 64];
	fullPath(path, sizeof(path), relative_path);

	for (char *p = path + 1; *p; p++) {
		if (*p != '/') continue;
		*p = '\0';
		if (mkdir(path, 0755) != 0 && errno != EEXIST) return false;
		*p = '/';
	}
	if (mkdir(path, 0755) != 0 && errno != EEXIST) return false;
	return true;
}
