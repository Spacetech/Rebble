/*******************************
	Subreddit Window
********************************/

#include "Rebble.h"
#include "SubredditWindow.h"
#include "SubredditListWindow.h"
#include "ThreadWindow.h"
#include "LoadingWindow.h"

Window *window_subreddit;

ScrollLayer *subreddit_scroll_layer;

GBitmap *bitmap_upvote;
GBitmap *bitmap_image;
GBitmap *bitmap_text;

GRect sub_score_rect;
GRect sub_subreddit_rect;
GSize text_size;

InverterLayer *inverter_layer;
Layer *thread_sub_layer;
Layer *thread_load_more_layer;
Layer *thread_refresh_layer;

int thread_offset = 0;
bool thread_offset_reset = false;
bool thread_load_more_visible = false;

int thread_loaded = 0;

struct ViewThreadData current_thread;

static void subreddit_click_config_provider(void *context);
static void subreddit_layer_update_proc(Layer *layer, GContext *ctx);
static void subreddit_sub_layer_update_proc(Layer *layer, GContext *ctx);
static void subreddit_scroll_layer_update_proc(Layer *layer, GContext *ctx);
static void subreddit_scroll_timer_callback(void *data);

void subreddit_init()
{
	loading_uninit();
	window_stack_push(window_subreddit, true);
}

void subreddit_window_load(Window *window)
{
	bitmap_upvote = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_UPVOTE);
	bitmap_text = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEXT);
	bitmap_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_IMAGE);

	sub_score_rect = GRect(22, 0, 50, THREAD_WINDOW_HEIGHT);
	sub_subreddit_rect = GRect(55, 0, 68, THREAD_WINDOW_HEIGHT);

	subreddit_scroll_layer = scroll_layer_create(window_frame);

	scroll_layer_set_shadow_hidden(subreddit_scroll_layer, true);

	window_set_click_config_provider_with_context(window, subreddit_click_config_provider, subreddit_scroll_layer);

	thread_sub_layer = layer_create(GRect(0, THREAD_WINDOW_HEIGHT, window_frame.size.w, THREAD_WINDOW_HEIGHT));
	layer_set_update_proc(thread_sub_layer, subreddit_sub_layer_update_proc);

	thread_refresh_layer = layer_create_with_data(GRect(0, 0, window_frame.size.w, THREAD_WINDOW_HEIGHT_SELECTED), 4);
	layer_set_update_proc(thread_refresh_layer, subreddit_layer_update_proc);
	layer_set_hidden(thread_refresh_layer, false);
	scroll_layer_add_child(subreddit_scroll_layer, thread_refresh_layer);
	*(int*)layer_get_data(thread_refresh_layer) = -1;
	
	for(int i = 0; i < MAX_THREADS; ++i)
	{
		struct ThreadData *thread = GetThread(i);

		thread->layer = layer_create_with_data(GRect(0, 0, window_frame.size.w, i == 0 ? THREAD_WINDOW_HEIGHT_SELECTED : THREAD_WINDOW_HEIGHT), 4);
		layer_set_update_proc(thread->layer, subreddit_layer_update_proc);
		layer_set_hidden(thread->layer, true);
		scroll_layer_add_child(subreddit_scroll_layer, thread->layer);
		*(int*)layer_get_data(thread->layer) = i;
	}

	thread_load_more_layer = layer_create_with_data(GRect(0, 0, window_frame.size.w, THREAD_WINDOW_HEIGHT_SELECTED), 4);
	layer_set_update_proc(thread_load_more_layer, subreddit_layer_update_proc);
	layer_set_hidden(thread_load_more_layer, true);
	scroll_layer_add_child(subreddit_scroll_layer, thread_load_more_layer);
	*(int*)layer_get_data(thread_load_more_layer) = MAX_THREADS;

	inverter_layer = inverter_layer_create(GRect(0, 0, window_frame.size.w, THREAD_WINDOW_HEIGHT_SELECTED + THREAD_LAYER_PADDING));
	scroll_layer_add_child(subreddit_scroll_layer, inverter_layer_get_layer(inverter_layer));
	layer_set_hidden(inverter_layer_get_layer(inverter_layer), true);

	scroll_layer_set_content_size(subreddit_scroll_layer, GSize(window_frame.size.w, 0));
	scroll_layer_set_content_offset(subreddit_scroll_layer, GPoint(0, 0), false);

	layer_set_update_proc(scroll_layer_get_layer(subreddit_scroll_layer), subreddit_scroll_layer_update_proc);

	layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(subreddit_scroll_layer));
}

void subreddit_window_appear(Window *window)
{
	subreddit_selection_changed(false);
}

void subreddit_window_disappear(Window *window)
{
	cancel_timer();
}

