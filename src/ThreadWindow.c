/*******************************
    Thread Window
********************************/

#include <pebble.h>
#include "Rebble.h"
#include "ThreadWindow.h"
#include "SubredditWindow.h"
#include "LoadingWindow.h"
#include "ThreadMenuWindow.h"
#include "CommentWindow.h"

Window *window_thread;

GSize scroll_layer_size;

ScrollLayer *thread_scroll_layer;
Layer *thread_title_layer;
TextLayer *thread_body_layer = NULL;
TextLayer *thread_view_comments_layer;

bool thread_view_comments_selected;

BitmapLayer *thread_bitmap_layer;

static void thread_click_config(void *context);
static void thread_offset_changed_handler(ScrollLayer *scroll_layer, void *context);
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
	scroll_layer_set_content_size(thread_scroll_layer, GSize(window_frame.size.w, 0));
	scroll_layer_set_content_offset(thread_scroll_layer, GPoint(0, 0), false);

	ScrollLayerCallbacks scrollOverride =
	{
		.click_config_provider = &thread_click_config,
		.content_offset_changed_handler = &thread_offset_changed_handler
	};
	scroll_layer_set_callbacks(thread_scroll_layer, scrollOverride);

	thread_title_layer = layer_create(GRect(0, 0, window_frame.size.w, 22));
	layer_set_update_proc(thread_title_layer, thread_title_layer_update_proc);
	scroll_layer_add_child(thread_scroll_layer, thread_title_layer);

	layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(thread_scroll_layer));

	thread_view_comments_layer = text_layer_create(GRect(0, 0, window_frame.size.w, LOAD_COMMENTS_HEIGHT));
	text_layer_set_text(thread_view_comments_layer, "View Comments");
	text_layer_set_font(thread_view_comments_layer, GetBiggerFont());
	text_layer_set_text_alignment(thread_view_comments_layer, GTextAlignmentCenter);
	scroll_layer_add_child(thread_scroll_layer, text_layer_get_layer(thread_view_comments_layer));

	thread_view_comments_selected = false;

	if(thread->type == 1)
	{
		// we are an image
		thread_body_layer = NULL;

		thread_bitmap_layer = bitmap_layer_create(GRect(0, 22, window_frame.size.w, window_frame.size.h));
		scroll_layer_add_child(thread_scroll_layer, bitmap_layer_get_layer(thread_bitmap_layer));

		scroll_layer_set_content_size(thread_scroll_layer, GSize(window_frame.size.w, 22 + window_frame.size.h + 10));

		thread_update_comments_position();
	}
	else
	{
		//current_thread.image = NULL;
		thread_bitmap_layer = NULL;

		thread_body_layer = text_layer_create(GRect(0, 22, window_frame.size.w, 10000));
		text_layer_set_font(thread_body_layer, GetFont());
		scroll_layer_add_child(thread_scroll_layer, text_layer_get_layer(thread_body_layer));
	}
}

void thread_window_appear(Window *window)
{
	thread_offset = 0;
	thread_offset_reset = false;

	text_size = graphics_text_layout_get_content_size(GetThreadTitle(GetSelectedThreadID()), GetFont(), GRect(0, 0, 1024, THREAD_WINDOW_HEIGHT), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

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

	if (current_thread.image != NULL)
	{
		gbitmap_destroy(current_thread.image);
		current_thread.image = NULL;
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

	text_layer_destroy(thread_view_comments_layer);
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

	if (current_thread.image)
	{
		gbitmap_destroy(current_thread.image);
		DEBUG_MSG("gbitmap_destroy 1");
	}

	current_thread.image = image;

	if(thread_bitmap_layer == NULL)
	{
		return;
	}

	DEBUG_MSG("thread_display_image!");

	bitmap_layer_set_bitmap(thread_bitmap_layer, image);
}

static void thread_offset_changed_handler(ScrollLayer *scroll_layer, void *context)
{
	GPoint offset = scroll_layer_get_content_offset(scroll_layer);

	bool selected = (scroll_layer_size.h + offset.y - window_frame.size.h) <= THREAD_WINDOW_HEIGHT;
	if(thread_view_comments_selected != selected)
	{
		thread_view_comments_selected = selected;
		text_layer_set_text_color(thread_view_comments_layer, thread_view_comments_selected ? GColorWhite : GColorBlack);
		text_layer_set_background_color(thread_view_comments_layer, thread_view_comments_selected ? GColorBlack : GColorWhite);
	}
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
	if(thread_view_comments_selected)
	{
		comment_load(-1);
	}
	else
	{
		threadmenu_init();
	}
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
	graphics_draw_text(ctx, GetThreadTitle(GetSelectedThreadID()), GetFont(), GRect(-thread_offset, 0, window_frame.size.w + thread_offset, THREAD_WINDOW_HEIGHT), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void thread_scroll_timer_callback(void *data)
{
	if(thread_offset_reset)
	{
		thread_offset_reset = false;
		thread_offset = -THREAD_WINDOW_PADDING_TEXT_LEFT;
	}
	else
	{
		thread_offset += 4;
	}

	if(text_size.w - thread_offset - THREAD_WINDOW_PADDING_TEXT_LEFT < window_frame.size.w)
	{
		thread_offset_reset = true;
		init_timer(app_timer_register(1000, thread_scroll_timer_callback, NULL));
	}
	else
	{
		init_timer(app_timer_register(thread_offset == -THREAD_WINDOW_PADDING_TEXT_LEFT ? 1000 : TITLE_SCROLL_SPEED, thread_scroll_timer_callback, NULL));
	}

	layer_mark_dirty(thread_title_layer);
}

void thread_update_comments_position()
{
	scroll_layer_size = scroll_layer_get_content_size(thread_scroll_layer);

	Layer *layer = text_layer_get_layer(thread_view_comments_layer);

	GRect rect = layer_get_frame(layer);
	rect.origin.y = scroll_layer_size.h;
	layer_set_frame(layer, rect);

	scroll_layer_size.h += LOAD_COMMENTS_HEIGHT;

	scroll_layer_set_content_size(thread_scroll_layer, scroll_layer_size);
}
