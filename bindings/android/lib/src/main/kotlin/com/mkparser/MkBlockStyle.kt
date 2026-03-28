package com.mkparser

import android.graphics.Typeface

/**
 * Visual style configuration for [MkBlockAdapter].
 * All size values are in dp; the adapter converts to px at render time.
 *
 * Two ready-made factories are provided:
 *   [MkBlockStyle.content] — normal content stream (larger text, full colour)
 *   [MkBlockStyle.step]    — step/think stream (smaller, muted colour)
 */
data class MkBlockStyle(

    // ── Paragraph ────────────────────────────────────────────────────────────
    val paragraphTextSizeDp: Float  = 14.5f,
    val paragraphTextColor: Int     = 0xFF333333.toInt(),
    val paragraphLineSpacingDp: Float = 23f,  // total line height (dp)

    // ── Headings (index 0 = H1 … 5 = H6) ────────────────────────────────────
    val headingTextSizesDp: FloatArray = floatArrayOf(22f, 19.5f, 17f, 14.5f, 14.5f, 14.5f),
    val headingTextColor: Int          = 0xFF1A1A1A.toInt(),
    val headingTypefaceStyle: Int      = Typeface.BOLD,

    // ── Links ─────────────────────────────────────────────────────────────────
    val linkColor: Int = 0xFFFF7044.toInt(),

    // ── Inline code ───────────────────────────────────────────────────────────
    val inlineCodeTextSizeDp: Float = 13f,
    val inlineCodeTextColor: Int    = 0xFF333333.toInt(),
    val inlineCodeBgColor: Int      = 0x4DD5D5DE.toInt(),

    // ── Fenced code block ─────────────────────────────────────────────────────
    val codeBlockTextSizeDp: Float  = 13f,
    val codeBlockTextColor: Int     = 0xFF333333.toInt(),
    val codeBlockBgColor: Int       = 0x4DE5E5F0.toInt(),
    val codeBlockCornerRadiusDp: Float = 8f,
    val codeBlockPaddingDp: Int     = 12,
    val codeBlockMarginVertDp: Int  = 4,
    val codeLangTextSizeDp: Float   = 11f,

    // ── Block quote ───────────────────────────────────────────────────────────
    val quoteTextSizeDp: Float      = 14.5f,
    val quoteTextColor: Int         = 0xFF333333.toInt(),
    val quoteIndicatorWidthDp: Int  = 3,
    val quoteIndicatorColor: Int    = 0x19000000.toInt(),
    val quoteBgColor: Int           = 0x4DE5E5F0.toInt(),
    val quoteCornerRadiusDp: Float  = 8f,
    val quotePaddingDp: Int         = 14,
    val quoteMarginVertDp: Int      = 8,

    // ── Thematic break ────────────────────────────────────────────────────────
    val dividerColor: Int           = 0xFF333333.toInt(),
    val dividerAlpha: Float         = 0.08f,
    val dividerHeightDp: Int        = 1,
    val dividerMarginHorzDp: Int    = 24,
    val dividerMarginVertDp: Int    = 16,

    // ── Lists (bullet + ordered) ──────────────────────────────────────────────
    val listTextSizeDp: Float       = 14.5f,
    val listTextColor: Int          = 0xFF262626.toInt(),
    val listBulletColor: Int        = 0xFF333333.toInt(),
    val listBulletSizeDp: Float     = 4.5f,
    val listDepthIndentDp: Int      = 8,
    val listBulletGapDp: Int        = 6,
    val listItemSpacingDp: Int      = 4,

    // ── Table ─────────────────────────────────────────────────────────────────
    val tableHeaderTextSizeDp: Float = 14f,
    val tableHeaderTextColor: Int    = 0xFF262626.toInt(),
    val tableHeaderTypefaceStyle: Int = Typeface.BOLD,
    val tableCellTextSizeDp: Float   = 14f,
    val tableCellTextColor: Int      = 0xFF333333.toInt(),
    val tableBorderColor: Int        = 0xFFC1C0C1.toInt(),
    val tableCellPaddingHorzDp: Int  = 12,
    val tableCellPaddingVertDp: Int  = 5,
    val tableMarginVertDp: Int       = 12,
    val tableMaxCellWidthDp: Int     = 139,

    // ── Streaming cursor ──────────────────────────────────────────────────────
    val showStreamingCursor: Boolean = true,
    val streamingCursorChar: String  = "▌",
) {
    companion object {

        /** Normal content-stream style. */
        fun content(): MkBlockStyle = MkBlockStyle()

        /** Step/think stream: smaller text, muted colour. */
        fun step(): MkBlockStyle = MkBlockStyle(
            paragraphTextSizeDp   = 13f,
            paragraphTextColor    = 0x80222222.toInt(),
            headingTextSizesDp    = floatArrayOf(13f, 13f, 13f, 13f, 13f, 13f),
            headingTextColor      = 0x80222222.toInt(),
            listTextSizeDp        = 13f,
            listTextColor         = 0x80222222.toInt(),
            listBulletColor       = 0x80222222.toInt(),
            listDepthIndentDp     = 17,
            quoteTextSizeDp       = 13f,
            quoteTextColor        = 0x80222222.toInt(),
        )
    }
}
