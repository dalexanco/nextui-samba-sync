#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "sync_config.h"
#include "sync_queue.h"
#include "links_list.h"

#define ROW_HEIGHT (PILL_SIZE + 6)

// While syncing, the copy loop owns the frame: redrawing a percentage 60
// times a second would spend a good third of the wall time in the renderer
// instead of in the transfer. 4 Hz is plenty for a percentage.
#define SYNC_REDRAW_MS 250

static int selected = 0;
static int scroll_offset = 0;

LinksListAction LinksList_input(int *dirty)
{
	QueueMode mode = sync_queue_mode();
	int count = sync_config_link_count();

	if (PAD_justPressed(BTN_B)) {
		if (mode == QUEUE_SYNCING) {
			sync_queue_cancel();
			*dirty = 1;
		}
		else {
			return LINKS_LIST_ACTION_QUIT; // an interrupted check needs no cleanup
		}
	}
	else if (mode == QUEUE_IDLE && PAD_justPressed(BTN_A) && sync_queue_can_sync()) {
		sync_queue_sync_all();
		*dirty = 1;
	}
	else if (mode == QUEUE_IDLE && PAD_justPressed(BTN_X)) {
		// Also picks up edits made to Samba Sync.toml while the pak runs.
		sync_queue_reload();
		if (selected >= sync_config_link_count()) selected = 0;
		sync_queue_check_all();
		*dirty = 1;
	}
	else if (mode != QUEUE_SYNCING && count > 0 && PAD_justPressed(BTN_Y)) {
		// A check in progress just pauses while screen 2 is open: only this
		// screen ticks the queue.
		return LINKS_LIST_ACTION_DETAIL;
	}
	else if (count > 0 && PAD_justRepeated(BTN_UP)) {
		selected = (selected - 1 + count) % count;
		*dirty = 1;
	}
	else if (count > 0 && PAD_justRepeated(BTN_DOWN)) {
		selected = (selected + 1) % count;
		*dirty = 1;
	}

	if (sync_queue_tick()) {
		static uint32_t last_redraw = 0;
		uint32_t now = SDL_GetTicks();
		// Anything but a sync in progress (a check finishing, the run ending)
		// is a one-off change and redraws right away.
		if (sync_queue_mode() != QUEUE_SYNCING || now - last_redraw >= SYNC_REDRAW_MS) {
			last_redraw = now;
			*dirty = 1;
		}
	}
	return LINKS_LIST_ACTION_NONE;
}

int LinksList_selected(void)
{
	return selected;
}

static void plural(char *out, size_t out_size, int n, const char *singular, const char *plural_form)
{
	snprintf(out, out_size, "%d %s", n, n > 1 ? plural_form : singular);
}

