#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#include <pebble.h>

extern char* *user_subreddits;
extern int subredditlist_num;
extern int thread_loaded;
extern ScrollLayer *subreddit_scroll_layer;
extern InverterLayer *inverter_layer;
extern TextLayer *loading_text_layer;
extern ScrollLayer *thread_scroll_layer;
extern TextLayer *thread_body_layer;

void app_message_init();
uint32_t app_message_index_size();

#ifdef DEBUG_MODE
char* app_message_result_to_string(AppMessageResult reason);
#endif

#endif
