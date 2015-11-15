/*******************************
	Comment Window
********************************/

#include "Rebble.h"
#include "CommentWindow.h"
#include "ThreadWindow.h"
#include "LoadingWindow.h"

extern GBitmap *bitmap_upvote;

Window *window_comment;

ScrollLayer *comment_scroll_layer;
Layer *comment_header_layer;
TextLayer *comment_body_layer;

GRect comment_author_rect;
GRect comment_author_fill_rect;
GRect comment_upvote_rect;
GRect comment_score_rect;

static void comment_header_layer_update_proc(Layer *layer, GContext *ctx);
static void comment_click_config(void *context);
static void comment_button_back(ClickRecognizerRef recognizer, void *context);
static void comment_button_up(ClickRecognizerRef recognizer, void *context);
static void comment_button_select(ClickRecognizerRef recognizer, void *context);
static void comment_button_down(ClickRecognizerRef recognizer, void *context);
static void comment_auto_resize_body();

void comment_load(int dir)
{
	Tuplet tuple = TupletInteger(LOAD_COMMENTS, dir);

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL)
	{
		return;
	}

	dict_write_tuplet(iter, &tuple);

	app_message_outbox_send();
	
	loading_init();

	loading_set_text("Loading Comments");
}

void comment_load_finished()
{
	if(loading_visible())
	{
		loading_uninit();
		comment_init();
	}
}

void comment_init()
{
	if(window_stack_contains_window(window_comment))
	{
		// we are changing comments
		// don't do anything..
		layer_mark_dirty(comment_header_layer);

		text_layer_destroy(comment_body_layer);
		comment_body_layer = comment_body_layer_create();
		scroll_layer_add_child(comment_scroll_layer, text_layer_get_layer(comment_body_layer));

		comment_auto_resize_body();

		scroll_layer_set_content_offset(comment_scroll_layer, GPoint(0, 0), false);
	}
	else
	{
		window_stack_pop(false);
		window_stack_push(window_comment, true);
	}
}

void comment_window_load(Window *window)
{
	comment_author_rect = GRect(1, -3, window_frame.size.w - 51, 22);
	comment_author_fill_rect = GRect(1, 1, window_frame.size.w - 51, 18);
	comment_upvote_rect = GRect(window_frame.size.w - 50, 2, 12, 14);
	comment_score_rect = GRect(window_frame.size.w - 35, -3, 35, 22);

	comment_scroll_layer = scroll_layer_create(window_frame);

	scroll_layer_set_shadow_hidden(comment_scroll_layer, true);
	scroll_layer_set_click_config_onto_window(comment_scroll_layer, window);
	scroll_layer_set_content_size(comment_scroll_layer, GSize(window_frame.size.w, 0));
	scroll_layer_set_content_offset(comment_scroll_layer, GPoint(0, 0), false);

	layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(comment_scroll_layer));

	ScrollLayerCallbacks scrollOverride =
	{
		.click_config_provider = &comment_click_config,
		.content_offset_changed_handler = NULL
	};
	scroll_layer_set_callbacks(comment_scroll_layer, scrollOverride);

	comment_header_layer = layer_create(GRect(0, 0, window_frame.size.w, THREAD_WINDOW_HEIGHT));
	layer_set_update_proc(comment_header_layer, comment_header_layer_update_proc);
	scroll_layer_add_child(comment_scroll_layer, comment_header_layer);	

	comment_body_layer = comment_body_layer_create();

	scroll_layer_add_child(comment_scroll_layer, text_layer_get_layer(comment_body_layer));

	comment_auto_resize_body();
}

void comment_window_unload(Window *window)
{
	if(current_thread.author != NULL)
	{
		nt_Free(current_thread.author);
		current_thread.author = NULL;
	}

	if(current_thread.score != NULL)
	{
		nt_Free(current_thread.score);
		current_thread.score = NULL;
	}

	if(current_thread.comment != NULL)
	{
		nt_Free(current_thread.comment);
		current_thread.comment = NULL;
	}

	layer_destroy(comment_header_layer);
	text_layer_destroy(comment_body_layer);

	scroll_layer_destroy(comment_scroll_layer);
}

static void comment_click_config(void *context)
{
	window_single_click_subscribe(BUTTON_ID_BACK , (ClickHandler) comment_button_back);
	window_long_click_subscribe(BUTTON_ID_UP, 0, comment_button_up, NULL);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) comment_button_select);
	window_long_click_subscribe(BUTTON_ID_DOWN, 0, comment_button_down, NULL);
}

static void comment_button_back(ClickRecognizerRef recognizer, void *context)
{
	if(current_thread.depth == 0)
	{
		// it should look like you're going back
		window_stack_pop(true);
		thread_load();
	}
	else
	{
		current_thread.depth--;
		comment_load(3);
	}
}

static void comment_button_up(ClickRecognizerRef recognizer, void *context)
{
	if(current_thread.index <= 0)
	{
		vibes_double_pulse();
	}
	else
	{
		comment_load(1);
	}
}

static void comment_button_select(ClickRecognizerRef recognizer, void *context)
{
	!current_thread.nextDepthPossible ? vibes_double_pulse() : comment_load(2);
}

static void comment_button_down(ClickRecognizerRef recognizer, void *context)
{
	if(current_thread.index >= (current_thread.max - 1))
	{
		vibes_double_pulse();
	}
	else
	{
		comment_load(0);
	}
}

static void comment_header_layer_update_proc(Layer *layer, GContext *ctx)
{
	graphics_context_set_text_color(ctx, GColorBlack);
	graphics_draw_text(ctx, current_thread.score, GetFont(), comment_score_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

	if(current_thread.thread_author != NULL && current_thread.author != NULL && strcmp(current_thread.thread_author, current_thread.author) == 0)
	{
		graphics_context_set_fill_color(ctx, GColorBlack);

		GSize textSize = graphics_text_layout_get_content_size(current_thread.author, GetFont(), comment_author_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
		comment_author_fill_rect.size.w = textSize.w + 1;
		graphics_fill_rect(ctx, comment_author_fill_rect, 0, GCornerNone);

		graphics_context_set_text_color(ctx, GColorWhite);
	}
	
	graphics_draw_text(ctx, current_thread.author, GetFont(), comment_author_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

	graphics_draw_bitmap_in_rect(ctx, bitmap_upvote, comment_upvote_rect);

	GPoint point = GPoint(0, THREAD_WINDOW_HEIGHT - 1);

	for(int i=0; i < (current_thread.depth + 1); i++)
	{
		GPoint nextPoint = GPoint(point.x + 20, point.y);
		graphics_draw_line(ctx, point, nextPoint);
		point = nextPoint;
		point.x += 10;
	}
}

static void comment_auto_resize_body()
{
	GSize size = text_layer_get_content_size(comment_body_layer);
	size.h += 5;
	text_layer_set_size(comment_body_layer, size);
	scroll_layer_set_content_size(comment_scroll_layer, GSize(window_frame.size.w, THREAD_WINDOW_HEIGHT + size.h + 5));
}

TextLayer* comment_body_layer_create()
{
	TextLayer *comment_body_layer = text_layer_create(GRect(0, THREAD_WINDOW_HEIGHT, window_frame.size.w, 10000));
	text_layer_set_font(comment_body_layer, GetFont());
	text_layer_set_text(comment_body_layer, current_thread.comment);
	return comment_body_layer;
}