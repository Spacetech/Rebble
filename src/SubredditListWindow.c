/*******************************
	Subreddit List Window
********************************/

#include "Rebble.h"
#include "SubredditListWindow.h"
#include "SubredditWindow.h"
#include "ThreadWindow.h"
#include "LoadingWindow.h"

Window *window_subredditlist;

MenuLayer *subredditlist_menu_layer;

char* *user_subreddits = NULL;
int subredditlist_num = -1;

static uint16_t subredditlist_menu_get_num_sections_callback(MenuLayer *menu_layer, void *data);
static uint16_t subredditlist_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data);
static void subredditlist_menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data);
static int16_t subredditlist_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data);
static void subredditlist_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data);
static void subredditlist_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data);

void subredditlist_load()
{
	Tuplet tuple = TupletCString(LOAD_SUBREDDITLIST, "");

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (iter == NULL)
	{
		return;
	}

	dict_write_tuplet(iter, &tuple);

	app_message_outbox_send();

	subredditlist_num = 0;
	
	loading_init();

	loading_set_text("Loading Subreddits");
}

void subredditlist_init()
{
	window_stack_push(window_subredditlist, true);
}

void subredditlist_window_load(Window *window)
{
	Layer *window_layer = window_get_root_layer(window);

	subredditlist_menu_layer = menu_layer_create(window_frame);

	menu_layer_set_callbacks(subredditlist_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_sections = subredditlist_menu_get_num_sections_callback,
		.get_num_rows = subredditlist_menu_get_num_rows_callback,
		.get_header_height = subredditlist_menu_get_header_height_callback,
		.draw_header = subredditlist_menu_draw_header_callback,
		.draw_row = subredditlist_menu_draw_row_callback,
		.select_click = subredditlist_menu_select_callback,
	});

	menu_layer_set_click_config_onto_window(subredditlist_menu_layer, window);

	layer_add_child(window_layer, menu_layer_get_layer(subredditlist_menu_layer));
}

void subredditlist_window_unload(Window *window)
{
	menu_layer_destroy(subredditlist_menu_layer);

	if(user_subreddits != NULL)
	{
		for(int i=0; i < subredditlist_num; i++)
		{
			nt_Free(user_subreddits[i]);
		}

		nt_Free(user_subreddits);
		user_subreddits = NULL;
	}

	subredditlist_num = -1;
}

static uint16_t subredditlist_menu_get_num_sections_callback(MenuLayer *menu_layer, void *data)
{
	return 1;
}

static uint16_t subredditlist_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
	switch (section_index)
	{
	case 0:
		return subredditlist_num + 1;

	default:
		return 0;
	}
}

static void subredditlist_menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data)
{
	switch (section_index)
	{
	case 0:
		menu_cell_basic_header_draw(ctx, cell_layer, "Subreddits");
		break;
	}
}

static int16_t subredditlist_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
	return 24;
}

static void subredditlist_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
	menu_cell_title_draw(ctx, cell_layer, cell_index->row  == 0 ? "Frontpage" : user_subreddits[cell_index->row - 1]);
}

static void subredditlist_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
	LoadSubreddit(cell_index->row == 0 ? "" : user_subreddits[cell_index->row - 1]);
	subreddit_load_setup();
}
