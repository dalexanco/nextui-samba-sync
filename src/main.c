// Samba Sync.pak -- pull-sync folders from Samba/SMB shares onto the SD
// card. Servers are declared offline in Samba Servers/<name>/server.txt;
// see ../SPEC.md for the UX and ../docs/ARCHITECTURE.md for the design.
//
// This is a project skeleton: it boots, renders a placeholder screen, and
// links against libsmb2 (proven below by actually calling into it), but has
// no sync functionality yet. Screens and modules described in
// docs/ARCHITECTURE.md will be split out of this file as they're built.

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

	int dirty = 1;
	int show_setting = 0;
	while (!quit) {
		GFX_startFrame();
		PAD_poll();

		if (PAD_justPressed(BTN_B)) quit = true;

		PWR_update(&dirty, &show_setting, NULL, NULL);

		if (dirty) {
			GFX_clear(screen);

			SDL_Surface *title = TTF_RenderUTF8_Blended(font.large, "Samba Sync", COLOR_WHITE);
			if (title) {
				SDL_BlitSurface(title, NULL, screen, &(SDL_Rect){
					(screen->w - title->w) / 2,
					(screen->h - title->h) / 2,
				});
				SDL_FreeSurface(title);
			}

			GFX_blitButtonGroup((char *[]){ "B", "EXIT", NULL }, 1, screen, 1);

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
