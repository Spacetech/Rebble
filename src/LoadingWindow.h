#ifndef LOADING_WINDOW_H
#define LOADING_WINDOW_H

#include <pebble.h>

Window* loading_create_window();

void loading_init();
void loading_uninit();

void loading_window_load(Window *window);
void loading_window_disappear(Window *window);
void loading_window_unload(Window *window);

bool loading_visible();
void loading_set_text(char *loadingText);
void loading_disable_dots();

void loading_disconnected();

#endif
