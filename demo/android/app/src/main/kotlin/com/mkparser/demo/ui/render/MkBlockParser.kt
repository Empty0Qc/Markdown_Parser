package com.mkparser.demo.ui.render

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import com.mkparser.MkParser
import com.mkparser.NodeType

/**
 * Parses a Markdown string into a flat [List<MkBlock>] by driving [MkParser]
 * and processing its push-events via a state machine.
 *
 * Block IDs are stable across re-parses (type + ordinal), so LazyColumn
 * can diff efficiently and only recompose changed items.
 */
object MkBlockParser {

    fun parse(markdown: String, isStreaming: Boolean = false): List<MkBlock> {
        if (markdown.isBlank()) return emptyList()
        val builder = BlockBuilder()
        val parser = MkParser(
            onNodeOpen  = { type, flags, attr -> builder.onOpen(type, flags, attr) },
            onNodeClose = { type              -> builder.onClose(type) },
            onText      = { text             -> builder.onText(text) },
        )
        try {
            parser.feed(markdown).finish()
        } finally {
            parser.destroy()
        }
        val blocks = builder.build()
        return if (isStreaming) blocks.markLastStreaming() else blocks
    }
}

// ── [F03] Incremental streaming parser ────────────────────────────────────────
//
// Holds a long-lived MkParser + BlockBuilder pair so each new token is fed
// only once instead of re-parsing the entire accumulated document.
// This turns the streaming render pass from O(n²) into O(n).

class MkStreamingParser {
    private val builder = BlockBuilder()
    private var parser: MkParser? = MkParser(
        onNodeOpen  = { type, flags, attr -> builder.onOpen(type, flags, attr) },
        onNodeClose = { type              -> builder.onClose(type) },
        onText      = { text             -> builder.onText(text) },
    )

    /** Feed the next chunk and return the current block list (marked streaming). */
    fun feed(chunk: String): List<MkBlock> {
        parser?.feed(chunk)
        return builder.build().markLastStreaming()
    }

    /** Signal end-of-stream and return the final block list. */
    fun finish(): List<MkBlock> {
        parser?.finish()?.destroy()
        parser = null
        return builder.build()
    }
}

// ── Colours used for inline spans ────────────────────────────────────────────

private val colLink   = Color(0xFF89DCEB)
private val colCode   = Color(0xFFCBA6F7)
private val colCodeBg = Color(0xFF313244)

// ── State machine ─────────────────────────────────────────────────────────────

private class BlockBuilder {

    private val blocks   = mutableListOf<MkBlock>()
    private val typeCounts = mutableMapOf<String, Int>()

    // ── Block-level stack ─────────────────────────────────────────────────────

    private data class Frame(val type: Int, val flags: Int, val attr: String?, val id: String)
    private val stack = ArrayDeque<Frame>()

    // Single shared inline builder (owned by the innermost content block).
    private var ib: AnnotatedString.Builder? = null

    // How many AnnotatedString pushes each open inline span created.
    private val ibPushes = ArrayDeque<Int>()

    // ── List context ──────────────────────────────────────────────────────────

    private data class ListCtx(val ordered: Boolean, var itemNum: Int = 0)
    private val listStack = ArrayDeque<ListCtx>()

    // Task state set by a TASK_LIST_ITEM leaf marker, consumed by LIST_ITEM close.
    private var pendingTaskState: Int = 0

    // ── Table context ─────────────────────────────────────────────────────────

    private val tableHeaders = mutableListOf<AnnotatedString>()
    private val tableRows    = mutableListOf<List<AnnotatedString>>()
    private val currentRow   = mutableListOf<AnnotatedString>()
    private var inTableHead  = false

    // ── Helpers ───────────────────────────────────────────────────────────────

    private fun newId(typeName: String): String {
        val n = typeCounts.getOrDefault(typeName, 0)
        typeCounts[typeName] = n + 1
        return "${typeName}_$n"
    }

    /** Block types that "own" the inline builder — PARAGRAPH inside them
     *  should not create its own builder or emit its own block. */
    private val containerTypes = setOf(
        NodeType.LIST_ITEM, NodeType.TASK_LIST_ITEM,
        NodeType.BLOCK_QUOTE, NodeType.TABLE_CELL,
    )

    private fun insideContainer(): Boolean =
        stack.any { it.type in containerTypes }

    // ── Public event handlers ─────────────────────────────────────────────────

