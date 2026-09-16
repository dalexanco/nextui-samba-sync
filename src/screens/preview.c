#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "sync_engine.h"
#include "preview.h"

static const Job *job = NULL;
static SyncPreview preview;

void Preview_enter(const Job *j)
{
	job = j;
	preview = sync_engine_preview(job);
}

PreviewAction Preview_input(int *dirty)
{
	(void)dirty; // nothing on this screen changes after Preview_enter()

	if (PAD_justPressed(BTN_B)) return PREVIEW_ACTION_BACK;
	if (preview.ok && PAD_justPressed(BTN_A)) {
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

	if (!preview.ok) {
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
	}

	if (preview.ok) GFX_blitButtonGroup((char *[]){ "A", "LANCER LA SYNCHRONISATION", NULL }, 0, screen, 0);
	GFX_blitButtonGroup((char *[]){ "B", "ANNULER", NULL }, 1, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
