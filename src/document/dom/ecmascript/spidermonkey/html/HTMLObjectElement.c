#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLObjectElement.h"
#include "dom/node.h"

static JSBool
HTMLObjectElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct OBJECT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLObjectElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OBJECT_ELEMENT_FORM:
		string_to_jsval(ctx, vp, html->form);
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE:
		string_to_jsval(ctx, vp, html->code);
		break;
	case JSP_HTML_OBJECT_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_OBJECT_ELEMENT_ARCHIVE:
		string_to_jsval(ctx, vp, html->archive);
		break;
	case JSP_HTML_OBJECT_ELEMENT_BORDER:
		string_to_jsval(ctx, vp, html->border);
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE_BASE:
		string_to_jsval(ctx, vp, html->code_base);
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE_TYPE:
		string_to_jsval(ctx, vp, html->code_type);
		break;
	case JSP_HTML_OBJECT_ELEMENT_DATA:
		string_to_jsval(ctx, vp, html->data);
		break;
	case JSP_HTML_OBJECT_ELEMENT_DECLARE:
		string_to_jsval(ctx, vp, html->declare);
		break;
	case JSP_HTML_OBJECT_ELEMENT_HEIGHT:
		string_to_jsval(ctx, vp, html->height);
		break;
	case JSP_HTML_OBJECT_ELEMENT_HSPACE:
		string_to_jsval(ctx, vp, html->hspace);
		break;
	case JSP_HTML_OBJECT_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_OBJECT_ELEMENT_STANDBY:
		string_to_jsval(ctx, vp, html->standby);
		break;
	case JSP_HTML_OBJECT_ELEMENT_TAB_INDEX:
		string_to_jsval(ctx, vp, html->tab_index);
		break;
	case JSP_HTML_OBJECT_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	case JSP_HTML_OBJECT_ELEMENT_USE_MAP:
		string_to_jsval(ctx, vp, html->use_map);
		break;
	case JSP_HTML_OBJECT_ELEMENT_VSPACE:
		string_to_jsval(ctx, vp, html->vspace);
		break;
	case JSP_HTML_OBJECT_ELEMENT_WIDTH:
		string_to_jsval(ctx, vp, html->width);
		break;
	case JSP_HTML_OBJECT_ELEMENT_CONTENT_DOCUMENT:
		string_to_jsval(ctx, vp, html->content_document);
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLObjectElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct OBJECT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLObjectElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OBJECT_ELEMENT_CODE:
		mem_free_set(&html->code, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_ARCHIVE:
		mem_free_set(&html->archive, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_BORDER:
		mem_free_set(&html->border, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE_BASE:
		mem_free_set(&html->code_base, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE_TYPE:
		mem_free_set(&html->code_type, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_DATA:
		mem_free_set(&html->data, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_DECLARE:
		mem_free_set(&html->declare, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_HEIGHT:
		mem_free_set(&html->height, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_HSPACE:
		mem_free_set(&html->hspace, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_STANDBY:
		mem_free_set(&html->standby, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_TAB_INDEX:
		mem_free_set(&html->tab_index, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_USE_MAP:
		mem_free_set(&html->use_map, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_VSPACE:
		mem_free_set(&html->vspace, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OBJECT_ELEMENT_WIDTH:
		mem_free_set(&html->width, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLObjectElement_props[] = {
	{ "form",		JSP_HTML_OBJECT_ELEMENT_FORM,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "code",		JSP_HTML_OBJECT_ELEMENT_CODE,			JSPROP_ENUMERATE },
	{ "align",		JSP_HTML_OBJECT_ELEMENT_ALIGN,			JSPROP_ENUMERATE },
	{ "archive",		JSP_HTML_OBJECT_ELEMENT_ARCHIVE,		JSPROP_ENUMERATE },
	{ "border",		JSP_HTML_OBJECT_ELEMENT_BORDER,			JSPROP_ENUMERATE },
	{ "codeBase",		JSP_HTML_OBJECT_ELEMENT_CODE_BASE,		JSPROP_ENUMERATE },
	{ "codeType",		JSP_HTML_OBJECT_ELEMENT_CODE_TYPE,		JSPROP_ENUMERATE },
	{ "data",		JSP_HTML_OBJECT_ELEMENT_DATA,			JSPROP_ENUMERATE },
	{ "declare",		JSP_HTML_OBJECT_ELEMENT_DECLARE,		JSPROP_ENUMERATE },
	{ "height",		JSP_HTML_OBJECT_ELEMENT_HEIGHT,			JSPROP_ENUMERATE },
	{ "hspace",		JSP_HTML_OBJECT_ELEMENT_HSPACE,			JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_OBJECT_ELEMENT_NAME,			JSPROP_ENUMERATE },
	{ "standby",		JSP_HTML_OBJECT_ELEMENT_STANDBY,		JSPROP_ENUMERATE },
	{ "tabIndex",		JSP_HTML_OBJECT_ELEMENT_TAB_INDEX,		JSPROP_ENUMERATE },
	{ "useMap",		JSP_HTML_OBJECT_ELEMENT_USE_MAP,		JSPROP_ENUMERATE },
	{ "vspace",		JSP_HTML_OBJECT_ELEMENT_VSPACE,			JSPROP_ENUMERATE },
	{ "width",		JSP_HTML_OBJECT_ELEMENT_WIDTH,			JSPROP_ENUMERATE },
	{ "contentDocument",	JSP_HTML_OBJECT_ELEMENT_CONTENT_DOCUMENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLObjectElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLObjectElement_class = {
	"HTMLObjectElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLObjectElement_getProperty, HTMLObjectElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};