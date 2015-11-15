/*
https://github.com/pebble-hacks/pebble-faces
*/

#include "netimage.h"
#include "Rebble.h"
#include "AppMessages.h"

NetImageContext *netimage_create_context(NetImageCallback callback)
{
	NetImageContext *ctx = nt_Malloc(sizeof(NetImageContext));

	ctx->length = 0;
	ctx->index = 0;
	ctx->data = NULL;
	ctx->callback = callback;

	return ctx;
}

void netimage_destroy_context(NetImageContext *ctx)
{
	if (ctx->data)
	{
		nt_Free(ctx->data);
	}
	nt_Free(ctx);
}

void netimage_request(int index)
{
	DictionaryIterator *outbox;
	
	app_message_outbox_begin(&outbox);
	
	dict_write_uint8(outbox, NETIMAGE_URL, (uint8_t)index);

	DEBUG_MSG("NETIMAGE_URL %d", index);

	app_message_outbox_send();
}

void netimage_receive(DictionaryIterator *iter)
{
	NetImageContext *ctx = get_netimage_context();

	Tuple *tuple = dict_read_first(iter);
	if (!tuple)
	{
		DEBUG_MSG("Got a message with no first key! Size of message: %li", (uint32_t)iter->end - (uint32_t)iter->dictionary);
		return;
	}

	switch (tuple->key)
	{
		case NETIMAGE_DATA:
			if (ctx->index + tuple->length <= ctx->length)
			{
				memcpy(ctx->data + ctx->index, tuple->value->data, tuple->length);
				ctx->index += tuple->length;
			}
			else
			{
				DEBUG_MSG("Not overriding rx buffer. Bufsize=%li BufIndex=%li DataLen=%i",
						ctx->length, ctx->index, tuple->length);
			}
			break;
		case NETIMAGE_BEGIN:
			DEBUG_MSG("Start transmission. Size=%lu", tuple->value->uint32);
			if (ctx->data != NULL)
			{
				nt_Free(ctx->data);
			}
			if(tuple->value->uint32 == 0)
			{
				ctx->data = NULL;
				break;
			}
			ctx->data = nt_Malloc(tuple->value->uint32);
			if (ctx->data != NULL)
			{
				ctx->length = tuple->value->uint32;
				ctx->index = 0;
			}
			else
			{
				DEBUG_MSG("Unable to allocate memory to receive image.");
				ctx->length = 0;
				ctx->index = 0;
			}
			break;
		case NETIMAGE_END:
			if (ctx->data && ctx->length > 0 && ctx->index > 0)
			{
				#ifdef PBL_BW
					GBitmap *bitmap = gbitmap_create_with_data(ctx->data);
				#else
					GBitmap *bitmap = gbitmap_create_from_png_data(ctx->data, ctx->length);
				#endif
				nt_Free(ctx->data);
				if (bitmap)
				{
					ctx->callback(bitmap);
				}
				else
				{
					ctx->callback(NULL);
					DEBUG_MSG("Unable to create GBitmap. Is this a valid PBI?");
				}
				ctx->data = NULL;
				ctx->index = ctx->length = 0;
			}
			else
			{
				ctx->callback(NULL);
				DEBUG_MSG("Got End message but we have no image...");
			}
			break;
		default:
			DEBUG_MSG("Unknown key in dict: %lu", tuple->key);
			break;
	}
}

