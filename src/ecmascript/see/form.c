/* The SEE form object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include <see/see.h>

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
#include "ecmascript/see/checktype.h"
#include "ecmascript/see/document.h"
#include "ecmascript/see/form.h"
#include "ecmascript/see/input.h"
#include "ecmascript/see/strings.h"
#include "ecmascript/see/window.h"
#include "intl/gettext/libintl.h"
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

static void input_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static void input_put(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *, int);
static void js_input_blur(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_input_click(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_input_focus(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_input_select(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static int input_canput(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static int input_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static struct js_input *js_get_input_object(struct SEE_interpreter *, struct js_form *, int);
static struct js_input *js_get_form_control_object(struct SEE_interpreter *, struct js_form *, enum form_type, int);

static void js_form_elems_item(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_form_elems_namedItem(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void form_elems_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static int form_elems_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);

static void js_forms_item(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_forms_namedItem(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void forms_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static int forms_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);

static void form_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static void form_put(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *, int);
static int form_canput(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static int form_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static void js_form_reset(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_form_submit(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);


struct SEE_objectclass js_input_object_class = {
	"input",
	input_get,
	input_put,
	input_canput,
	input_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	NULL,
	NULL,
	NULL,
	NULL
};

struct SEE_objectclass js_form_elems_class = {
	"elements",
	form_elems_get,
	SEE_no_put,
	SEE_no_canput,
	form_elems_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	NULL,
	NULL,
	NULL,
	NULL
};

struct SEE_objectclass js_forms_object_class = {
	"forms",
	forms_get,
	SEE_no_put,
	SEE_no_canput,
	forms_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	NULL,
	NULL,
	NULL,
	NULL
};

struct SEE_objectclass js_form_class = {
	"form",
	form_get,
	form_put,
	form_canput,
	form_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	NULL,
	NULL,
	NULL,
	NULL
};

struct js_input {
	struct SEE_object object;
	struct js_form *parent;
	struct SEE_object *blur;
	struct SEE_object *click;
	struct SEE_object *focus;
	struct SEE_object *select;
	int form_number;
};

struct js_forms_object {
	struct SEE_object object;
	struct js_document_object *parent;
	struct SEE_object *item;
	struct SEE_object *namedItem;
};

struct js_form_elems {
	struct SEE_object object;
	struct js_form *parent;
	struct SEE_object *item;
	struct SEE_object *namedItem;
};


static inline struct form_state *
form_state_of_js_input(struct view_state *vs, const struct js_input *input)
{
	assert(input->form_number >= 0);
	assert(input->form_number < vs->form_info_len);
	if_assert_failed return NULL;
	return &vs->form_info[input->form_number];
}

static void
input_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_input *input = (struct js_input *)o;
	struct js_form *parent = input->parent;
	struct form_state *fs = form_state_of_js_input(vs, input);
	struct form_control *fc = find_form_control(document, fs);
	int linknum;
	struct link *link = NULL;
	struct SEE_string *str;

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];
	SEE_SET_UNDEFINED(res);

	if (p == s_accessKey) {
		struct SEE_string *keystr;
		if (!link)
			return;

		keystr = SEE_string_new(interp, 0);
		if (link->accesskey)
			append_unicode_to_SEE_string(interp, keystr,
						     link->accesskey);
		SEE_SET_STRING(res, keystr);
	} else if (p == s_alt) {
		str = string_to_SEE_string(interp, fc->alt);
		SEE_SET_STRING(res, str);
	} else if (p == s_checked) {
		SEE_SET_BOOLEAN(res, fs->state);
	} else if (p == s_defaultChecked) {
		SEE_SET_BOOLEAN(res, fc->default_state);
	} else if (p == s_defaultValue) {
		/* FIXME (bug 805): convert from the charset of the document */
		str = string_to_SEE_string(interp, fc->default_value);
		SEE_SET_STRING(res, str);
	} else if (p == s_disabled) {
		/* FIXME: <input readonly disabled> --pasky */
		SEE_SET_BOOLEAN(res, fc->mode == FORM_MODE_DISABLED);
	} else if (p == s_form) {
		SEE_SET_OBJECT(res, (struct SEE_object *)parent);
	} else if (p == s_maxLength) {
		SEE_SET_NUMBER(res, fc->maxlength);
	} else if (p == s_name) {
		str = string_to_SEE_string(interp, fc->name);
		SEE_SET_STRING(res, str);
	} else if (p == s_readonly) {
		/* FIXME: <input readonly disabled> --pasky */
		SEE_SET_BOOLEAN(res, fc->mode == FORM_MODE_READONLY);
	} else if (p == s_size) {
		SEE_SET_NUMBER(res, fc->size);
	} else if (p == s_src) {
		if (link && link->where_img) {
			str = string_to_SEE_string(interp, link->where_img);
			SEE_SET_STRING(res, str);
		}
	} else if (p == s_tabindex) {
		if (link) {
			/* FIXME: This is WRONG. --pasky */
			SEE_SET_NUMBER(res, link->number);
		}
	} else if (p == s_type) {
		switch (fc->type) {
		case FC_TEXT: str = s_text; break;
		case FC_PASSWORD: str = s_password; break;
		case FC_FILE: str = s_file; break;
		case FC_CHECKBOX: str = s_checkbox; break;
		case FC_RADIO: str = s_radio; break;
		case FC_SUBMIT: str = s_submit; break;
		case FC_IMAGE: str = s_image; break;
		case FC_RESET: str = s_reset; break;
		case FC_BUTTON: str = s_button; break;
		case FC_HIDDEN: str = s_hidden; break;
		case FC_SELECT: str = s_select; break;
		default: str = NULL;
		}
		if (str) {
			SEE_SET_STRING(res, str);
		}
	} else if (p == s_value) {
		str = string_to_SEE_string(interp, fs->value);
		SEE_SET_STRING(res, str);
	} else if (p == s_selectedIndex) {
		if (fc->type == FC_SELECT) SEE_SET_NUMBER(res, fs->state);
	} else if (p == s_blur) {
		SEE_SET_OBJECT(res, input->blur);
	}  else if (p == s_click) {
		SEE_SET_OBJECT(res, input->click);
	} else if (p == s_focus) {
		SEE_SET_OBJECT(res, input->focus);
	} else if (p == s_select) {
		SEE_SET_OBJECT(res, input->select);
	}
}

