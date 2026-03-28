package com.mkparser

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.TextPaint
import android.text.style.ClickableSpan
import android.text.style.ForegroundColorSpan
import android.text.style.ReplacementSpan
import android.text.style.StrikethroughSpan
import android.text.style.StyleSpan
import android.text.style.TypefaceSpan
import android.text.style.URLSpan
import android.view.View

/**
 * Parses a Markdown string into a flat [List<MkBlock>] by driving [MkParser]
 * and processing its push-events via a state machine.
 *
 * Block IDs are stable across re-parses (type + ordinal), so RecyclerView
 * can diff efficiently and only rebind changed items.
 *
 * @param onLinkClick      optional callback for link clicks. Receives the URL.
 *                         If null, a standard [URLSpan] is used (opens browser).
 * @param onFootnoteClick  optional callback for footnote badge taps. Receives the
 *                         raw label string (e.g. "1-2-3"). If null, badges are
 *                         non-interactive.
 * @param footnoteDisplayText optional mapper from raw label (e.g. "1-2-3") to
 *                         the text shown inside the badge (e.g. "引3"). If null
 *                         the raw label is shown.
 */
object MkBlockParser {

    fun parse(
        markdown: String,
        isStreaming: Boolean = false,
        onLinkClick: ((url: String) -> Unit)? = null,
        onFootnoteClick: ((label: String) -> Unit)? = null,
        footnoteDisplayText: ((label: String) -> String)? = null,
    ): List<MkBlock> {
        if (markdown.isBlank()) return emptyList()
        val processed = footnotesToHtml(markdown)
        val builder = BlockBuilder(onLinkClick, onFootnoteClick, footnoteDisplayText)
        val parser = MkParser(
            onNodeOpen  = { type, flags, attr -> builder.onOpen(type, flags, attr) },
            onNodeClose = { type              -> builder.onClose(type) },
            onText      = { text             -> builder.onText(text) },
        )
        try {
            parser.feed(processed).finish()
        } finally {
            parser.destroy()
        }
        val blocks = builder.build()
        return if (isStreaming) blocks.markLastStreaming() else blocks
    }

    /**
     * Pre-process markdown to make footnote references compatible with mk-parser:
     *  1. Strip GFM footnote definition lines (e.g. `[^1-2]: 1`).
     *  2. Convert inline refs `[^label]` → raw HTML `<sup>label</sup>`.
     * mk-parser then emits these as HTML_INLINE events with the raw HTML as attr.
     */
    private val FOOTNOTE_DEF = Regex("""(?m)^\[\^[^\]]+\]:[^\n]*\n?""")
    private val FOOTNOTE_REF = Regex("""\[\^([^\]]+)\]""")

    internal fun footnotesToHtml(markdown: String): String {
        val noDefs = FOOTNOTE_DEF.replace(markdown, "")
        return FOOTNOTE_REF.replace(noDefs) { "<sup>${it.groupValues[1]}</sup>" }
    }
}

// ── State machine ──────────────────────────────────────────────────────────────

