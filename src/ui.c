#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "ui.h"

int UI_renderText(SDL_Surface *screen, const char *text, TTF_Font *font, SDL_Color color, int x, int y)
{
	SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
	if (!surface) return 0;
	int width = surface->w;
	SDL_BlitSurface(surface, NULL, screen, &(SDL_Rect){ x, y });
	SDL_FreeSurface(surface);
	return width;
}

int UI_textWidth(const char *text, TTF_Font *font)
{
	int w = 0, h = 0;
	TTF_SizeUTF8(font, text, &w, &h);
	return w;
}

int UI_renderTextCentered(SDL_Surface *screen, const char *text, TTF_Font *font, SDL_Color color, int y)
{
	return UI_renderText(screen, text, font, color, (screen->w - UI_textWidth(text, font)) / 2, y);
}

int UI_fitText(TTF_Font *font, const char *text, char *out, size_t out_size, int max_width)
{
	snprintf(out, out_size, "%s", text);
	int w = UI_textWidth(out, font);
	if (w <= max_width) return w;

	static const char ELLIPSIS[] = "…";
	size_t len = strlen(out);
	while (len > 0) {
		// Step back one UTF-8 character (skip continuation bytes 10xxxxxx).
		do len--; while (len > 0 && ((unsigned char)out[len] & 0xC0) == 0x80);
		if (len + sizeof(ELLIPSIS) > out_size) continue;
		memcpy(out + len, ELLIPSIS, sizeof(ELLIPSIS));
		w = UI_textWidth(out, font);
		if (w <= max_width) return w;
	}
	snprintf(out, out_size, "%s", ELLIPSIS);
	return UI_textWidth(out, font);
}

void UI_renderTitle(SDL_Surface *screen, const char *name, int show_setting)
{
	int max_width = screen->w - SCALE1(PADDING * 2);
	if (screen->w >= SCALE1(320)) {
		int ow = GFX_blitHardwareGroup(screen, show_setting);
		max_width = screen->w - SCALE1(PADDING * 2) - ow;
	}

	char title[256];
	int text_width = GFX_truncateText(font.large, name, title, max_width, SCALE1(BUTTON_PADDING * 2));
	max_width = MIN(max_width, text_width);

	SDL_Surface *text = TTF_RenderUTF8_Blended(font.large, title, COLOR_WHITE);
	GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){ SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE) });
	if (text) {
		SDL_BlitSurface(text, &(SDL_Rect){ 0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h }, screen,
		                &(SDL_Rect){ SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4) });
		SDL_FreeSurface(text);
	}
}

void UI_formatBytes(long long bytes, char *out, size_t out_size)
{
	if (bytes >= 1024LL * 1024 * 1024) snprintf(out, out_size, "%.1f Go", bytes / (1024.0 * 1024 * 1024));
	else if (bytes >= 1024LL * 1024) snprintf(out, out_size, "%.1f Mo", bytes / (1024.0 * 1024));
	else if (bytes >= 1024) snprintf(out, out_size, "%.1f Ko", bytes / 1024.0);
	else snprintf(out, out_size, "%lld o", bytes);

	// Decimal comma: the UI is in French, and printf's locale is "C" here.
	for (char *p = out; *p; p++)
		if (*p == '.') *p = ',';
}
