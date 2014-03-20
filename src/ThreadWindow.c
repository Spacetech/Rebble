/*******************************
    Thread Window
********************************/

#include <pebble.h>
#include "Rebble.h"
#include "ThreadWindow.h"
#include "SubredditWindow.h"
#include "LoadingWindow.h"
#include "ThreadMenuWindow.h"

Window *window_thread;

ScrollLayer *thread_scroll_layer;
Layer *thread_title_layer;
TextLayer *thread_body_layer = NULL;

BitmapLayer *thread_bitmap_layer;
GBitmap *thread_image = NULL;

static void thread_click_config(void *context);
static void thread_button_up(ClickRecognizerRef recognizer, void *context);
static void thread_button_select(ClickRecognizerRef recognizer, void *context);
static void thread_button_down(ClickRecognizerRef recognizer, void *context);
static void thread_title_layer_update_proc(Layer *layer, GContext *ctx);
static void thread_scroll_timer_callback(void *data);

void thread_load()
{
	loading_init();
	
	if(GetSelectedThread()->type == 1)
	{
		loading_set_text("Loading Image");

		init_netimage(GetSelectedThreadID());
	}
	else
	{
		loading_set_text("Loading Thread");
		
		LoadThread(GetSelectedThreadID());
	}
}

void thread_load_finished()
{
	if(loading_visible())
	{
		loading_uninit();
		thread_init();
	}
}

void thread_init()
{
	window_stack_push(window_thread, true);
}

void thread_window_load(Window *window)
{
	struct ThreadData *thread = GetSelectedThread();
	
	thread_scroll_layer = scroll_layer_create(window_frame);

	scroll_layer_set_shadow_hidden(thread_scroll_layer, true);
	scroll_layer_set_click_config_onto_window(thread_scroll_layer, window);
	scroll_layer_set_content_size(thread_scroll_layer, GSize(144, 0));
	scroll_layer_set_content_offset(thread_scroll_layer, GPoint(0, 0), false);

	ScrollLayerCallbacks scrollOverride =
	{
		.click_config_provider = &thread_click_config,
		.content_offset_changed_handler = NULL
	};
	scroll_layer_set_callbacks(thread_scroll_layer, scrollOverride);

	thread_title_layer = layer_create(GRect(0, 0, window_frame.size.w, 22));
	layer_set_update_proc(thread_title_layer, thread_title_layer_update_proc);
	scroll_layer_add_child(thread_scroll_layer, thread_title_layer);

	layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(thread_scroll_layer));

	if(thread->type == 1)
	{
		// we are an image
		thread_body_layer = NULL;

		thread_bitmap_layer = bitmap_layer_create(GRect(0, 22, window_frame.size.w, window_frame.size.h));
		scroll_layer_add_child(thread_scroll_layer, bitmap_layer_get_layer(thread_bitmap_layer));

		scroll_layer_set_content_size(thread_scroll_layer, GSize(window_frame.size.w, 22 + window_frame.size.h));
	}
	else
	{
		thread_image = NULL;
		thread_bitmap_layer = NULL;

		thread_body_layer = text_layer_create(GRect(0, 22, window_frame.size.w, 10000));
		text_layer_set_font(thread_body_layer, GetFont());
		text_layer_set_text_alignment(thread_body_layer, GTextAlignmentCenter);
		scroll_layer_add_child(thread_scroll_layer, text_layer_get_layer(thread_body_layer));
	}
}

void thread_window_appear(Window *window)
{
	struct ThreadData *thread = GetSelectedThread();
	
	thread_offset = 0;
	thread_offset_reset = false;

	text_size = graphics_text_layout_get_content_size(thread->title, GetFont(), GRect(0, 0, 1024, THREAD_WINDOW_HEIGHT), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

	if(text_size.w > window_frame.size.w)
	{
		init_timer(app_timer_register(600, thread_scroll_timer_callback, NULL));
	}
}

void thread_window_disappear(Window *window)
{
	cancel_timer();
}

void thread_window_unload(Window *window)
{
	DEBUG_MSG("thread_window_unload");

	free_netimage();

	if(current_thread.body != NULL)
	{
		nt_Free(current_thread.body);
		current_thread.body = NULL;
	}

	if (thread_image != NULL)
	{
		DEBUG_MSG("gbitmap_destroy 2");
		gbitmap_destroy(thread_image);
		thread_image = NULL;
	}

	layer_destroy(thread_title_layer);

	if(thread_body_layer != NULL)
	{
		text_layer_destroy(thread_body_layer);
		thread_body_layer = NULL;
	}

	if(thread_bitmap_layer != NULL)
	{
		bitmap_layer_destroy(thread_bitmap_layer);
		thread_bitmap_layer = NULL;
	}

	scroll_layer_destroy(thread_scroll_layer);
}

void thread_display_image(GBitmap *image)
{
	if(image == NULL)
	{
		loading_disable_dots();
		loading_set_text("Unable to load image");
		return;
	}

	thread_load_finished();

	if (thread_image)
	{
		gbitmap_destroy(thread_image);
		DEBUG_MSG("gbitmap_destroy 1");
	}

	thread_image = image;

	if(thread_bitmap_layer == NULL)
	{
		return;
	}

	DEBUG_MSG("thread_display_image!");

	bitmap_layer_set_bitmap(thread_bitmap_layer, image);
}

static void thread_click_config(void *context)
{
	window_long_click_subscribe(BUTTON_ID_UP, 0, thread_button_up, NULL);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) thread_button_select);
	window_long_click_subscribe(BUTTON_ID_DOWN, 0, thread_button_down, NULL);
}

static void thread_button_up(ClickRecognizerRef recognizer, void *context)
{
	int current = GetSelectedThreadID();
	subreddit_button_up(recognizer, context);
	if(current != GetSelectedThreadID())
	{
		window_stack_pop(false);
		thread_load();
	}
}

static void thread_button_select(ClickRecognizerRef recognizer, void *context)
{
	threadmenu_init();
}

static void thread_button_down(ClickRecognizerRef recognizer, void *context)
{
	int current = GetSelectedThreadID();
	subreddit_button_down(recognizer, context);
	if(current != GetSelectedThreadID())
	{
		window_stack_pop(false);
		thread_load();
	}
}

static void thread_title_layer_update_proc(Layer *layer, GContext *ctx)
{
	graphics_context_set_text_color(ctx, GColorBlack);

	graphics_draw_text(ctx, GetSelectedThread()->title, GetFont(), GRect(-thread_offset, 0, window_frame.size.w + thread_offset, THREAD_WINDOW_HEIGHT), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void thread_scroll_timer_callback(void *data)
{
	if(thread_offset_reset)
	{
		thread_offset_reset = false;
		thread_offset = 0;
	}
	else
	{
		thread_offset += 4;
	}

	if(text_size.w - thread_offset < window_frame.size.w)
	{
		thread_offset_reset = true;
		init_timer(app_timer_register(1000, thread_scroll_timer_callback, NULL));
	}
	else
	{
		init_timer(app_timer_register(thread_offset == 0 ? 1000 : TITLE_SCROLL_SPEED, thread_scroll_timer_callback, NULL));
	}

	layer_mark_dirty(thread_title_layer);
}
