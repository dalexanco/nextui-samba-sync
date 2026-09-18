#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "error.h"

#define MESSAGE_MAX 256

static char message[MESSAGE_MAX];

void Error_enter(const char *msg)
{
	snprintf(message, sizeof(message), "%s", msg);
}

ErrorAction Error_input(int *dirty)
{
	(void)dirty; // nothing on this screen changes after Error_enter()

	if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) return ERROR_ACTION_BACK;
	return ERROR_ACTION_NONE;
}

// No text-wrapping helper exists in ui.h (see its own comment) and none of
// this codebase's other inline error messages wrap either, so the two fixed
// lines below are kept short enough to fit on screen at font.small; the
// caller-supplied `message` line is expected to do the same (see
// screens/error.h).
void Error_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);
	UI_renderTitle(screen, "Erreur de synchronisation", show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
	UI_renderTextCentered(screen, message, font.small, COLOR_DARK_TEXT, content_y + SCALE1(20));
	UI_renderTextCentered(screen, "Connexion perdue ou écriture impossible.",
	                       font.small, COLOR_DARK_TEXT, content_y + SCALE1(50));
	UI_renderTextCentered(screen, "Vérifiez la connexion réseau au serveur Samba.",
	                       font.small, COLOR_DARK_TEXT, content_y + SCALE1(72));

	GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 0, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
