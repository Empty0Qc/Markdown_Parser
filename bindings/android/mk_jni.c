/* mk_jni.c — Android JNI bridge for mk_parser (M8c)
 *
 * Exposes the Push API to Kotlin/Java via JNI.
 * Each parser instance is created from Kotlin and holds its state in a
 * long (native pointer) stored on the Kotlin side.
 *
 * Kotlin usage:
 *   val parser = MkParser()
 *   parser.onNodeOpen  = { type, flags, attr -> … }
 *   parser.onText      = { text -> … }
 *   parser.feed("# Hello\n")
 *   parser.finish()
 *   parser.destroy()
 *
 * JNI method naming convention:
 *   Java_com_mkparser_MkParser_<method>
 */

#include <jni.h>
#include "../../include/mk_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── Helper: create a jstring from a UTF-8 byte slice ────────────────────── */

/* Converts UTF-8 bytes → UTF-16 jchar[] then calls NewString().
 * This avoids NewStringUTF() which requires "Modified UTF-8" and aborts the
 * VM when it receives a continuation byte at a sequence start (which can
 * happen when the C parser slices text at byte-level rather than char-level).
 * Invalid bytes are replaced with U+FFFD instead of crashing. */
static jstring jstring_from_slice(JNIEnv *env, const char *text, size_t len) {
    if (!text || len == 0) return NULL;

    /* Upper-bound on UTF-16 code units: each input byte → at most 2 jchars
     * (surrogate pair for supplementary characters). */
    jchar *out = (jchar *)malloc((len + 1) * 2 * sizeof(jchar));
    if (!out) return NULL;

    jsize  out_len = 0;
    size_t i       = 0;

    while (i < len) {
        unsigned char c = (unsigned char)text[i];
        uint32_t cp = 0xFFFD; /* replacement for invalid sequences */

        if (c < 0x80) {                              /* 1-byte ASCII */
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0               /* 2-byte sequence */
                   && i + 1 < len
                   && ((unsigned char)text[i+1] & 0xC0) == 0x80) {
            cp = ((uint32_t)(c & 0x1F) << 6)
               | ((uint32_t)((unsigned char)text[i+1] & 0x3F));
            i += 2;
        } else if ((c & 0xF0) == 0xE0               /* 3-byte sequence */
                   && i + 2 < len
                   && ((unsigned char)text[i+1] & 0xC0) == 0x80
                   && ((unsigned char)text[i+2] & 0xC0) == 0x80) {
            cp = ((uint32_t)(c & 0x0F) << 12)
               | ((uint32_t)((unsigned char)text[i+1] & 0x3F) << 6)
               | ((uint32_t)((unsigned char)text[i+2] & 0x3F));
            i += 3;
        } else if ((c & 0xF8) == 0xF0               /* 4-byte sequence */
                   && i + 3 < len
                   && ((unsigned char)text[i+1] & 0xC0) == 0x80
                   && ((unsigned char)text[i+2] & 0xC0) == 0x80
                   && ((unsigned char)text[i+3] & 0xC0) == 0x80) {
            cp = ((uint32_t)(c & 0x07) << 18)
               | ((uint32_t)((unsigned char)text[i+1] & 0x3F) << 12)
               | ((uint32_t)((unsigned char)text[i+2] & 0x3F) <<  6)
               | ((uint32_t)((unsigned char)text[i+3] & 0x3F));
            i += 4;
        } else {
            /* Invalid / misaligned byte — skip one byte, emit U+FFFD */
            cp = 0xFFFD;
            i += 1;
        }

        if (cp < 0x10000) {
            out[out_len++] = (jchar)cp;
        } else {
            /* Supplementary character → surrogate pair */
            cp -= 0x10000;
            out[out_len++] = (jchar)(0xD800 | (cp >> 10));
            out[out_len++] = (jchar)(0xDC00 | (cp & 0x3FF));
        }
    }

    jstring result = (*env)->NewString(env, out, out_len);
    free(out);
    return result;
}

/* ── Helper: convert Java String to standard UTF-8 byte buffer ───────────── */

/* GetStringUTFChars returns Modified UTF-8 which encodes:
 *   - U+0000  as 0xC0 0x80  (2 bytes instead of 1)
 *   - U+10000+ as two 3-byte surrogate sequences (not standard 4-byte UTF-8)
 * mk_parser expects standard UTF-8.  We go through UTF-16 to avoid both.
 * Returns a malloc'd null-terminated UTF-8 string; caller must free(). */
