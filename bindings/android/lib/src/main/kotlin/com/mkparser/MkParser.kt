package com.mkparser

/**
 * MkParser — Kotlin wrapper for the mk_parser native library (M8c).
 *
 * Incremental Markdown parser for AI streaming scenarios.
 *
 * Usage:
 * ```kotlin
 * val parser = MkParser(
 *     onNodeOpen  = { type, flags -> … },
 *     onNodeClose = { type -> … },
 *     onText      = { text -> … },
 *     onModify    = { type -> … },
 * )
 * parser.feed("# Hello\n")
 * parser.feed("World\n")
 * parser.finish()
 * parser.destroy()
 * ```
 */
class MkParser(
    private val onNodeOpen:  ((type: Int, flags: Int, attr: String?) -> Unit)? = null,
    private val onNodeClose: ((type: Int) -> Unit)?                            = null,
    private val onText:      ((text: String) -> Unit)?                         = null,
    private val onModify:    ((type: Int) -> Unit)?                            = null,
) {
    private var nativeHandle: Long = 0

    init {
        System.loadLibrary("mk_parser_jni")
        nativeHandle = nativeCreate()
        check(nativeHandle != 0L) { "mk_parser native init failed" }
    }

    /** Feed a chunk of Markdown text. */
    fun feed(text: String): MkParser {
        check(nativeHandle != 0L) { "parser destroyed" }
        val rc = nativeFeed(nativeHandle, text)
        check(rc == 0) { "mk_feed failed: $rc" }
        return this
    }

    /** Signal end of stream. */
    fun finish(): MkParser {
        check(nativeHandle != 0L) { "parser destroyed" }
        val rc = nativeFinish(nativeHandle)
        check(rc == 0) { "mk_finish failed: $rc" }
        return this
    }

    /** Release all native resources. */
    fun destroy() {
        if (nativeHandle != 0L) {
            nativeDestroy(nativeHandle)
            nativeHandle = 0L
        }
    }

    // ── Called from native (JNI callbacks) ───────────────────────────────────

    @Suppress("unused")
    private fun onNativeNodeOpen(type: Int, flags: Int, attr: String?) {
        onNodeOpen?.invoke(type, flags, attr)
    }

    @Suppress("unused")
    private fun onNativeNodeClose(type: Int) {
        onNodeClose?.invoke(type)
    }

    @Suppress("unused")
    private fun onNativeText(text: String) {
        onText?.invoke(text)
    }

    @Suppress("unused")
    private fun onNativeNodeModify(type: Int) {
        onModify?.invoke(type)
    }

    // ── JNI declarations ─────────────────────────────────────────────────────

    private external fun nativeCreate(): Long
    private external fun nativeFeed(handle: Long, text: String): Int
    private external fun nativeFinish(handle: Long): Int
    private external fun nativeDestroy(handle: Long)
}

// ── NodeType constants (mirror MkNodeType enum) ───────────────────────────────

object NodeType {
    const val DOCUMENT       = 0
    const val HEADING        = 1
    const val PARAGRAPH      = 2
    const val CODE_BLOCK     = 3
    const val BLOCK_QUOTE    = 4
    const val LIST           = 5
    const val LIST_ITEM      = 6
    const val THEMATIC_BREAK = 7
    const val HTML_BLOCK     = 8
    const val TABLE          = 9
    const val TABLE_HEAD     = 10
    const val TABLE_ROW      = 11
    const val TABLE_CELL     = 12
    const val TEXT           = 13
    const val SOFT_BREAK     = 14
    const val HARD_BREAK     = 15
    const val EMPHASIS       = 16
    const val STRONG         = 17
    const val STRIKETHROUGH  = 18
    const val INLINE_CODE    = 19
    const val LINK           = 20
    const val IMAGE          = 21
    const val AUTO_LINK      = 22
    const val HTML_INLINE    = 23
    const val TASK_LIST_ITEM = 24
}
