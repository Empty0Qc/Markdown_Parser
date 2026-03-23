package com.mkparser.demo.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mkparser.demo.EventItem
import com.mkparser.demo.EventKind

// ── Colour palette matching demo/web/index.html ───────────────────────────────

private val ColorOpen   = Color(0xFF3FB950)
private val ColorClose  = Color(0xFFF7851A)
private val ColorText   = Color(0xFF58A6FF)
private val ColorModify = Color(0xFFD29922)
private val ColorMuted  = Color(0xFF8B949E)
private val ColorSurface = Color(0xFF161B22)

@Composable
fun EventsPanel(
    events: List<EventItem>,
    modifier: Modifier = Modifier,
) {
    val listState = rememberLazyListState()

    // Auto-scroll to the latest event
    LaunchedEffect(events.size) {
        if (events.isNotEmpty()) listState.animateScrollToItem(events.size - 1)
    }

    LazyColumn(
        state     = listState,
        modifier  = modifier
            .fillMaxSize()
            .background(Color(0xFF0D1117)),
        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
        verticalArrangement = Arrangement.spacedBy(1.dp),
    ) {
        items(events, key = { it.index }) { ev ->
            EventRow(ev)
        }
    }
}

@Composable
private fun EventRow(ev: EventItem) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 1.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        // Index
        Text(
            text       = ev.index.toString(),
            color      = ColorMuted,
            fontSize   = 10.sp,
            fontFamily = FontFamily.Monospace,
            modifier   = Modifier.width(28.dp),
        )

        // Kind badge
        val (kindLabel, kindColor) = when (ev.kind) {
            EventKind.OPEN   -> "OPEN"   to ColorOpen
            EventKind.CLOSE  -> "CLOSE"  to ColorClose
            EventKind.TEXT   -> "TEXT"   to ColorText
            EventKind.MODIFY -> "MODIFY" to ColorModify
        }
        Text(
            text       = kindLabel,
            color      = kindColor,
            fontSize   = 10.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            modifier   = Modifier.width(46.dp),
        )

        // Type name
        Text(
            text       = ev.typeName,
            color      = Color(0xFFC9D1D9),
            fontSize   = 11.sp,
            fontFamily = FontFamily.Monospace,
            modifier   = Modifier.weight(1f),
        )

        // Attrs / text preview
        if (ev.textPreview.isNotEmpty()) {
            Text(
                text       = ev.textPreview,
                color      = ColorMuted,
                fontSize   = 10.sp,
                fontFamily = FontFamily.Monospace,
                maxLines   = 1,
                modifier   = Modifier.weight(2f),
            )
        }
    }
}