// Right-hand status of a link, per the phases described in SPEC.md screen 1.
static void statusText(int index, char *out, size_t out_size)
{
	const Link *link = sync_config_link_get(index);
	if (link->config_error[0]) {
		snprintf(out, out_size, "%s", link->config_error);
		return;
	}

	char a[64], b[64];
	switch (sync_queue_phase(index)) {
	case LINK_PHASE_IDLE:
		snprintf(out, out_size, "Not checked");
		return;
	case LINK_PHASE_CHECK_PENDING:
	case LINK_PHASE_SYNC_PENDING:
		snprintf(out, out_size, "Waiting");
		return;
	case LINK_PHASE_CHECKING:
		snprintf(out, out_size, "Checking…");
		return;
	case LINK_PHASE_CHECKED: {
		// Just "there is something to do" here; the counts (and what mirror
		// mode would delete) are on screen 2.
		const LinkCheck *check = sync_queue_check(index);
		if (!check->ok) snprintf(out, out_size, "Error: %s", check->message);
		else if (check->to_copy_count == 0 && check->to_delete_count == 0) snprintf(out, out_size, "Up to date");
		else snprintf(out, out_size, "[New]");
		return;
	}
	case LINK_PHASE_SYNCING: {
		SyncState state = sync_engine_state();
		SyncProgress p = sync_engine_progress();
		if (state == SYNC_STATE_DELETING) {
			snprintf(out, out_size, "Deleting %d/%d", p.files_deleted, p.to_delete_count);
		}
		else if (state == SYNC_STATE_COPYING && p.current_file[0]) {
			// Progress by volume, not by file count: files vary wildly in
			// size, so "5/8" tells you little about how long is left.
			int percent = p.to_copy_bytes > 0 ? (int)(p.bytes_done * 100 / p.to_copy_bytes) : 100;
			if (percent > 100) percent = 100;
			const char *name = strrchr(p.current_file, '/');
			name = name ? name + 1 : p.current_file;
			char total[32];
			UI_formatBytes(p.to_copy_bytes, total, sizeof(total));
			snprintf(out, out_size, "%s · %d%% of %s", name, percent, total);
		}
		else {
			snprintf(out, out_size, "Connecting…");
		}
		return;
	}
	case LINK_PHASE_SYNCED: {
		const LinkState *last = sync_queue_last(index);
		plural(a, sizeof(a), last->files_copied, "copied", "copied");
		plural(b, sizeof(b), last->files_deleted, "deleted", "deleted");
		if (last->status == LINK_STATUS_ERROR) {
			snprintf(out, out_size, "Error: %s", last->error_count > 0 ? last->errors[0].reason : "failed");
		}
		else if (last->status == LINK_STATUS_CANCELLED) {
			snprintf(out, out_size, "Cancelled · %s", a);
		}
		else if (last->status == LINK_STATUS_PARTIAL) {
			char c[64];
			plural(c, sizeof(c), last->error_total, "error", "errors");
			snprintf(out, out_size, "Partial · %s · %s", a, c);
		}
		else if (last->files_copied == 0 && last->files_deleted == 0) {
			snprintf(out, out_size, "Up to date");
		}
		else if (last->files_deleted > 0) {
			snprintf(out, out_size, "%s · %s", a, b);
		}
		else {
			snprintf(out, out_size, "%s", a);
		}
		return;
	}
	case LINK_PHASE_SYNC_CANCELLED:
		snprintf(out, out_size, "Cancelled");
		return;
	}
}

static void renderRow(SDL_Surface *screen, int index, bool is_selected, int y)
{
	const Link *link = sync_config_link_get(index);
	int x = SCALE1(PADDING);
	int w = screen->w - SCALE1(PADDING * 2);
	int inner = SCALE1(BUTTON_PADDING);

	GFX_blitPill(is_selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
	             &(SDL_Rect){ x, y, w, SCALE1(PILL_SIZE) });
	SDL_Color color = is_selected ? COLOR_BLACK : COLOR_WHITE;

	// Status first: it gets up to 60% of the row, the name gets the rest.
	char status[CONFIG_ERROR_MAX + BROWSE_STR_MAX];
	statusText(index, status, sizeof(status));
	char status_fit[sizeof(status)];
	int status_w = UI_fitText(font.small, status, status_fit, sizeof(status_fit), (w - inner * 2) * 6 / 10);

	char label[CONFIG_STR_MAX + 32];
	snprintf(label, sizeof(label), "%s%s%s",
	         link_state_failed(sync_queue_last(index)) ? "! " : "",
	         link->name,
	         link->mode == LINK_MODE_MIRROR ? " [Mirror]" : "");
	char label_fit[sizeof(label)];
	UI_fitText(font.medium, label, label_fit, sizeof(label_fit), w - inner * 3 - status_w);

	UI_renderText(screen, label_fit, font.medium, color, x + inner, y + SCALE1(4));
	UI_renderText(screen, status_fit, font.small, color, x + w - inner - status_w, y + SCALE1(6));
}

// "Last sync: 18/09 at 18:42 · 1 failed" -- most recent end time among
// every link's persisted state, and how many links failed their last sync.
static void footerText(char *out, size_t out_size)
{
	int last_time = 0;
	int failures = 0;
	for (int i = 0; i < sync_config_link_count(); i++) {
		const LinkState *last = sync_queue_last(i);
		if (last->time > last_time) last_time = last->time;
		if (link_state_failed(last)) failures++;
	}
	if (last_time == 0) {
		snprintf(out, out_size, "Never synced");
		return;
	}

	time_t t = last_time;
	char when[32];
	strftime(when, sizeof(when), "%d/%m at %H:%M", localtime(&t));
	if (failures > 0) snprintf(out, out_size, "Last sync: %s · %d failed", when, failures);
	else snprintf(out, out_size, "Last sync: %s", when);
}

