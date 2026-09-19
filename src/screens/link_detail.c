#include <stdio.h>
#include <string.h>
#include <time.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "sync_config.h"
#include "sync_queue.h"
#include "link_detail.h"

#define MAX_LINES (16 + LINK_STATE_MAX_ERRORS)
#define DETAIL_LINE_MAX (BROWSE_STR_MAX + LINK_ERROR_REASON_MAX + 16)
#define LINE_HEIGHT (PILL_SIZE + 4)
#define GAP_HEIGHT (PILL_SIZE / 2)

typedef struct {
	char label[32]; // left column, "" for a full-width line
	char text[DETAIL_LINE_MAX];
	bool dim;
} Line;

static int link_index = 0;
static Line lines[MAX_LINES];
static int line_count = 0;
static int scroll = 0;

static Line *addLine(const char *label, bool dim)
{
	if (line_count >= MAX_LINES) return NULL;
	Line *line = &lines[line_count++];
	snprintf(line->label, sizeof(line->label), "%s", label);
	line->text[0] = '\0';
	line->dim = dim;
	return line;
}

static const char *statusLabel(LinkStatus status)
{
	switch (status) {
	case LINK_STATUS_NEVER: return "jamais";
	case LINK_STATUS_OK: return "réussie";
	case LINK_STATUS_PARTIAL: return "partielle";
	case LINK_STATUS_ERROR: return "échouée";
	case LINK_STATUS_CANCELLED: return "annulée";
	}
	return "";
}

static void addConfig(const Link *link)
{
	Line *l;
	if ((l = addLine("Serveur", false))) {
		if (link->server) snprintf(l->text, sizeof(l->text), "%s (%s)", link->server_name, link->server->host);
		else snprintf(l->text, sizeof(l->text), "%s", link->server_name[0] ? link->server_name : "—");
	}
	if ((l = addLine("Distant", false)))
		snprintf(l->text, sizeof(l->text), "%s%s%s", link->share, link->remote[0] ? "/" : "", link->remote);
	if ((l = addLine("Local", false))) snprintf(l->text, sizeof(l->text), "%s", link->local[0] ? link->local : "—");
	if ((l = addLine("Mode", false))) snprintf(l->text, sizeof(l->text), "%s", link->mode == LINK_MODE_MIRROR ? "Miroir" : "Ajout");
}

static void addCheck(const Link *link)
{
	Line *l = addLine("Vérification", false);
	if (!l) return;
	if (link->config_error[0]) {
		snprintf(l->text, sizeof(l->text), "%s", link->config_error);
		return;
	}

	LinkPhase phase = sync_queue_phase(link_index);
	const LinkCheck *check = sync_queue_check(link_index);
	if (phase != LINK_PHASE_CHECKED) {
		// After a sync the last check is stale, so it isn't shown as current.
		snprintf(l->text, sizeof(l->text), "%s",
		         phase == LINK_PHASE_SYNCED || phase == LINK_PHASE_SYNC_CANCELLED
		             ? "à revérifier (touche X)"
		             : "non vérifiée");
	}
	else if (!check->ok) {
		snprintf(l->text, sizeof(l->text), "Erreur : %s", check->message);
	}
	else if (check->to_copy_count == 0 && check->to_delete_count == 0) {
		snprintf(l->text, sizeof(l->text), "À jour");
	}
	else {
		char size[32];
		UI_formatBytes(check->to_copy_bytes, size, sizeof(size));
		snprintf(l->text, sizeof(l->text), "%d nouveau%s (%s) · %d à supprimer",
		         check->to_copy_count, check->to_copy_count > 1 ? "x" : "", size, check->to_delete_count);
	}
}

