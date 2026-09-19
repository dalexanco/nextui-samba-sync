#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "link_state.h"

#define STATE_DIR SHARED_USERDATA_PATH "/samba-sync/state"

static const char *STATUS_NAMES[] = { "never", "ok", "partial", "error", "cancelled" };

// Lower-cases and replaces anything that isn't [a-z0-9] with '-', collapsing
// runs and trimming ends, so link names with spaces/parens turn into boring,
// filesystem-safe filenames. Never produces an empty string.
static void slugify(const char *name, char *out, size_t out_size)
{
	size_t o = 0;
	bool last_was_dash = false;
	for (const unsigned char *p = (const unsigned char *)name; *p && o < out_size - 1; p++) {
		if (isalnum(*p)) {
			out[o++] = tolower(*p);
			last_was_dash = false;
		}
		else if (!last_was_dash && o > 0) {
			out[o++] = '-';
			last_was_dash = true;
		}
	}
	while (o > 0 && out[o - 1] == '-') o--;
	out[o] = '\0';
	if (!out[0]) snprintf(out, out_size, "link");
}

static void statePath(const Link *link, char *out, size_t out_size)
{
	char slug[CONFIG_STR_MAX];
	slugify(link->name, slug, sizeof(slug));
	snprintf(out, out_size, "%s/%s.txt", STATE_DIR, slug);
}

static LinkStatus parseStatus(const char *value)
{
	for (int i = 0; i < (int)(sizeof(STATUS_NAMES) / sizeof(STATUS_NAMES[0])); i++)
		if (strcmp(value, STATUS_NAMES[i]) == 0) return (LinkStatus)i;
	return LINK_STATUS_NEVER;
}

void link_state_load(const Link *link, LinkState *out)
{
	memset(out, 0, sizeof(*out));

	char path[MAX_PATH];
	statePath(link, path, sizeof(path));
	FILE *file = fopen(path, "r");
	if (!file) return;

	LinkState s;
	memset(&s, 0, sizeof(s));
	bool name_matches = false;
	int error_total = 0;

	char line[BROWSE_STR_MAX + LINK_ERROR_REASON_MAX + 16];
	while (fgets(line, sizeof(line), file)) {
		trimTrailingNewlines(line);
		char *sep = strchr(line, '=');
		if (!sep) continue;
		*sep = '\0';
		const char *key = line;
		char *value = sep + 1;

		if (strcmp(key, "name") == 0) name_matches = strcmp(value, link->name) == 0;
		else if (strcmp(key, "status") == 0) s.status = parseStatus(value);
		else if (strcmp(key, "time") == 0) s.time = atoi(value);
		else if (strcmp(key, "copied") == 0) s.files_copied = atoi(value);
		else if (strcmp(key, "bytes") == 0) s.bytes_copied = atoll(value);
		else if (strcmp(key, "deleted") == 0) s.files_deleted = atoi(value);
		else if (strcmp(key, "errors") == 0) error_total = atoi(value);
		else if (strcmp(key, "error") == 0) {
			// error=<reason>\t<path>: reasons never contain a tab, paths might
			// contain anything else.
			char *tab = strchr(value, '\t');
			if (tab) *tab = '\0';
			link_state_add_error(&s, tab ? tab + 1 : "", value);
		}
	}
	fclose(file);

	if (error_total > s.error_total) s.error_total = error_total;
	if (name_matches) *out = s;
}

// mkdir -p: SHARED_USERDATA_PATH itself may not exist yet on a fresh card.
static void ensureDir(const char *path)
{
	char buf[MAX_PATH];
	snprintf(buf, sizeof(buf), "%s", path);
	for (char *p = buf + 1; *p; p++) {
		if (*p != '/') continue;
		*p = '\0';
		mkdir(buf, 0755);
		*p = '/';
	}
	mkdir(buf, 0755);
}

void link_state_save(const Link *link, const LinkState *state)
{
	ensureDir(STATE_DIR);

	char path[MAX_PATH];
	statePath(link, path, sizeof(path));
	FILE *file = fopen(path, "w");
	if (!file) {
		LOG_info("sambasync: cannot write %s\n", path);
		return;
	}

	fprintf(file, "name=%s\n", link->name);
	fprintf(file, "status=%s\n", STATUS_NAMES[state->status]);
	fprintf(file, "time=%d\n", state->time);
	fprintf(file, "copied=%d\n", state->files_copied);
	fprintf(file, "bytes=%lld\n", state->bytes_copied);
	fprintf(file, "deleted=%d\n", state->files_deleted);
	fprintf(file, "errors=%d\n", state->error_total);
	for (int i = 0; i < state->error_count; i++)
		fprintf(file, "error=%s\t%s\n", state->errors[i].reason, state->errors[i].path);
	fclose(file);
}

void link_state_add_error(LinkState *state, const char *path, const char *reason)
{
	state->error_total++;
	if (state->error_count >= LINK_STATE_MAX_ERRORS) return;
	LinkError *e = &state->errors[state->error_count++];
	snprintf(e->path, sizeof(e->path), "%s", path);
	snprintf(e->reason, sizeof(e->reason), "%s", reason);
}

bool link_state_failed(const LinkState *state)
{
	return state->status == LINK_STATUS_ERROR || state->status == LINK_STATUS_PARTIAL;
}
