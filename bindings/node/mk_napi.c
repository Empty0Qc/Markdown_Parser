/* mk_napi.c — Node-API (N-API) binding for mk_parser (M8b)
 *
 * Exposes the full Push/Pull API to Node.js via the stable Node-API ABI.
 * Builds as a native addon: mk_parser_native.node
 *
 * Build:
 *   npm install --build-from-source  (uses binding.gyp)
 *   or via CMake: cmake -B build/node && cmake --build build/node
 *
 * JS usage:
 *   const { MkParser, NodeType } = require('./mk_parser_native');
 *   const p = new MkParser();
 *   p.onNodeOpen  = (type, nodeData) => { ... };
 *   p.onNodeClose = (type, nodeData) => { ... };
 *   p.onText      = (text)          => { ... };
 *   p.onModify    = (type, nodeData) => { ... };
 *   p.feed('# Hello\n');
 *   p.finish();
 *   p.destroy();
 */

#include <node_api.h>
#include "../../include/mk_parser.h"
#include <stdlib.h>
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────────────── */

#define NAPI_CALL(env, call)                                          \
  do {                                                                \
    napi_status _st = (call);                                         \
    if (_st != napi_ok) {                                             \
      const napi_extended_error_info *info = NULL;                    \
      napi_get_last_error_info((env), &info);                         \
      napi_throw_error((env), NULL,                                   \
          info && info->error_message ? info->error_message           \
                                      : "N-API call failed");         \
      return NULL;                                                     \
    }                                                                  \
  } while (0)

/* Build a plain JS object with node attributes for the callback */
static napi_value build_node_info(napi_env env, MkNode *n) {
    napi_value obj;
    napi_create_object(env, &obj);

    /* type */
    napi_value vtype;
    napi_create_int32(env, (int32_t)n->type, &vtype);
    napi_set_named_property(env, obj, "type", vtype);

    /* flags */
    napi_value vflags;
    napi_create_uint32(env, n->flags, &vflags);
    napi_set_named_property(env, obj, "flags", vflags);

    /* type-specific fields */
    switch (n->type) {
    case MK_NODE_HEADING: {
        napi_value v; napi_create_int32(env, mk_node_heading_level(n), &v);
        napi_set_named_property(env, obj, "level", v);
        break;
    }
    case MK_NODE_CODE_BLOCK: {
        const char *lang = mk_node_code_lang(n);
        napi_value vl, vf;
        if (lang) napi_create_string_utf8(env, lang, mk_node_code_lang_len(n), &vl);
        else      napi_get_null(env, &vl);
        napi_set_named_property(env, obj, "lang", vl);
        napi_create_int32(env, mk_node_code_fenced(n), &vf);
        napi_set_named_property(env, obj, "fenced", vf);
        break;
    }
    case MK_NODE_LIST: {
        napi_value vo, vs;
        napi_create_int32(env, mk_node_list_ordered(n), &vo);
        napi_create_int32(env, mk_node_list_start(n),   &vs);
        napi_set_named_property(env, obj, "ordered", vo);
        napi_set_named_property(env, obj, "start",   vs);
        break;
    }
    case MK_NODE_TABLE: {
        napi_value vc; napi_create_uint32(env, (uint32_t)mk_node_table_col_count(n), &vc);
        napi_set_named_property(env, obj, "colCount", vc);
        break;
    }
    case MK_NODE_TABLE_CELL: {
        napi_value va, vi;
        napi_create_int32(env, (int32_t)mk_node_table_cell_align(n),     &va);
        napi_create_uint32(env, (uint32_t)mk_node_table_cell_col_index(n), &vi);
        napi_set_named_property(env, obj, "align",    va);
        napi_set_named_property(env, obj, "colIndex", vi);
        break;
    }
    case MK_NODE_LINK: {
        napi_value vh, vt;
        napi_create_string_utf8(env, mk_node_link_href(n)  ? mk_node_link_href(n)  : "", mk_node_link_href_len(n),  &vh);
        if (mk_node_link_title(n))
            napi_create_string_utf8(env, mk_node_link_title(n), mk_node_link_title_len(n), &vt);
        else
            napi_get_null(env, &vt);
        napi_set_named_property(env, obj, "href",  vh);
        napi_set_named_property(env, obj, "title", vt);
        break;
    }
    case MK_NODE_IMAGE: {
        napi_value vs, va, vt;
        napi_create_string_utf8(env, mk_node_image_src(n)   ? mk_node_image_src(n)   : "", mk_node_image_src_len(n),   &vs);
        napi_create_string_utf8(env, mk_node_image_alt(n)   ? mk_node_image_alt(n)   : "", mk_node_image_alt_len(n),   &va);
        if (mk_node_image_title(n))
            napi_create_string_utf8(env, mk_node_image_title(n), mk_node_image_title_len(n), &vt);
        else
            napi_get_null(env, &vt);
        napi_set_named_property(env, obj, "src",   vs);
        napi_set_named_property(env, obj, "alt",   va);
        napi_set_named_property(env, obj, "title", vt);
        break;
    }
    case MK_NODE_AUTO_LINK: {
        napi_value vu, ve;
        napi_create_string_utf8(env, mk_node_autolink_url(n) ? mk_node_autolink_url(n) : "", mk_node_autolink_url_len(n), &vu);
        napi_create_int32(env, mk_node_autolink_is_email(n), &ve);
        napi_set_named_property(env, obj, "url",     vu);
        napi_set_named_property(env, obj, "isEmail", ve);
        break;
    }
    case MK_NODE_TEXT: {
        napi_value vt;
        napi_create_string_utf8(env, mk_node_text_content(n) ? mk_node_text_content(n) : "", mk_node_text_content_len(n), &vt);
        napi_set_named_property(env, obj, "text", vt);
        break;
    }
    case MK_NODE_INLINE_CODE: {
        napi_value vt;
        napi_create_string_utf8(env, mk_node_inline_code_text(n) ? mk_node_inline_code_text(n) : "", mk_node_inline_code_text_len(n), &vt);
        napi_set_named_property(env, obj, "text", vt);
        break;
    }
    case MK_NODE_TASK_LIST_ITEM: {
        napi_value vc; napi_create_int32(env, mk_node_task_list_item_checked(n), &vc);
        napi_set_named_property(env, obj, "checked", vc);
        break;
    }
    default: break;
    }

    return obj;
}

