/*******************************
	App Messsage Handlers
********************************/

#include "Rebble.h"
#include "AppMessages.h"
#include "ThreadWindow.h"
#include "SubredditWindow.h"
#include "SubredditListWindow.h"

#include "netimage.h"
#include "LoadingWindow.h"

extern struct ViewThreadData current_thread;
extern struct ThreadData threads[MAX_THREADS];

uint32_t inboxSize = 0;

bool loadedSubredditList = false;
bool refreshSubreddit = false;

static void in_received_handler(DictionaryIterator *iter, void *context);
static void in_dropped_handler(AppMessageResult reason, void *context);
static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context);
static void app_message_send_ready_reply();

static void in_received_handler(DictionaryIterator *iter, void *context)
{
	Tuple *netimage_begin = dict_find(iter, NETIMAGE_BEGIN);
	Tuple *netimage_data = dict_find(iter, NETIMAGE_DATA);
	Tuple *netimage_end = dict_find(iter, NETIMAGE_END);

	//DEBUG_MSG("in_received_handler");

	if(netimage_begin || netimage_data || netimage_end)
	{
		if(get_netimage_context() != NULL)
		{
			netimage_receive(iter);
		}
		else
		{
			//DEBUG_MSG("null get_netimage_context");
		}	
		return;
	}

	Tuple *thread_id_tuple = dict_find(iter, THREAD_ID);
	Tuple *thread_title_tuple = dict_find(iter, THREAD_TITLE);
	Tuple *thread_score_tuple = dict_find(iter, THREAD_SCORE);
	Tuple *thread_type_tuple = dict_find(iter, THREAD_TYPE);
	Tuple *thread_subreddit_tuple = dict_find(iter, THREAD_SUBREDDIT);

	Tuple *thread_body_tuple = dict_find(iter, THREAD_BODY);

	Tuple *user_subreddit_tuple = dict_find(iter, USER_SUBREDDIT);

	Tuple *ready_tuple = dict_find(iter, READY);

	if(ready_tuple)
	{
		if(ready_tuple->value->uint8 == 2)
		{
			refreshSubreddit = true;
			return;
		}

		SetLoggedIn(ready_tuple->value->uint8 == 1);

		app_message_send_ready_reply();

		return;
	}

	if(user_subreddit_tuple)
	{
		if(subredditlist_num == -1 || !loading_visible())
		{
			DEBUG_MSG("%s", user_subreddit_tuple->value->cstring);
			return;
		}

		char *subredditlist = user_subreddit_tuple->value->cstring;
		int len = strlen(subredditlist);

		int start = 0;

		for(int i=0; i < len; i++)
		{
			char c = subredditlist[i];

			if(c == ',')
			{
				int nameLength = i - start;
				if(nameLength == 0)
				{
					// maybe they put in a double comma by mistake, skip this one
					start = i + 1;
					continue;
				}

				char *name = nt_Malloc(sizeof(char) * (nameLength + 1));
				if(name == NULL)
				{
					goto done;
				}

				strncpy(name, subredditlist + start, i - start);
				name[i - start] = '\0';

				if(user_subreddits == NULL)
				{
					subredditlist_num = 1;
					user_subreddits = (char**)nt_Malloc(sizeof(char*));
					if(user_subreddits == NULL)
					{
						nt_Free(name);
						subredditlist_num--;
						goto done;
					}
				}
				else
				{
					subredditlist_num++;

					char** subredditlist_new = (char**)nt_Malloc(sizeof(char*) * subredditlist_num);
					if(subredditlist_new == NULL)
					{
						nt_Free(name);
						subredditlist_num--;
						goto done;
					}
					memcpy(subredditlist_new, user_subreddits, sizeof(char*) * (subredditlist_num - 1));

					nt_Free(user_subreddits);

					user_subreddits = subredditlist_new;
				}

				//DEBUG_MSG("Subreddit: '%s', %d", name, subredditlist_num);

				user_subreddits[subredditlist_num - 1] = name;

				start = i + 1;
			}
			else if(c == ';')
			{
				goto done;
			}
		}

		goto done_skip;

done:
		loading_uninit();
		subredditlist_init();

done_skip:
		return;		
	}

	if(thread_type_tuple && thread_type_tuple->value->uint8 == 255)
	{
		DEBUG_MSG("Done loading thread list");
		subreddit_show_load_more();
		scroll_layer_set_content_size(subreddit_scroll_layer, GSize(144, (thread_loaded + 1) * (THREAD_WINDOW_HEIGHT + THREAD_LAYER_PADDING) + (THREAD_WINDOW_HEIGHT_SELECTED + THREAD_LAYER_PADDING)));
		return;
	}

	if(thread_title_tuple && thread_score_tuple && thread_type_tuple)
	{
		if(thread_loaded >= MAX_THREADS)
		{
			//DEBUG_MSG("got too many..");
			return;
		}

		if(thread_loaded == 0)
		{
			subreddit_init();
		}

		struct ThreadData *thread = &threads[thread_loaded];

		// title
		if(thread->title != NULL)
		{
			nt_Free(thread->title);
		}
		thread->title = (char*)nt_Malloc(sizeof(char) * (strlen(thread_title_tuple->value->cstring) + 1));

		// score
		if(thread->score != NULL)
		{
			nt_Free(thread->score);
		}
		thread->score = (char*)nt_Malloc(sizeof(char) * (strlen(thread_score_tuple->value->cstring) + 1));

		strcpy(thread->title, thread_title_tuple->value->cstring);
		strcpy(thread->score, thread_score_tuple->value->cstring);

		// subreddit (only given if we are viewing the frontpage)
		if(thread->subreddit != NULL)
		{
			nt_Free(thread->subreddit);
		}
		if(thread_subreddit_tuple != NULL)
		{
			thread->subreddit = (char*)nt_Malloc(sizeof(char) * (strlen(thread_subreddit_tuple->value->cstring) + 1));
			strcpy(thread->subreddit, thread_subreddit_tuple->value->cstring);
		}

		thread->type = thread_type_tuple->value->uint8;

		layer_set_hidden(thread->layer, false);

		thread_loaded++;

		scroll_layer_set_content_size(subreddit_scroll_layer, GSize(144, thread_loaded * (THREAD_WINDOW_HEIGHT + THREAD_LAYER_PADDING) + THREAD_WINDOW_HEIGHT_SELECTED + THREAD_LAYER_PADDING));
		
		if(thread_loaded == 1)
		{
			layer_set_hidden(inverter_layer_get_layer(inverter_layer), false);
			subreddit_selection_changed(false);
		}
	}
	/*else if(thread_id_tuple &&)
	{
		// an eror occured
		//text_layer_set_text(loading_text_layer, "Failed to load subreddit. Long press select to try again.");
	}*/

	if(thread_id_tuple)
	{
		if(thread_body_tuple)
		{
			if(thread_id_tuple->value->uint8 != GetSelectedThreadID())
			{
				DEBUG_MSG("loading old thread");
				return;
			}

			if(current_thread.body != NULL)
			{
				nt_Free(current_thread.body);
			}

			current_thread.body = (char*)nt_Malloc(sizeof(char) * (strlen(thread_body_tuple->value->cstring) + 1));
			strcpy(current_thread.body, thread_body_tuple->value->cstring);

			//DEBUG_MSG("filled body, %d", strlen(current_thread.body));
			//DEBUG_MSG("Thread load...?");

			thread_load_finished();

			if(thread_body_layer == NULL)
			{
				return;
			}

			text_layer_set_text(thread_body_layer, current_thread.body);
			text_layer_set_text_alignment(thread_body_layer, GTextAlignmentLeft);

			GSize size = text_layer_get_content_size(thread_body_layer);
			size.h += 5;
			text_layer_set_size(thread_body_layer, size);

			scroll_layer_set_content_size(thread_scroll_layer, GSize(144, 22 + size.h + 5));
		}
		else
		{
			// unable to load subreddit
			DEBUG_MSG("Unable to load subreddit");

			subreddit_init();
		}
	}
}

