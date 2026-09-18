#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "defines.h"
#include "utils.h"
#include "settings.h"

#define SETTINGS_DIR SHARED_USERDATA_PATH "/samba-sync"
#define SETTINGS_PATH SETTINGS_DIR "/settings.txt"

static Settings settings = {
	.overwrite_existing = false,
	.preview_before_sync = true,
	.network_timeout = 10,
};

static void writeSettingsFile(void)
{
	struct stat st;
	if (stat(SETTINGS_DIR, &st) != 0) mkdir(SETTINGS_DIR, 0777);

	FILE *file = fopen(SETTINGS_PATH, "w");
	if (!file) return;

	fprintf(file, "overwrite_existing=%d\n", settings.overwrite_existing ? 1 : 0);
	fprintf(file, "preview_before_sync=%d\n", settings.preview_before_sync ? 1 : 0);
	fprintf(file, "network_timeout=%d\n", settings.network_timeout);
	fclose(file);
}

void settings_load(void)
{
	FILE *file = fopen(SETTINGS_PATH, "r");
	if (!file) {
		writeSettingsFile(); // first run: persist the defaults so the file exists
		return;
	}

	char line[128];
	while (fgets(line, sizeof(line), file)) {
		trimTrailingNewlines(line);
		char *sep = strchr(line, '=');
		if (!sep) continue;
		*sep = '\0';
		const char *key = line;
		const char *value = sep + 1;

		if (strcmp(key, "overwrite_existing") == 0) settings.overwrite_existing = atoi(value) != 0;
		else if (strcmp(key, "preview_before_sync") == 0) settings.preview_before_sync = atoi(value) != 0;
		else if (strcmp(key, "network_timeout") == 0) settings.network_timeout = atoi(value);
	}
	fclose(file);
}

const Settings *settings_get(void)
{
	return &settings;
}

void settings_set_overwrite_existing(bool value)
{
	settings.overwrite_existing = value;
	writeSettingsFile();
}

void settings_set_preview_before_sync(bool value)
{
	settings.preview_before_sync = value;
	writeSettingsFile();
}

void settings_set_network_timeout(int value)
{
	settings.network_timeout = value;
	writeSettingsFile();
}