/* ── MkParser object state ────────────────────────────────────────────────── */

typedef struct {
    MkArena   *arena;
    MkParser  *parser;
    napi_env   env;
    napi_ref   this_ref;   /* strong ref to the JS object (prevents GC) */

    /* JS callback function refs */
    napi_ref cb_open;
    napi_ref cb_close;
    napi_ref cb_text;
    napi_ref cb_modify;
} NapiParserState;

/* ── Push callback bridges ────────────────────────────────────────────────── */

static void call_js_cb(NapiParserState *s, napi_ref cb_ref,
                       napi_value *args, size_t argc)
{
    if (!cb_ref) return;
    napi_value cb, undef, this_val;
    napi_get_reference_value(s->env, cb_ref, &cb);
    if (!cb) return;
    napi_get_undefined(s->env, &undef);
    napi_get_reference_value(s->env, s->this_ref, &this_val);
    napi_call_function(s->env, this_val, cb, argc, args, NULL);
}

static void napi_on_open(void *ud, MkNode *n) {
    NapiParserState *s = (NapiParserState *)ud;
    napi_value args[2];
    napi_create_int32(s->env, (int32_t)n->type, &args[0]);
    args[1] = build_node_info(s->env, n);
    call_js_cb(s, s->cb_open, args, 2);
}

static void napi_on_close(void *ud, MkNode *n) {
    NapiParserState *s = (NapiParserState *)ud;
    napi_value args[2];
    napi_create_int32(s->env, (int32_t)n->type, &args[0]);
    args[1] = build_node_info(s->env, n);
    call_js_cb(s, s->cb_close, args, 2);
}

static void napi_on_text(void *ud, MkNode *n,
                         const char *text, size_t len) {
    NapiParserState *s = (NapiParserState *)ud;
    (void)n;
    napi_value vtext;
    napi_create_string_utf8(s->env, text, len, &vtext);
    call_js_cb(s, s->cb_text, &vtext, 1);
}

static void napi_on_modify(void *ud, MkNode *n) {
    NapiParserState *s = (NapiParserState *)ud;
    napi_value args[2];
    napi_create_int32(s->env, (int32_t)n->type, &args[0]);
    args[1] = build_node_info(s->env, n);
    call_js_cb(s, s->cb_modify, args, 2);
}

/* ── Finalizer: called when JS object is GC'd ─────────────────────────────── */

static void parser_finalize(napi_env env, void *data, void *hint) {
    (void)env; (void)hint;
    NapiParserState *s = (NapiParserState *)data;
    if (s->parser)  mk_parser_free(s->parser);
    if (s->arena)   mk_arena_free(s->arena);
    if (s->cb_open)   napi_delete_reference(env, s->cb_open);
    if (s->cb_close)  napi_delete_reference(env, s->cb_close);
    if (s->cb_text)   napi_delete_reference(env, s->cb_text);
    if (s->cb_modify) napi_delete_reference(env, s->cb_modify);
    free(s);
}

/* ── MkParser constructor ─────────────────────────────────────────────────── */

