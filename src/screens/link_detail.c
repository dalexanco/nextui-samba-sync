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
#define MAX_ROWS (MAX_LINES * 3) // a value may wrap over several rows
#define MAX_WRAP 8               // rows a single value may take
#define DETAIL_LINE_MAX (BROWSE_STR_MAX + LINK_ERROR_REASON_MAX + 16)
#define LINE_HEIGHT (PILL_SIZE + 4)
#define GAP_HEIGHT (PILL_SIZE / 2)

typedef struct {
	char label[32]; // left column, "" for a line with no label of its own
	char text[DETAIL_LINE_MAX];
	bool dim;
	bool indent; // no label, but still aligned under the value column
} Line;

// One rendered row: a line whose value fitted, or one slice of a wrapped
// value. Rebuilt on every render, since wrapping depends on the surface width.
typedef struct {
	const char *label;  // "" except on a value's first row
	char text[DETAIL_LINE_MAX];
	bool dim;
	bool gap;    // blank separator, half height, nothing drawn
	bool indent; // continuation of a labelled value: aligned under it
} Row;

static int link_index = 0;
static Line lines[MAX_LINES];
static int line_count = 0;
static Row rows[MAX_ROWS];
static int row_count = 0;
static int scroll = 0;

static Line *addLine(const char *label, bool dim)
{
	if (line_count >= MAX_LINES) return NULL;
	Line *line = &lines[line_count++];
	snprintf(line->label, sizeof(line->label), "%s", label);
	line->text[0] = '\0';
	line->dim = dim;
	line->indent = false;
	return line;
}

// A further line of the value above: no label, same alignment.
static Line *addValueLine(bool dim)
{
	Line *line = addLine("", dim);
	if (line) line->indent = true;
	return line;
}

static const char *statusLabel(LinkStatus status)
{
	switch (status) {
	case LINK_STATUS_NEVER: return "never";
	case LINK_STATUS_OK: return "succeeded";
	case LINK_STATUS_PARTIAL: return "partial";
	case LINK_STATUS_ERROR: return "failed";
	case LINK_STATUS_CANCELLED: return "cancelled";
	}
	return "";
}

static void addConfig(const Link *link)
{
	Line *l;
	if ((l = addLine("Server", false))) {
		if (link->server) snprintf(l->text, sizeof(l->text), "%s (%s)", link->server_name, link->server->host);
		else snprintf(l->text, sizeof(l->text), "%s", link->server_name[0] ? link->server_name : "—");
	}
	if ((l = addLine("Remote", false)))
		snprintf(l->text, sizeof(l->text), "%s%s%s", link->share, link->remote[0] ? "/" : "", link->remote);
	if ((l = addLine("Local", false))) snprintf(l->text, sizeof(l->text), "%s", link->local[0] ? link->local : "—");
	if ((l = addLine("Mode", false))) snprintf(l->text, sizeof(l->text), "%s", link->mode == LINK_MODE_MIRROR ? "Mirror" : "Add");
}

static void addCheck(const Link *link)
{
	Line *l = addLine("Check", false);
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
		             ? "stale, press X to re-check"
		             : "not checked");
	}
	else if (!check->ok) {
		snprintf(l->text, sizeof(l->text), "Error: %s", check->message);
	}
	else if (check->to_copy_count == 0 && check->to_delete_count == 0) {
		snprintf(l->text, sizeof(l->text), "Up to date");
	}
	else {
		// One figure per line: what arrives, then what goes away.
		char size[32];
		UI_formatBytes(check->to_copy_bytes, size, sizeof(size));
		snprintf(l->text, sizeof(l->text), "%d new (%s)", check->to_copy_count, size);
		if (link->mode == LINK_MODE_MIRROR && (l = addValueLine(false))) {
			UI_formatBytes(check->to_delete_bytes, size, sizeof(size));
			snprintf(l->text, sizeof(l->text), "%d to delete (%s)", check->to_delete_count, size);
		}
		// Net effect on the card: negative when mirror mode frees more than
		// the copies bring in.
		if ((l = addValueLine(false))) {
			long long delta = check->delta_bytes;
			UI_formatBytes(delta < 0 ? -delta : delta, size, sizeof(size));
			snprintf(l->text, sizeof(l->text), "%s%s on the SD card", delta < 0 ? "-" : "+", size);
		}
	}
}