static char *java_string_to_utf8(JNIEnv *env, jstring str, size_t *out_len) {
    if (!str) { *out_len = 0; return NULL; }
    jsize utf16_len = (*env)->GetStringLength(env, str);
    const jchar *chars = (*env)->GetStringChars(env, str, NULL);
    if (!chars) { *out_len = 0; return NULL; }

    /* First pass: calculate required UTF-8 byte count */
    size_t byte_count = 0;
    for (jsize i = 0; i < utf16_len; ) {
        jchar c = chars[i++];
        if (c < 0x80) {
            byte_count += 1;
        } else if (c < 0x800) {
            byte_count += 2;
        } else if (c >= 0xD800 && c <= 0xDBFF && i < utf16_len) {
            /* High surrogate — consume low surrogate too → 4-byte sequence */
            jchar low = chars[i];
            if (low >= 0xDC00 && low <= 0xDFFF) { i++; byte_count += 4; }
            else byte_count += 3;  /* unpaired high surrogate → replacement */
        } else {
            byte_count += 3;
        }
    }

    char *buf = (char *)malloc(byte_count + 1);
    if (!buf) {
        (*env)->ReleaseStringChars(env, str, chars);
        *out_len = 0;
        return NULL;
    }

    /* Second pass: encode */
    char *p = buf;
    for (jsize i = 0; i < utf16_len; ) {
        jchar c = chars[i++];
        if (c < 0x80) {
            *p++ = (char)c;
        } else if (c < 0x800) {
            *p++ = (char)(0xC0 | (c >> 6));
            *p++ = (char)(0x80 | (c & 0x3F));
        } else if (c >= 0xD800 && c <= 0xDBFF && i < utf16_len) {
            jchar low = chars[i];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                i++;
                uint32_t cp = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
                *p++ = (char)(0xF0 | (cp >> 18));
                *p++ = (char)(0x80 | ((cp >> 12) & 0x3F));
                *p++ = (char)(0x80 | ((cp >>  6) & 0x3F));
                *p++ = (char)(0x80 | ( cp        & 0x3F));
            } else {
                /* Unpaired surrogate → U+FFFD replacement */
                *p++ = (char)0xEF; *p++ = (char)0xBF; *p++ = (char)0xBD;
            }
        } else {
            *p++ = (char)(0xE0 | (c >> 12));
            *p++ = (char)(0x80 | ((c >> 6) & 0x3F));
            *p++ = (char)(0x80 | ( c       & 0x3F));
        }
    }
    *p = '\0';

    (*env)->ReleaseStringChars(env, str, chars);
    *out_len = (size_t)(p - buf);
    return buf;
}

/* ── Per-parser JNI state ─────────────────────────────────────────────────── */

typedef struct {
    MkArena   *arena;
    MkParser  *parser;
    JavaVM    *jvm;
    jobject    java_obj;   /* global ref to the Kotlin MkParser object */
    jmethodID  cb_open;    /* void onNativeNodeOpen(int type, int flags, String? attr) */
    jmethodID  cb_close;   /* void onNativeNodeClose(int type) */
    jmethodID  cb_text;    /* void onNativeText(String text) */
    jmethodID  cb_modify;  /* void onNativeNodeModify(int type) */
} JniState;

/* Get JNIEnv for current thread (handles attach for background threads) */
static JNIEnv *get_env(JavaVM *jvm) {
    JNIEnv *env = NULL;
    if ((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) == JNI_EDETACHED)
        (*jvm)->AttachCurrentThread(jvm, &env, NULL);
    return env;
}

/* ── Push callback bridges ────────────────────────────────────────────────── */

static void jni_on_open(void *ud, MkNode *n) {
    JniState *s   = (JniState *)ud;
    JNIEnv   *env = get_env(s->jvm);
    if (!env || !s->cb_open) return;

    /* Encode type-specific attributes into a single int + optional string. */
    jint    attrs = 0;
    jstring jattr = NULL;

    switch (n->type) {
        case MK_NODE_HEADING:
            attrs = (jint)mk_node_heading_level(n);
            break;
        case MK_NODE_LIST:
            attrs = mk_node_list_ordered(n) ? 1 : 0;
            break;
        case MK_NODE_LIST_ITEM:
            attrs = (jint)mk_node_list_item_task_state(n);
            break;
        case MK_NODE_TASK_LIST_ITEM:
            /* map checked(0/1) → taskState(1=unchecked / 2=checked) */
            attrs = mk_node_task_list_item_checked(n) ? 2 : 1;
            break;
        case MK_NODE_CODE_BLOCK: {
            size_t len = mk_node_code_lang_len(n);
            if (len > 0) {
                const char *lang = mk_node_code_lang(n);
                if (lang) jattr = jstring_from_slice(env, lang, len);
            }
            break;
        }
        case MK_NODE_LINK:
        case MK_NODE_AUTO_LINK: {
            size_t len = mk_node_link_href_len(n);
            if (len > 0) {
                const char *href = mk_node_link_href(n);
                if (href) jattr = jstring_from_slice(env, href, len);
            }
            break;
        }
        case MK_NODE_HTML_INLINE: {
            size_t len = mk_node_html_inline_raw_len(n);
            if (len > 0) {
                const char *raw = mk_node_html_inline_raw(n);
                if (raw) jattr = jstring_from_slice(env, raw, len);
            }
            break;
        }
        default:
            attrs = (jint)n->flags;
            break;
    }

    (*env)->CallVoidMethod(env, s->java_obj, s->cb_open,
                          (jint)n->type, attrs, jattr);
    if (jattr) (*env)->DeleteLocalRef(env, jattr);
}

