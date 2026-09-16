#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "servers.h"
#include "smb_client.h"
#include "browse.h"
#include "local_fs.h"
#include "job_wizard.h"

#define ROW_HEIGHT (PILL_SIZE + 6)
#define CONNECT_TIMEOUT_SECONDS 5

typedef enum {
	CONNECT_UNTRIED,
	CONNECT_OK,
	CONNECT_FAILED,
} ConnectStatus;

// Écran 2a, écran 2b and écran 3 (étapes 1/3, 2/3 et 3/3) live in this one
// file, per docs/ARCHITECTURE.md -- the whole job-creation assistant is one
// state machine, just like sync_engine.c will be for écran 4. Écran 2c
// (récapitulatif) doesn't exist yet, so "Y Choisir ce dossier" on écran 3
// has nowhere to go and is withheld, same as jobs_list.h's pattern for
// actions without a backing screen yet -- écran 2b's own Y is wired now
// that its target (écran 3) exists.
typedef enum {
	WIZARD_STEP_CHOOSE_SERVER,
	WIZARD_STEP_BROWSE_REMOTE,
	WIZARD_STEP_BROWSE_LOCAL,
} WizardStep;

static WizardStep step;

// Étape 1 (choix serveur) state.
static int selected = 0;
static int scroll_offset = 0;
static ConnectStatus connect_status = CONNECT_UNTRIED;

// Étape 2 (parcours distant) state. Invariant: session is non-NULL only
// while step == WIZARD_STEP_BROWSE_REMOTE -- every transition out of that
// step (back to root, cancelling, or picking a source folder) disconnects
// it first.
static int chosen_server_index;
static SmbSession *session = NULL;
static char remote_current_path[BROWSE_STR_MAX];
static BrowseEntry remote_entries[BROWSE_MAX_ENTRIES];
static int remote_entry_count = 0;
static char remote_list_error[128];

// Source folder chosen at the end of étape 2 (on Y) -- unlike
// remote_current_path above, this is set once and not touched again by
// further browsing. Feeds étape 3's default folder name (X "Créer") and
// will feed étape 2c's recap once that screen exists.
static char remote_path[BROWSE_STR_MAX];

// Étape 3 (parcours SD locale) state.
static char local_current_path[BROWSE_STR_MAX];
static BrowseEntry local_entries[BROWSE_MAX_ENTRIES];
static int local_entry_count = 0;
static char local_list_error[128];

void JobWizard_reset(void)
{
	if (session) {
		smb_disconnect(session);
		session = NULL;
	}
	step = WIZARD_STEP_CHOOSE_SERVER;
	selected = 0;
	scroll_offset = 0;
	connect_status = CONNECT_UNTRIED;
	remote_list_error[0] = '\0';
	local_list_error[0] = '\0';
}

// Lists path into remote_entries[]/remote_entry_count and, on success,
// commits it as remote_current_path (resetting the on-screen
// selection/scroll for the new listing). On failure the previous
// listing/remote_current_path is left untouched (so a failed "Entrer" just
// leaves the user where they were, with remote_list_error set) and false
// is returned.
static bool tryList(const char *path)
{
	SmbError error;
	int n = smb_list(session, path, remote_entries, BROWSE_MAX_ENTRIES, &error);
	if (n < 0) {
		snprintf(remote_list_error, sizeof(remote_list_error), "Erreur de connexion au dossier distant");
		return false;
	}

	remote_entry_count = n;
	browse_sort(remote_entries, remote_entry_count);
	snprintf(remote_current_path, sizeof(remote_current_path), "%s", path);
	selected = 0;
	scroll_offset = 0;
	remote_list_error[0] = '\0';
	return true;
}

// Symmetric to tryList() above but for étape 3's local SD browsing.
static bool tryListLocal(const char *path)
{
	int n = local_list(path, local_entries, BROWSE_MAX_ENTRIES);
	if (n < 0) {
		snprintf(local_list_error, sizeof(local_list_error), "Erreur de lecture de la carte SD");
		return false;
	}

	local_entry_count = n;
	browse_sort(local_entries, local_entry_count);
	snprintf(local_current_path, sizeof(local_current_path), "%s", path);
	selected = 0;
	scroll_offset = 0;
	local_list_error[0] = '\0';
	return true;
}

