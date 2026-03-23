package com.mkparser.demo.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.mkparser.demo.MkDemoViewModel
import com.mkparser.demo.llm.LlmConfig

/**
 * LLM panel — configure the backend and start streaming.
 *
 * Supports two modes:
 *   1. Mock — no network, uses MockProvider for offline testing.
 *   2. OpenAI-compatible — any endpoint that speaks the /chat/completions SSE
 *      protocol (OpenAI, Ollama, LM Studio, vLLM, llama.cpp, …).
 *
 * For local Ollama on a device/emulator:
 *   Base URL: http://10.0.2.2:11434/v1   (emulator loopback to host)
 *   API Key:  ollama
 *   Model:    llama3.2  (or any model you've pulled)
 */
@Composable
fun LlmPanel(
    viewModel: MkDemoViewModel,
    modifier: Modifier = Modifier,
) {
    val uiState by viewModel.uiState.collectAsState()
    val cfg = uiState.llmConfig

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {

        // ── Mode toggle ───────────────────────────────────────────────────────
        Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
            Text("Backend:", style = MaterialTheme.typography.labelMedium)
            Spacer(Modifier.width(12.dp))
            FilterChip(
                selected = uiState.useMock,
                onClick  = { if (!uiState.useMock) viewModel.toggleMock() },
                label    = { Text("Mock (offline)") },
            )
            Spacer(Modifier.width(8.dp))
            FilterChip(
                selected = !uiState.useMock,
                onClick  = { if (uiState.useMock) viewModel.toggleMock() },
                label    = { Text("OpenAI-compatible") },
            )
        }

        // ── OpenAI config fields (hidden in mock mode) ────────────────────────
        if (!uiState.useMock) {
            OutlinedTextField(
                value         = cfg.baseUrl,
                onValueChange = { viewModel.updateLlmConfig(cfg.copy(baseUrl = it)) },
                label         = { Text("Base URL") },
                placeholder   = { Text("https://api.openai.com/v1") },
                singleLine    = true,
                modifier      = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value         = cfg.apiKey,
                onValueChange = { viewModel.updateLlmConfig(cfg.copy(apiKey = it)) },
                label         = { Text("API Key") },
                placeholder   = { Text("sk-…  (or 'ollama' for Ollama)") },
                singleLine    = true,
                modifier      = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value         = cfg.model,
                onValueChange = { viewModel.updateLlmConfig(cfg.copy(model = it)) },
                label         = { Text("Model") },
                placeholder   = { Text("gpt-4o-mini  /  llama3.2  /  …") },
                singleLine    = true,
                modifier      = Modifier.fillMaxWidth(),
            )

            // Hint for Ollama users
            Text(
                text  = "Tip — Ollama on host machine: use http://10.0.2.2:11434/v1 from emulator.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }

        HorizontalDivider()

        // ── Prompt ────────────────────────────────────────────────────────────
        OutlinedTextField(
            value         = uiState.llmPrompt,
            onValueChange = viewModel::updateLlmPrompt,
            label         = { Text("Prompt") },
            minLines      = 3,
            modifier      = Modifier.fillMaxWidth(),
        )

        // ── Start / Stop ──────────────────────────────────────────────────────
        Button(
            onClick  = viewModel::startLlmStream,
            modifier = Modifier.fillMaxWidth(),
            colors   = if (uiState.isStreaming)
                           ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
                       else ButtonDefaults.buttonColors(),
        ) {
            Text(if (uiState.isStreaming) "■  Stop" else "▶  Generate & Parse")
        }

        // ── Error banner ──────────────────────────────────────────────────────
        uiState.errorMessage?.let { msg ->
            Card(colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.errorContainer,
            )) {
                Row(
                    modifier = Modifier.padding(12.dp).fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text(msg, style = MaterialTheme.typography.bodySmall,
                         modifier = Modifier.weight(1f))
                    TextButton(onClick = viewModel::dismissError) { Text("Dismiss") }
                }
            }
        }
    }
}