static void jni_on_close(void *ud, MkNode *n) {
    JniState *s = (JniState *)ud;
    JNIEnv   *env = get_env(s->jvm);
    if (!env || !s->cb_close) return;
    (*env)->CallVoidMethod(env, s->java_obj, s->cb_close, (jint)n->type);
}

static void jni_on_text(void *ud, MkNode *n,
                        const char *text, size_t len) {
    (void)n;
    JniState *s = (JniState *)ud;
    JNIEnv   *env = get_env(s->jvm);
    if (!env || !s->cb_text) return;
    jstring jtext = jstring_from_slice(env, text, len);
    if (!jtext) return;
    (*env)->CallVoidMethod(env, s->java_obj, s->cb_text, jtext);
    (*env)->DeleteLocalRef(env, jtext);
}

static void jni_on_modify(void *ud, MkNode *n) {
    JniState *s = (JniState *)ud;
    JNIEnv   *env = get_env(s->jvm);
    if (!env || !s->cb_modify) return;
    (*env)->CallVoidMethod(env, s->java_obj, s->cb_modify, (jint)n->type);
}

/* ── JNI functions ────────────────────────────────────────────────────────── */

JNIEXPORT jlong JNICALL
Java_com_mkparser_MkParser_nativeCreate(JNIEnv *env, jobject obj) {
    JavaVM *jvm = NULL;
    (*env)->GetJavaVM(env, &jvm);

    JniState *s = calloc(1, sizeof(JniState));
    if (!s) return 0;

    s->jvm       = jvm;
    s->java_obj  = (*env)->NewGlobalRef(env, obj);
    s->arena     = mk_arena_new();
    if (!s->arena) { (*env)->DeleteGlobalRef(env, s->java_obj); free(s); return 0; }

    /* Cache method IDs */
    jclass cls = (*env)->GetObjectClass(env, obj);
    s->cb_open   = (*env)->GetMethodID(env, cls, "onNativeNodeOpen",  "(IILjava/lang/String;)V");
    s->cb_close  = (*env)->GetMethodID(env, cls, "onNativeNodeClose", "(I)V");
    s->cb_text   = (*env)->GetMethodID(env, cls, "onNativeText",      "(Ljava/lang/String;)V");
    s->cb_modify = (*env)->GetMethodID(env, cls, "onNativeNodeModify","(I)V");

    MkCallbacks cbs = {
        .user_data      = s,
        .on_node_open   = jni_on_open,
        .on_node_close  = jni_on_close,
        .on_text        = jni_on_text,
        .on_node_modify = jni_on_modify,
    };
    s->parser = mk_parser_new(s->arena, &cbs);
    if (!s->parser) {
        mk_arena_free(s->arena);
        (*env)->DeleteGlobalRef(env, s->java_obj);
        free(s);
        return 0;
    }
    return (jlong)(intptr_t)s;
}

JNIEXPORT jint JNICALL
Java_com_mkparser_MkParser_nativeFeed(JNIEnv *env, jobject obj,
                                      jlong handle, jstring text)
{
    (void)obj;
    JniState *s = (JniState *)(intptr_t)handle;
    if (!s || !s->parser) return -1;
    size_t len = 0;
    char *utf8 = java_string_to_utf8(env, text, &len);
    if (!utf8) return -1;
    jint rc = mk_feed(s->parser, utf8, len);
    free(utf8);
    return rc;
}

JNIEXPORT jint JNICALL
Java_com_mkparser_MkParser_nativeFinish(JNIEnv *env, jobject obj, jlong handle) {
    (void)env; (void)obj;
    JniState *s = (JniState *)(intptr_t)handle;
    if (!s || !s->parser) return -1;
    return mk_finish(s->parser);
}

JNIEXPORT void JNICALL
Java_com_mkparser_MkParser_nativeDestroy(JNIEnv *env, jobject obj, jlong handle) {
    (void)obj;
    JniState *s = (JniState *)(intptr_t)handle;
    if (!s) return;
    if (s->parser)   mk_parser_free(s->parser);
    if (s->arena)    mk_arena_free(s->arena);
    if (s->java_obj) (*env)->DeleteGlobalRef(env, s->java_obj);
    free(s);
}
