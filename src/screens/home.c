#include <stdio.h>
#include <string.h>
#include <time.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "ui.h"
#include "jobs.h"
#include "home.h"

// Status line: job count, most recent sync across all jobs (regardless of
// per-job status), and how many jobs are currently in error -- mirrors the
// example in SPEC.md ("dernière synchro : il y a 2h · 1 job en erreur").
static void statusLine(char *out, size_t out_size)
{
	int count = jobs_count();
	if (count == 0) {
		snprintf(out, out_size, "Aucun job configuré · Y pour en créer un");
		return;
	}

	int error_count = 0;
	int last_sync_time = 0;
	for (int i = 0; i < count; i++) {
		const Job *job = jobs_get(i);
		if (strcmp(job->last_sync_status, "error") == 0) error_count++;
		if (job->last_sync_time > last_sync_time) last_sync_time = job->last_sync_time;
	}

	char sync_part[STR_MAX];
	if (last_sync_time > 0) {
		char duration[64];
		serializeTime(duration, (int)(time(NULL) - last_sync_time));
		snprintf(sync_part, sizeof(sync_part), "dernière synchro : il y a %s", duration);
	}
	else {
		snprintf(sync_part, sizeof(sync_part), "jamais synchronisé");
	}

	if (error_count > 0) {
		snprintf(out, out_size, "%d job%s configuré%s · %s · %d en erreur",
		         count, count > 1 ? "s" : "", count > 1 ? "s" : "", sync_part, error_count);
	}
	else {
		snprintf(out, out_size, "%d job%s configuré%s · %s",
		         count, count > 1 ? "s" : "", count > 1 ? "s" : "", sync_part);
	}
}

HomeAction Home_input(void)
{
	if (PAD_justPressed(BTN_B)) return HOME_ACTION_QUIT;
	if (PAD_justPressed(BTN_Y)) return HOME_ACTION_MANAGE_JOBS;
	if (PAD_justPressed(BTN_A) && jobs_count() > 0) return HOME_ACTION_SYNC_ALL;
	return HOME_ACTION_NONE;
}

void Home_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);
	UI_renderTitle(screen, "Samba Sync", show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN) + SCALE1(30);

	bool active = jobs_count() > 0;
	int w = SCALE1(240);
	int x = (screen->w - w) / 2;
	GFX_blitPill(active ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
	             &(SDL_Rect){ x, content_y, w, SCALE1(PILL_SIZE) });
	UI_renderTextCentered(screen, "Tout synchroniser", font.medium, active ? COLOR_BLACK : COLOR_DARK_TEXT, content_y + SCALE1(4));

	char status[STR_MAX];
	statusLine(status, sizeof(status));
	UI_renderTextCentered(screen, status, font.small, COLOR_DARK_TEXT, content_y + SCALE1(PILL_SIZE + 16));

	GFX_blitButtonGroup((char *[]){ "Y", "GERER LES JOBS", NULL }, 0, screen, 0);
	GFX_blitButtonGroup((char *[]){ "A", "SYNCHRONISER", "B", "QUITTER", NULL }, 1, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