// Basename of the remote_path chosen in étape 2, used as the default name
// for X "Créer" in étape 3 (e.g. "Roms/GBA" -> "GBA"). Falls back to the
// share name for the edge case where the user picked the share root itself
// as the source (remote_path == "").
static const char *sourceFolderName(void)
{
	if (!remote_path[0]) return servers_get(chosen_server_index)->share;
	const char *slash = strrchr(remote_path, '/');
	return slash ? slash + 1 : remote_path;
}

static JobWizardAction inputChooseServer(int *dirty)
{
	int count = servers_count();

	if (PAD_justPressed(BTN_B)) return JOB_WIZARD_ACTION_CANCEL;

	if (count > 0) {
		if (PAD_justRepeated(BTN_UP)) {
			selected = (selected - 1 + count) % count;
			connect_status = CONNECT_UNTRIED;
			*dirty = 1;
		}
		else if (PAD_justRepeated(BTN_DOWN)) {
			selected = (selected + 1) % count;
			connect_status = CONNECT_UNTRIED;
			*dirty = 1;
		}
		else if (PAD_justPressed(BTN_A)) {
			SmbError error;
			SmbSession *new_session = smb_connect(servers_get(selected), CONNECT_TIMEOUT_SECONDS, &error);
			if (!new_session) {
				connect_status = CONNECT_FAILED;
			}
			else {
				session = new_session;
				chosen_server_index = selected;
				if (tryList("")) {
					step = WIZARD_STEP_BROWSE_REMOTE;
				}
				else {
					smb_disconnect(session);
					session = NULL;
					connect_status = CONNECT_FAILED;
				}
			}
			*dirty = 1;
		}
	}

	return JOB_WIZARD_ACTION_NONE;
}

static JobWizardAction inputBrowseRemote(int *dirty)
{
	if (PAD_justPressed(BTN_B)) {
		char parent[BROWSE_STR_MAX];
		snprintf(parent, sizeof(parent), "%s", remote_current_path);
		if (browse_path_pop(parent)) {
			tryList(parent);
		}
		else {
			smb_disconnect(session);
			session = NULL;
			step = WIZARD_STEP_CHOOSE_SERVER;
			selected = chosen_server_index;
			scroll_offset = 0;
			connect_status = CONNECT_UNTRIED;
		}
		*dirty = 1;
		return JOB_WIZARD_ACTION_NONE;
	}

	if (PAD_justPressed(BTN_Y)) {
		snprintf(remote_path, sizeof(remote_path), "%s", remote_current_path);
		smb_disconnect(session);
		session = NULL;
		tryListLocal("");
		step = WIZARD_STEP_BROWSE_LOCAL;
		*dirty = 1;
		return JOB_WIZARD_ACTION_NONE;
	}

	if (remote_entry_count > 0) {
		if (PAD_justRepeated(BTN_UP)) {
			selected = (selected - 1 + remote_entry_count) % remote_entry_count;
			*dirty = 1;
		}
		else if (PAD_justRepeated(BTN_DOWN)) {
			selected = (selected + 1) % remote_entry_count;
			*dirty = 1;
		}
		else if (PAD_justPressed(BTN_A)) {
			char new_path[BROWSE_STR_MAX];
			browse_path_push(new_path, remote_current_path, remote_entries[selected].name);
			tryList(new_path);
			*dirty = 1;
		}
	}

	return JOB_WIZARD_ACTION_NONE;
}

