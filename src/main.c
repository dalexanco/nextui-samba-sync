// Samba Sync.pak -- pull-sync folders from Samba/SMB shares onto the SD
// card. Everything to sync ("links") is declared offline in
// SDCARD_PATH "/Samba Sync.toml"; the UI only checks, triggers and reports.
// See ../SPEC.md for the UX and ../docs/ARCHITECTURE.md for the design.

#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "sync_queue.h"
#include "screens/links_list.h"
#include "screens/link_detail.h"

static bool quit = false;

static void sigHandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) quit = true;
}

typedef enum {
	SCREEN_LINKS_LIST,
	SCREEN_LINK_DETAIL,
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

	// SPEC.md: checking starts as soon as the pak opens.
	sync_queue_reload();
	sync_queue_check_all();

	Screen active_screen = SCREEN_LINKS_LIST;
	int dirty = 1;
	int show_setting = 0;
	while (!quit) {
		GFX_startFrame();
		PAD_poll();

		switch (active_screen) {
		case SCREEN_LINKS_LIST: {
			LinksListAction action = LinksList_input(&dirty);
			if (action == LINKS_LIST_ACTION_QUIT) quit = true;
			else if (action == LINKS_LIST_ACTION_DETAIL) {
				LinkDetail_enter(LinksList_selected());
				active_screen = SCREEN_LINK_DETAIL;
				dirty = 1;
			}
			break;
		}
		case SCREEN_LINK_DETAIL: {
			LinkDetailAction action = LinkDetail_input(&dirty);
			if (action == LINK_DETAIL_ACTION_BACK) {
				active_screen = SCREEN_LINKS_LIST;
				dirty = 1;
			}
			break;
		}
		}

		PWR_update(&dirty, &show_setting, NULL, NULL);

		if (dirty) {
			switch (active_screen) {
			case SCREEN_LINKS_LIST: LinksList_render(screen, show_setting); break;
			case SCREEN_LINK_DETAIL: LinkDetail_render(screen, show_setting); break;
			}
			GFX_flip(screen);
			dirty = 0;
		}
		else {
			GFX_sync();
		}
	}

	// Quitting mid-sync (SIGTERM) must not leave a .part file behind.
	if (sync_queue_mode() != QUEUE_IDLE) sync_queue_cancel();

	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();

	return EXIT_SUCCESS;
}
