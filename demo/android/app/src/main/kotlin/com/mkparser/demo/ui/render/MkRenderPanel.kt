package com.mkparser.demo.ui.render

import androidx.compose.animation.core.*
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

// ── Palette (Catppuccin Mocha) ────────────────────────────────────────────────

private val BgSurface  = Color(0xFF1E1E2E)
private val BgCode     = Color(0xFF181825)
private val ColText    = Color(0xFFCDD6F4)
private val ColSubtext = Color(0xFFA6ADC8)
private val ColMauve   = Color(0xFFCBA6F7)   // headings, blockquote border
private val ColGreen   = Color(0xFFA6E3A1)   // code text
private val ColPeach   = Color(0xFFFAB387)   // bullet
private val ColYellow  = Color(0xFFF9E2AF)   // ordered list number
private val ColSky     = Color(0xFF89DCEB)   // link
private val ColRed     = Color(0xFFF38BA8)   // strike / unchecked

// ── Public composable ─────────────────────────────────────────────────────────

@Composable
fun MkRenderPanel(
    blocks: List<MkBlock>,
    modifier: Modifier = Modifier,
) {
    val listState = rememberLazyListState()

    // Auto-scroll to bottom when streaming (new content appended)
    val lastId = blocks.lastOrNull()?.id
    LaunchedEffect(lastId) {
        if (blocks.isNotEmpty()) listState.animateScrollToItem(blocks.lastIndex)
    }

    LazyColumn(
        state     = listState,
        modifier  = modifier.background(BgSurface),
        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        items(blocks, key = { it.id }) { block ->
            BlockItem(block)
        }
    }
}

// ── Dispatch ──────────────────────────────────────────────────────────────────

@Composable
private fun BlockItem(block: MkBlock) {
    when (block) {
        is MkBlock.Heading      -> HeadingItem(block)
        is MkBlock.Paragraph    -> ParagraphItem(block)
        is MkBlock.FencedCode   -> FencedCodeItem(block)
        is MkBlock.BulletItem   -> BulletListItem(block)
        is MkBlock.OrderedItem  -> OrderedListItem(block)
        is MkBlock.BlockQuote   -> BlockQuoteItem(block)
        is MkBlock.ThematicBreak -> HorizontalDivider(color = ColSubtext.copy(alpha = 0.4f), modifier = Modifier.padding(vertical = 8.dp))
        is MkBlock.TableBlock   -> TableItem(block)
    }
}

// ── Streaming cursor ──────────────────────────────────────────────────────────

