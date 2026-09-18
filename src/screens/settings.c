#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "ui.h"
#include "settings.h" // this file's own header (screens/settings.h)
#include "../settings.h" // the settings data module (src/settings.h) -- same basename, disambiguated by path

#define ROW_HEIGHT (PILL_SIZE + 6)
#define ROW_COUNT 4

// Bounded preset cycled through by A on the timeout row -- no free-text
// entry anywhere in this pak (see SPEC.md).
static const int TIMEOUT_PRESETS[] = { 5, 10, 15, 20, 30, 60 };
#define TIMEOUT_PRESET_COUNT (int)(sizeof(TIMEOUT_PRESETS) / sizeof(TIMEOUT_PRESETS[0]))

static int selected = 0;

void Settings_reset(void)
{
	selected = 0;
}

SettingsAction Settings_input(int *dirty)
{
	if (PAD_justPressed(BTN_B)) return SETTINGS_ACTION_BACK;

	if (PAD_justRepeated(BTN_UP)) {
		selected = (selected - 1 + ROW_COUNT) % ROW_COUNT;
		*dirty = 1;
	}
	else if (PAD_justRepeated(BTN_DOWN)) {
		selected = (selected + 1) % ROW_COUNT;
		*dirty = 1;
	}
	else if (PAD_justPressed(BTN_A)) {
		const Settings *settings = settings_get();
		if (selected == 0) {
			settings_set_overwrite_existing(!settings->overwrite_existing);
		}
		else if (selected == 1) {
			settings_set_preview_before_sync(!settings->preview_before_sync);
		}
		else if (selected == 2) {
			int index = 0;
			while (index < TIMEOUT_PRESET_COUNT && TIMEOUT_PRESETS[index] != settings->network_timeout) index++;
			index = (index + 1) % TIMEOUT_PRESET_COUNT;
			settings_set_network_timeout(TIMEOUT_PRESETS[index]);
		}
		else {
			return SETTINGS_ACTION_VIEW_SERVERS;
		}
		*dirty = 1;
	}

	return SETTINGS_ACTION_NONE;
}

static void renderRow(SDL_Surface *screen, const char *label, const char *value, bool is_selected, int y)
{
	int x = SCALE1(PADDING);
	int w = screen->w - SCALE1(PADDING * 2);

	GFX_blitPill(is_selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
	             &(SDL_Rect){ x, y, w, SCALE1(PILL_SIZE) });

	SDL_Color color = is_selected ? COLOR_BLACK : COLOR_WHITE;
	UI_renderText(screen, label, font.medium, color, x + SCALE1(BUTTON_PADDING), y + SCALE1(4));

	if (value) {
		int vw = UI_textWidth(value, font.medium);
		UI_renderText(screen, value, font.medium, color, x + w - SCALE1(BUTTON_PADDING) - vw, y + SCALE1(4));
	}
}

void Settings_render(SDL_Surface *screen, int show_setting)
{
	GFX_clear(screen);
	UI_renderTitle(screen, "Réglages", show_setting);

	int content_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);

	const Settings *settings = settings_get();

	char timeout_value[16];
	snprintf(timeout_value, sizeof(timeout_value), "[ %d ]", settings->network_timeout);

	renderRow(screen, "Écraser les fichiers existants", settings->overwrite_existing ? "[ Oui ]" : "[ Non ]",
	          selected == 0, content_y + 0 * SCALE1(ROW_HEIGHT));
	renderRow(screen, "Vérifier avant de synchroniser", settings->preview_before_sync ? "[ Oui ]" : "[ Non ]",
	          selected == 1, content_y + 1 * SCALE1(ROW_HEIGHT));
	renderRow(screen, "Timeout réseau (secondes)", timeout_value,
	          selected == 2, content_y + 2 * SCALE1(ROW_HEIGHT));
	renderRow(screen, "Voir les serveurs...", NULL,
	          selected == 3, content_y + 3 * SCALE1(ROW_HEIGHT));

	GFX_blitButtonGroup((char *[]){ "A", "BASCULER/ÉDITER", NULL }, 0, screen, 0);
	GFX_blitButtonGroup((char *[]){ "B", "RETOUR", NULL }, 0, screen, 1);
	if (show_setting) GFX_blitHardwareHints(screen, show_setting);
}
