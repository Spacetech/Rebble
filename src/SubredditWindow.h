#ifndef SUBREDDIT_WINDOW_H
#define SUBREDDIT_WINDOW_H

#include <pebble.h>

extern GRect window_frame;

void subreddit_init();

void subreddit_window_load(Window *window);
void subreddit_window_appear(Window *window);
void subreddit_window_disappear(Window *window);
void subreddit_window_unload(Window *window);

void subreddit_show_load_more();
void subreddit_hide_load_more();

void subreddit_selection_changed(bool before);

void subreddit_button_up(ClickRecognizerRef recognizer, void *context);
void subreddit_button_select(ClickRecognizerRef recognizer, void *context);
void subreddit_button_select_long(ClickRecognizerRef recognizer, void *context);
void subreddit_button_down(ClickRecognizerRef recognizer, void *context);

void subreddit_set_content_offset(int index);

void subreddit_load_setup();

#endif
