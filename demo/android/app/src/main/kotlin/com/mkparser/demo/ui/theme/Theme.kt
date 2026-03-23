package com.mkparser.demo.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val DarkColorScheme = darkColorScheme(
    primary         = Color(0xFF58A6FF),
    secondary       = Color(0xFF3FB950),
    tertiary        = Color(0xFFD29922),
    background      = Color(0xFF0D1117),
    surface         = Color(0xFF161B22),
    surfaceContainer= Color(0xFF21262D),
    onBackground    = Color(0xFFC9D1D9),
    onSurface       = Color(0xFFC9D1D9),
)

@Composable
fun MkParserDemoTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        content     = content,
    )
}
