#ifndef SERVERS_H
#define SERVERS_H

// Samba servers declared offline by the user in
// SDCARD_PATH "/Samba Servers/<name>/server.txt" (key=value, one folder per
// server -- same convention as nextui-gift-code's Gifts/<name>/manifest.txt).
// This module only reads that config; servers are never created or edited
// from the pak itself (see SPEC.md, écran 1bis).

#define MAX_SERVERS 32
#define SERVER_STR_MAX 128

typedef struct {
	char name[SERVER_STR_MAX];
	char host[SERVER_STR_MAX];
	int port;
	char share[SERVER_STR_MAX];
	char username[SERVER_STR_MAX];
	char password[SERVER_STR_MAX];
	char domain[SERVER_STR_MAX];
} Server;

// Rescans SDCARD_PATH "/Samba Servers" from scratch. Malformed entries
// (missing name/host/share) are skipped silently. Safe to call again later
// (e.g. from écran 1bis "Recharger") to pick up edits made while the pak is
// running.
void servers_rescan(void);

int servers_count(void);
const Server *servers_get(int index);

// NULL if no server with this name is currently declared (e.g. a job
// referencing a renamed/deleted server).
const Server *servers_find(const char *name);

#endif