static void
input_put(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *val, int attr)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_input *input = (struct js_input *)o;
	struct form_state *fs = form_state_of_js_input(vs, input);
	struct form_control *fc = find_form_control(document, fs);
	int linknum;
	struct link *link = NULL;
	unsigned char *string = NULL;

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (p == s_accessKey) {
		struct SEE_value conv;
		unicode_val_T accesskey;

		SEE_ToString(interp, val, &conv);
		if (conv.u.string->length)
			accesskey = see_string_to_unicode(interp, conv.u.string);
		else
			accesskey = 0;

		if (link)
			link->accesskey = accesskey;
	} else if (p == s_alt) {
		string = see_value_to_unsigned_char(interp, val);
		mem_free_set(&fc->alt, string);
	} else if (p == s_checked) {
		if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO)
			return;
		fs->state = SEE_ToUint32(interp, val);
	} else if (p == s_disabled) {
		/* FIXME: <input readonly disabled> --pasky */
		SEE_uint32_t boo = SEE_ToUint32(interp, val);
		fc->mode = (boo ? FORM_MODE_DISABLED
		: (fc->mode == FORM_MODE_READONLY ? FORM_MODE_READONLY
		: FORM_MODE_NORMAL));
	} else if (p == s_maxLength) {
		string = see_value_to_unsigned_char(interp, val);
		if (!string)
			return;
		fc->maxlength = atol(string);
		mem_free(string);
	} else if (p == s_name) {
		string = see_value_to_unsigned_char(interp, val);
		mem_free_set(&fc->name, string);
	} else if (p == s_readonly) {
		SEE_uint32_t boo = SEE_ToUint32(interp, val);
		fc->mode = (boo ? FORM_MODE_READONLY
		: fc->mode == FORM_MODE_DISABLED ? FORM_MODE_DISABLED
	        : FORM_MODE_NORMAL);
	} else if (p == s_src) {
		if (link) {
			string = see_value_to_unsigned_char(interp, val);
			mem_free_set(&link->where_img, string);
		}
	} else if (p == s_value) {
		if (fc->type == FC_FILE)
			return;
		string = see_value_to_unsigned_char(interp, val);
		mem_free_set(&fs->value, string);
		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD)
			fs->state = strlen(fs->value);
	} else if (p == s_selectedIndex) {
		if (fc->type == FC_SELECT) {
			SEE_uint32_t item = SEE_ToUint32(interp, val);

			if (item >=0 && item < fc->nvalues) {
				fs->state = item;
				mem_free_set(&fs->value, stracpy(fc->values[item]));
			}
		}
	}
}

