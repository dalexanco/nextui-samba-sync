#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "servers.h"
#include "smb_client.h"
#include "servers_list.h"

#define ROW_HEIGHT (PILL_SIZE + 6)
#define TEST_TIMEOUT_SECONDS 5

typedef enum {
	TEST_UNTESTED,
	TEST_OK,
	TEST_FAILED,
} TestStatus;

static int selected = 0;
static int scroll_offset = 0;
static TestStatus test_status[MAX_SERVERS];

void ServersList_reset(void)
{
	selected = 0;
	scroll_offset = 0;
	for (int i = 0; i < MAX_SERVERS; i++) test_status[i] = TEST_UNTESTED;
}

ServersListAction ServersList_input(int *dirty)
{
	int count = servers_count();

	if (PAD_justPressed(BTN_B)) return SERVERS_LIST_ACTION_BACK;

	if (PAD_justPressed(BTN_X)) {
		servers_rescan();
		ServersList_reset();
		*dirty = 1;
		return SERVERS_LIST_ACTION_NONE;
	}

	if (count > 0) {
		if (PAD_justRepeated(BTN_UP)) {
			selected = (selected - 1 + count) % count;
			*dirty = 1;
		}
		else if (PAD_justRepeated(BTN_DOWN)) {
			selected = (selected + 1) % count;
			*dirty = 1;
		}
		else if (PAD_justPressed(BTN_A)) {
			SmbError error;
			SmbSession *session = smb_connect(servers_get(selected), TEST_TIMEOUT_SECONDS, &error);
			if (session) {
				smb_disconnect(session);
				test_status[selected] = TEST_OK;
			}
			else {
				test_status[selected] = TEST_FAILED;
			}
			*dirty = 1;
		}
	}

	return SERVERS_LIST_ACTION_NONE;
}

static void renderRow(SDL_Surface *screen, const Server *server, TestStatus status, bool is_selected, int y)
{
	int x = SCALE1(PADDING);
	int w = screen->w - SCALE1(PADDING * 2);

	GFX_blitPill(is_selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
	             &(SDL_Rect){ x, y, w, SCALE1(PILL_SIZE) });

	SDL_Color color = is_selected ? COLOR_BLACK : COLOR_WHITE;

	UI_renderText(screen, server->name, font.medium, color, x + SCALE1(BUTTON_PADDING), y + SCALE1(4));

	const char *status_text = status == TEST_OK ? "\xe2\x9c\x94 connect\xc3\xa9"
	                         : status == TEST_FAILED ? "\xe2\x9c\x98 injoignable"
	                         : "";

	char value[SERVER_STR_MAX * 2];
	snprintf(value, sizeof(value), "%s / %s%s%s", server->host, server->share,
	         status_text[0] ? "   " : "", status_text);

	int vw = UI_textWidth(value, font.small);
	UI_renderText(screen, value, font.small, color, x + w - SCALE1(BUTTON_PADDING) - vw, y + SCALE1(4));
}

void ServersList_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);
	UI_renderTitle(screen, "Serveurs", show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	int count = servers_count();

	if (count == 0) {
		UI_renderTextCentered(screen, "Aucun serveur déclaré — créez un dossier dans Samba Servers/",
		                       font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	}
	else {
		int visible_rows = (screen->h - content_y - SCALE1(PADDING + PILL_SIZE)) / SCALE1(ROW_HEIGHT);
		if (visible_rows < 1) visible_rows = 1;

		if (selected < scroll_offset) scroll_offset = selected;
		if (selected >= scroll_offset + visible_rows) scroll_offset = selected - visible_rows + 1;

		int last = MIN(count, scroll_offset + visible_rows);
		for (int i = scroll_offset; i < last; i++) {
			int row = i - scroll_offset;
			renderRow(screen, servers_get(i), test_status[i], i == selected, content_y + row * SCALE1(ROW_HEIGHT));
		}
	}

	GFX_blitButtonGroup((char *[]){ "X", "RECHARGER", NULL }, 0, screen, 0);
	GFX_blitButtonGroup((char *[]){ "A", "TESTER", "B", "RETOUR", NULL }, 1, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
