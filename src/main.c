// Samba Sync.pak -- pull-sync folders from Samba/SMB shares onto the SD
// card. Servers are declared offline in Samba Servers/<name>/server.txt;
// see ../SPEC.md for the UX and ../docs/ARCHITECTURE.md for the design.
//
// Écran 0 (accueil) and Écran 1 (gestion des jobs) are wired below; the
// rest of the screens in docs/ARCHITECTURE.md (assistant, browse, progress,
// settings...) don't exist yet, so "Tout synchroniser"/A/X/Y/MENU on those
// two screens are only as active as their backing modules allow.

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <msettings.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "defines.h"
#include "api.h"
#include "servers.h"
#include "jobs.h"
#include "screens/home.h"
#include "screens/jobs_list.h"

static bool quit = false;

static void sigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) quit = true;
}

// Proves the Makefile's libsmb2 include/link wiring actually works end to
// end (a static lib with no referenced symbols can "link" successfully
// while silently contributing nothing) -- not a functional SMB check.
static void checkLibsmb2Linked(void)
{
	struct smb2_context *smb2 = smb2_init_context();
	if (smb2) {
		LOG_info("sambasync: libsmb2 context init OK\n");
		smb2_destroy_context(smb2);
	}
	else {
		LOG_info("sambasync: libsmb2 context init FAILED\n");
	}
}

typedef enum {
	SCREEN_HOME,
	SCREEN_JOBS_LIST,
} Screen;

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;

	InitSettings();
	PWR_setCPUSpeed(CPU_SPEED_AUTO);

	SDL_Surface *screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();

	signal(SIGINT, sigHandler);
	signal(SIGTERM, sigHandler);

	checkLibsmb2Linked();

	servers_rescan();
	jobs_rescan();

	Screen active_screen = SCREEN_HOME;

	int dirty = 1;
	int show_setting = 0;
	while (!quit) {
		GFX_startFrame();
		PAD_poll();

		switch (active_screen) {
		case SCREEN_HOME: {
			HomeAction action = Home_input();
			if (action == HOME_ACTION_QUIT) quit = true;
			else if (action == HOME_ACTION_MANAGE_JOBS) {
				JobsList_reset();
				active_screen = SCREEN_JOBS_LIST;
				dirty = 1;
			}
			// HOME_ACTION_SYNC_ALL: no sync_engine yet, nothing to do.
			break;
		}
		case SCREEN_JOBS_LIST: {
			JobsListAction action = JobsList_input(&dirty);
			if (action == JOBS_LIST_ACTION_BACK) {
				active_screen = SCREEN_HOME;
				dirty = 1;
			}
			break;
		}
		}

		PWR_update(&dirty, &show_setting, NULL, NULL);

		if (dirty) {
			switch (active_screen) {
			case SCREEN_HOME: Home_render(screen, show_setting); break;
			case SCREEN_JOBS_LIST: JobsList_render(screen, show_setting); break;
			}

			GFX_flip(screen);
			dirty = 0;
		}
		else {
			GFX_sync();
		}
	}

	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();

	return EXIT_SUCCESS;
}
