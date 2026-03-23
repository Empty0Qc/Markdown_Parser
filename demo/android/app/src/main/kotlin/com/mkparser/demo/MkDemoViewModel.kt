package com.mkparser.demo

import androidx.compose.runtime.mutableStateListOf
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.mkparser.MkParser
import com.mkparser.NodeType
import com.mkparser.demo.llm.LlmConfig
import com.mkparser.demo.ui.render.MkBlock
import com.mkparser.demo.ui.render.MkBlockParser
import com.mkparser.demo.llm.LlmProvider
import com.mkparser.demo.llm.MockProvider
import com.mkparser.demo.llm.OpenAiProvider
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch

// ── Event model ────────────────────────────────────────────────────────────────

enum class EventKind { OPEN, CLOSE, TEXT, MODIFY }

data class EventItem(
    val index: Int,
    val kind: EventKind,
    val typeName: String,
    val attrs: String = "",
    val textPreview: String = "",
)

// ── UI state ───────────────────────────────────────────────────────────────────

data class DemoUiState(
    val markdown: String         = DEFAULT_MARKDOWN,
    val isStreaming: Boolean     = false,
    val streamChunk: Int         = 0,
    val streamTotal: Int         = 0,
    val llmConfig: LlmConfig     = LlmConfig(),
    val llmPrompt: String        = "Write a short Markdown essay about the Kotlin language.",
    val llmConnected: Boolean    = false,
    val useMock: Boolean         = true,     // toggle real vs mock LLM
    val errorMessage: String?    = null,
)

// ── ViewModel ─────────────────────────────────────────────────────────────────

class MkDemoViewModel : ViewModel() {

    private val _uiState   = MutableStateFlow(DemoUiState())
    val uiState: StateFlow<DemoUiState> = _uiState.asStateFlow()

    private val _renderedBlocks = MutableStateFlow<List<MkBlock>>(emptyList())
    val renderedBlocks: StateFlow<List<MkBlock>> = _renderedBlocks.asStateFlow()

    val events = mutableStateListOf<EventItem>()

    private var streamJob: Job? = null
    private var eventCounter   = 0

    init { parseAll() }

    // ── Markdown editing ───────────────────────────────────────────────────────

    fun onMarkdownChanged(text: String) {
        _uiState.update { it.copy(markdown = text) }
        parseAll(text)
    }

    // ── Parse (batch) ──────────────────────────────────────────────────────────

    fun parseAll(md: String = _uiState.value.markdown) {
        events.clear()
        eventCounter = 0
        val parser = buildParser()
        parser.feed(md).finish().destroy()
        _renderedBlocks.value = MkBlockParser.parse(md)
    }

    // ── Stream simulation ─────────────────────────────────────────────────────

    fun startStream() {
        if (_uiState.value.isStreaming) { stopStream(); return }

        val md = _uiState.value.markdown
        events.clear()
        eventCounter = 0

        _uiState.update { it.copy(isStreaming = true, streamChunk = 0, streamTotal = md.length) }

        streamJob = viewModelScope.launch {
            val parser = buildParser()
            val chunkSize = 7
            md.chunked(chunkSize).forEachIndexed { i, chunk ->
                parser.feed(chunk)
                val taken = md.take((i + 1) * chunkSize)
                _uiState.update { it.copy(streamChunk = (i + 1) * chunkSize) }
                _renderedBlocks.value = MkBlockParser.parse(taken, isStreaming = true)
                delay(40L)
            }
            parser.finish().destroy()
            _renderedBlocks.value = MkBlockParser.parse(md)
            _uiState.update { it.copy(isStreaming = false) }
        }
        streamJob?.invokeOnCompletion { _uiState.update { it.copy(isStreaming = false) } }
    }

    fun stopStream() {
        streamJob?.cancel()
        _uiState.update { it.copy(isStreaming = false) }
    }

    // ── LLM streaming ─────────────────────────────────────────────────────────

    fun updateLlmConfig(config: LlmConfig) = _uiState.update { it.copy(llmConfig = config) }
    fun updateLlmPrompt(prompt: String)    = _uiState.update { it.copy(llmPrompt = prompt) }
    fun toggleMock()                       = _uiState.update { it.copy(useMock = !it.useMock) }

