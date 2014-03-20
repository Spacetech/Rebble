#ifndef THREAD_MENU_WINDOW_H
#define THREAD_MENU_WINDOW_H

#include <pebble.h>

extern int thread_loaded;
extern ScrollLayer *subreddit_scroll_layer;

void threadmenu_load();

void threadmenu_init();

void threadmenu_window_load(Window *window);
void threadmenu_window_unload(Window *window);

#endif