/* ECMAScript browser scripting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey-shared.h"
#include "network/connection.h"
#include "protocol/uri.h"
#include "protocol/protocol.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/cache_object.h"
#include "scripting/smjs/elinks_object.h"
#include "session/download.h"


struct smjs_load_uri_hop {
	struct session *ses;

	/* SpiderMonkey versions earlier than 1.8 cannot properly call
	 * a closure if given just a JSFunction pointer.  They need a
	 * JS::Value that points to the corresponding JSObject.  Besides,
	 * JS_AddNamedRoot is not documented to support JSFunction
	 * pointers.  */
	JS::MutableHandleValue callback;
};

static void
smjs_loading_callback(struct download *download, void *data)
{
	struct session *saved_smjs_ses = smjs_ses;
	struct smjs_load_uri_hop *hop = data;

	JS::Value args[1], rval;
	JSObject *cache_entry_object;

	if (is_in_progress_state(download->state)) return;

	JS::CallArgs argv;
	JS::RootedValue r_rval(smjs_ctx, rval);

	if (!download->cached) goto end;

	/* download->cached->object.refcount is typically 0 here
	 * because no struct document uses the cache entry.  Because
	 * the connection is no longer using the cache entry either,
	 * it can be garbage collected.  Don't let that happen while
	 * the script is using it.  */
	object_lock(download->cached);

	smjs_ses = hop->ses;

	cache_entry_object = smjs_get_cache_entry_object(download->cached);
	if (!cache_entry_object) goto end;

	args[0] = JS::ObjectValue(*cache_entry_object);
	argv = CallArgsFromVp(1, args);

	JS_CallFunctionValue(smjs_ctx, nullptr, hop->callback, argv, &r_rval);

end:
	if (download->cached)
		object_unlock(download->cached);
	mem_free(download->data);
	mem_free(download);

	smjs_ses = saved_smjs_ses;
}

static bool
smjs_load_uri(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::CallArgs args = CallArgsFromVp(argc, rval);

	struct smjs_load_uri_hop *hop;
	struct download *download;
	protocol_external_handler_T *external_handler;
	char *uri_string;
	struct uri *uri;

	if (argc < 2) return false;

	uri_string = jsval_to_string(smjs_ctx, args[0]);
	if (!uri_string || !*uri_string) return false;

	uri = get_uri(uri_string, 0);
	if (!uri) return false;

	external_handler = get_protocol_external_handler(NULL, uri);
	if (external_handler) {
		/* Because smjs_load_uri is carrying out an asynchronous
		 * operation, it is inappropriate to call an external
		 * handler here, so just return.  */
		return false;
	}

	download = mem_alloc(sizeof(*download));
	if (!download) {
		done_uri(uri);
		return false;
	}

	hop = mem_alloc(sizeof(*hop));
	if (!hop) {
		mem_free(download);
		done_uri(uri);
		return false;
	}

	hop->callback.set(args[1]);
	hop->ses = smjs_ses;

	download->data = hop;
	download->callback = (download_callback_T *) smjs_loading_callback;

	load_uri(uri, NULL, download, PRI_MAIN, CACHE_MODE_NORMAL, -1);

	done_uri(uri);

	return true;
}

void
smjs_init_load_uri_interface(void)
{
	if (!smjs_ctx || !smjs_elinks_object)
		return;

	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);

	JS_DefineFunction(smjs_ctx, r_smjs_elinks_object, "load_uri",
	                  &smjs_load_uri, 2, 0);
}
