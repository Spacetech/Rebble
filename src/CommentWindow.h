#ifndef COMMENT_WINDOW_H
#define COMMENT_WINDOW_H

#include <pebble.h>

void comment_load(int dir);
void comment_load_finished();

void comment_init();

void comment_window_load(Window *window);
void comment_window_unload(Window *window);

TextLayer* comment_body_layer_create();

#endif
