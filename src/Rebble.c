/*******************************
    Rebble
********************************/

#include "Rebble.h"
#include "LoadingWindow.h"
#include "ThreadWindow.h"
#include "SubredditWindow.h"
#include "SubredditListWindow.h"
#include "ThreadMenuWindow.h"
#include "CommentWindow.h"
#include "AppMessages.h"
#include "netimage.h"

bool loggedIn = false;

int selectedThread = 0;
AppTimer *timerHandle = NULL;
NetImageContext *netimage_ctx = NULL;

#ifdef USE_PERSIST_STRINGS
char persist_index_title = -1;
char persist_index_score = -1;
char persist_index_subreddit = -1;

char persist_string_title[PERSIST_STRING_MAX_LENGTH];
char persist_string_score[PERSIST_STRING_MAX_LENGTH];
char persist_string_subreddit[PERSIST_STRING_MAX_LENGTH];
#endif

struct ThreadData threads[MAX_THREADS];

GFont font = NULL;
GFont biggerFont = NULL;

GFont GetFont()
{
	return font;
}

GFont GetBiggerFont()
{
	return biggerFont;
}

bool IsLoggedIn()
{
	return loggedIn;
}

void SetLoggedIn(bool lin)
{
	loggedIn = lin;
}

/*******************************
    Reddit Actions
********************************/

void LoadSubreddit(char *name)
{
	DEBUG_MSG("LoadSubreddit: %s", name);

	Tuplet tuple = TupletCString(VIEW_SUBREDDIT, name);

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL)
	{
		DEBUG_MSG("failed LoadSubreddit");
		return;
	}

	dict_write_tuplet(iter, &tuple);

	app_message_outbox_send();
}

void LoadThread(int index)
{
	DEBUG_MSG("LoadThread");

	Tuplet tuple = TupletInteger(VIEW_THREAD, index);

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL)
	{
		return;
	}

	dict_write_tuplet(iter, &tuple);

	app_message_outbox_send();
}

void LoadThreadNext()
{
	DEBUG_MSG("LoadThreadNext");

	Tuplet tuple = TupletCString(VIEW_SUBREDDIT_NEXT, " ");

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL)
	{
		return;
	}

	dict_write_tuplet(iter, &tuple);

	app_message_outbox_send();
}

void UpvoteThread(int index)
{
	DEBUG_MSG("UpvoteThread");

	Tuplet tuple = TupletInteger(THREAD_UPVOTE, index);

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL)
	{
		return;
	}

	dict_write_tuplet(iter, &tuple);

	app_message_outbox_send();
}

void DownvoteThread(int index)
{
	DEBUG_MSG("DownvoteThread");

	Tuplet tuple = TupletInteger(THREAD_DOWNVOTE, index);

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL)
	{
		return;
	}

	dict_write_tuplet(iter, &tuple);

	app_message_outbox_send();
}

void SaveThread(int index)
{
	DEBUG_MSG("SaveThread");

	Tuplet tuple = TupletInteger(THREAD_SAVE, index);

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL)
	{
		return;
	}

	dict_write_tuplet(iter, &tuple);

	app_message_outbox_send();
}

/*******************************
    Memory Utilities
********************************/

#ifdef DEBUG_MODE

int count = 0;

void *nt_Malloc_Raw(size_t size, const char *function, int line)
{
	void *pointer = malloc(size);

	if(pointer == NULL)
	{
		DEBUG_MSG("nt_Malloc: Failed to mallocc %d", size);
		DEBUG_MSG("%s: %d", function, line);
	}
	else
	{
		//DEBUG_MSG("nt_Malloc: %d, %s: %d", size, function, line);
	}

	count++;

	return pointer;
}

void nt_Free_Raw(void *pointer)
{
	count--;

	if(pointer == NULL)
	{
		count++;
		DEBUG_MSG("nt_Free: Tried to freee NULL");
		return;
	}

	free(pointer);
	pointer = NULL;
}

void nt_Stats()
{
	DEBUG_MSG("nt_Stats: %d", count);
}

#endif

/*******************************
    Thread Utilities
********************************/

struct ThreadData *GetThread(int index)
{
	return &threads[index];
}

struct ThreadData *GetSelectedThread()
{
	return &threads[selectedThread];
}