void subreddit_window_unload(Window *window)
{
	for(int i = 0; i < MAX_THREADS; ++i)
	{
		layer_destroy(GetThread(i)->layer);
	}

	gbitmap_destroy(bitmap_upvote);
	gbitmap_destroy(bitmap_text);
 	gbitmap_destroy(bitmap_image);

 	layer_destroy(thread_refresh_layer);
 	layer_destroy(thread_load_more_layer);
	layer_destroy(thread_sub_layer);

	inverter_layer_destroy(inverter_layer);
	scroll_layer_destroy(subreddit_scroll_layer);
}

static void subreddit_click_config_provider(void *context)
{
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) subreddit_button_up);
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) subreddit_button_down);

	window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, (ClickHandler) subreddit_button_up);
	window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, (ClickHandler) subreddit_button_down);

	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) subreddit_button_select);
	window_long_click_subscribe(BUTTON_ID_SELECT, 750, (ClickHandler) subreddit_button_select_long, NULL);
}

static void subreddit_layer_update_proc(Layer *layer, GContext *ctx)
{
	graphics_context_set_text_color(ctx, GColorBlack);

	void *data = layer_get_data(layer);
	int index = *(int*)data;

	struct ThreadData *thread = NULL;

draw_text:
	if(index == -1 || index == MAX_THREADS || thread != NULL)
	{
		GRect rect = GRect(0, index == GetSelectedThreadID() ? 10 : -1, window_frame.size.w, THREAD_WINDOW_HEIGHT_SELECTED);
		graphics_draw_text(ctx, thread != NULL ? thread->title : (index == -1 ? "Refresh" : "Load More"), index == GetSelectedThreadID() ? GetBiggerFont() : GetFont(), rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
		return;
	}

	thread = GetThread(index);
	if(thread->type >= 250)
	{
		goto draw_text;
	}

	GRect rect = GRect(0, 0, window_frame.size.w, THREAD_WINDOW_HEIGHT);

	if(index == GetSelectedThreadID())
	{
		rect.origin.x -= thread_offset;
		rect.size.w += thread_offset;
	}
	
	graphics_draw_text(ctx, thread->title, GetFont(), rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void subreddit_sub_layer_update_proc(Layer *layer, GContext *ctx)
{
	struct ThreadData *thread = GetSelectedThread();
	if(thread->type >= 250)
	{
		return;
	}

	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_context_set_text_color(ctx, GColorBlack);

	graphics_draw_bitmap_in_rect(ctx, bitmap_upvote, GRect(5, 5, 12, 15));

	graphics_draw_text(ctx, thread->score, GetFont(), sub_score_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

	if(thread->subreddit != NULL)
	{
		graphics_draw_text(ctx, thread->subreddit, GetFont(), sub_subreddit_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
	}

	graphics_draw_bitmap_in_rect(ctx, thread->type == 1 ? bitmap_image : bitmap_text, GRect(window_frame.size.w - (thread->type == 1 ? 20 : 21), 8, thread->type == 1 ? 16 : 15, thread->type == 1 ? 12 : 9));
}

static void subreddit_scroll_layer_update_proc(Layer *layer, GContext *ctx)
{
	GPoint offset = scroll_layer_get_content_offset(subreddit_scroll_layer);

	int y = offset.y + THREAD_LAYER_PADDING_HALF;

	int max = thread_loaded;

	if(thread_load_more_visible)
	{
		max++;
	}

	for(int i=-1; i < max; i++)
	{
		y += (GetSelectedThreadID() == i ? THREAD_WINDOW_HEIGHT_SELECTED : THREAD_WINDOW_HEIGHT);
		y += THREAD_LAYER_PADDING_HALF;
		graphics_draw_line(ctx, GPoint(0, y), GPoint(window_frame.size.w, y));
		y += THREAD_LAYER_PADDING_HALF;
	}
}

void subreddit_show_load_more()
{
	thread_load_more_visible = true;
	layer_set_hidden(thread_load_more_layer, false);
}

void subreddit_hide_load_more()
{
	thread_load_more_visible = false;
	layer_set_hidden(thread_load_more_layer, true);
}

#define UPDATE_LAYER_PROPS(layer, index) \
	height = THREAD_WINDOW_HEIGHT;\
	if(GetSelectedThreadID() == index)\
	{\
		height = THREAD_WINDOW_HEIGHT_SELECTED;\
		layer_set_frame(inverter_layer_get_layer(inverter_layer), GRect(0, y - THREAD_LAYER_PADDING_HALF, window_frame.size.w, height + THREAD_LAYER_PADDING));\
	}\
	layer_set_frame(layer, GRect(0, y, window_frame.size.w, height));\
	y += height + THREAD_LAYER_PADDING;

void subreddit_selection_changed(bool before)
{
	if(thread_loaded == 0)
	{
		return;
	}

	if(before)
	{
		if(GetSelectedThreadID() == -1 || GetSelectedThreadID() == MAX_THREADS)
		{
			return;
		}
		layer_remove_from_parent(thread_sub_layer);
		layer_mark_dirty(GetSelectedThread()->layer);
		return;
	}

	cancel_timer();

	int y = THREAD_LAYER_PADDING_HALF;
	int height;

	UPDATE_LAYER_PROPS(thread_refresh_layer, -1);
	for(int i=0; i < MAX_THREADS; i++)
	{
		UPDATE_LAYER_PROPS(GetThread(i)->layer, i);
	}
	UPDATE_LAYER_PROPS(thread_load_more_layer, MAX_THREADS);

	if(GetSelectedThreadID() == -1 || GetSelectedThreadID() == MAX_THREADS)
	{
		return;
	}

	thread_offset = 0;
	thread_offset_reset = false;

	struct ThreadData *thread = GetSelectedThread();

	layer_add_child(thread->layer, thread_sub_layer);

	text_size = graphics_text_layout_get_content_size(thread->title, GetFont(), GRect(0, 0, 1024, THREAD_WINDOW_HEIGHT), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

	if(text_size.w > window_frame.size.w)
	{
		init_timer(app_timer_register(600, subreddit_scroll_timer_callback, NULL));
	}
}

void subreddit_button_up(ClickRecognizerRef recognizer, void *context)
{
	if(GetSelectedThreadID() > -1)
	{
		subreddit_selection_changed(true);
		SetSelectedThreadID(GetSelectedThreadID() - 1);
		subreddit_set_content_offset(GetSelectedThreadID());
		subreddit_selection_changed(false);
	}
}

void subreddit_button_select(ClickRecognizerRef recognizer, void *context)
{
	if(GetSelectedThreadID() == -1)
	{
		subreddit_load_setup();
		LoadSubreddit("0");
	}
	else if(GetSelectedThreadID() == MAX_THREADS)
	{
		subreddit_load_setup();
		LoadThreadNext();
	}
	else if(thread_loaded > 0 && GetSelectedThread()->type < 250)
	{
		thread_load();
	}
}

void subreddit_button_select_long(ClickRecognizerRef recognizer, void *context)
{
	subredditlist_load();
}

void subreddit_button_down(ClickRecognizerRef recognizer, void *context)
{
	if(GetSelectedThreadID() < thread_loaded - 1 || (thread_loaded == MAX_THREADS && GetSelectedThreadID() < MAX_THREADS && thread_load_more_visible))
	{
		subreddit_selection_changed(true);
		SetSelectedThreadID(GetSelectedThreadID() + 1);
		subreddit_set_content_offset(GetSelectedThreadID());
		subreddit_selection_changed(false);
	}
}

void subreddit_set_content_offset(int index)
{
	int selectionY = THREAD_WINDOW_HEIGHT + THREAD_LAYER_PADDING + THREAD_LAYER_PADDING_HALF;
	selectionY += index * THREAD_WINDOW_HEIGHT + index * THREAD_LAYER_PADDING + THREAD_LAYER_PADDING_HALF;
	selectionY += (THREAD_WINDOW_HEIGHT_SELECTED / 2) - (window_frame.size.h / 2);
	selectionY = selectionY > 0 ? selectionY : 0;

	GPoint offset = scroll_layer_get_content_offset(subreddit_scroll_layer);
	offset.y = -selectionY;
	scroll_layer_set_content_offset(subreddit_scroll_layer, offset, true);
}

static void subreddit_scroll_timer_callback(void *data)
{
	struct ThreadData *thread = GetSelectedThread();

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
		init_timer(app_timer_register(1000, subreddit_scroll_timer_callback, NULL));
	}
	else
	{
		init_timer(app_timer_register(thread_offset == 0 ? 1000 : TITLE_SCROLL_SPEED, subreddit_scroll_timer_callback, NULL));
	}

	layer_mark_dirty(thread->layer);
}

void subreddit_load_setup()
{
	thread_loaded = 0;
	thread_offset_reset = false;
	thread_offset = 0;

	SetSelectedThreadID(0);

	scroll_layer_set_content_offset(subreddit_scroll_layer, GPoint(0, 0), false);
	scroll_layer_set_content_size(subreddit_scroll_layer, GSize(window_frame.size.w, 0));

	subreddit_hide_load_more();

	layer_remove_from_parent(thread_sub_layer);

	for(int i = 0; i < MAX_THREADS; ++i)
	{
		struct ThreadData *thread = GetThread(i);
		
		if(thread->title != NULL)
		{
			nt_Free(thread->title);
			thread->title = NULL;
		}

		if(thread->score != NULL)
		{
			nt_Free(thread->score);
			thread->score = NULL;
		}

		if(thread->subreddit != NULL)
		{
			nt_Free(thread->subreddit);
			thread->subreddit = NULL;
		}

		layer_set_hidden(thread->layer, true);
	}

	window_stack_pop_all(true);
	loading_init();
}
