 /*******************************
	Loading Window
********************************/

#include "Rebble.h"
#include "LoadingWindow.h"

#define CONNECTION_LOST_TEXT "Lost connection\nto phone\n"

Window *window_loading;

GRect window_frame;

GBitmap *icon;

Layer *loading_layer;
TextLayer *loading_text_layer;
TextLayer *loading_text_progress_layer;

char loading_text[3];
int dots = 0;
bool dotting = true;

static void loading_layer_update_proc(Layer *layer, GContext *ctx);
static void loading_progress_update(void *data);

void loading_init()
{
	dots = 0;
	dotting = true;

	window_stack_push(window_loading, false);

	if(IsBluetoothConnected())
	{
		text_layer_set_text(loading_text_layer, "Loading");
	}
	else
	{
		loading_disable_dots();
		text_layer_set_text(loading_text_layer, CONNECTION_LOST_TEXT);
	}
}

void loading_uninit()
{
	window_stack_remove(window_loading, false);
}

void loading_window_load(Window *window)
{
	Layer *window_layer = window_get_root_layer(window);
	window_frame = layer_get_frame(window_layer);

	icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON);

	loading_layer = layer_create(window_frame);
	layer_set_update_proc(loading_layer, loading_layer_update_proc);

	GRect rect = window_frame;
	rect.origin.y = window_frame.size.h * 0.55;
	rect.size.h = rect.origin.y;

	loading_text_layer = text_layer_create(rect);
	text_layer_set_font(loading_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_alignment(loading_text_layer, GTextAlignmentCenter);
	layer_add_child(loading_layer, text_layer_get_layer(loading_text_layer));

	loading_text_progress_layer = text_layer_create(GRect(window_frame.size.w * 0.6, window_frame.size.h * 0.385, window_frame.size.w * 0.4, 30));
	text_layer_set_text(loading_text_progress_layer, loading_text);
	layer_add_child(loading_layer, text_layer_get_layer(loading_text_progress_layer));

	layer_add_child(window_layer, loading_layer);

	init_timer(app_timer_register(500, loading_progress_update, NULL));
}

void loading_window_disappear(Window *window)
{
	cancel_timer();
}

void loading_window_unload(Window *window)
{
	text_layer_destroy(loading_text_layer);
	text_layer_destroy(loading_text_progress_layer);
	layer_destroy(loading_layer);
	gbitmap_destroy(icon);
}

static void loading_layer_update_proc(Layer *layer, GContext *ctx)
{
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_draw_bitmap_in_rect(ctx, icon, GRect(window_frame.size.w / 2 - 12, window_frame.size.h / 2 - 30, 24, 28));
}

static void loading_progress_update(void *data)
{
	if(!dotting)
	{
		return;
	}

	for(int i=0; i < dots; i++)
	{
		loading_text[i] = '.';
	}

	loading_text[dots] = '\0';

	dots++;

	if(dots > 3)
	{
		dots = 0;
	}

	layer_mark_dirty(text_layer_get_layer(loading_text_progress_layer));

	init_timer(app_timer_register(500, loading_progress_update, NULL));
}

bool loading_visible()
{
	return window_stack_contains_window(window_loading);
}

void loading_set_text(char *loadingText)
{
	if(IsBluetoothConnected())
	{
		text_layer_set_text(loading_text_layer, loadingText);
	}
}

void loading_disable_dots()
{
	dotting = false;
	loading_text[0] = '\0';
}

void loading_disconnected()
{
	loading_disable_dots();
	loading_set_text(CONNECTION_LOST_TEXT);
}
