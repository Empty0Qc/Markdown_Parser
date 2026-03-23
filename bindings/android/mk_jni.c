/* mk_jni.c — Android JNI bridge for mk_parser (M8c)
 *
 * Exposes the Push API to Kotlin/Java via JNI.
 * Each parser instance is created from Kotlin and holds its state in a
 * long (native pointer) stored on the Kotlin side.
 *
 * Kotlin usage:
 *   val parser = MkParser()
 *   parser.onNodeOpen  = { type, node -> … }
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

/* ── Per-parser JNI state ─────────────────────────────────────────────────── */

typedef struct {
    MkArena   *arena;
    MkParser  *parser;
    JavaVM    *jvm;
    jobject    java_obj;   /* global ref to the Kotlin MkParser object */
    jmethodID  cb_open;    /* void onNativeNodeOpen(int type, int flags, ...) */
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
    JniState *s = (JniState *)ud;
    JNIEnv   *env = get_env(s->jvm);
    if (!env || !s->cb_open) return;
    (*env)->CallVoidMethod(env, s->java_obj, s->cb_open,
                          (jint)n->type, (jint)n->flags);
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
    jstring jtext = (*env)->NewStringUTF(env, text);
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
    s->cb_open   = (*env)->GetMethodID(env, cls, "onNativeNodeOpen",  "(II)V");
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
    const char *utf = (*env)->GetStringUTFChars(env, text, NULL);
    jint rc = mk_feed(s->parser, utf, strlen(utf));
    (*env)->ReleaseStringUTFChars(env, text, utf);
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