static void renderEmpty(SDL_Surface *screen, int y)
{
	const char *line1 = NULL;
	const char *line2 = NULL;
	switch (sync_config_status()) {
	case CONFIG_MISSING:
		line1 = "No configuration";
		line2 = "Create “" CONFIG_FILE_NAME "” at the root of the SD card";
		break;
	case CONFIG_PARSE_ERROR:
		line1 = "Configuration unreadable";
		line2 = sync_config_error_message();
		break;
	case CONFIG_OK:
		line1 = "No links";
		line2 = "Add a [links.\"…\"] table to “" CONFIG_FILE_NAME "”";
		break;
	}

	int max_w = screen->w - SCALE1(PADDING * 2);
	char fit[CONFIG_ERROR_MAX + 64];
	UI_renderTextCentered(screen, line1, font.medium, COLOR_WHITE, y + SCALE1(20));
	UI_fitText(font.small, line2, fit, sizeof(fit), max_w);
	UI_renderTextCentered(screen, fit, font.small, COLOR_DARK_TEXT, y + SCALE1(20 + PILL_SIZE));
}

void LinksList_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);

	QueueMode mode = sync_queue_mode();
	char title[64];
	if (mode == QUEUE_SYNCING)
		snprintf(title, sizeof(title), "Syncing %d/%d", sync_queue_sync_position(), sync_queue_sync_total());
	else if (sync_queue_sync_finished())
		snprintf(title, sizeof(title), "Sync finished");
	else
		snprintf(title, sizeof(title), "Samba Sync");
	UI_renderTitle(screen, title, show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int count = sync_config_link_count();

	if (count == 0) {
		renderEmpty(screen, content_y);
	}
	else {
		// Rows, then one footer line, then the button hints.
		int footer_h = SCALE1(PILL_SIZE);
		int visible_rows = (screen->h - content_y - footer_h - SCALE1(PADDING + PILL_SIZE)) / SCALE1(ROW_HEIGHT);
		if (visible_rows < 1) visible_rows = 1;

		if (selected < scroll_offset) scroll_offset = selected;
		if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;

		// Scroll to keep the link being worked on visible, without moving the
		// user's cursor.
		if (mode != QUEUE_IDLE) {
			for (int i = 0; i < count; i++) {
				LinkPhase phase = sync_queue_phase(i);
				if (phase != LINK_PHASE_CHECKING && phase != LINK_PHASE_SYNCING) continue;
				if (i < scroll_offset) scroll_offset = i;
				else if (i >= scroll_offset + visible_rows) scroll_offset = i - visible_rows + 1;
				break;
			}
		}

		int last = MIN(count, scroll_offset + visible_rows);
		for (int i = scroll_offset; i < last; i++)
			renderRow(screen, i, i == selected, content_y + (i - scroll_offset) * SCALE1(ROW_HEIGHT));

		char footer[128];
		footerText(footer, sizeof(footer));
		int footer_y = screen->h - SCALE1(PADDING + PILL_SIZE) - footer_h + SCALE1(4);
		UI_renderText(screen, footer, font.small, COLOR_DARK_TEXT, SCALE1(PADDING + BUTTON_PADDING), footer_y);
	}

	if (mode == QUEUE_SYNCING) {
		GFX_blitButtonGroup((char *[]){ "B", "CANCEL", NULL }, 0, screen, 1);
	}
	else {
		if (count > 0 && mode == QUEUE_IDLE) GFX_blitButtonGroup((char *[]){ "X", "CHECK", "Y", "DETAILS", NULL }, 0, screen, 0);
		else if (count > 0) GFX_blitButtonGroup((char *[]){ "Y", "DETAILS", NULL }, 0, screen, 0);
		else GFX_blitButtonGroup((char *[]){ "X", "RELOAD", NULL }, 0, screen, 0);

		if (sync_queue_can_sync()) GFX_blitButtonGroup((char *[]){ "A", "SYNC", "B", "QUIT", NULL }, 1, screen, 1);
		else GFX_blitButtonGroup((char *[]){ "B", "QUIT", NULL }, 1, screen, 1);
	}
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
