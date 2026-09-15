#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "servers.h"

#define SERVERS_PATH SDCARD_PATH "/Samba Servers"

static Server servers[MAX_SERVERS];
static int server_count = 0;

// Parses one server.txt (key=value) into *out. Returns false and leaves
// *out untouched if a required field (name/host/share) is missing, so the
// caller can skip this server without crashing the scan.
static bool parseServerFile(const char *path, Server *out)
{
	FILE *file = fopen(path, "r");
	if (!file) return false;

	Server s;
	memset(&s, 0, sizeof(s));
	s.port = 445;

	char line[512];
	while (fgets(line, sizeof(line), file)) {
		trimTrailingNewlines(line);
		char *sep = strchr(line, '=');
		if (!sep) continue;
		*sep = '\0';
		const char *key = line;
		const char *value = sep + 1;

		if (strcmp(key, "name") == 0) strncpy(s.name, value, sizeof(s.name) - 1);
		else if (strcmp(key, "host") == 0) strncpy(s.host, value, sizeof(s.host) - 1);
		else if (strcmp(key, "port") == 0) s.port = atoi(value);
		else if (strcmp(key, "share") == 0) strncpy(s.share, value, sizeof(s.share) - 1);
		else if (strcmp(key, "username") == 0) strncpy(s.username, value, sizeof(s.username) - 1);
		else if (strcmp(key, "password") == 0) strncpy(s.password, value, sizeof(s.password) - 1);
		else if (strcmp(key, "domain") == 0) strncpy(s.domain, value, sizeof(s.domain) - 1);
	}
	fclose(file);

	if (!s.name[0] || !s.host[0] || !s.share[0]) {
		LOG_info("sambasync: skipping malformed server file %s\n", path);
		return false;
	}
	if (s.port <= 0) s.port = 445;

	*out = s;
	return true;
}

void servers_rescan(void)
{
	server_count = 0;

	struct stat st;
	if (stat(SERVERS_PATH, &st) != 0) {
		mkdir(SERVERS_PATH, 0777);
		return;
	}

	DIR *dir = opendir(SERVERS_PATH);
	if (!dir) return;

	struct dirent *entry;
	while ((entry = readdir(dir)) && server_count < MAX_SERVERS) {
		if (entry->d_name[0] == '.') continue;

		char server_dir[MAX_PATH];
		snprintf(server_dir, sizeof(server_dir), "%s/%s", SERVERS_PATH, entry->d_name);

		struct stat dst;
		if (stat(server_dir, &dst) != 0 || !S_ISDIR(dst.st_mode)) continue;

		char server_file[MAX_PATH];
		snprintf(server_file, sizeof(server_file), "%s/server.txt", server_dir);
		if (!exists(server_file)) continue;

		if (parseServerFile(server_file, &servers[server_count])) server_count++;
	}
	closedir(dir);
}

int servers_count(void)
{
	return server_count;
}

const Server *servers_get(int index)
{
	if (index < 0 || index >= server_count) return NULL;
	return &servers[index];
}

const Server *servers_find(const char *name)
{
	for (int i = 0; i < server_count; i++)
		if (strcmp(servers[i].name, name) == 0) return &servers[i];
	return NULL;
}