static void addLastSync(void)
{
	const LinkState *last = sync_queue_last(link_index);
	Line *l = addLine("Dernière synchro", false);
	if (!l) return;
	if (last->status == LINK_STATUS_NEVER) {
		snprintf(l->text, sizeof(l->text), "jamais");
		return;
	}

	time_t t = last->time;
	char when[32];
	strftime(when, sizeof(when), "%d/%m/%Y à %H:%M", localtime(&t));
	snprintf(l->text, sizeof(l->text), "%s · %s", when, statusLabel(last->status));

	char size[32];
	UI_formatBytes(last->bytes_copied, size, sizeof(size));
	if ((l = addLine("", false)))
		snprintf(l->text, sizeof(l->text), "%d copié%s (%s) · %d supprimé%s",
		         last->files_copied, last->files_copied > 1 ? "s" : "", size,
		         last->files_deleted, last->files_deleted > 1 ? "s" : "");

	if (last->error_total == 0) return;
	if ((l = addLine("", false))) {
		if (last->error_total > last->error_count)
			snprintf(l->text, sizeof(l->text), "%d erreurs (les %d premières) :", last->error_total, last->error_count);
		else
			snprintf(l->text, sizeof(l->text), "%d erreur%s :", last->error_total, last->error_total > 1 ? "s" : "");
	}
	for (int i = 0; i < last->error_count; i++) {
		if (!(l = addLine("", true))) break;
		const LinkError *e = &last->errors[i];
		if (e->path[0]) snprintf(l->text, sizeof(l->text), "  %s — %s", e->path, e->reason);
		else snprintf(l->text, sizeof(l->text), "  %s", e->reason);
	}
}

void LinkDetail_enter(int index)
{
	link_index = index;
	line_count = 0;
	scroll = 0;

	const Link *link = sync_config_link_get(index);
	addConfig(link);
	addLine("", false);
	addCheck(link);
	addLine("", false);
	addLastSync();
}

// How many lines fit, computed by the last render (input has no surface).
static int visible_lines = 1;

static int visibleLines(void)
{
	return visible_lines;
}

LinkDetailAction LinkDetail_input(int *dirty)
{
	if (PAD_justPressed(BTN_B)) return LINK_DETAIL_ACTION_BACK;

	int max_scroll = line_count - visibleLines();
	if (max_scroll < 0) max_scroll = 0;
	if (PAD_justRepeated(BTN_UP) && scroll > 0) {
		scroll--;
		*dirty = 1;
	}
	else if (PAD_justRepeated(BTN_DOWN) && scroll < max_scroll) {
		scroll++;
		*dirty = 1;
	}
	return LINK_DETAIL_ACTION_NONE;
}

void LinkDetail_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);
	UI_renderTitle(screen, sync_config_link_get(link_index)->name, show_setting);

	int x = SCALE1(PADDING);
	int y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int w = screen->w - x * 2;
	int inner = SCALE1(BUTTON_PADDING);
	int label_w = SCALE1(140);

	int bottom = screen->h - SCALE1(PADDING + PILL_SIZE);

	// Text sits on black pills, like the list screen: the background colour
	// is themeable, so plain text on it has no guaranteed contrast. Blank
	// separators are shorter than a row, so how many lines fit depends on
	// which ones they are -- counted here for LinkDetail_input()'s scrolling.
	visible_lines = 0;
	for (int i = scroll; i < line_count; i++) {
		const Line *line = &lines[i];
		if (!line->label[0] && !line->text[0]) {
			if (y + SCALE1(GAP_HEIGHT) > bottom) break;
			y += SCALE1(GAP_HEIGHT);
			visible_lines++;
			continue;
		}
		if (y + SCALE1(PILL_SIZE) > bottom) break;
		visible_lines++;

		GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){ x, y, w, SCALE1(PILL_SIZE) });
		char fit[DETAIL_LINE_MAX];
		int text_y = y + SCALE1(6);
		if (line->label[0]) {
			UI_renderText(screen, line->label, font.small, COLOR_LIGHT_TEXT, x + inner, text_y);
			UI_fitText(font.small, line->text, fit, sizeof(fit), w - label_w - inner * 2);
			UI_renderText(screen, fit, font.small, COLOR_WHITE, x + label_w, text_y);
		}
		else {
			UI_fitText(font.small, line->text, fit, sizeof(fit), w - inner * 2);
			UI_renderText(screen, fit, font.small, line->dim ? COLOR_LIGHT_TEXT : COLOR_WHITE, x + inner, text_y);
		}
		y += SCALE1(LINE_HEIGHT);
	}

	GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 0, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
