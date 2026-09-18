#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "sync_engine.h"
#include "sync_queue.h"
#include "preview.h"

static const Job *job = NULL;
static SyncPreview preview;

static bool multi_mode = false;
static SyncQueuePreview queue_preview;

void Preview_enter(const Job *j)
{
	multi_mode = false;
	job = j;
	preview = sync_engine_preview(job);
}

void Preview_enterAll(void)
{
	multi_mode = true;
	sync_queue_build();
	queue_preview = sync_queue_preview();
}

bool Preview_isMultiMode(void)
{
	return multi_mode;
}

PreviewAction Preview_input(int *dirty)
{
	(void)dirty; // nothing on this screen changes after Preview_enter()/Preview_enterAll()

	if (PAD_justPressed(BTN_B)) return PREVIEW_ACTION_BACK;

	if (multi_mode) {
		if (sync_queue_count() > 0 && PAD_justPressed(BTN_A)) {
			sync_queue_confirm();
			return PREVIEW_ACTION_START_SYNC;
		}
	}
	else if (preview.ok && PAD_justPressed(BTN_A)) {
		sync_engine_confirm(job);
		return PREVIEW_ACTION_START_SYNC;
	}

	return PREVIEW_ACTION_NONE;
}

void Preview_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);
	UI_renderTitle(screen, "Aperçu de la synchronisation", show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int row_h = SCALE1(26);
	int x = SCALE1(PADDING);

	if (multi_mode) {
		char size_str[32];
		UI_formatBytes(queue_preview.to_copy_bytes, size_str, sizeof(size_str));

		char copy_line[96];
		snprintf(copy_line, sizeof(copy_line), "À copier : %d fichier%s (%s)",
		         queue_preview.to_copy_count, queue_preview.to_copy_count > 1 ? "s" : "", size_str);
		UI_renderText(screen, copy_line, font.medium, COLOR_WHITE, x, content_y);

		if (queue_preview.mirror_count > 0) {
			int delete_y = content_y + row_h + SCALE1(12);
			UI_renderText(screen, "À supprimer (jobs en mode Miroir) :", font.small, COLOR_DARK_TEXT, x, delete_y);

			// One row per mirror job, not scrolled/truncated -- same
			// no-overflow-handling convention as the rest of this codebase's
			// fixed-layout screens (see e.g. summary.c).
			for (int i = 0; i < queue_preview.mirror_count; i++) {
				int count = queue_preview.mirror[i].to_delete_count;
				char line[JOB_STR_MAX + 32];
				snprintf(line, sizeof(line), "%s : %d fichier%s",
				         queue_preview.mirror[i].job->name, count, count > 1 ? "s" : "");
				UI_renderText(screen, line, font.small, COLOR_WHITE,
				              x + SCALE1(12), delete_y + row_h + i * SCALE1(20));
			}
		}

		GFX_blitButtonGroup((char *[]){ "A", "LANCER LA SYNCHRONISATION", NULL }, 0, screen, 0);
	}
	else if (!preview.ok) {
		UI_renderTextCentered(screen, "Impossible de calculer l'aperçu (connexion ou lecture impossible)",
		                       font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else {
		char size_str[32];
		UI_formatBytes(preview.to_copy_bytes, size_str, sizeof(size_str));

		char copy_line[96];
		snprintf(copy_line, sizeof(copy_line), "À copier : %d fichier%s (%s)",
		         preview.to_copy_count, preview.to_copy_count > 1 ? "s" : "", size_str);
		UI_renderText(screen, copy_line, font.medium, COLOR_WHITE, x, content_y);

		if (job->mirror) {
			int delete_y = content_y + row_h + SCALE1(12);
			UI_renderText(screen, "À supprimer (mode Miroir) :", font.small, COLOR_DARK_TEXT, x, delete_y);

			char delete_line[64];
			snprintf(delete_line, sizeof(delete_line), "%d fichier%s",
			         preview.to_delete_count, preview.to_delete_count > 1 ? "s" : "");
			UI_renderText(screen, delete_line, font.medium, COLOR_WHITE, x, delete_y + row_h);
		}

		if (preview.ok) GFX_blitButtonGroup((char *[]){ "A", "LANCER LA SYNCHRONISATION", NULL }, 0, screen, 0);
	}

	GFX_blitButtonGroup((char *[]){ "B", "ANNULER", NULL }, 1, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