void SetSelectedThreadID(int index)
{
	selectedThread = index;
}

int GetSelectedThreadID()
{
	return selectedThread;
}

char* GetThreadTitle(int index)
{
#ifdef USE_PERSIST_STRINGS
	if(persist_index_title != index)
	{
		persist_index_title = index;
		persist_read_string(PERSIST_OFFSET_THREAD_TITLE + index, persist_string_title, PERSIST_STRING_MAX_LENGTH);
	}
	return persist_string_title;
#else
	return threads[index].title;
#endif
}

char* GetThreadScore(int index)
{
#ifdef USE_PERSIST_STRINGS
	if(persist_index_score != index)
	{
		persist_index_score = index;
		persist_read_string(PERSIST_OFFSET_THREAD_SCORE + index, persist_string_score, PERSIST_STRING_MAX_LENGTH);
	}
	return persist_string_score;
#else
	return threads[index].score;
#endif
}

char* GetThreadSubreddit(int index)
{
#ifdef USE_PERSIST_STRINGS
	if(persist_index_subreddit != index)
	{
		persist_index_subreddit = index;
		if(persist_read_string(PERSIST_OFFSET_THREAD_SUBREDDIT + index, persist_string_subreddit, PERSIST_STRING_MAX_LENGTH) == E_DOES_NOT_EXIST)
		{
			return NULL;
		}
	}
	return persist_string_subreddit;
#else
	return threads[index].subreddit;
#endif
}

void SetThreadTitle(struct ThreadData* thread, int index, char* str)
{
#ifdef USE_PERSIST_STRINGS
	persist_write_string(PERSIST_OFFSET_THREAD_SCORE + index, str);
#else
	if(thread->title != NULL)
	{
		nt_Free(thread->title);
	}
	thread->title = (char*)nt_Malloc(sizeof(char) * (strlen(str) + 1));
	strcpy(thread->title, str);
#endif
}

void SetThreadScore(struct ThreadData* thread, int index, char* str)
{
#ifdef USE_PERSIST_STRINGS
	persist_write_string(PERSIST_OFFSET_THREAD_SCORE + index, str);
#else
	if(thread->score != NULL)
	{
		nt_Free(thread->score);
	}
	thread->score = (char*)nt_Malloc(sizeof(char) * (strlen(str) + 1));
	strcpy(thread->score, str);
#endif
}

void SetThreadSubreddit(struct ThreadData* thread, int index, char* str)
{
#ifdef USE_PERSIST_STRINGS
	if(str != NULL)
	{
		persist_write_string(PERSIST_OFFSET_THREAD_SUBREDDIT + index, str);
	}
	else
	{
		persist_delete(PERSIST_OFFSET_THREAD_SUBREDDIT + index);
	}
#else
	if(thread->subreddit != NULL)
	{
		nt_Free(thread->subreddit);
	}
	if(str != NULL)
	{
		thread->subreddit = (char*)nt_Malloc(sizeof(char) * (strlen(str) + 1));
		strcpy(thread->subreddit, str);
	}
#endif
}

/*******************************
    Timer
********************************/

void init_timer(AppTimer *handle)
{
	//DEBUG_MSG("init_timer");
	//cancel_timer();
	timerHandle = handle;
}

void cancel_timer()
{
	//DEBUG_MSG("cancel_timer");
	if(timerHandle != NULL)
	{
		app_timer_cancel(timerHandle);
		timerHandle = NULL;
	}
}

/*******************************
    Net Image
********************************/

void init_netimage(int index)
{
	DEBUG_MSG("init_netimage %d", index);
	free_netimage();
	netimage_ctx = netimage_create_context(callback_netimage);
	netimage_request(index);
}

void callback_netimage(GBitmap *image)
{
	DEBUG_MSG("callback_netimage");
	thread_display_image(image);
}

NetImageContext *get_netimage_context()
{
	return netimage_ctx;
}

void free_netimage()
{
	if(netimage_ctx != NULL)
	{
		netimage_destroy_context(netimage_ctx);
		netimage_ctx = NULL;
	}
}

/*******************************
    Bluetooth Handler
********************************/

bool bluetoothConnected = true;

bool IsBluetoothConnected()
{
	return bluetoothConnected;
}