static void
js_input_blur(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	see_check_class(interp, thisobj, &js_input_object_class);
	SEE_SET_BOOLEAN(res, 0);
	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
}

static void
js_input_click(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses = doc_view->session;
	struct js_input *input = (
		see_check_class(interp, thisobj, &js_input_object_class),
		(struct js_input *)thisobj);
	struct form_state *fs = form_state_of_js_input(vs, input);
	struct form_control *fc;
	int linknum;

	SEE_SET_BOOLEAN(res, 0);
	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		return;

	/* Restore old current_link afterwards? */
	jump_to_link_number(ses, doc_view, linknum);
	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH)
		refresh_view(ses, doc_view, 0);
	else
		print_screen_status(ses);
}

static void
js_input_focus(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses = doc_view->session;
	struct js_input *input = (
		see_check_class(interp, thisobj, &js_input_object_class),
		(struct js_input *)thisobj);
	struct form_state *fs = form_state_of_js_input(vs, input);
	struct form_control *fc;
	int linknum;

	SEE_SET_BOOLEAN(res, 0);
	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
	 	return;

	jump_to_link_number(ses, doc_view, linknum);

}

static void
js_input_select(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	see_check_class(interp, thisobj, &js_input_object_class);
	SEE_SET_BOOLEAN(res, 0);
	/* We support no text selecting yet.  So we do nothing for now.
	 * (That was easy, too.) */
}

static int
input_canput(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	return 1;
}

static int
input_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	/* all unknown properties return UNDEFINED value */
	return 1;
}

static struct js_input *
js_get_input_object(struct SEE_interpreter *interp, struct js_form *jsform, int num)
{
	struct js_input *jsinput;


#if 0
	if (fs->ecmascript_obj)
		return fs->ecmascript_obj;
#endif
	/* jsform ('form') is input's parent */
	/* FIXME: That is NOT correct since the real containing element
	 * should be its parent, but gimme DOM first. --pasky */
	jsinput = SEE_NEW(interp, struct js_input);

	jsinput->object.objectclass = &js_input_object_class;
	jsinput->object.Prototype = NULL;

	jsinput->blur = SEE_cfunction_make(interp, js_input_blur, s_blur, 0);
	jsinput->click = SEE_cfunction_make(interp, js_input_click, s_click, 0);
	jsinput->focus = SEE_cfunction_make(interp, js_input_focus, s_focus, 0);
	jsinput->select = SEE_cfunction_make(interp, js_input_select, s_select, 0);

	jsinput->form_number = num;
	jsinput->parent = jsform;
	return jsinput;
}

static struct js_input *
js_get_form_control_object(struct SEE_interpreter *interp, struct js_form *jsform,
	enum form_type type, int num)
{
	switch (type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
		case FC_CHECKBOX:
		case FC_RADIO:
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_BUTTON:
		case FC_HIDDEN:
		case FC_SELECT:
			return js_get_input_object(interp, jsform, num);

		case FC_TEXTAREA:
			/* TODO */
			return NULL;

		default:
			INTERNAL("Weird fc->type %d", type);
			return NULL;
	}
}




static void
js_form_elems_item(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_form_elems *jsfe = (
		see_check_class(interp, thisobj, &js_form_elems_class),
		(struct js_form_elems *)thisobj);
	struct js_form *parent_form = jsfe->parent;
	struct form_view *fv = parent_form->fv;
	struct form *form = find_form_by_form_view(document, fv);
	struct form_control *fc;
	unsigned char *string;
	int counter = -1;
	int index;

	SEE_SET_UNDEFINED(res);
	if (argc < 1)
		return;
	string = see_value_to_unsigned_char(interp, argv[0]);
	if (!string)
		return;
	index = atol(string);
	mem_free(string);

	foreach (fc, form->items) {
		counter++;
		if (counter == index) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				struct js_input *fcobj = js_get_form_control_object(interp, parent_form, fc->type, fc->g_ctrl_num);

				if (fcobj)
					SEE_SET_OBJECT(res, (struct SEE_object *)fcobj);
			}
			break;
		}
	}

}