    fun startLlmStream() {
        if (_uiState.value.isStreaming) { stopStream(); return }

        val state = _uiState.value
        val provider: LlmProvider = if (state.useMock) {
            MockProvider()
        } else {
            OpenAiProvider(state.llmConfig)
        }

        events.clear()
        eventCounter = 0
        var accumulated = ""

        _uiState.update { it.copy(isStreaming = true, markdown = "") }
        _renderedBlocks.value = emptyList()

        streamJob = viewModelScope.launch {
            val parser = buildParser()
            provider.stream(state.llmPrompt)
                .catch { e ->
                    _uiState.update { it.copy(errorMessage = e.message, isStreaming = false) }
                }
                .collect { token ->
                    accumulated += token
                    parser.feed(token)
                    _uiState.update { it.copy(markdown = accumulated) }
                    _renderedBlocks.value = MkBlockParser.parse(accumulated, isStreaming = true)
                }
            parser.finish().destroy()
            _renderedBlocks.value = MkBlockParser.parse(accumulated)
            _uiState.update { it.copy(isStreaming = false) }
        }
        streamJob?.invokeOnCompletion { _uiState.update { it.copy(isStreaming = false) } }
    }

    fun dismissError() = _uiState.update { it.copy(errorMessage = null) }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private fun buildParser() = MkParser(
        onNodeOpen  = { type, _, _ ->
            val name = nodeTypeName(type)
            events.add(EventItem(++eventCounter, EventKind.OPEN,  name))
        },
        onNodeClose = { type ->
            val name = nodeTypeName(type)
            events.add(EventItem(++eventCounter, EventKind.CLOSE, name))
        },
        onText = { text ->
            val preview = text.take(60).replace('\n', '↵')
            events.add(EventItem(++eventCounter, EventKind.TEXT, "TEXT",
                textPreview = "\"$preview${if (text.length > 60) "…" else ""}\""))
        },
        onModify = { type ->
            val name = nodeTypeName(type)
            events.add(EventItem(++eventCounter, EventKind.MODIFY, name))
        },
    )

    private fun nodeTypeName(type: Int) = when (type) {
        NodeType.DOCUMENT       -> "DOCUMENT"
        NodeType.HEADING        -> "HEADING"
        NodeType.PARAGRAPH      -> "PARAGRAPH"
        NodeType.CODE_BLOCK     -> "CODE_BLOCK"
        NodeType.BLOCK_QUOTE    -> "BLOCK_QUOTE"
        NodeType.LIST           -> "LIST"
        NodeType.LIST_ITEM      -> "LIST_ITEM"
        NodeType.THEMATIC_BREAK -> "THEMATIC_BREAK"
        NodeType.HTML_BLOCK     -> "HTML_BLOCK"
        NodeType.TABLE          -> "TABLE"
        NodeType.TABLE_HEAD     -> "TABLE_HEAD"
        NodeType.TABLE_ROW      -> "TABLE_ROW"
        NodeType.TABLE_CELL     -> "TABLE_CELL"
        NodeType.TEXT           -> "TEXT"
        NodeType.SOFT_BREAK     -> "SOFT_BREAK"
        NodeType.HARD_BREAK     -> "HARD_BREAK"
        NodeType.EMPHASIS       -> "EMPHASIS"
        NodeType.STRONG         -> "STRONG"
        NodeType.STRIKETHROUGH  -> "STRIKETHROUGH"
        NodeType.INLINE_CODE    -> "INLINE_CODE"
        NodeType.LINK           -> "LINK"
        NodeType.IMAGE          -> "IMAGE"
        NodeType.AUTO_LINK      -> "AUTO_LINK"
        NodeType.HTML_INLINE    -> "HTML_INLINE"
        NodeType.TASK_LIST_ITEM -> "TASK_LIST_ITEM"
        else                    -> "CUSTOM($type)"
    }
}

const val DEFAULT_MARKDOWN = """# mk-parser Demo

Streaming Markdown parser for AI output.

## Features

- Incremental parsing — works char by char
- **Bold**, *italic*, ~~strikethrough~~, `code`
- [Links](https://github.com) and images
- Fenced code blocks:

```kotlin
val parser = MkParser(onText = { print(it) })
parser.feed("# Hello\n").finish()
```

## Task list

- [x] Push API
- [x] Android JNI binding
- [ ] Your next feature

## Table

| Layer    | Status |
|----------|:------:|
| C core   | ✅     |
| JNI      | ✅     |
| Compose  | ✅     |
"""
