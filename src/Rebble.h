#ifndef REBBLE_H
#define REBBLE_H

#include <pebble.h>
#include "netimage.h"

#define MAX_THREADS 15
#define TITLE_SCROLL_SPEED 100

#define THREAD_LAYER_PADDING 8
#define THREAD_LAYER_PADDING_HALF (THREAD_LAYER_PADDING / 2)
#define THREAD_LAYER_PADDING_TEXT_LEFT 2

#define THREAD_WINDOW_HEIGHT 22
#define THREAD_WINDOW_HEIGHT_SELECTED (THREAD_WINDOW_HEIGHT * 2)
#define THREAD_WINDOW_PADDING_TEXT_LEFT 2

#define LOAD_COMMENTS_HEIGHT 32

//#define USE_PERSIST_STRINGS

#ifdef USE_PERSIST_STRINGS
#define PERSIST_OFFSET_THREAD_TITLE 4096
#define PERSIST_OFFSET_THREAD_SCORE 5120
#define PERSIST_OFFSET_THREAD_SUBREDDIT 6144
#endif

//#define DEBUG_MODE

#ifdef DEBUG_MODE
	#define DEBUG_MSG(args...) APP_LOG(APP_LOG_LEVEL_DEBUG, args)
#else
	#define DEBUG_MSG(args...)
#endif

#define MSG(args...) APP_LOG(APP_LOG_LEVEL_DEBUG, args)

enum
{
	STATE_SUBREDDIT = 0x0,
	STATE_THREAD = 0x0
};

enum
{
	VIEW_SUBREDDIT = 0x0,
	VIEW_THREAD = 0x1,

	VIEW_SUBREDDIT_NEXT = 0x2,

	THREAD_ID = 0x3,
	THREAD_TITLE = 0x4,
	THREAD_SCORE = 0x5,
	THREAD_TYPE = 0x6,

	THREAD_BODY = 0x7,

	THREAD_UPVOTE = 0x8,
	THREAD_DOWNVOTE = 0x9,
	THREAD_SAVE = 0xA,

	USER_SUBREDDIT = 0xB,

	LOAD_SUBREDDITLIST = 0xD,

	CHUNK_SIZE = 0xE,

	READY = 0xF,

	THREAD_SUBREDDIT = 0x10,

	LOAD_COMMENTS = 0x11,
	THREAD_COMMENT = 0x12,

	LAST_DICT_ITEM = 0x13
};

struct ThreadData
{
#ifndef USE_PERSIST_STRINGS
	char *title;
	char *score;
	char *subreddit;
#endif
	unsigned char type;
	Layer *layer;
};

struct ViewThreadData
{
	// only body & thread_author is actual thread data
	// the other stuff is for comments
	char *thread_author;
	char *score;
	char *body;
	char *comment;
	char *author;
	GBitmap *image;
	unsigned char depth;
	unsigned char index;
	unsigned char max;
	bool nextDepthPossible;
};

extern Window *window_subreddit;
extern Window *window_subredditlist;
extern Window *window_thread;
extern Window *window_threadmenu;
extern Window *window_loading;
extern Window *window_comment;

extern int thread_offset;

extern struct ViewThreadData current_thread;

GFont* GetFont();
GFont* GetBiggerFont();

bool IsLoggedIn();
void SetLoggedIn(bool lin);

void LoadSubreddit(char *name);
void LoadThread(int index);
void LoadThreadNext();
void UpvoteThread(int index);
void DownvoteThread(int index);
void SaveThread(int index);

#ifdef DEBUG_MODE
void* nt_Malloc_Raw(size_t size, const char* function, int line);
void nt_Free_Raw(void* pointer);
void nt_Stats();
#endif

struct ThreadData* GetThread(int index);
struct ThreadData* GetSelectedThread();
void SetSelectedThreadID(int index);
int GetSelectedThreadID();

char* GetThreadTitle(int index);
char* GetThreadScore(int index);
char* GetThreadSubreddit(int index);

void SetThreadTitle(struct ThreadData* thread, int index, char* str);
void SetThreadScore(struct ThreadData* thread, int index, char* str);
void SetThreadSubreddit(struct ThreadData* thread, int index, char* str);

void init_netimage(int index);
void callback_netimage(GBitmap *image);
NetImageContext* get_netimage_context();
void free_netimage();

void init_timer(AppTimer *handle);
void cancel_timer();

bool IsBluetoothConnected();
void OnBluetoothConnection(bool connected);

#ifdef DEBUG_MODE
	#define nt_Malloc(size) nt_Malloc_Raw((size), __FUNCTION__, __LINE__)
	#define nt_Free(pointer) nt_Free_Raw((pointer))
#else
	#define nt_Malloc(size) malloc((size))
	#define nt_Free(pointer) free((pointer))
#endif

#endif
