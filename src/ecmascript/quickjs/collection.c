/* The QuickJS html collection object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs/collection.h"
#include "ecmascript/quickjs/element.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>
#include <libxml++/attributenode.h>
#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <algorithm>
#include <string>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_htmlCollection_class_id;

static JSValue
js_htmlCollection_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	xmlpp::Node::NodeSet *ns = JS_GetOpaque(this_val, js_htmlCollection_class_id);

	if (!ns) {
		return JS_NewInt32(ctx, 0);
	}

	return JS_NewInt32(ctx, ns->size());
}

static JSValue
js_htmlCollection_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeSet *ns = JS_GetOpaque(this_val, js_htmlCollection_class_id);

	if (!ns) {
		return JS_UNDEFINED;
	}

	xmlpp::Element *element;

	try {
		element = ns->at(idx);
	} catch (std::out_of_range e) { return JS_UNDEFINED;}

	if (!element) {
		return JS_UNDEFINED;
	}

	return getElement(ctx, element);
}

static JSValue
js_htmlCollection_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_UNDEFINED;
	}

	int index;
	JS_ToInt32(ctx, &index, argv[0]);

	return js_htmlCollection_item2(ctx, this_val, index);
}

static JSValue
js_htmlCollection_namedItem2(JSContext *ctx, JSValueConst this_val, const char *str)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeSet *ns = JS_GetOpaque(this_val, js_htmlCollection_class_id);

	if (!ns) {
		return JS_UNDEFINED;
	}

	xmlpp::ustring name = str;

	auto it = ns->begin();
	auto end = ns->end();

	for (; it != end; ++it) {
		const auto element = dynamic_cast<const xmlpp::Element*>(*it);

		if (!element) {
			continue;
		}

		if (name == element->get_attribute_value("id")
		|| name == element->get_attribute_value("name")) {
			return getElement(ctx, element);
		}
	}

	return JS_UNDEFINED;
}

static JSValue
js_htmlCollection_namedItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_UNDEFINED;
	}

	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}

	JSValue ret = js_htmlCollection_namedItem2(ctx, this_val, str);
	JS_FreeCString(ctx, str);

	return ret;
}

static void
js_htmlCollection_set_items(JSContext *ctx, JSValue this_val, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int counter = 0;

	xmlpp::Node::NodeSet *ns = JS_GetOpaque(this_val, js_htmlCollection_class_id);

	if (!ns) {
		return;
	}

	xmlpp::Element *element;

	while (1) {
		try {
			element = ns->at(counter);
		} catch (std::out_of_range e) { return;}

		if (!element) {
			return;
		}

		JSValue obj = getElement(ctx, element);
		JS_SetPropertyUint32(ctx, this_val, counter, obj);

		xmlpp::ustring name = element->get_attribute_value("id");
		if (name == "") {
			name = element->get_attribute_value("name");
		}
		if (name != "" && name != "item" && name != "namedItem") {
			JS_DefinePropertyValueStr(ctx, this_val, name.c_str(), obj, 0);
		}
		counter++;
	}
}

static const JSCFunctionListEntry js_htmlCollection_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_htmlCollection_get_property_length, nullptr),
	JS_CFUNC_DEF("item", 1, js_htmlCollection_item),
	JS_CFUNC_DEF("namedItem", 1, js_htmlCollection_namedItem),
};

static JSClassDef js_htmlCollection_class = {
	"htmlCollection",
};

static JSValue
js_htmlCollection_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_htmlCollection_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	return obj;

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_htmlCollection_init(JSContext *ctx, JSValue global_obj)
{
	JSValue htmlCollection_proto, htmlCollection_class;

	/* create the htmlCollection class */
	JS_NewClassID(&js_htmlCollection_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_htmlCollection_class_id, &js_htmlCollection_class);

	htmlCollection_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, htmlCollection_proto, js_htmlCollection_proto_funcs, countof(js_htmlCollection_proto_funcs));

	htmlCollection_class = JS_NewCFunction2(ctx, js_htmlCollection_ctor, "htmlCollection", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, htmlCollection_class, htmlCollection_proto);
	JS_SetClassProto(ctx, js_htmlCollection_class_id, htmlCollection_proto);

	JS_SetPropertyStr(ctx, global_obj, "htmlCollection", htmlCollection_proto);
	return 0;
}

JSValue
getCollection(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue htmlCollection_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, htmlCollection_obj, js_htmlCollection_proto_funcs, countof(js_htmlCollection_proto_funcs));
//	htmlCollection_class = JS_NewCFunction2(ctx, js_htmlCollection_ctor, "htmlCollection", 0, JS_CFUNC_constructor, 0);
//	JS_SetConstructor(ctx, htmlCollection_class, htmlCollection_obj);
	JS_SetClassProto(ctx, js_htmlCollection_class_id, htmlCollection_obj);

	JS_SetOpaque(htmlCollection_obj, node);
	js_htmlCollection_set_items(ctx, htmlCollection_obj, node);

	return htmlCollection_obj;
}
