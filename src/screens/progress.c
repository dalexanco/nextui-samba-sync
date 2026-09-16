#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "sync_engine.h"
#include "progress.h"

static const Job *job = NULL;

void Progress_enter(const Job *j)
{
	job = j;
}

ProgressAction Progress_input(int *dirty)
{
	*dirty = 1; // animates every frame while running; cheap enough to redraw unconditionally once terminal too

	SyncState state = sync_engine_state();
	bool running = (state == SYNC_STATE_COPYING || state == SYNC_STATE_DELETING);

	if (PAD_justPressed(BTN_B)) {
		if (running) sync_engine_cancel();
		else return PROGRESS_ACTION_BACK;
		return PROGRESS_ACTION_NONE;
	}

	if (running) sync_engine_tick();

	return PROGRESS_ACTION_NONE;
}

void Progress_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);

	char title[JOB_STR_MAX + 32];
	snprintf(title, sizeof(title), "Synchronisation : %s%s", job->name, job->mirror ? " (Miroir)" : "");
	UI_renderTitle(screen, title, show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int x = SCALE1(PADDING);
	int w = screen->w - SCALE1(PADDING * 2);

	SyncState state = sync_engine_state();
	SyncProgress progress = sync_engine_progress();
	SyncPreview totals = sync_engine_totals();

	if (state == SYNC_STATE_ERROR) {
		UI_renderTextCentered(screen, "Erreur pendant la synchronisation (connexion perdue ou écriture impossible)",
		                       font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else if (state == SYNC_STATE_CANCELLED) {
		UI_renderTextCentered(screen, "Synchronisation annulée", font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else {
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

		if (job->mirror && totals.to_delete_count > 0) {
			char delete_line[64];
			snprintf(delete_line, sizeof(delete_line), "Suppression des fichiers obsolètes : %d / %d",
			         progress.files_deleted, totals.to_delete_count);
			UI_renderText(screen, delete_line, font.small, COLOR_WHITE, x, line_y + SCALE1(22));
		}

		if (state == SYNC_STATE_DONE) {
			UI_renderText(screen, "Synchronisation terminée", font.medium, COLOR_WHITE, x, line_y + SCALE1(48));
		}
	}

	if (state == SYNC_STATE_COPYING || state == SYNC_STATE_DELETING) {
		GFX_blitButtonGroup((char *[]){ "B", "ANNULER", NULL }, 0, screen, 1);
	}
	else {
		GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 0, screen, 1);
	}
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