    fun onOpen(type: Int, flags: Int, attr: String?) {
        stack.addLast(Frame(type, flags, attr, newId(typeKey(type))))

        when (type) {
            NodeType.HEADING -> ib = AnnotatedString.Builder()

            NodeType.CODE_BLOCK -> ib = AnnotatedString.Builder()

            NodeType.BLOCK_QUOTE -> ib = AnnotatedString.Builder()

            NodeType.PARAGRAPH -> {
                if (!insideContainer()) ib = AnnotatedString.Builder()
            }

            NodeType.LIST_ITEM -> {
                pendingTaskState = 0        // reset; TASK_LIST_ITEM sibling will set it
                ib = AnnotatedString.Builder()
            }

            NodeType.TASK_LIST_ITEM -> {
                // TASK_LIST_ITEM is a leaf marker node (sibling of PARAGRAPH inside LIST_ITEM).
                // Store its checked state so LIST_ITEM close can use it.
                // Do NOT overwrite ib — LIST_ITEM already owns the builder.
                // If ib is null (standalone TASK_LIST_ITEM without a LIST_ITEM parent),
                // create a builder as fallback so text is not lost.
                pendingTaskState = flags
                if (ib == null) ib = AnnotatedString.Builder()
            }

            NodeType.LIST -> listStack.addLast(ListCtx(ordered = flags == 1))

            NodeType.TABLE -> {
                tableHeaders.clear()
                tableRows.clear()
            }

            NodeType.TABLE_HEAD -> inTableHead = true

            NodeType.TABLE_ROW -> if (!inTableHead) currentRow.clear()

            NodeType.TABLE_CELL -> ib = AnnotatedString.Builder()

            NodeType.THEMATIC_BREAK -> { /* leaf, handled on close */ }

            // ── Inline spans ──────────────────────────────────────────────────

            NodeType.STRONG -> ibPushes.addLast(
                ib?.let { it.pushStyle(SpanStyle(fontWeight = FontWeight.Bold)); 1 } ?: 0
            )
            NodeType.EMPHASIS -> ibPushes.addLast(
                ib?.let { it.pushStyle(SpanStyle(fontStyle = FontStyle.Italic)); 1 } ?: 0
            )
            NodeType.STRIKETHROUGH -> ibPushes.addLast(
                ib?.let { it.pushStyle(SpanStyle(textDecoration = TextDecoration.LineThrough)); 1 } ?: 0
            )
            NodeType.INLINE_CODE -> ibPushes.addLast(
                ib?.let {
                    it.pushStyle(SpanStyle(fontFamily = FontFamily.Monospace,
                        color = colCode, background = colCodeBg))
                    1
                } ?: 0
            )
            NodeType.LINK,
            NodeType.AUTO_LINK -> {
                val url = attr ?: ""
                val pushed = ib?.let { b ->
                    b.pushStyle(SpanStyle(color = colLink, textDecoration = TextDecoration.Underline))
                    if (url.isNotEmpty()) { b.pushStringAnnotation("URL", url); 2 } else 1
                } ?: 0
                ibPushes.addLast(pushed)
            }

            NodeType.SOFT_BREAK -> ib?.append(" ")
            NodeType.HARD_BREAK -> ib?.append("\n")
        }
    }

    fun onClose(type: Int) {
        val frame = stack.removeLastOrNull() ?: return

        when (type) {

            // ── Inline span pop ───────────────────────────────────────────────

            NodeType.STRONG,
            NodeType.EMPHASIS,
            NodeType.STRIKETHROUGH,
            NodeType.INLINE_CODE,
            NodeType.LINK,
            NodeType.AUTO_LINK -> {
                val count = ibPushes.removeLastOrNull() ?: 0
                repeat(count) { ib?.pop() }
            }

            // ── Block emit ────────────────────────────────────────────────────

            NodeType.HEADING -> {
                val c = ib?.toAnnotatedString() ?: AnnotatedString(""); ib = null
                blocks += MkBlock.Heading(frame.id, frame.flags, c)
            }

            NodeType.PARAGRAPH -> {
                if (!insideContainer()) {
                    val c = ib?.toAnnotatedString() ?: AnnotatedString(""); ib = null
                    blocks += MkBlock.Paragraph(frame.id, c)
                }
                // else: container still owns the builder
            }

            NodeType.CODE_BLOCK -> {
                val code = ib?.toAnnotatedString()?.text?.trimEnd('\n') ?: ""; ib = null
                blocks += MkBlock.FencedCode(frame.id, frame.attr ?: "", code)
            }

            NodeType.BLOCK_QUOTE -> {
                val c = ib?.toAnnotatedString() ?: AnnotatedString(""); ib = null
                blocks += MkBlock.BlockQuote(frame.id, c)
            }

            NodeType.LIST_ITEM -> {
                val c = ib?.toAnnotatedString() ?: AnnotatedString(""); ib = null
                val ctx = listStack.lastOrNull()
                if (ctx?.ordered == true) {
                    ctx.itemNum++
                    blocks += MkBlock.OrderedItem(frame.id, listStack.size - 1, ctx.itemNum, c)
                } else {
                    // pendingTaskState is set by a TASK_LIST_ITEM sibling marker (1=unchecked,2=checked).
                    // frame.flags carries mk_node_list_item_task_state as fallback.
                    val taskState = if (pendingTaskState != 0) pendingTaskState else frame.flags
                    pendingTaskState = 0
                    blocks += MkBlock.BulletItem(frame.id, listStack.size - 1, c, taskState = taskState)
                }
            }

            NodeType.TASK_LIST_ITEM -> {
                // If there is a LIST_ITEM ancestor, this is a leaf marker — it emits nothing.
                // The LIST_ITEM close handler will emit the block using pendingTaskState.
                // If standalone (no LIST_ITEM parent), emit as a regular bullet block.
                val hasListItemParent = stack.any { it.type == NodeType.LIST_ITEM }
                if (!hasListItemParent) {
                    val c = ib?.toAnnotatedString() ?: AnnotatedString(""); ib = null
                    blocks += MkBlock.BulletItem(frame.id, listStack.size - 1, c, taskState = frame.flags)
                }
                // else: do nothing — LIST_ITEM close handles emission
            }

            NodeType.LIST -> listStack.removeLastOrNull()

            NodeType.THEMATIC_BREAK -> blocks += MkBlock.ThematicBreak(frame.id)

            NodeType.TABLE_CELL -> {
                val c = ib?.toAnnotatedString() ?: AnnotatedString(""); ib = null
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

    // Stable key prefix per block type
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
