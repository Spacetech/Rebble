/*******************************
    Rebble
********************************/

#include "Rebble.h"
#include "LoadingWindow.h"
#include "ThreadWindow.h"
#include "SubredditWindow.h"
#include "SubredditListWindow.h"
#include "ThreadMenuWindow.h"
#include "AppMessages.h"
#include "netimage.h"

bool loggedIn = false;

int selectedThread = 0;
AppTimer *timerHandle = NULL;
NetImageContext *netimage_ctx = NULL;

struct ThreadData threads[MAX_THREADS];

GFont *font = NULL;
GFont *biggerFont = NULL;

GFont *GetFont()
{
	return font;
}

GFont *GetBiggerFont()
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

int count = 0;

void *nt_Malloc_Raw(size_t size, const char *function, int line)
{
	count++;

	void *pointer = malloc(size);

	if(pointer == NULL)
	{
		count--;
		DEBUG_MSG("nt_Malloc: Failed to mallocc %d", size);
		DEBUG_MSG("%s: %d", function, line);
	}
	else
	{
		//DEBUG_MSG("nt_Malloc: %d, %s: %d", size, function, line);
	}

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
}

inline void windows_destroy()
{
	window_destroy(window_loading);
	window_destroy(window_subredditlist);
	window_destroy(window_thread);
	window_destroy(window_threadmenu);
	window_destroy(window_subreddit);
}

int main()
{
	current_thread.body = NULL;

	for(int i = 0; i < MAX_THREADS; ++i)
	{
		struct ThreadData *thread = GetThread(i);
		thread->title = NULL;
		thread->score = NULL;
		thread->subreddit = NULL;
	}

	font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
	biggerFont = fonts_get_system_font(FONT_KEY_GOTHIC_24);

	bluetoothConnected = bluetooth_connection_service_peek();
	bluetooth_connection_service_subscribe(OnBluetoothConnection);

	///////////////////////////////////////

	windows_create();

	app_message_init();

	loading_init();

	app_event_loop();

	windows_destroy();

	///////////////////////////////////////

	bluetooth_connection_service_unsubscribe();

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

	if(user_subreddits != NULL)
	{
		for(int i = 0; i < subredditlist_num; i++)
		{
			nt_Free(user_subreddits[i]);
		}

		nt_Free(user_subreddits);
	}

	free_netimage();

	nt_Stats();
}