static void
js_form_elems_namedItem(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_form_elems *jsfe = (
		see_check_class(interp, thisobj, &js_form_elems_class),
		(struct js_form_elems *)thisobj);
	struct js_form *parent_form = jsfe->parent;
	struct form_view *fv = parent_form->fv;
	struct form *form = find_form_by_form_view(document, fv);
	struct form_control *fc;
	unsigned char *string;

	SEE_SET_UNDEFINED(res);
	if (argc < 1)
		return;
	string = see_value_to_unsigned_char(interp, argv[0]);
	if (!string)
		return;

	foreach (fc, form->items) {
		if ((fc->id && !strcasecmp(string, fc->id)) || (fc->name && !strcasecmp(string, fc->name))) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				struct js_input *fcobj = js_get_form_control_object(interp, parent_form, fc->type, fc->g_ctrl_num);

				if (fcobj)
					SEE_SET_OBJECT(res, (struct SEE_object *)fcobj);
			}
			break;
		}
	}
	mem_free(string);
}

static void
form_elems_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_form_elems *jsfe = (struct js_form_elems *)o;
	struct js_form *parent_form = jsfe->parent;
	struct form_view *fv = parent_form->fv;
	struct form *form = find_form_by_form_view(document, fv);

	if (p == s_length) {
		SEE_number_t length = list_size(&form->items);
		SEE_SET_NUMBER(res, length);
	} else if (p == s_item) {
		SEE_SET_OBJECT(res, jsfe->item);
	} else if (p == s_namedItem) {
		SEE_SET_OBJECT(res, jsfe->namedItem);
	} else {
		unsigned char *string = see_string_to_unsigned_char(p);
		struct SEE_value arg0;
		struct SEE_value *argv[1] = { &arg0 };

		if (!string) {
			SEE_SET_UNDEFINED(res);
			return;
		}
		SEE_SET_STRING(&arg0, p);
		if (string[0] >= '0' && string[0] <= '9') {
			js_form_elems_item(interp, jsfe->item, o, 1,
					   argv, res);
		} else {
			js_form_elems_namedItem(interp, jsfe->namedItem, o, 1,
						argv, res);
		}
		mem_free(string);
	}
}

static int
form_elems_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	/* all unknown properties return UNDEFINED value */
	return 1;
}


static void
js_forms_item(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct js_forms_object *fo = (
		see_check_class(interp, thisobj, &js_forms_object_class),
		(struct js_forms_object *)thisobj);
	struct js_document_object *doc = fo->parent;
	struct form_view *fv;
	unsigned char *string;
	int counter = -1;
	int index;

	SEE_SET_UNDEFINED(res);
	if (argc < 1)
		return;

	string = see_value_to_unsigned_char(interp, argv[0]);
	if (!string)
		return;
	index = atol(string);
	mem_free(string);

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			struct js_form *obj = js_get_form_object(interp, doc, fv);

			SEE_SET_OBJECT(res, (struct SEE_object *)obj);
			break;
		}
	}
}

static void
js_forms_namedItem(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_forms_object *fo = (
		see_check_class(interp, thisobj, &js_forms_object_class),
		(struct js_forms_object *)thisobj);
	struct js_document_object *doc = fo->parent;
	struct form *form;
	unsigned char *string;

	SEE_SET_UNDEFINED(res);
	if (argc < 1)
		return;

	string = see_value_to_unsigned_char(interp, argv[0]);
	if (!string)
		return;
	foreach (form, document->forms) {
		if (form->name && !strcasecmp(string, form->name)) {
			struct form_view *fv = find_form_view(doc_view, form);
			struct js_form *obj = js_get_form_object(interp,
			 doc, fv);

			SEE_SET_OBJECT(res, (struct SEE_object *)obj);
			break;

		}
	}
	mem_free(string);
}

