package com.mkparser.demo.ui.render

import androidx.compose.ui.text.AnnotatedString

/**
 * Block-level Markdown node, ready for direct rendering.
 * Each block has a stable [id] based on its type + sequence, so LazyColumn
 * can key on it and recompose only changed items during streaming.
 */
sealed class MkBlock {
    abstract val id: String
    abstract val isStreaming: Boolean

    data class Heading(
        override val id: String,
        val level: Int,                  // 1–6
        val content: AnnotatedString,
        override val isStreaming: Boolean = false,
    ) : MkBlock()

    data class Paragraph(
        override val id: String,
        val content: AnnotatedString,
        override val isStreaming: Boolean = false,
    ) : MkBlock()

    data class FencedCode(
        override val id: String,
        val language: String,
        val code: String,
        override val isStreaming: Boolean = false,
    ) : MkBlock()

    data class BulletItem(
        override val id: String,
        val depth: Int,
        val content: AnnotatedString,
        val taskState: Int = 0,          // 0=none 1=unchecked 2=checked
        override val isStreaming: Boolean = false,
    ) : MkBlock()

    data class OrderedItem(
        override val id: String,
        val depth: Int,
        val number: Int,
        val content: AnnotatedString,
        override val isStreaming: Boolean = false,
    ) : MkBlock()

    data class BlockQuote(
        override val id: String,
        val content: AnnotatedString,
        override val isStreaming: Boolean = false,
    ) : MkBlock()

    data class ThematicBreak(
        override val id: String,
        override val isStreaming: Boolean = false,
    ) : MkBlock()

    data class TableBlock(
        override val id: String,
        val headers: List<AnnotatedString>,
        val rows: List<List<AnnotatedString>>,
        override val isStreaming: Boolean = false,
    ) : MkBlock()
}

/** Mark the last block in the list as streaming (for cursor rendering). */
fun List<MkBlock>.markLastStreaming(): List<MkBlock> {
    if (isEmpty()) return this
    return toMutableList().also {
        it[it.lastIndex] = when (val b = it.last()) {
            is MkBlock.Heading      -> b.copy(isStreaming = true)
            is MkBlock.Paragraph    -> b.copy(isStreaming = true)
            is MkBlock.FencedCode   -> b.copy(isStreaming = true)
            is MkBlock.BulletItem   -> b.copy(isStreaming = true)
            is MkBlock.OrderedItem  -> b.copy(isStreaming = true)
            is MkBlock.BlockQuote   -> b.copy(isStreaming = true)
            is MkBlock.ThematicBreak -> b
            is MkBlock.TableBlock   -> b.copy(isStreaming = true)
        }
    }
}
