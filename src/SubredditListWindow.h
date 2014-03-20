#ifndef SUBREDDITLIST_WINDOW_H
#define SUBREDDITLIST_WINDOW_H

#include <pebble.h>

void subredditlist_load();

void subredditlist_init();

void subredditlist_window_load(Window *window);
void subredditlist_window_unload(Window *window);

#endif