static napi_value parser_new(napi_env env, napi_callback_info info) {
    napi_value this_val;
    NAPI_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &this_val, NULL));

    NapiParserState *s = calloc(1, sizeof(NapiParserState));
    if (!s) { napi_throw_error(env, NULL, "OOM"); return NULL; }

    s->env   = env;
    s->arena = mk_arena_new();
    if (!s->arena) { free(s); napi_throw_error(env, NULL, "mk_arena_new failed"); return NULL; }

    MkCallbacks cbs = {
        .user_data      = s,
        .on_node_open   = napi_on_open,
        .on_node_close  = napi_on_close,
        .on_text        = napi_on_text,
        .on_node_modify = napi_on_modify,
    };
    s->parser = mk_parser_new(s->arena, &cbs);
    if (!s->parser) {
        mk_arena_free(s->arena); free(s);
        napi_throw_error(env, NULL, "mk_parser_new failed"); return NULL;
    }

    napi_create_reference(env, this_val, 1, &s->this_ref);
    napi_wrap(env, this_val, s, parser_finalize, NULL, NULL);
    return this_val;
}

/* Helper to unwrap state */
static NapiParserState *unwrap(napi_env env, napi_callback_info info,
                               napi_value *this_out)
{
    napi_value this_val;
    napi_get_cb_info(env, info, NULL, NULL, &this_val, NULL);
    if (this_out) *this_out = this_val;
    NapiParserState *s = NULL;
    napi_unwrap(env, this_val, (void**)&s);
    return s;
}

/* Helper to register a JS callback */
static napi_value set_cb(napi_env env, napi_callback_info info, napi_ref *ref) {
    size_t argc = 1;
    napi_value args[1], this_val;
    napi_get_cb_info(env, info, &argc, args, &this_val, NULL);

    NapiParserState *s = NULL;
    napi_unwrap(env, this_val, (void**)&s);
    if (!s) { napi_throw_error(env, NULL, "not a MkParser"); return NULL; }

    if (*ref) { napi_delete_reference(env, *ref); *ref = NULL; }

    napi_valuetype vt;
    napi_typeof(env, args[0], &vt);
    if (vt == napi_function)
        napi_create_reference(env, args[0], 1, ref);

    return this_val; /* chainable */
}

/* ── MkParser.prototype.feed(text) ───────────────────────────────────────── */

static napi_value parser_feed(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1], this_val;
    napi_get_cb_info(env, info, &argc, args, &this_val, NULL);

    NapiParserState *s = NULL;
    napi_unwrap(env, this_val, (void**)&s);
    if (!s || !s->parser) { napi_throw_error(env, NULL, "destroyed"); return NULL; }

    size_t len;
    napi_get_value_string_utf8(env, args[0], NULL, 0, &len);
    char *buf = malloc(len + 1);
    if (!buf) { napi_throw_error(env, NULL, "OOM"); return NULL; }
    napi_get_value_string_utf8(env, args[0], buf, len + 1, &len);

    int rc = mk_feed(s->parser, buf, len);
    free(buf);

    if (rc != 0) { napi_throw_error(env, NULL, "mk_feed failed"); return NULL; }
    return this_val;
}

/* ── MkParser.prototype.finish() ─────────────────────────────────────────── */

static napi_value parser_finish(napi_env env, napi_callback_info info) {
    NapiParserState *s = unwrap(env, info, NULL);
    if (!s || !s->parser) { napi_throw_error(env, NULL, "destroyed"); return NULL; }
    mk_finish(s->parser);
    napi_value undef; napi_get_undefined(env, &undef);
    return undef;
}

/* ── MkParser.prototype.destroy() ────────────────────────────────────────── */

static napi_value parser_destroy(napi_env env, napi_callback_info info) {
    napi_value this_val;
    NapiParserState *s = unwrap(env, info, &this_val);
    if (s) {
        if (s->parser) { mk_parser_free(s->parser); s->parser = NULL; }
        if (s->arena)  { mk_arena_free(s->arena);   s->arena  = NULL; }
    }
    napi_value undef; napi_get_undefined(env, &undef);
    return undef;
}

/* ── Callback setters ─────────────────────────────────────────────────────── */

static napi_value set_on_open  (napi_env e, napi_callback_info i) {
    NapiParserState *s = unwrap(e, i, NULL);
    return s ? set_cb(e, i, &s->cb_open)   : NULL; }
static napi_value set_on_close (napi_env e, napi_callback_info i) {
    NapiParserState *s = unwrap(e, i, NULL);
    return s ? set_cb(e, i, &s->cb_close)  : NULL; }
static napi_value set_on_text  (napi_env e, napi_callback_info i) {
    NapiParserState *s = unwrap(e, i, NULL);
    return s ? set_cb(e, i, &s->cb_text)   : NULL; }
static napi_value set_on_modify(napi_env e, napi_callback_info i) {
    NapiParserState *s = unwrap(e, i, NULL);
    return s ? set_cb(e, i, &s->cb_modify) : NULL; }

