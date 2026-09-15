#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "jobs.h"
#include "servers.h"
#include "jobs_list.h"

#define ROW_HEIGHT (PILL_SIZE + 6)

static int selected = 0;
static int scroll_offset = 0;

void JobsList_reset(void)
{
	selected = 0;
	scroll_offset = 0;
}

JobsListAction JobsList_input(int *dirty)
{
	int count = jobs_count();

	if (PAD_justPressed(BTN_B)) return JOBS_LIST_ACTION_BACK;
	if (PAD_justPressed(BTN_MENU)) return JOBS_LIST_ACTION_SERVERS;

	if (count > 0) {
		if (PAD_justRepeated(BTN_UP)) {
			selected = (selected - 1 + count) % count;
			*dirty = 1;
		}
		else if (PAD_justRepeated(BTN_DOWN)) {
			selected = (selected + 1) % count;
			*dirty = 1;
		}
	}

	return JOBS_LIST_ACTION_NONE;
}

static void renderRow(SDL_Surface *screen, const Job *job, bool is_selected, int y)
{
	int x = SCALE1(PADDING);
	int w = screen->w - SCALE1(PADDING * 2);

	GFX_blitPill(is_selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
	             &(SDL_Rect){ x, y, w, SCALE1(PILL_SIZE) });

	SDL_Color color = is_selected ? COLOR_BLACK : COLOR_WHITE;

	char label[JOB_STR_MAX + 16];
	snprintf(label, sizeof(label), "%s%s", job->name, job->mirror ? " [Miroir]" : "");
	UI_renderText(screen, label, font.medium, color, x + SCALE1(BUTTON_PADDING), y + SCALE1(4));

	const Server *server = servers_find(job->server);
	char value[JOB_STR_MAX * 2];
	if (!server) {
		snprintf(value, sizeof(value), "serveur introuvable");
	}
	else if (strcmp(job->last_sync_status, "error") == 0) {
		snprintf(value, sizeof(value), "%s/%s · erreur", server->host, job->remote_path);
	}
	else if (job->last_sync_time == 0) {
		snprintf(value, sizeof(value), "%s/%s · jamais synchronisé", server->host, job->remote_path);
	}
	else {
		snprintf(value, sizeof(value), "%s/%s", server->host, job->remote_path);
	}

	int vw = UI_textWidth(value, font.small);
	UI_renderText(screen, value, font.small, color, x + w - SCALE1(BUTTON_PADDING) - vw, y + SCALE1(4));
}

void JobsList_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);
	UI_renderTitle(screen, "Gestion des jobs", show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int count = jobs_count();

	if (count == 0) {
		const char *message = servers_count() == 0
			? "Aucun serveur déclaré — créez un dossier dans Samba Servers/"
			: "Aucun job configuré pour l'instant";
		UI_renderTextCentered(screen, message, font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else {
		int visible_rows = (screen->h - content_y - SCALE1(PADDING + PILL_SIZE)) / SCALE1(ROW_HEIGHT);
		if (visible_rows < 1) visible_rows = 1;

		if (selected < scroll_offset) scroll_offset = selected;
		if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;

		int last = MIN(count, scroll_offset + visible_rows);
		for (int i = scroll_offset; i < last; i++) {
			int row = i - scroll_offset;
			renderRow(screen, jobs_get(i), i == selected, content_y + row * SCALE1(ROW_HEIGHT));
		}
	}

	GFX_blitButtonGroup((char *[]){ "MENU", "SERVEURS", NULL }, 0, screen, 0);
	GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 1, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