static void
forms_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_forms_object *fo = (struct js_forms_object *)o;

	if (p == s_length) {
		SEE_number_t length = list_size(&document->forms);
		SEE_SET_NUMBER(res, length);
	} else if (p == s_item) {
		SEE_SET_OBJECT(res, fo->item);
	} else if (p == s_namedItem) {
		SEE_SET_OBJECT(res, fo->namedItem);
	} else {
		unsigned char *string = see_string_to_unsigned_char(p);
		struct SEE_value arg0;
		struct SEE_value *argv[1] = { &arg0 };

		if (!string) {
			SEE_SET_UNDEFINED(res);
			return;
		}
		SEE_SET_STRING(&arg0, p);
		if (string[0] >= '0' && string[0] <= '9') {
			js_forms_item(interp, fo->item, o, 1, argv, res);
		} else {
			js_forms_namedItem(interp, fo->namedItem, o, 1, argv, res);
		}
		mem_free(string);
	}
}

static int
forms_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	/* all unknown properties return UNDEFINED value */
	return 1;
}



static void
form_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct js_form *js_form = (struct js_form *)o;
	struct form_view *fv = js_form->fv;
	struct form *form = find_form_by_form_view(doc_view->document, fv);
	struct SEE_string *str;

	SEE_SET_UNDEFINED(res);

	if (p == s_action) {
		str = string_to_SEE_string(interp, form->action);
		SEE_SET_STRING(res, str);
	} else if (p == s_encoding) {
		switch (form->method) {
		case FORM_METHOD_GET:
		case FORM_METHOD_POST:
			/* "application/x-www-form-urlencoded" */
			SEE_SET_STRING(res, s_application_);
			break;
		case FORM_METHOD_POST_MP:
			/* "multipart/form-data" */
			SEE_SET_STRING(res, s_multipart_);
			break;
		case FORM_METHOD_POST_TEXT_PLAIN:
			/* "text/plain") */
			SEE_SET_STRING(res, s_textplain);
			break;
		}
	} else if (p == s_length) {
		SEE_number_t num = list_size(&form->items);
		SEE_SET_NUMBER(res, num);
	} else if (p == s_method) {
		switch (form->method) {
		case FORM_METHOD_GET:
			SEE_SET_STRING(res, s_GET);
			break;

		case FORM_METHOD_POST:
		case FORM_METHOD_POST_MP:
		case FORM_METHOD_POST_TEXT_PLAIN:
			SEE_SET_STRING(res, s_POST);
			break;
		}
	} else if (p == s_name) {
		str = string_to_SEE_string(interp, form->name);
		SEE_SET_STRING(res, str);
	} else if (p == s_target) {
		str = string_to_SEE_string(interp, form->target);
		SEE_SET_STRING(res, str);
	} else if (p == s_elements) {
		struct js_form_elems *jsfe = SEE_NEW(interp, struct js_form_elems);

		jsfe->object.objectclass = &js_form_elems_class;
		jsfe->object.Prototype = NULL;
		jsfe->parent = js_form;
		jsfe->item = SEE_cfunction_make(interp, js_form_elems_item, s_item, 1);
		jsfe->namedItem = SEE_cfunction_make(interp, js_form_elems_namedItem, s_namedItem, 1);
		SEE_SET_OBJECT(res, (struct SEE_object *)jsfe);
	} else if (p == s_submit) {
		SEE_SET_OBJECT(res, js_form->submit);
	} else if (p == s_reset) {
		SEE_SET_OBJECT(res, js_form->reset);
	} else {
		unsigned char *string = see_string_to_unsigned_char(p);
		struct form_control *fc;

		if (!string)
			return;

		foreach(fc, form->items) {
			struct js_input *fcobj = NULL;
			struct form_state *fs;

			if ((!fc->id || strcasecmp(string, fc->id)) && (!fc->name || strcasecmp(string, fc->name)))
				continue;
			fs = find_form_state(doc_view, fc);
			if (fs) {
				fcobj = js_get_form_control_object(interp, js_form, fc->type, fc->g_ctrl_num);

				if (fcobj)
					SEE_SET_OBJECT(res, (struct SEE_object *)fcobj);
			}
			break;
		}
		mem_free(string);
	}
}