@Composable
private fun streamingContent(base: AnnotatedString, isStreaming: Boolean): AnnotatedString {
    if (!isStreaming) return base
    val transition = rememberInfiniteTransition(label = "cursor")
    val alpha by transition.animateFloat(
        initialValue = 1f,
        targetValue  = 0f,
        animationSpec = infiniteRepeatable(
            animation = tween(500, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse,
        ),
        label = "cursorAlpha",
    )
    return buildAnnotatedString {
        append(base)
        pushStyle(SpanStyle(color = ColMauve.copy(alpha = alpha)))
        append("▌")
        pop()
    }
}

@Composable
private fun streamingCode(base: String, isStreaming: Boolean): String {
    if (!isStreaming) return base
    val transition = rememberInfiniteTransition(label = "codeCursor")
    val alpha by transition.animateFloat(
        initialValue = 1f,
        targetValue  = 0f,
        animationSpec = infiniteRepeatable(
            animation = tween(500, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse,
        ),
        label = "codeCursorAlpha",
    )
    // alpha drives the visibility — render cursor character always but with alpha modifier
    return if (alpha > 0.5f) "$base▌" else base
}

// ── Block renderers ───────────────────────────────────────────────────────────

@Composable
private fun HeadingItem(block: MkBlock.Heading) {
    val (sizeSp, topDp) = when (block.level) {
        1    -> 26.sp to 12.dp
        2    -> 22.sp to 10.dp
        3    -> 19.sp to 8.dp
        4    -> 17.sp to 6.dp
        else -> 15.sp to 4.dp
    }
    val content = streamingContent(block.content, block.isStreaming)
    Text(
        text     = content,
        style    = TextStyle(
            color      = ColMauve,
            fontSize   = sizeSp,
            fontWeight = FontWeight.Bold,
        ),
        modifier = Modifier.padding(top = topDp, bottom = 2.dp),
    )
}

@Composable
private fun ParagraphItem(block: MkBlock.Paragraph) {
    val content = streamingContent(block.content, block.isStreaming)
    Text(
        text  = content,
        style = TextStyle(color = ColText, fontSize = 14.sp, lineHeight = 22.sp),
    )
}

@Composable
private fun FencedCodeItem(block: MkBlock.FencedCode) {
    val code = streamingCode(block.code, block.isStreaming)
    Surface(
        shape = RoundedCornerShape(8.dp),
        color = BgCode,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            if (block.language.isNotEmpty()) {
                Text(
                    text  = block.language,
                    style = TextStyle(color = ColSubtext, fontSize = 11.sp, fontFamily = FontFamily.Monospace),
                    modifier = Modifier.padding(bottom = 6.dp),
                )
            }
            val scrollState = rememberScrollState()
            Text(
                text     = code,
                style    = TextStyle(
                    color      = ColGreen,
                    fontSize   = 13.sp,
                    fontFamily = FontFamily.Monospace,
                    lineHeight = 20.sp,
                ),
                modifier = Modifier.horizontalScroll(scrollState),
            )
        }
    }
}

@Composable
private fun BulletListItem(block: MkBlock.BulletItem) {
    val content = streamingContent(block.content, block.isStreaming)
    Row(
        modifier            = Modifier.padding(start = (block.depth * 16).dp),
        verticalAlignment   = Alignment.Top,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        when (block.taskState) {
            1 -> Text("☐", style = TextStyle(color = ColRed,   fontSize = 14.sp), modifier = Modifier.padding(top = 2.dp))
            2 -> Text("☑", style = TextStyle(color = ColGreen, fontSize = 14.sp), modifier = Modifier.padding(top = 2.dp))
            else -> Text("•", style = TextStyle(color = ColPeach, fontSize = 18.sp, lineHeight = 20.sp))
        }
        Text(
            text  = content,
            style = TextStyle(color = ColText, fontSize = 14.sp, lineHeight = 22.sp),
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
private fun OrderedListItem(block: MkBlock.OrderedItem) {
    val content = streamingContent(block.content, block.isStreaming)
    Row(
        modifier              = Modifier.padding(start = (block.depth * 16).dp),
        verticalAlignment     = Alignment.Top,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(
            text  = "${block.number}.",
            style = TextStyle(color = ColYellow, fontSize = 14.sp, fontWeight = FontWeight.SemiBold),
            modifier = Modifier.width(28.dp),
        )
        Text(
            text  = content,
            style = TextStyle(color = ColText, fontSize = 14.sp, lineHeight = 22.sp),
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
private fun BlockQuoteItem(block: MkBlock.BlockQuote) {
    val content = streamingContent(block.content, block.isStreaming)
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(ColMauve.copy(alpha = 0.06f), RoundedCornerShape(4.dp)),
    ) {
        Box(
            modifier = Modifier
                .width(4.dp)
                .heightIn(min = 36.dp)
                .background(ColMauve, RoundedCornerShape(topStart = 4.dp, bottomStart = 4.dp))
                .padding(top = 6.dp, bottom = 6.dp),
        )
        Text(
            text  = content,
            style = TextStyle(color = ColSubtext, fontSize = 14.sp, lineHeight = 22.sp),
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp),
        )
    }
}

@Composable
private fun TableItem(block: MkBlock.TableBlock) {
    Surface(
        shape = RoundedCornerShape(8.dp),
        color = BgCode,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(modifier = Modifier.padding(4.dp)) {
            // Header row
            if (block.headers.isNotEmpty()) {
                Row(modifier = Modifier.fillMaxWidth()) {
                    block.headers.forEach { header ->
                        Text(
                            text     = header,
                            style    = TextStyle(color = ColMauve, fontSize = 13.sp, fontWeight = FontWeight.SemiBold),
                            modifier = Modifier.weight(1f).padding(6.dp),
                        )
                    }
                }
                HorizontalDivider(color = ColSubtext.copy(alpha = 0.3f))
            }
            // Data rows
            block.rows.forEachIndexed { rowIdx, row ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(if (rowIdx % 2 == 0) Color.Transparent else ColMauve.copy(alpha = 0.04f)),
                ) {
                    row.forEach { cell ->
                        Text(
                            text     = cell,
                            style    = TextStyle(color = ColText, fontSize = 13.sp),
                            modifier = Modifier.weight(1f).padding(6.dp),
                        )
                    }
                }
            }
        }
    }
}
