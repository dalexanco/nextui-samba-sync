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

int local_list(const char *path, BrowseEntry *out, int max_entries)
{
	char full[BROWSE_STR_MAX + 64];
	fullPath(full, sizeof(full), path);

	DIR *dir = opendir(full);
	if (!dir) return -1;

	int count = 0;
	struct dirent *entry;
	while (count < max_entries && (entry = readdir(dir))) {
		if (entry->d_type != DT_DIR) continue;
		if (hide(entry->d_name)) continue;

		snprintf(out[count].name, BROWSE_STR_MAX, "%s", entry->d_name);
		out[count].is_dir = true;
		count++;
	}

	closedir(dir);
	return count;
}

bool local_create_folder(const char *parent_path, const char *base_name, char *out_name)
{
	char parent_full[BROWSE_STR_MAX + 64];
	fullPath(parent_full, sizeof(parent_full), parent_path);

	for (int suffix = 1; suffix <= 999; suffix++) {
		if (suffix == 1) snprintf(out_name, BROWSE_STR_MAX, "%s", base_name);
		else snprintf(out_name, BROWSE_STR_MAX, "%s (%d)", base_name, suffix);

		char candidate[BROWSE_STR_MAX * 2 + 64];
		snprintf(candidate, sizeof(candidate), "%s/%s", parent_full, out_name);

		if (mkdir(candidate, 0755) == 0) return true;
		if (errno != EEXIST) return false;
	}
	return false;
}
