# mk-parser iOS Demo

SwiftUI demo app for [mk-parser](../../README.md) — the incremental Markdown parser for AI streaming scenarios.

## Requirements

| Tool    | Version |
|---------|---------|
| Xcode   | 15+     |
| iOS     | 16+     |
| macOS   | 13+ (Catalyst) |
| Swift   | 5.9+    |

## Opening in Xcode

```
File → Open → demo/ios/Package.swift
```

Xcode will resolve the `MkParser` local package dependency from the project root automatically.

## Features

| Tab     | Description |
|---------|-------------|
| Events  | Live OPEN / CLOSE / TEXT / MODIFY event stream from the parser |
| Preview | Rendered Markdown blocks (Catppuccin Mocha theme) |
| LLM     | Stream from a real OpenAI-compatible API or an offline mock |

## Architecture

```
App/
├── MkParserDemoApp.swift     @main entry point
├── ContentView.swift         Input field + action bar + TabView
├── DemoViewModel.swift       ObservableObject state + parsing logic
├── EventsView.swift          Colour-coded event list
├── render/
│   ├── MkBlock.swift         Swift enum with associated values (≈ Kotlin sealed class)
│   ├── MkBlockParser.swift   Push-event → [MkBlock] state machine
│   └── MkRenderView.swift    SwiftUI List renderer
└── llm/
    ├── LlmProvider.swift     Protocol + LlmConfig
    ├── MockProvider.swift    Offline simulation (no API key needed)
    └── OpenAIProvider.swift  URLSession async/await SSE streaming
```

## Streaming pipeline

```
User input / LLM token
        ↓
   MkParser.feed()           (C core push events)
        ↓
   DemoViewModel              accumulates text
        ↓
   MkBlockParser.parse()      state machine → [MkBlock]
        ↓
   MkRenderView               List { blockRow($block) }
```

## Running the LLM tab

1. Toggle off "Use Mock Provider"
2. Enter your OpenAI API key
3. Type a prompt and tap **▶ Start LLM Stream**

The mock provider requires no API key and works fully offline.
