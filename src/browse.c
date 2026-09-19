#include <stdio.h>
#include <string.h>

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