/* ── Module init ──────────────────────────────────────────────────────────── */

#define DEFINE_METHOD(env, proto, name, fn)                        \
  do {                                                              \
    napi_value _fn;                                                 \
    napi_create_function((env), (name), NAPI_AUTO_LENGTH,          \
                         (fn), NULL, &_fn);                        \
    napi_set_named_property((env), (proto), (name), _fn);          \
  } while (0)

#define SET_INT_CONST(env, obj, name, val)                         \
  do {                                                              \
    napi_value _v; napi_create_int32((env), (val), &_v);           \
    napi_set_named_property((env), (obj), (name), _v);             \
  } while (0)

static napi_value init(napi_env env, napi_value exports) {
    /* ── NodeType enum object ── */
    napi_value nt_obj;
    napi_create_object(env, &nt_obj);
    SET_INT_CONST(env, nt_obj, "DOCUMENT",       MK_NODE_DOCUMENT);
    SET_INT_CONST(env, nt_obj, "HEADING",        MK_NODE_HEADING);
    SET_INT_CONST(env, nt_obj, "PARAGRAPH",      MK_NODE_PARAGRAPH);
    SET_INT_CONST(env, nt_obj, "CODE_BLOCK",     MK_NODE_CODE_BLOCK);
    SET_INT_CONST(env, nt_obj, "BLOCK_QUOTE",    MK_NODE_BLOCK_QUOTE);
    SET_INT_CONST(env, nt_obj, "LIST",           MK_NODE_LIST);
    SET_INT_CONST(env, nt_obj, "LIST_ITEM",      MK_NODE_LIST_ITEM);
    SET_INT_CONST(env, nt_obj, "THEMATIC_BREAK", MK_NODE_THEMATIC_BREAK);
    SET_INT_CONST(env, nt_obj, "HTML_BLOCK",     MK_NODE_HTML_BLOCK);
    SET_INT_CONST(env, nt_obj, "TABLE",          MK_NODE_TABLE);
    SET_INT_CONST(env, nt_obj, "TABLE_HEAD",     MK_NODE_TABLE_HEAD);
    SET_INT_CONST(env, nt_obj, "TABLE_ROW",      MK_NODE_TABLE_ROW);
    SET_INT_CONST(env, nt_obj, "TABLE_CELL",     MK_NODE_TABLE_CELL);
    SET_INT_CONST(env, nt_obj, "TEXT",           MK_NODE_TEXT);
    SET_INT_CONST(env, nt_obj, "SOFT_BREAK",     MK_NODE_SOFT_BREAK);
    SET_INT_CONST(env, nt_obj, "HARD_BREAK",     MK_NODE_HARD_BREAK);
    SET_INT_CONST(env, nt_obj, "EMPHASIS",       MK_NODE_EMPHASIS);
    SET_INT_CONST(env, nt_obj, "STRONG",         MK_NODE_STRONG);
    SET_INT_CONST(env, nt_obj, "STRIKETHROUGH",  MK_NODE_STRIKETHROUGH);
    SET_INT_CONST(env, nt_obj, "INLINE_CODE",    MK_NODE_INLINE_CODE);
    SET_INT_CONST(env, nt_obj, "LINK",           MK_NODE_LINK);
    SET_INT_CONST(env, nt_obj, "IMAGE",          MK_NODE_IMAGE);
    SET_INT_CONST(env, nt_obj, "AUTO_LINK",      MK_NODE_AUTO_LINK);
    SET_INT_CONST(env, nt_obj, "HTML_INLINE",    MK_NODE_HTML_INLINE);
    SET_INT_CONST(env, nt_obj, "TASK_LIST_ITEM", MK_NODE_TASK_LIST_ITEM);
    napi_set_named_property(env, exports, "NodeType", nt_obj);

    /* ── MkParser class ── */
    napi_value cons_fn, proto;
    napi_create_function(env, "MkParser", NAPI_AUTO_LENGTH,
                         parser_new, NULL, &cons_fn);
    napi_get_named_property(env, cons_fn, "prototype", &proto);

    DEFINE_METHOD(env, proto, "feed",        parser_feed);
    DEFINE_METHOD(env, proto, "finish",      parser_finish);
    DEFINE_METHOD(env, proto, "destroy",     parser_destroy);
    DEFINE_METHOD(env, proto, "onNodeOpen",  set_on_open);
    DEFINE_METHOD(env, proto, "onNodeClose", set_on_close);
    DEFINE_METHOD(env, proto, "onText",      set_on_text);
    DEFINE_METHOD(env, proto, "onModify",    set_on_modify);

    napi_set_named_property(env, exports, "MkParser", cons_fn);

    return exports;
}

NAPI_MODULE(mk_parser_native, init)
