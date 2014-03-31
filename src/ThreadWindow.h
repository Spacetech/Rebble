#ifndef THREAD_WINDOW_H
#define THREAD_WINDOW_H

#include <pebble.h>

extern GRect window_frame;
extern bool thread_offset_reset;
extern GSize text_size;

void thread_load();
void thread_load_finished();

void thread_init();

void thread_window_load(Window *window);
void thread_window_appear(Window *window);
void thread_window_disappear(Window *window);
void thread_window_unload(Window *window);

void thread_display_image(GBitmap *image);

void thread_update_comments_position();

#endif
