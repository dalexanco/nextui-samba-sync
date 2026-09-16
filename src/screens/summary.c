#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "sync_engine.h"
#include "summary.h"

static const Job *job = NULL;

void Summary_enter(const Job *j)
{
	job = j;
}

SummaryAction Summary_input(int *dirty)
{
	(void)dirty; // static once entered, nothing here changes frame to frame

	if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) return SUMMARY_ACTION_BACK;
	return SUMMARY_ACTION_NONE;
}

void Summary_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);

	char title[JOB_STR_MAX + 32];
	snprintf(title, sizeof(title), "Synchronisation terminée : %s%s", job->name, job->mirror ? " (Miroir)" : "");
	UI_renderTitle(screen, title, show_setting);

	int line_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int row_h = SCALE1(26);
	int x = SCALE1(PADDING);

	SyncProgress progress = sync_engine_progress();
	SyncPreview totals = sync_engine_totals();

	char size_str[32];
	UI_formatBytes(progress.bytes_copied, size_str, sizeof(size_str));
	char copied_line[96];
	snprintf(copied_line, sizeof(copied_line), "Copiés : %d fichier%s (%s)",
	         progress.files_copied, progress.files_copied > 1 ? "s" : "", size_str);
	UI_renderText(screen, copied_line, font.medium, COLOR_WHITE, x, line_y);
	line_y += row_h;

	char ignored_line[64];
	snprintf(ignored_line, sizeof(ignored_line), "Déjà présents (ignorés) : %d fichier%s",
	         totals.already_present_count, totals.already_present_count > 1 ? "s" : "");
	UI_renderText(screen, ignored_line, font.medium, COLOR_WHITE, x, line_y);
	line_y += row_h;

	if (job->mirror) {
		char deleted_line[64];
		snprintf(deleted_line, sizeof(deleted_line), "Supprimés (obsolètes) : %d fichier%s",
		         progress.files_deleted, progress.files_deleted > 1 ? "s" : "");
		UI_renderText(screen, deleted_line, font.medium, COLOR_WHITE, x, line_y);
		line_y += row_h;
	}

	UI_renderText(screen, "Erreurs : 0", font.medium, COLOR_WHITE, x, line_y);

	GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 0, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
