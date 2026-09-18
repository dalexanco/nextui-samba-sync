#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "sync_engine.h"
#include "sync_queue.h"
#include "progress.h"

static const Job *job = NULL;
static char error_message[JOB_STR_MAX + 96];

static bool multi_mode = false;

void Progress_enter(const Job *j)
{
	multi_mode = false;
	job = j;
}

void Progress_enterAll(void)
{
	multi_mode = true;
}

bool Progress_isMultiMode(void)
{
	return multi_mode;
}

ProgressAction Progress_input(int *dirty)
{
	*dirty = 1; // animates every frame while running; cheap enough to redraw unconditionally once terminal too

	if (multi_mode) {
		QueueState state = sync_queue_state();
		bool running = (state == QUEUE_STATE_RUNNING);

		if (PAD_justPressed(BTN_B)) {
			if (running) sync_queue_cancel();
			else return PROGRESS_ACTION_BACK;
			return PROGRESS_ACTION_NONE;
		}

		if (running) sync_queue_tick();

		if (sync_queue_state() == QUEUE_STATE_DONE) return PROGRESS_ACTION_DONE;
		return PROGRESS_ACTION_NONE;
	}

	SyncState state = sync_engine_state();
	bool running = (state == SYNC_STATE_COPYING || state == SYNC_STATE_DELETING);

	if (PAD_justPressed(BTN_B)) {
		if (running) sync_engine_cancel();
		else return PROGRESS_ACTION_BACK;
		return PROGRESS_ACTION_NONE;
	}

	if (running) sync_engine_tick();

	state = sync_engine_state();
	if (state == SYNC_STATE_DONE) return PROGRESS_ACTION_DONE;
	if (state == SYNC_STATE_ERROR) {
		snprintf(error_message, sizeof(error_message), "Échec de synchronisation : %s", job->name);
		return PROGRESS_ACTION_ERROR;
	}

	return PROGRESS_ACTION_NONE;
}

const char *Progress_errorMessage(void)
{
	return error_message;
}

void Progress_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);

	// In multi mode, sync_queue.c ticks sync_engine.c directly for whichever
	// job is currently active, so its progress/totals accessors already
	// reflect the right job here -- no separate queue-level accessors needed.
	const Job *active_job = multi_mode ? sync_queue_currentJob() : job;
	bool cancelled = multi_mode
		? (sync_queue_state() == QUEUE_STATE_CANCELLED)
		: (sync_engine_state() == SYNC_STATE_CANCELLED);
	bool running = multi_mode
		? (sync_queue_state() == QUEUE_STATE_RUNNING)
		: (sync_engine_state() == SYNC_STATE_COPYING || sync_engine_state() == SYNC_STATE_DELETING);

	if (multi_mode) {
		UI_renderTitle(screen, "Synchronisation : Tout synchroniser", show_setting);
	}
	else {
		char title[JOB_STR_MAX + 32];
		snprintf(title, sizeof(title), "Synchronisation : %s%s", job->name, job->mirror ? " (Miroir)" : "");
		UI_renderTitle(screen, title, show_setting);
	}

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int x = SCALE1(PADDING);
	int w = screen->w - SCALE1(PADDING * 2);

	if (cancelled) {
		UI_renderTextCentered(screen, "Synchronisation annulée", font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else {
		if (multi_mode) {
			char position_line[JOB_STR_MAX + 32];
			snprintf(position_line, sizeof(position_line), "Job %d/%d : %s%s",
			         sync_queue_current_index() + 1, sync_queue_count(),
			         active_job ? active_job->name : "", (active_job && active_job->mirror) ? " (Miroir)" : "");
			UI_renderText(screen, position_line, font.small, COLOR_WHITE, x, content_y);
			content_y += SCALE1(24);
		}

		SyncProgress progress = sync_engine_progress();
		SyncPreview totals = sync_engine_totals();

		if (progress.current_file[0]) {
			UI_renderText(screen, progress.current_file, font.small, COLOR_WHITE, x, content_y);

			int percent = totals.to_copy_bytes > 0
				? (int)(progress.bytes_copied * 100 / totals.to_copy_bytes)
				: 100;
			if (percent > 100) percent = 100;

			int bar_y = content_y + SCALE1(22);
			int bar_h = SCALE1(8);
			SDL_FillRect(screen, &(SDL_Rect){ x, bar_y, w, bar_h }, SDL_MapRGB(screen->format, 60, 60, 60));
			SDL_FillRect(screen, &(SDL_Rect){ x, bar_y, w * percent / 100, bar_h }, SDL_MapRGB(screen->format, 255, 255, 255));

			char percent_str[8];
			snprintf(percent_str, sizeof(percent_str), "%d%%", percent);
			UI_renderText(screen, percent_str, font.small, COLOR_DARK_TEXT,
			              x + w - UI_textWidth(percent_str, font.small), content_y);
		}

		int line_y = content_y + SCALE1(48);

		char copied_str[32], total_str[32];
		UI_formatBytes(progress.bytes_copied, copied_str, sizeof(copied_str));
		UI_formatBytes(totals.to_copy_bytes, total_str, sizeof(total_str));
		char copy_line[128];
		snprintf(copy_line, sizeof(copy_line), "Copié : %d / %d fichiers · %s / %s",
		         progress.files_copied, totals.to_copy_count, copied_str, total_str);
		UI_renderText(screen, copy_line, font.small, COLOR_WHITE, x, line_y);

		if (active_job && active_job->mirror && totals.to_delete_count > 0) {
			char delete_line[64];
			snprintf(delete_line, sizeof(delete_line), "Suppression des fichiers obsolètes : %d / %d",
			         progress.files_deleted, totals.to_delete_count);
			UI_renderText(screen, delete_line, font.small, COLOR_WHITE, x, line_y + SCALE1(22));
		}
	}

	if (running) {
		GFX_blitButtonGroup((char *[]){ "B", "ANNULER", NULL }, 0, screen, 1);
	}
	else {
		GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 0, screen, 1);
	}
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