static void addLastSync(void)
{
	const LinkState *last = sync_queue_last(link_index);
	Line *l = addLine("Last sync", false);
	if (!l) return;
	if (last->status == LINK_STATUS_NEVER) {
		snprintf(l->text, sizeof(l->text), "never");
		return;
	}

	time_t t = last->time;
	char when[32];
	strftime(when, sizeof(when), "%d/%m/%Y at %H:%M", localtime(&t));
	// Date and status on separate lines: joined by a "·" they overflow the
	// value column and wrap, leaving the separator dangling at the end.
	snprintf(l->text, sizeof(l->text), "%s", when);
	if ((l = addValueLine(false)))
		snprintf(l->text, sizeof(l->text), "%s", statusLabel(last->status));

	char size[32];
	UI_formatBytes(last->bytes_copied, size, sizeof(size));
	if ((l = addValueLine(false)))
		snprintf(l->text, sizeof(l->text), "%d copied (%s)", last->files_copied, size);
	if ((l = addValueLine(false)))
		snprintf(l->text, sizeof(l->text), "%d deleted", last->files_deleted);

	if (last->error_total == 0) return;
	if ((l = addValueLine(false))) {
		if (last->error_total > last->error_count)
			snprintf(l->text, sizeof(l->text), "%d errors (first %d):", last->error_total, last->error_count);
		else
			snprintf(l->text, sizeof(l->text), "%d error%s:", last->error_total, last->error_total > 1 ? "s" : "");
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

	// What you open this screen for comes first -- the configuration is
	// reference material, and pushed the figures off the bottom of the screen.
	const Link *link = sync_config_link_get(index);
	addCheck(link);
	addLine("", false);
	addLastSync();
	addLine("", false);
	addConfig(link);
}

// How many rows fit, computed by the last render (input has no surface).
static int visible_lines = 1;

static int visibleLines(void)
{
	return visible_lines;
}

LinkDetailAction LinkDetail_input(int *dirty)
{
	if (PAD_justPressed(BTN_B)) return LINK_DETAIL_ACTION_BACK;

	int max_scroll = row_count - visibleLines();
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

// Wraps every line's value to the width available for it, so long values run
// over several rows instead of being cut off with an ellipsis.
static void buildRows(TTF_Font *font, int value_width, int full_width)
{
	row_count = 0;
	for (int i = 0; i < line_count && row_count < MAX_ROWS; i++) {
		const Line *line = &lines[i];
		if (!line->label[0] && !line->text[0]) {
			rows[row_count].label = "";
			rows[row_count].text[0] = '\0';
			rows[row_count].dim = false;
			rows[row_count].gap = true;
			rows[row_count].indent = false;
			row_count++;
			continue;
		}

		bool labelled = line->label[0] != '\0';
		bool aligned = labelled || line->indent;
		static char wrapped[MAX_WRAP][DETAIL_LINE_MAX];
		int n = UI_wrapText(font, line->text, wrapped[0], DETAIL_LINE_MAX, MAX_WRAP,
		                    aligned ? value_width : full_width);

		for (int k = 0; k < n && row_count < MAX_ROWS; k++) {
			rows[row_count].label = (labelled && k == 0) ? line->label : "";
			snprintf(rows[row_count].text, DETAIL_LINE_MAX, "%s", wrapped[k]);
			rows[row_count].dim = line->dim;
			rows[row_count].gap = false;
			rows[row_count].indent = aligned;
			row_count++;
		}
	}
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

	buildRows(font.small, w - label_w - inner, w - inner * 2);
	if (scroll > row_count - 1) scroll = row_count > 0 ? row_count - 1 : 0;

	// Text sits on black pills, like the list screen: the background colour
	// is themeable, so plain text on it has no guaranteed contrast. Blank
	// separators are shorter than a row, so how many rows fit depends on
	// which ones they are -- counted here for LinkDetail_input()'s scrolling.
	visible_lines = 0;
	for (int i = scroll; i < row_count; i++) {
		const Row *row = &rows[i];
		if (row->gap) {
			if (y + SCALE1(GAP_HEIGHT) > bottom) break;
			y += SCALE1(GAP_HEIGHT);
			visible_lines++;
			continue;
		}
		if (y + SCALE1(PILL_SIZE) > bottom) break;
		visible_lines++;

		GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){ x, y, w, SCALE1(PILL_SIZE) });
		int text_y = y + SCALE1(6);
		if (row->label[0]) UI_renderText(screen, row->label, font.small, COLOR_LIGHT_TEXT, x + inner, text_y);
		UI_renderText(screen, row->text, font.small, row->dim ? COLOR_LIGHT_TEXT : COLOR_WHITE,
		              row->indent ? x + label_w : x + inner, text_y);
		y += SCALE1(LINE_HEIGHT);
	}

	GFX_blitButtonGroup((char *[]){ "B", "BACK", NULL }, 0, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
