/*******************************
	Thread Menu Window
********************************/

#include "Rebble.h"
#include "SubredditWindow.h"
#include "ThreadMenuWindow.h"

Window *window_threadmenu;

MenuLayer *threadmenu_menu_layer;

static uint16_t threadmenu_menu_get_num_sections_callback(MenuLayer *menu_layer, void *data);
static uint16_t threadmenu_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data);
static void threadmenu_menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data);
static int16_t threadmenu_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data);
static void threadmenu_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data);
static void threadmenu_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data);

void threadmenu_init()
{
	window_stack_push(window_threadmenu, true);
}

void threadmenu_window_load(Window *window)
{
	Layer *window_layer = window_get_root_layer(window);

	threadmenu_menu_layer = menu_layer_create(window_frame);

	menu_layer_set_callbacks(threadmenu_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_sections = threadmenu_menu_get_num_sections_callback,
		.get_num_rows = threadmenu_menu_get_num_rows_callback,
		.get_header_height = threadmenu_menu_get_header_height_callback,
		.draw_header = threadmenu_menu_draw_header_callback,
		.draw_row = threadmenu_menu_draw_row_callback,
		.select_click = threadmenu_menu_select_callback,
	});

	menu_layer_set_click_config_onto_window(threadmenu_menu_layer, window);

	layer_add_child(window_layer, menu_layer_get_layer(threadmenu_menu_layer));
}

void threadmenu_window_unload(Window *window)
{
	menu_layer_destroy(threadmenu_menu_layer);
}

static uint16_t threadmenu_menu_get_num_sections_callback(MenuLayer *menu_layer, void *data)
{
	return 1;
}

static uint16_t threadmenu_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
	switch (section_index)
	{
	case 0:
		return 3;

	default:
		return 0;
	}
}

static void threadmenu_menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data)
{
	switch (section_index)
	{
	case 0:
		menu_cell_basic_header_draw(ctx, cell_layer, "Thread Menu");
		break;
	}
}

static int16_t threadmenu_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
	return 24;
}

static void threadmenu_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
	menu_cell_title_draw(ctx, cell_layer, cell_index->row == 0 ? "Upvote" : (cell_index->row == 1 ? "Downvote" : "Save"));
}

static void threadmenu_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
	if(!IsLoggedIn())
	{
		vibes_double_pulse();
		return;
	}

	int selected = GetSelectedThreadID();

	switch (cell_index->row)
	{
	case 0:
		UpvoteThread(selected);
		break;

	case 1:
		DownvoteThread(selected);
		break;

	case 2:
		SaveThread(selected);
		break;
	}

	vibes_short_pulse();

	window_stack_pop(true);
}
