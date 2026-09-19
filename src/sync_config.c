#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "vendor/tomlc17/tomlc17.h"
#include "sync_config.h"

#define CONFIG_PATH SDCARD_PATH "/" CONFIG_FILE_NAME
#define DEFAULT_TIMEOUT 10
#define DEFAULT_PORT 445

static Server servers[MAX_SERVERS];
static int server_count = 0;
static Link links[MAX_LINKS];
static int link_count = 0;
static int timeout = DEFAULT_TIMEOUT;
static ConfigStatus status = CONFIG_MISSING;
static char error_message[CONFIG_ERROR_MAX + 64];

// Reads an optional string key. Returns false (and writes a French reason
// into error) only if the key is present with a non-string type; a missing
// key leaves out untouched.
static bool readString(toml_datum_t table, const char *key, char *out, size_t out_size,
                       char *error, size_t error_size)
{
	toml_datum_t d = toml_get(table, key);
	if (d.type == TOML_UNKNOWN) return true;
	if (d.type != TOML_STRING) {
		snprintf(error, error_size, "Config : « %s » doit être une chaîne", key);
		return false;
	}
	snprintf(out, out_size, "%s", d.u.s);
	return true;
}

static void parseServer(const char *name, toml_datum_t table, Server *out)
{
	memset(out, 0, sizeof(*out));
	snprintf(out->name, sizeof(out->name), "%s", name);
	out->port = DEFAULT_PORT;

	if (table.type != TOML_TABLE) {
		snprintf(out->error, sizeof(out->error), "Config : serveur « %s » invalide", name);
		return;
	}

	char *e = out->error;
	size_t es = sizeof(out->error);
	if (!readString(table, "host", out->host, sizeof(out->host), e, es)) return;
	if (!readString(table, "username", out->username, sizeof(out->username), e, es)) return;
	if (!readString(table, "password", out->password, sizeof(out->password), e, es)) return;
	if (!readString(table, "domain", out->domain, sizeof(out->domain), e, es)) return;

	toml_datum_t port = toml_get(table, "port");
	if (port.type == TOML_INT64 && port.u.int64 > 0 && port.u.int64 <= 65535) {
		out->port = (int)port.u.int64;
	}
	else if (port.type != TOML_UNKNOWN) {
		snprintf(e, es, "Config : port du serveur « %s » invalide", name);
		return;
	}

	if (!out->host[0]) snprintf(e, es, "Config : host manquant pour « %s »", name);
}

static const Server *findServer(const char *name)
{
	for (int i = 0; i < server_count; i++)
		if (strcmp(servers[i].name, name) == 0) return &servers[i];
	return NULL;
}

// A local path must stay inside the SD card: relative, and no ".." segment.
static bool isSafeLocalPath(const char *path)
{
	if (path[0] == '/') return false;
	const char *p = path;
	while (*p) {
		const char *end = strchr(p, '/');
		size_t len = end ? (size_t)(end - p) : strlen(p);
		if (len == 2 && p[0] == '.' && p[1] == '.') return false;
		if (!end) break;
		p = end + 1;
	}
	return true;
}

// Strips leading/trailing "/" so remote/local compose cleanly with
// browse_path_push() ("/GBA/" and "GBA" mean the same thing).
static void trimSlashes(char *path)
{
	size_t len = strlen(path);
	while (len > 0 && path[len - 1] == '/') path[--len] = '\0';
	size_t start = 0;
	while (path[start] == '/') start++;
	if (start) memmove(path, path + start, len - start + 1);
}