private class BlockBuilder(
    private val onLinkClick: ((url: String) -> Unit)? = null,
    private val onFootnoteClick: ((label: String) -> Unit)? = null,
    private val footnoteDisplayText: ((label: String) -> String)? = null,
) {

    private val blocks     = mutableListOf<MkBlock>()
    private val typeCounts = mutableMapOf<String, Int>()

    // ── Block-level stack ──────────────────────────────────────────────────────

    private data class Frame(val type: Int, val flags: Int, val attr: String?, val id: String)
    private val stack = ArrayDeque<Frame>()

    // Single shared inline builder owned by the innermost content block.
    private var ib: SpannableStringBuilder? = null

    // ── Pending span: records start pos + the span object to apply on close ────

    private data class SpanMark(val start: Int, val span: Any?, val urlSpan: Any?)
    private val ibSpans = ArrayDeque<SpanMark>()

    // ── List context ───────────────────────────────────────────────────────────

    private data class ListCtx(val ordered: Boolean, var itemNum: Int = 0)
    private val listStack = ArrayDeque<ListCtx>()

    // Task state set by a TASK_LIST_ITEM leaf marker, consumed by LIST_ITEM close.
    private var pendingTaskState: Int = 0

    // ── Table context ──────────────────────────────────────────────────────────

    private val tableHeaders = mutableListOf<SpannableStringBuilder>()
    private val tableRows    = mutableListOf<List<SpannableStringBuilder>>()
    private val currentRow   = mutableListOf<SpannableStringBuilder>()
    private var inTableHead  = false

    // ── Helpers ────────────────────────────────────────────────────────────────

    private fun newId(typeName: String): String {
        val n = typeCounts.getOrDefault(typeName, 0)
        typeCounts[typeName] = n + 1
        return "${typeName}_$n"
    }

    private val containerTypes = setOf(
        NodeType.LIST_ITEM, NodeType.TASK_LIST_ITEM,
        NodeType.BLOCK_QUOTE, NodeType.TABLE_CELL,
    )

    private fun insideContainer(): Boolean =
        stack.any { it.type in containerTypes }

    // ── Public event handlers ──────────────────────────────────────────────────

    fun onOpen(type: Int, flags: Int, attr: String?) {
        stack.addLast(Frame(type, flags, attr, newId(typeKey(type))))

        when (type) {
            NodeType.HEADING      -> ib = SpannableStringBuilder()
            NodeType.CODE_BLOCK   -> ib = SpannableStringBuilder()
            NodeType.BLOCK_QUOTE  -> ib = SpannableStringBuilder()

            NodeType.PARAGRAPH -> {
                if (!insideContainer()) ib = SpannableStringBuilder()
            }

            NodeType.LIST_ITEM -> {
                pendingTaskState = 0
                ib = SpannableStringBuilder()
            }

            NodeType.TASK_LIST_ITEM -> {
                pendingTaskState = flags
                if (ib == null) ib = SpannableStringBuilder()
            }

            NodeType.LIST -> listStack.addLast(ListCtx(ordered = flags == 1))

            NodeType.TABLE -> {
                tableHeaders.clear()
                tableRows.clear()
            }

            NodeType.TABLE_HEAD -> inTableHead = true
            NodeType.TABLE_ROW  -> if (!inTableHead) currentRow.clear()
            NodeType.TABLE_CELL -> ib = SpannableStringBuilder()

            NodeType.THEMATIC_BREAK -> { /* leaf, handled on close */ }

            // ── Inline spans: record start position ───────────────────────────

            NodeType.STRONG -> {
                ibSpans.addLast(SpanMark(ib?.length ?: 0, StyleSpan(Typeface.BOLD), null))
            }
            NodeType.EMPHASIS -> {
                ibSpans.addLast(SpanMark(ib?.length ?: 0, StyleSpan(Typeface.ITALIC), null))
            }
            NodeType.STRIKETHROUGH -> {
                ibSpans.addLast(SpanMark(ib?.length ?: 0, StrikethroughSpan(), null))
            }
            NodeType.INLINE_CODE -> {
                ibSpans.addLast(SpanMark(ib?.length ?: 0, TypefaceSpan("monospace"), null))
            }
            NodeType.LINK,
            NodeType.AUTO_LINK -> {
                val url = attr ?: ""
                val span: Any? = if (url.isNotEmpty()) {
                    if (onLinkClick != null) {
                        object : ClickableSpan() {
                            override fun onClick(widget: View) = onLinkClick.invoke(url)
                        }
                    } else {
                        URLSpan(url)
                    }
                } else null
                ibSpans.addLast(SpanMark(ib?.length ?: 0, null, span))
            }

            NodeType.IMAGE -> {
                // IMAGE placeholder — full Glide support added in C3
                val src = attr ?: ""
                val span: Any? = if (src.isNotEmpty()) URLSpan(src) else null
                ibSpans.addLast(SpanMark(ib?.length ?: 0, null, span))
            }

            NodeType.HTML_INLINE -> {
                // Detect <sup>label</sup> from preprocessed footnote references
                val raw = attr ?: ""
                val m = SUP_REGEX.find(raw)
                if (m != null && ib != null) {
                    val label   = m.groupValues[1]
                    val display = footnoteDisplayText?.invoke(label) ?: label
                    val start   = ib!!.length
                    ib!!.append(display)
                    val end = ib!!.length
                    if (end > start) {
                        ib!!.setSpan(
                            MkBadgeSpan(display, MkBadgeBgColor, MkBadgeFgColor),
                            start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
                        )
                        if (onFootnoteClick != null) {
                            ib!!.setSpan(
                                object : ClickableSpan() {
                                    override fun onClick(widget: View) =
                                        onFootnoteClick.invoke(label)
                                },
                                start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
                            )
                        }
                    }
                }
                // Other HTML inline tags (e.g. <br>, comments) are silently ignored
            }

            NodeType.SOFT_BREAK -> ib?.append(" ")
            NodeType.HARD_BREAK -> ib?.append("\n")
        }
    }

    fun onClose(type: Int) {
        val frame = stack.removeLastOrNull() ?: return

        when (type) {

            // ── Inline span apply ──────────────────────────────────────────────

            NodeType.STRONG,
            NodeType.EMPHASIS,
            NodeType.STRIKETHROUGH,
            NodeType.INLINE_CODE -> {
                val mark = ibSpans.removeLastOrNull() ?: return
                val end = ib?.length ?: 0
                if (mark.span != null && end > mark.start) {
                    ib?.setSpan(mark.span, mark.start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
                }
            }

            NodeType.LINK,
            NodeType.AUTO_LINK,
            NodeType.IMAGE -> {
                val mark = ibSpans.removeLastOrNull() ?: return
                val end = ib?.length ?: 0
                if (mark.urlSpan != null && end > mark.start) {
                    ib?.setSpan(mark.urlSpan, mark.start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
                    ib?.setSpan(
                        ForegroundColorSpan(MkLinkColor),
                        mark.start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
                    )
                }
            }

            // ── Block emit ─────────────────────────────────────────────────────

            NodeType.HEADING -> {
                val c = ib ?: SpannableStringBuilder(); ib = null
                blocks += MkBlock.Heading(frame.id, frame.flags, c)
            }

            NodeType.PARAGRAPH -> {
                if (!insideContainer()) {
                    val c = ib ?: SpannableStringBuilder(); ib = null
                    blocks += MkBlock.Paragraph(frame.id, c)
                }
            }

            NodeType.CODE_BLOCK -> {
                val code = ib?.toString()?.trimEnd('\n') ?: ""; ib = null
                blocks += MkBlock.FencedCode(frame.id, frame.attr ?: "", code)
            }

            NodeType.BLOCK_QUOTE -> {
                val c = ib ?: SpannableStringBuilder(); ib = null
                blocks += MkBlock.BlockQuote(frame.id, c)
            }

            NodeType.LIST_ITEM -> {
                val c = ib ?: SpannableStringBuilder(); ib = null
                val ctx = listStack.lastOrNull()
                if (ctx?.ordered == true) {
                    ctx.itemNum++
                    blocks += MkBlock.OrderedItem(frame.id, listStack.size - 1, ctx.itemNum, c)
                } else {
                    val taskState = if (pendingTaskState != 0) pendingTaskState else frame.flags
                    pendingTaskState = 0
                    blocks += MkBlock.BulletItem(frame.id, listStack.size - 1, c, taskState)
                }
            }

            NodeType.TASK_LIST_ITEM -> {
                val hasListItemParent = stack.any { it.type == NodeType.LIST_ITEM }
                if (!hasListItemParent) {
                    val c = ib ?: SpannableStringBuilder(); ib = null
                    blocks += MkBlock.BulletItem(frame.id, listStack.size - 1, c, taskState = frame.flags)
                }
            }

            NodeType.LIST -> listStack.removeLastOrNull()

            NodeType.THEMATIC_BREAK -> blocks += MkBlock.ThematicBreak(frame.id)

            NodeType.TABLE_CELL -> {
                val c = ib ?: SpannableStringBuilder(); ib = null
                if (inTableHead) tableHeaders += c else currentRow += c
            }

            NodeType.TABLE_ROW -> {
                if (!inTableHead) { tableRows += currentRow.toList(); currentRow.clear() }
            }

            NodeType.TABLE_HEAD -> inTableHead = false

            NodeType.TABLE -> blocks += MkBlock.TableBlock(
                frame.id, tableHeaders.toList(), tableRows.toList()
            )
        }
    }

    fun onText(text: String) { ib?.append(text) }

    fun build(): List<MkBlock> = blocks.toList()

    private fun typeKey(type: Int) = when (type) {
        NodeType.HEADING        -> "H"
        NodeType.PARAGRAPH      -> "P"
        NodeType.CODE_BLOCK     -> "CODE"
        NodeType.BLOCK_QUOTE    -> "BQ"
        NodeType.LIST_ITEM,
        NodeType.TASK_LIST_ITEM -> "LI"
        NodeType.THEMATIC_BREAK -> "HR"
        NodeType.TABLE          -> "TBL"
        NodeType.TABLE_CELL     -> "TD"
        else                    -> "X"
    }
}

/** Default link color (can be overridden by adapter style). */
internal var MkLinkColor: Int = 0xFF0066CC.toInt()

/** Badge background / foreground colors for footnote spans. */
internal var MkBadgeBgColor: Int = 0x1AFF7044.toInt()
internal var MkBadgeFgColor: Int = 0xFF7D7C7F.toInt()

/** Regex matching a simple <sup>label</sup> produced by footnotesToHtml(). */
private val SUP_REGEX = Regex("""<sup>([^<]+)</sup>""", RegexOption.IGNORE_CASE)

/**
 * A [ReplacementSpan] that draws the display text inside a pill-shaped badge.
 * Used for footnote references in the rendered output.
 */
internal class MkBadgeSpan(
    private val displayText: String,
    private val bgColor: Int,
    private val fgColor: Int,
) : ReplacementSpan() {

    override fun getSize(
        paint: Paint, text: CharSequence?, start: Int, end: Int,
        fm: Paint.FontMetricsInt?,
    ): Int {
        val tp = TextPaint(paint).also { it.textSize = paint.textSize * TEXT_SCALE }
        return (HPAD * 2 + tp.measureText(displayText) + 0.5f).toInt()
    }

    override fun draw(
        canvas: Canvas, text: CharSequence?, start: Int, end: Int,
        x: Float, top: Int, y: Int, bottom: Int, paint: Paint,
    ) {
        val tp = TextPaint(paint).also {
            it.textSize = paint.textSize * TEXT_SCALE
            it.color    = fgColor
        }
        val textW = tp.measureText(displayText)
        val w     = HPAD * 2 + textW
        val mid   = (top + bottom) / 2f
        val h     = (bottom - top) * 0.65f
        val r     = h / 2f

        val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = bgColor
            style = Paint.Style.FILL
        }
        canvas.drawRoundRect(RectF(x, mid - r, x + w, mid + r), r, r, bgPaint)
        val textY = mid - (tp.ascent() + tp.descent()) / 2f
        canvas.drawText(displayText, x + HPAD, textY, tp)
    }

    companion object {
        private const val HPAD       = 6f
        private const val TEXT_SCALE = 0.75f
    }
}