static JobWizardAction inputBrowseLocal(int *dirty)
{
	if (PAD_justPressed(BTN_B)) {
		char parent[BROWSE_STR_MAX];
		snprintf(parent, sizeof(parent), "%s", local_current_path);
		if (browse_path_pop(parent)) {
			tryListLocal(parent);
		}
		else {
			// At the SD root there's no earlier wizard step to fall back
			// to (remote_path is already chosen and the SMB session
			// already closed) -- per SPEC.md this just cancels the
			// assistant, same as B at étape 2a.
			return JOB_WIZARD_ACTION_CANCEL;
		}
		*dirty = 1;
		return JOB_WIZARD_ACTION_NONE;
	}

	if (PAD_justPressed(BTN_X)) {
		char created_name[BROWSE_STR_MAX];
		if (local_create_folder(local_current_path, sourceFolderName(), created_name)) {
			char new_path[BROWSE_STR_MAX];
			browse_path_push(new_path, local_current_path, created_name);
			tryListLocal(new_path);
		}
		else {
			snprintf(local_list_error, sizeof(local_list_error), "Impossible de créer le dossier");
		}
		*dirty = 1;
		return JOB_WIZARD_ACTION_NONE;
	}

	if (local_entry_count > 0) {
		if (PAD_justRepeated(BTN_UP)) {
			selected = (selected - 1 + local_entry_count) % local_entry_count;
			*dirty = 1;
		}
		else if (PAD_justRepeated(BTN_DOWN)) {
			selected = (selected + 1) % local_entry_count;
			*dirty = 1;
		}
		else if (PAD_justPressed(BTN_A)) {
			char new_path[BROWSE_STR_MAX];
			browse_path_push(new_path, local_current_path, local_entries[selected].name);
			tryListLocal(new_path);
			*dirty = 1;
		}
	}

	return JOB_WIZARD_ACTION_NONE;
}

JobWizardAction JobWizard_input(int *dirty)
{
	switch (step) {
	case WIZARD_STEP_CHOOSE_SERVER: return inputChooseServer(dirty);
	case WIZARD_STEP_BROWSE_REMOTE: return inputBrowseRemote(dirty);
	case WIZARD_STEP_BROWSE_LOCAL:  return inputBrowseLocal(dirty);
	}
	return JOB_WIZARD_ACTION_NONE;
}

static void renderRow(SDL_Surface *screen, const char *label, bool is_selected, int y)
{
	int x = SCALE1(PADDING);
	int w = screen->w - SCALE1(PADDING * 2);

	GFX_blitPill(is_selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
	             &(SDL_Rect){ x, y, w, SCALE1(PILL_SIZE) });

	SDL_Color color = is_selected ? COLOR_BLACK : COLOR_WHITE;
	UI_renderText(screen, label, font.medium, color, x + SCALE1(BUTTON_PADDING), y + SCALE1(4));
}

