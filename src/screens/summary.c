#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "sync_engine.h"
#include "sync_queue.h"
#include "summary.h"

static const Job *job = NULL;
static bool multi_mode = false;

void Summary_enter(const Job *j)
{
	multi_mode = false;
	job = j;
}

void Summary_enterAll(void)
{
	multi_mode = true;
}

bool Summary_isMultiMode(void)
{
	return multi_mode;
}

SummaryAction Summary_input(int *dirty)
{
	(void)dirty; // static once entered, nothing here changes frame to frame

	if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) return SUMMARY_ACTION_BACK;
	return SUMMARY_ACTION_NONE;
}

static void renderSingle(SDL_Surface *screen, int show_setting)
{
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
}

// Aggregate header (all jobs summed) followed by one breakdown line per job,
// per SPEC.md's multi-job écran 5bis mockup. Plain text rather than the
// mockup's ✔/🗑/✘ glyphs, matching this codebase's existing deviation
// (renderSingle() above and error.c/progress.c never use them either).
static void renderMulti(SDL_Surface *screen, int show_setting)
{
	int result_count = sync_queue_resultCount();

	char title[48];
	snprintf(title, sizeof(title), "Synchronisation terminée : %d jobs", result_count);
	UI_renderTitle(screen, title, show_setting);

	int line_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int row_h = SCALE1(22);
	int x = SCALE1(PADDING);

	int total_copied = 0, total_deleted = 0, total_errors = 0;
	long long total_bytes = 0;
	for (int i = 0; i < result_count; i++) {
		const SyncQueueResult *r = sync_queue_result(i);
		if (r->ok) {
			total_copied += r->files_copied;
			total_bytes += r->bytes_copied;
			total_deleted += r->files_deleted;
		}
		else {
			total_errors++;
		}
	}

	char size_str[32];
	UI_formatBytes(total_bytes, size_str, sizeof(size_str));
	char agg_line[128];
	snprintf(agg_line, sizeof(agg_line), "%d fichiers copiés (%s) · %d supprimés · %d erreur%s",
	         total_copied, size_str, total_deleted, total_errors, total_errors > 1 ? "s" : "");
	UI_renderText(screen, agg_line, font.medium, COLOR_WHITE, x, line_y);
	line_y += row_h + SCALE1(14);

	for (int i = 0; i < result_count; i++) {
		const SyncQueueResult *r = sync_queue_result(i);
		char line[JOB_STR_MAX + 64];
		if (!r->ok) {
			snprintf(line, sizeof(line), "%s%s : erreur", r->job->name, r->job->mirror ? " (Miroir)" : "");
		}
		else if (r->job->mirror) {
			snprintf(line, sizeof(line), "%s (Miroir) : %d copié%s, %d supprimé%s",
			         r->job->name, r->files_copied, r->files_copied > 1 ? "s" : "",
			         r->files_deleted, r->files_deleted > 1 ? "s" : "");
		}
		else {
			snprintf(line, sizeof(line), "%s : %d copié%s",
			         r->job->name, r->files_copied, r->files_copied > 1 ? "s" : "");
		}
		UI_renderText(screen, line, font.small, COLOR_WHITE, x + SCALE1(12), line_y + i * SCALE1(20));
	}
}

void Summary_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);

	if (multi_mode) renderMulti(screen, show_setting);
	else renderSingle(screen, show_setting);

	GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 0, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
