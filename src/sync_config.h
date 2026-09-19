#ifndef SYNC_CONFIG_H
#define SYNC_CONFIG_H

#include <stdbool.h>

// Everything that describes what to sync, declared offline by the user in
// SDCARD_PATH "/Samba Sync.toml" (TOML, parsed by vendor/tomlc17) -- see
// SPEC.md "Configuration". The pak only ever reads this file.
//
// Named sync_config rather than config so it can't shadow NextUI's own
// common/config.h through the Makefile's -I. include path.

#define MAX_SERVERS 32
#define MAX_LINKS 64
#define CONFIG_STR_MAX 256
#define CONFIG_ERROR_MAX (CONFIG_STR_MAX + 64)

#define CONFIG_FILE_NAME "Samba Sync.toml"

typedef struct {
	char name[CONFIG_STR_MAX]; // table key in [servers."<name>"]
	char host[CONFIG_STR_MAX];
	int port;
	char username[CONFIG_STR_MAX];
	char password[CONFIG_STR_MAX];
	char domain[CONFIG_STR_MAX];
	char error[CONFIG_ERROR_MAX]; // "" if valid, else why (shown to the user)
} Server;

typedef enum {
	LINK_MODE_ADD,
	LINK_MODE_MIRROR,
} LinkMode;

typedef struct {
	char name[CONFIG_STR_MAX]; // table key in [links."<name>"], also the key of its persisted state
	char server_name[CONFIG_STR_MAX];
	const Server *server; // resolved server_name, NULL if unknown
	char share[CONFIG_STR_MAX];
	char remote[CONFIG_STR_MAX]; // share-relative, "" = share root
	char local[CONFIG_STR_MAX];  // SDCARD_PATH-relative
	LinkMode mode;
	// "" if the link can be checked/synced; otherwise a short reason
	// ("Config: server “NAS” not found") shown in place of its status. A link with a config error is never checked nor synced.
	char config_error[CONFIG_ERROR_MAX];
} Link;

typedef enum {
	CONFIG_OK,
	CONFIG_MISSING,     // no Samba Sync.toml on the SD card
	CONFIG_PARSE_ERROR, // not valid TOML: no link could be read at all
} ConfigStatus;

// (Re)reads Samba Sync.toml from scratch. Never fails hard: a missing or
// unparsable file leaves zero links and sets sync_config_status(); a
// malformed server or link is kept with its error filled in so the UI can
// show it (unknown tables/keys are ignored).
void sync_config_load(void);

ConfigStatus sync_config_status(void);

// For CONFIG_PARSE_ERROR: the parser's message, prefixed with the file name
// and line ("Samba Sync.toml, line 12: ..."). "" otherwise.
const char *sync_config_error_message(void);

int sync_config_timeout(void); // [settings] timeout, seconds, default 10

// Links in file order (see vendor/tomlc17/VENDORED.md).
int sync_config_link_count(void);
const Link *sync_config_link_get(int index);

#endif
