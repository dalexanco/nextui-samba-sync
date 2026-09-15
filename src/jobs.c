#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