void OnBluetoothConnection(bool connected)
{
	DEBUG_MSG("OnBluetoothConnection: %s", connected ? "woo" : "nope");
	bluetoothConnected = connected;
	loading_disconnected();
}

/*******************************
    Main
********************************/

inline void windows_create()
{
	window_loading = window_create();
	window_set_window_handlers(window_loading, (WindowHandlers)
	{
		.load = loading_window_load,
		.disappear = loading_window_disappear,
		.unload = loading_window_unload,
	});

	window_subreddit = window_create();
	window_set_window_handlers(window_subreddit, (WindowHandlers)
	{
		.load = subreddit_window_load,
		.appear = subreddit_window_appear,
		.disappear = subreddit_window_disappear,
		.unload = subreddit_window_unload,
	});

	window_thread = window_create();
	window_set_window_handlers(window_thread, (WindowHandlers)
	{
		.load = thread_window_load,
		.appear = thread_window_appear,
		.disappear = thread_window_disappear,
		.unload = thread_window_unload,
	});

	window_threadmenu = window_create();
	window_set_window_handlers(window_threadmenu, (WindowHandlers)
	{
		.load = threadmenu_window_load,
		.unload = threadmenu_window_unload,
	});

	window_subredditlist = window_create();
	window_set_window_handlers(window_subredditlist, (WindowHandlers)
	{
		.load = subredditlist_window_load,
		.unload = subredditlist_window_unload,
	});

	window_comment = window_create();
	window_set_window_handlers(window_comment, (WindowHandlers)
	{
		.load = comment_window_load,
		.unload = comment_window_unload,
	});
}

inline void windows_destroy()
{
	window_destroy(window_loading);
	window_destroy(window_subredditlist);
	window_destroy(window_thread);
	window_destroy(window_threadmenu);
	window_destroy(window_subreddit);
	window_destroy(window_comment);
}

int main()
{
	current_thread.author = NULL;
	current_thread.score = NULL;
	current_thread.body = NULL;
	current_thread.comment = NULL;
	current_thread.image = NULL;
	current_thread.thread_author = NULL;

#ifndef USE_PERSIST_STRINGS
	for(int i = 0; i < MAX_THREADS; ++i)
	{
		struct ThreadData *thread = GetThread(i);		
		thread->title = NULL;
		thread->score = NULL;
		thread->subreddit = NULL;
	}
#endif

	font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
	biggerFont = fonts_get_system_font(FONT_KEY_GOTHIC_24);

	bluetoothConnected = bluetooth_connection_service_peek();
	bluetooth_connection_service_subscribe(OnBluetoothConnection);

	/*
	for(int i=0; i < 20; i++)
	{
		char *test = nt_Malloc(PERSIST_STRING_MAX_LENGTH * sizeof(char));
		strcpy(test, "hey");
		test[4] = '\0';

		if(persist_exists(i))
		{
			test[0] = '\0';
			persist_read_string(i, test, PERSIST_STRING_MAX_LENGTH);
			MSG("%d: %s", i, test);
			persist_delete(i);
		}
		//persist_write_string(i, test);

		nt_Free(test);
	}
	*/

	//app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

	///////////////////////////////////////

	windows_create();

	app_message_init();

	loading_init();

	app_event_loop();

	windows_destroy();

	///////////////////////////////////////

	bluetooth_connection_service_unsubscribe();

#ifndef USE_PERSIST_STRINGS
	for(int i = 0; i < MAX_THREADS; ++i)
	{
		struct ThreadData *thread = GetThread(i);

		if(thread->title != NULL)
		{
			nt_Free(thread->title);
		}

		if(thread->score != NULL)
		{
			nt_Free(thread->score);
		}

		if(thread->subreddit != NULL)
		{
			nt_Free(thread->subreddit);
		}
	}
#endif

	if(current_thread.thread_author != NULL)
	{
		nt_Free(current_thread.thread_author);
	}

	if(user_subreddits != NULL)
	{
		for(int i = 0; i < subredditlist_num; i++)
		{
			nt_Free(user_subreddits[i]);
		}

		nt_Free(user_subreddits);
	}

	free_netimage();

#ifdef DEBUG_MODE
	nt_Stats();
#endif
}
