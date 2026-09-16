#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "browse.h"

void browse_path_push(char *out, const char *base, const char *name)
{
	if (base[0]) snprintf(out, BROWSE_STR_MAX, "%s/%s", base, name);
	else snprintf(out, BROWSE_STR_MAX, "%s", name);
}

bool browse_path_pop(char *path)
{
	char *sep = strrchr(path, '/');
	if (!sep) return false;
	*sep = '\0';
	return true;
}

static int compareEntries(const void *a, const void *b)
{
	const BrowseEntry *ea = a;
	const BrowseEntry *eb = b;
	if (ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
	return strcasecmp(ea->name, eb->name);
}

void browse_sort(BrowseEntry *entries, int count)
{
	qsort(entries, count, sizeof(BrowseEntry), compareEntries);
}