static void parseLink(const char *name, toml_datum_t table, Link *out)
{
	memset(out, 0, sizeof(*out));
	snprintf(out->name, sizeof(out->name), "%s", name);
	out->mode = LINK_MODE_ADD;

	char *e = out->config_error;
	size_t es = sizeof(out->config_error);

	if (table.type != TOML_TABLE) {
		snprintf(e, es, "Config : liaison invalide");
		return;
	}

	char mode[32] = "";
	if (!readString(table, "server", out->server_name, sizeof(out->server_name), e, es)) return;
	if (!readString(table, "share", out->share, sizeof(out->share), e, es)) return;
	if (!readString(table, "remote", out->remote, sizeof(out->remote), e, es)) return;
	if (!readString(table, "local", out->local, sizeof(out->local), e, es)) return;
	if (!readString(table, "mode", mode, sizeof(mode), e, es)) return;

	// "local" is checked for traversal before trimming, so "/Roms" (absolute)
	// is rejected rather than silently turned into "Roms".
	if (!out->server_name[0]) { snprintf(e, es, "Config : server manquant"); return; }
	if (!out->share[0]) { snprintf(e, es, "Config : share manquant"); return; }
	if (!out->local[0]) { snprintf(e, es, "Config : local manquant"); return; }
	if (!isSafeLocalPath(out->local)) { snprintf(e, es, "Config : local doit rester dans la carte SD"); return; }
	trimSlashes(out->remote);
	trimSlashes(out->local);
	if (!out->local[0]) { snprintf(e, es, "Config : local manquant"); return; }

	if (!mode[0] || strcmp(mode, "add") == 0) out->mode = LINK_MODE_ADD;
	else if (strcmp(mode, "mirror") == 0) out->mode = LINK_MODE_MIRROR;
	else { snprintf(e, es, "Config : mode « %s » inconnu", mode); return; }

	out->server = findServer(out->server_name);
	if (!out->server) {
		snprintf(e, es, "Config : serveur « %s » introuvable", out->server_name);
		return;
	}
	if (out->server->error[0]) {
		snprintf(e, es, "%s", out->server->error);
		return;
	}
}

// tomlc17 reports "(line N) message"; reshaped into the "<file>, ligne N :
// message" form SPEC.md shows. The message itself stays in the parser's
// English.
static void formatParseError(const char *errmsg)
{
	int line = 0;
	int consumed = 0;
	if (sscanf(errmsg, "(line %d)%n", &line, &consumed) == 1 && consumed > 0) {
		const char *rest = errmsg + consumed;
		while (*rest == ' ') rest++;
		snprintf(error_message, sizeof(error_message), "%s, ligne %d : %s", CONFIG_FILE_NAME, line, rest);
	}
	else {
		snprintf(error_message, sizeof(error_message), "%s : %s", CONFIG_FILE_NAME, errmsg);
	}
}

void sync_config_load(void)
{
	server_count = 0;
	link_count = 0;
	timeout = DEFAULT_TIMEOUT;
	error_message[0] = '\0';

	FILE *file = fopen(CONFIG_PATH, "r");
	if (!file) {
		status = CONFIG_MISSING;
		return;
	}
	toml_result_t result = toml_parse_file(file);
	fclose(file);

	if (!result.ok) {
		status = CONFIG_PARSE_ERROR;
		formatParseError(result.errmsg);
		LOG_info("sambasync: %s\n", error_message);
		toml_free(result);
		return;
	}
	status = CONFIG_OK;

	toml_datum_t settings = toml_get(result.toptab, "settings");
	if (settings.type == TOML_TABLE) {
		toml_datum_t t = toml_get(settings, "timeout");
		if (t.type == TOML_INT64 && t.u.int64 > 0 && t.u.int64 <= 600) timeout = (int)t.u.int64;
	}

	// Servers first: links resolve their server by name.
	toml_datum_t server_table = toml_get(result.toptab, "servers");
	if (server_table.type == TOML_TABLE) {
		for (int i = 0; i < server_table.u.tab.size && server_count < MAX_SERVERS; i++) {
			parseServer(server_table.u.tab.key[i], server_table.u.tab.value[i], &servers[server_count++]);
		}
	}

	toml_datum_t link_table = toml_get(result.toptab, "links");
	if (link_table.type == TOML_TABLE) {
		for (int i = 0; i < link_table.u.tab.size && link_count < MAX_LINKS; i++) {
			parseLink(link_table.u.tab.key[i], link_table.u.tab.value[i], &links[link_count]);
			if (links[link_count].config_error[0])
				LOG_info("sambasync: link \"%s\": %s\n", links[link_count].name, links[link_count].config_error);
			link_count++;
		}
	}

	toml_free(result);
}

ConfigStatus sync_config_status(void)
{
	return status;
}

const char *sync_config_error_message(void)
{
	return error_message;
}

int sync_config_timeout(void)
{
	return timeout;
}

int sync_config_link_count(void)
{
	return link_count;
}

const Link *sync_config_link_get(int index)
{
	if (index < 0 || index >= link_count) return NULL;
	return &links[index];
}
