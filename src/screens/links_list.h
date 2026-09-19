#ifndef LINKS_LIST_H
#define LINKS_LIST_H

#include "api.h"

// Screen 1 (SPEC.md): the list of links with their check/sync status, "sync
// everything", "check again" and access to each link's detail. Drives
// sync_queue.c every frame while a check or sync is running.

typedef enum {
	LINKS_LIST_ACTION_NONE,
	LINKS_LIST_ACTION_QUIT,
	LINKS_LIST_ACTION_DETAIL, // open screen 2 for LinksList_selected()
} LinksListAction;

LinksListAction LinksList_input(int *dirty);
int LinksList_selected(void);
void LinksList_render(SDL_Surface *screen, int show_setting);

#endif
