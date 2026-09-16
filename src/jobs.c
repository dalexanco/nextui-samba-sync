#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "jobs.h"

#define JOBS_DIR SHARED_USERDATA_PATH "/samba-sync"
#define JOBS_PATH JOBS_DIR "/jobs"

static Job jobs[MAX_JOBS];
static int job_count = 0;

// Parses one <slug>.txt (key=value) into *out. Returns false and leaves
// *out untouched if a required field (name/server/remote_path/local_path)
// is missing, so the caller can skip this job without crashing the scan.
static bool parseJobFile(const char *path, Job *out)
{
	FILE *file = fopen(path, "r");
	if (!file) return false;

	Job j;
	memset(&j, 0, sizeof(j));

	char line[512];
	while (fgets(line, sizeof(line), file)) {
		trimTrailingNewlines(line);
		char *sep = strchr(line, '=');
		if (!sep) continue;
		*sep = '\0';
		const char *key = line;
		const char *value = sep + 1;

		if (strcmp(key, "name") == 0) strncpy(j.name, value, sizeof(j.name) - 1);
		else if (strcmp(key, "server") == 0) strncpy(j.server, value, sizeof(j.server) - 1);
		else if (strcmp(key, "remote_path") == 0) strncpy(j.remote_path, value, sizeof(j.remote_path) - 1);
		else if (strcmp(key, "local_path") == 0) strncpy(j.local_path, value, sizeof(j.local_path) - 1);
		else if (strcmp(key, "mirror") == 0) j.mirror = atoi(value) != 0;
		else if (strcmp(key, "last_sync_status") == 0) strncpy(j.last_sync_status, value, sizeof(j.last_sync_status) - 1);
		else if (strcmp(key, "last_sync_time") == 0) j.last_sync_time = atoi(value);
	}
	fclose(file);

	if (!j.name[0] || !j.server[0] || !j.remote_path[0] || !j.local_path[0]) {
		LOG_info("sambasync: skipping malformed job file %s\n", path);
		return false;
	}

	*out = j;
	return true;
}

void jobs_rescan(void)
{
	job_count = 0;

	struct stat st;
	if (stat(JOBS_PATH, &st) != 0) {
		mkdir(JOBS_DIR, 0777);
		mkdir(JOBS_PATH, 0777);
		return;
	}

	DIR *dir = opendir(JOBS_PATH);
	if (!dir) return;

	struct dirent *entry;
	while ((entry = readdir(dir)) && job_count < MAX_JOBS) {
		if (entry->d_name[0] == '.') continue;
		if (!suffixMatch(".txt", entry->d_name)) continue;

		char job_file[MAX_PATH];
		snprintf(job_file, sizeof(job_file), "%s/%s", JOBS_PATH, entry->d_name);

		struct stat fst;
		if (stat(job_file, &fst) != 0 || !S_ISREG(fst.st_mode)) continue;

		if (parseJobFile(job_file, &jobs[job_count])) job_count++;
	}
	closedir(dir);
}

int jobs_count(void) { return job_count; }

const Job *jobs_get(int index)
{
	if (index < 0 || index >= job_count) return NULL;
	return &jobs[index];
}

static bool nameExists(const char *name)
{
	for (int i = 0; i < job_count; i++) {
		if (strcmp(jobs[i].name, name) == 0) return true;
	}
	return false;
}

void jobs_unique_name(const char *base_name, char *out_name)
{
	int suffix = 1;
	do {
		if (suffix == 1) snprintf(out_name, JOB_STR_MAX, "%s", base_name);
		else snprintf(out_name, JOB_STR_MAX, "%s (%d)", base_name, suffix);
		suffix++;
	} while (nameExists(out_name) && suffix <= 999);
}

// Lower-cases and replaces anything that isn't [a-z0-9] with '-', collapsing
// runs and trimming ends, so job names with spaces/parens (e.g. "GBA (2)")
// turn into boring, filesystem-safe filenames. Never produces an empty
// string -- falls back to "job" if name sanitizes away to nothing.
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
	if (!out[0]) snprintf(out, out_size, "job");
}

bool jobs_create(const char *name, const char *server, const char *remote_path, const char *local_path, bool mirror)
{
	struct stat st;
	if (stat(JOBS_PATH, &st) != 0) {
		mkdir(JOBS_DIR, 0777);
		mkdir(JOBS_PATH, 0777);
	}

	char slug[JOB_STR_MAX];
	slugify(name, slug, sizeof(slug));

	char job_file[MAX_PATH];
	int suffix = 1;
	for (;;) {
		if (suffix == 1) snprintf(job_file, sizeof(job_file), "%s/%s.txt", JOBS_PATH, slug);
		else snprintf(job_file, sizeof(job_file), "%s/%s-%d.txt", JOBS_PATH, slug, suffix);
		if (!exists(job_file)) break;
		if (++suffix > 999) return false;
	}

	FILE *file = fopen(job_file, "w");
	if (!file) return false;

	fprintf(file, "name=%s\n", name);
	fprintf(file, "server=%s\n", server);
	fprintf(file, "remote_path=%s\n", remote_path);
	fprintf(file, "local_path=%s\n", local_path);
	fprintf(file, "mirror=%d\n", mirror ? 1 : 0);
	fprintf(file, "last_sync_status=\n");
	fprintf(file, "last_sync_time=0\n");
	fclose(file);

	jobs_rescan();
	return true;
}