static void
form_put(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *val, int attr)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct js_form *js_form = (struct js_form *)o;
	struct form_view *fv = js_form->fv;
	struct form *form = find_form_by_form_view(doc_view->document, fv);
	unsigned char *string = see_value_to_unsigned_char(interp, val);

	if (!string)
		return;

	if (p == s_action) {
		if (form->action) {
			ecmascript_set_action(&form->action, string);
		} else {
			mem_free_set(&form->action, string);
		}
	} else if (p == s_encoding) {
		if (!strcasecmp(string, "application/x-www-form-urlencoded")) {
			form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
			                                               : FORM_METHOD_POST;
		} else if (!strcasecmp(string, "multipart/form-data")) {
			form->method = FORM_METHOD_POST_MP;
		} else if (!strcasecmp(string, "text/plain")) {
			form->method = FORM_METHOD_POST_TEXT_PLAIN;
		}
		mem_free(string);
	} else if (p == s_method) {
		if (!strcasecmp(string, "GET")) {
			form->method = FORM_METHOD_GET;
		} else if (!strcasecmp(string, "POST")) {
			form->method = FORM_METHOD_POST;
		}
		mem_free(string);
	} else if (p == s_name) {
		mem_free_set(&form->name, string);
	} else if (p == s_target) {
		mem_free_set(&form->target, string);
	}
}

static int
form_canput(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	return 1;
}

static int
form_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	return 1;
}

static void
js_form_reset(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct js_form *js_form = (
		see_check_class(interp, thisobj, &js_form_class),
		(struct js_form *)thisobj);
	struct form_view *fv = js_form->fv;
	struct form *form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	do_reset_form(doc_view, form);
	draw_forms(doc_view->session->tab->term, doc_view);
	SEE_SET_BOOLEAN(res, 0);
}

static void
js_form_submit(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;
	struct js_form *js_form = (
		see_check_class(interp, thisobj, &js_form_class),
		(struct js_form *)thisobj);
	struct form_view *fv = js_form->fv;
	struct form *form = find_form_by_form_view(doc_view->document, fv);
	struct delayed_submit_form *dsf;

	assert(form);
	dsf = mem_calloc(1, sizeof(*dsf));
	if (dsf) {
		dsf->ses = ses;
		dsf->vs = vs;
		dsf->form = form;
		register_bottom_half(delayed_submit_given_form, dsf);
	}
	SEE_SET_BOOLEAN(res, 0);
}

struct js_form *js_get_form_object(struct SEE_interpreter *interp,
	struct js_document_object *doc, struct form_view *fv)
{
	struct js_form *js_form;

#if 0
	if (fv->ecmascript_obj)
		return fv->ecmascript_obj;
#endif
	/* jsdoc ('document') is fv's parent */
	/* FIXME: That is NOT correct since the real containing element
	 * should be its parent, but gimme DOM first. --pasky */
	js_form = SEE_NEW(interp, struct js_form);
	js_form->object.objectclass = &js_form_class;
	js_form->object.Prototype = NULL; /* TODO: use prototype for form */
	js_form->parent = doc;
	js_form->reset = SEE_cfunction_make(interp, js_form_reset, s_reset, 0);
	js_form->submit = SEE_cfunction_make(interp, js_form_submit, s_submit, 0);
	js_form->fv = fv;

	fv->ecmascript_obj = js_form;
	return js_form;
}

void
init_js_forms_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v, document;
	struct js_forms_object *forms = SEE_NEW(interp,
	 struct js_forms_object);

	forms->object.objectclass = &js_forms_object_class;
	forms->object.Prototype = NULL;

	SEE_OBJECT_GET(interp, interp->Global, s_document, &document);

	forms->item = SEE_cfunction_make(interp, js_forms_item, s_item, 1);
	forms->namedItem = SEE_cfunction_make(interp, js_forms_namedItem,
	 s_namedItem, 1);
	forms->parent = (struct js_document_object *)document.u.object;
	SEE_SET_OBJECT(&v, (struct SEE_object *)forms);
	SEE_OBJECT_PUT(interp, interp->Global, s_forms, &v, 0);
}