#ifdef DEBUG_MODE
char* app_message_result_to_string(AppMessageResult reason)
{
	switch (reason)
	{
		case APP_MSG_OK:
			return "APP_MSG_OK";
		case APP_MSG_SEND_TIMEOUT:
			return "APP_MSG_SEND_TIMEOUT";
		case APP_MSG_SEND_REJECTED:
			return "APP_MSG_SEND_REJECTED";
		case APP_MSG_NOT_CONNECTED:
			return "APP_MSG_NOT_CONNECTED";
		case APP_MSG_APP_NOT_RUNNING:
			return "APP_MSG_APP_NOT_RUNNING";
		case APP_MSG_INVALID_ARGS:
			return "APP_MSG_INVALID_ARGS";
		case APP_MSG_BUSY:
			return "APP_MSG_BUSY";
		case APP_MSG_BUFFER_OVERFLOW:
			return "APP_MSG_BUFFER_OVERFLOW";
		case APP_MSG_ALREADY_RELEASED:
			return "APP_MSG_ALREADY_RELEASED";
		case APP_MSG_CALLBACK_ALREADY_REGISTERED:
			return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
		case APP_MSG_CALLBACK_NOT_REGISTERED:
			return "APP_MSG_CALLBACK_NOT_REGISTERED";
		case APP_MSG_OUT_OF_MEMORY:
			return "APP_MSG_OUT_OF_MEMORY";
		case APP_MSG_CLOSED:
			return "APP_MSG_CLOSED";
		case APP_MSG_INTERNAL_ERROR:
			return "APP_MSG_INTERNAL_ERROR";
		default:
			return "UNKNOWN ERROR";
	}
}
#endif

static void in_dropped_handler(AppMessageResult reason, void *context)
{
	DEBUG_MSG("App Message Dropped! %d, %s", (int)reason, app_message_result_to_string(reason));
	/*
	if(loading_visible())
	{
		loading_set_text("An error occured\nPlease try again");
	}
	*/
}

static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context)
{
	DEBUG_MSG("App Message Failed to Send! %d, %s", (int)reason, app_message_result_to_string(reason));

	Tuple* tuple = dict_read_first(failed);

	while(tuple != NULL)
	{
		DEBUG_MSG("[%ld]", tuple->key);

		tuple = dict_read_next(failed);
	}

	if(reason == APP_MSG_NOT_CONNECTED)
	{
		loading_disconnected();
	}
	/*else
	{
		if(loading_visible())
		{
			loading_set_text("An error occured\nPlease try again");
		}
	}*/
}

void app_message_init()
{
	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_failed(out_failed_handler);

	int max = app_message_inbox_size_maximum();

	inboxSize = max > 1024 ? 1024 : max;

	app_message_open(inboxSize, 256);
}

uint32_t app_message_index_size()
{
	return inboxSize;
}

static void app_message_send_ready_reply()
{
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if(iter == NULL)
	{
		return;
	}

	uint32_t chunk_size = app_message_index_size() - 8;

	dict_write_int(iter, CHUNK_SIZE, &chunk_size, sizeof(uint32_t), false);

	if(refreshSubreddit)
	{
		refreshSubreddit = false;
		
		subreddit_load_setup();
		
		Tuplet tuple = TupletCString(VIEW_SUBREDDIT, "0");

		dict_write_tuplet(iter, &tuple);
	}

	app_message_outbox_send();
}
