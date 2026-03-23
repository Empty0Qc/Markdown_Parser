package com.mkparser.demo.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.mkparser.demo.MkDemoViewModel
import com.mkparser.demo.ui.render.MkRenderPanel

// ── Tab definitions ────────────────────────────────────────────────────────────

private enum class DemoTab(val label: String) {
    EVENTS("Events"),
    PREVIEW("Preview"),
    LLM("LLM"),
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(vm: MkDemoViewModel = viewModel()) {
    val uiState    by vm.uiState.collectAsState()
    val events     = vm.events
    val renderedBlocks by vm.renderedBlocks.collectAsState()
    var activeTab  by remember { mutableStateOf(DemoTab.EVENTS) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("mk-parser demo") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surfaceContainer,
                ),
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding),
        ) {

            // ── Markdown input ────────────────────────────────────────────────
            OutlinedTextField(
                value         = uiState.markdown,
                onValueChange = vm::onMarkdownChanged,
                label         = { Text("Markdown input") },
                modifier      = Modifier
                    .fillMaxWidth()
                    .heightIn(min = 100.dp, max = 200.dp)
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                textStyle     = MaterialTheme.typography.bodySmall.copy(
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                ),
            )

            // ── Action bar ────────────────────────────────────────────────────
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Button(
                    onClick = vm::startStream,
                    colors  = if (uiState.isStreaming)
                                  ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
                              else ButtonDefaults.buttonColors(),
                ) {
                    Text(if (uiState.isStreaming) "■ Stop" else "▶ Stream")
                }
                OutlinedButton(onClick = { vm.parseAll() }) { Text("Parse All") }
                OutlinedButton(onClick = { vm.onMarkdownChanged("") }) { Text("Clear") }

                Spacer(Modifier.weight(1f))

                // Stream progress
                if (uiState.isStreaming && uiState.streamTotal > 0) {
                    val progress = uiState.streamChunk.toFloat() / uiState.streamTotal
                    LinearProgressIndicator(
                        progress  = { progress },
                        modifier  = Modifier.width(80.dp).height(4.dp),
                    )
                }
            }

            Spacer(Modifier.height(4.dp))

            // ── Tab bar ───────────────────────────────────────────────────────
            TabRow(selectedTabIndex = activeTab.ordinal) {
                DemoTab.entries.forEach { tab ->
                    Tab(
                        selected = activeTab == tab,
                        onClick  = { activeTab = tab },
                        text     = { Text(tab.label) },
                    )
                }
            }

            // ── Tab content ───────────────────────────────────────────────────
            Box(modifier = Modifier.weight(1f)) {
                when (activeTab) {
                    DemoTab.EVENTS ->
                        EventsPanel(events = events, modifier = Modifier.fillMaxSize())

                    DemoTab.PREVIEW ->
                        MkRenderPanel(
                            blocks   = renderedBlocks,
                            modifier = Modifier.fillMaxSize(),
                        )

                    DemoTab.LLM ->
                        LlmPanel(viewModel = vm, modifier = Modifier.fillMaxSize())
                }
            }
        }
    }
}