static void renderChooseServer(SDL_Surface *screen, int show_setting)
{
	UI_renderTitle(screen, "Nouveau job — Choisir un serveur", show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int count = servers_count();

	if (count == 0) {
		UI_renderTextCentered(screen, "Aucun serveur déclaré — créez un dossier dans Samba Servers/",
		                       font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else {
		int visible_rows = (screen->h - content_y - SCALE1(PADDING + PILL_SIZE)) / SCALE1(ROW_HEIGHT);
		if (visible_rows < 1) visible_rows = 1;

		if (selected < scroll_offset) scroll_offset = selected;
		if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;

		int last = MIN(count, scroll_offset + visible_rows);
		for (int i = scroll_offset; i < last; i++) {
			const Server *server = servers_get(i);
			ConnectStatus status = i == selected ? connect_status : CONNECT_UNTRIED;
			const char *status_text = status == CONNECT_OK ? "\xe2\x9c\x94 connect\xc3\xa9"
			                         : status == CONNECT_FAILED ? "\xe2\x9c\x98 injoignable"
			                         : "";

			int row = i - scroll_offset;
			int y = content_y + row * SCALE1(ROW_HEIGHT);
			int x = SCALE1(PADDING);
			int w = screen->w - SCALE1(PADDING * 2);

			GFX_blitPill(i == selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
			             &(SDL_Rect){ x, y, w, SCALE1(PILL_SIZE) });
			SDL_Color color = i == selected ? COLOR_BLACK : COLOR_WHITE;
			UI_renderText(screen, server->name, font.medium, color, x + SCALE1(BUTTON_PADDING), y + SCALE1(4));

			char value[SERVER_STR_MAX * 2];
			snprintf(value, sizeof(value), "%s / %s%s%s", server->host, server->share,
			         status_text[0] ? "   " : "", status_text);
			int vw = UI_textWidth(value, font.small);
			UI_renderText(screen, value, font.small, color, x + w - SCALE1(BUTTON_PADDING) - vw, y + SCALE1(4));
		}
	}

	GFX_blitButtonGroup((char *[]){ "A", "CHOISIR", NULL }, 0, screen, 0);
	GFX_blitButtonGroup((char *[]){ "B", "ANNULER", NULL }, 1, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}

static void renderBrowseRemote(SDL_Surface *screen, int show_setting)
{
	char title[SERVER_STR_MAX + 48];
	snprintf(title, sizeof(title), "%s — Choisir un dossier distant", servers_get(chosen_server_index)->name);
	UI_renderTitle(screen, title, show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);

	if (remote_list_error[0]) {
		UI_renderTextCentered(screen, remote_list_error, font.small, COLOR_DARK_TEXT, content_y);
		content_y += SCALE1(24);
	}

	if (remote_entry_count == 0) {
		UI_renderTextCentered(screen, "Aucun sous-dossier ici", font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else {
		int visible_rows = (screen->h - content_y - SCALE1(PADDING + PILL_SIZE)) / SCALE1(ROW_HEIGHT);
		if (visible_rows < 1) visible_rows = 1;

		if (selected < scroll_offset) scroll_offset = selected;
		if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;

		int last = MIN(remote_entry_count, scroll_offset + visible_rows);
		for (int i = scroll_offset; i < last; i++) {
			char label[BROWSE_STR_MAX + 8];
			snprintf(label, sizeof(label), "\xf0\x9f\x93\x81 %s/", remote_entries[i].name);
			int row = i - scroll_offset;
			renderRow(screen, label, i == selected, content_y + row * SCALE1(ROW_HEIGHT));
		}
	}

	GFX_blitButtonGroup((char *[]){ "A", "ENTRER", NULL }, 0, screen, 0);
	GFX_blitButtonGroup((char *[]){ "Y", "CHOISIR", "B", "RETOUR", NULL }, 0, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}

static void renderBrowseLocal(SDL_Surface *screen, int show_setting)
{
	UI_renderTitle(screen, "Nouveau job — Choisir un dossier local", show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);

	if (local_list_error[0]) {
		UI_renderTextCentered(screen, local_list_error, font.small, COLOR_DARK_TEXT, content_y);
		content_y += SCALE1(24);
	}

	if (local_entry_count == 0) {
		UI_renderTextCentered(screen, "Aucun sous-dossier ici", font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else {
		int visible_rows = (screen->h - content_y - SCALE1(PADDING + PILL_SIZE)) / SCALE1(ROW_HEIGHT);
		if (visible_rows < 1) visible_rows = 1;

		if (selected < scroll_offset) scroll_offset = selected;
		if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;

		int last = MIN(local_entry_count, scroll_offset + visible_rows);
		for (int i = scroll_offset; i < last; i++) {
			char label[BROWSE_STR_MAX + 8];
			snprintf(label, sizeof(label), "\xf0\x9f\x93\x81 %s/", local_entries[i].name);
			int row = i - scroll_offset;
			renderRow(screen, label, i == selected, content_y + row * SCALE1(ROW_HEIGHT));
		}
	}

	GFX_blitButtonGroup((char *[]){ "X", "CRÉER", "A", "ENTRER", NULL }, 1, screen, 0);
	GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 1, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}

void JobWizard_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);
	switch (step) {
	case WIZARD_STEP_CHOOSE_SERVER: renderChooseServer(screen, show_setting); break;
	case WIZARD_STEP_BROWSE_REMOTE: renderBrowseRemote(screen, show_setting); break;
	case WIZARD_STEP_BROWSE_LOCAL:  renderBrowseLocal(screen, show_setting); break;
	}
}
