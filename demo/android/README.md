# Android Demo — mk-parser

Jetpack Compose demo app that integrates the `mk_parser` C library via JNI and renders Markdown natively — no WebView required.

## Features

| Tab | Description |
|---|---|
| **Events** | Live parse event stream (OPEN / CLOSE / TEXT / MODIFY) with colour coding |
| **Preview** | Native Compose renderer — `LazyColumn` with stable keys for partial recomposition during streaming |
| **LLM** | Connect to any OpenAI-compatible API (or Ollama) and parse the streamed response in real-time |

## Prerequisites

- Android Studio Hedgehog (2023.1) or newer
- NDK r25 or newer (install via SDK Manager → SDK Tools)
- CMake 3.22+ (install via SDK Manager → SDK Tools)
- For Ollama integration: Ollama running on the host machine

## Open in Android Studio

```
File → Open → demo/android/
```

Android Studio will sync Gradle, download dependencies, and configure the CMake build automatically.

## Build from CLI

```sh
cd demo/android
./gradlew assembleDebug
# Install on connected device / emulator:
./gradlew installDebug
```

## Project layout

```
demo/android/
├── app/
│   ├── src/main/
│   │   ├── cpp/
│   │   │   ├── CMakeLists.txt           Native build (links mk_parser C sources)
│   │   │   └── mk_jni.c                 JNI bridge (nativeCreate/Feed/Finish/Destroy)
│   │   ├── kotlin/com/mkparser/
│   │   │   ├── MkParser.kt              JNI wrapper (from bindings/android/)
│   │   │   └── demo/
│   │   │       ├── MainActivity.kt      Entry point
│   │   │       ├── MkDemoViewModel.kt   State + parse/stream/LLM logic
│   │   │       └── ui/
│   │   │           ├── MainScreen.kt    Tab layout (Events / Preview / LLM)
│   │   │           ├── EventsPanel.kt   Parse event list
│   │   │           ├── LlmPanel.kt      LLM config + generate
│   │   │           ├── render/
│   │   │           │   ├── MkBlock.kt        Sealed class hierarchy for all block types
│   │   │           │   ├── MkBlockParser.kt  Push-event → List<MkBlock> state machine
│   │   │           │   └── MkRenderPanel.kt  Compose LazyColumn renderer
│   │   │           └── theme/Theme.kt   Dark colour scheme (Catppuccin Mocha)
│   │   └── res/
│   └── build.gradle.kts
├── gradle/libs.versions.toml
├── settings.gradle.kts
└── README.md
```

## Native render pipeline

```
MkParser (JNI)
    │  onNodeOpen / onNodeClose / onText
    ▼
MkBlockParser.BlockBuilder          ← push-event state machine
    │  produces List<MkBlock>
    ▼
MkBlock sealed class                ← Heading / Paragraph / FencedCode /
    │                                  BulletItem / OrderedItem / BlockQuote /
    │                                  ThematicBreak / TableBlock
    ▼
MkRenderPanel (LazyColumn)          ← key = { it.id } for stable partial recomposition
    │  streaming cursor (▌ blink)
    ▼
Compose UI
```

### Stable block IDs

Each `MkBlock` carries a stable `id` (`H_0`, `P_1`, `CODE_0`, `LI_0`, …) based on
type + ordinal within the document. `LazyColumn` uses this as a key so only the
last (actively-streaming) block recomposes on each new token.

### Streaming cursor

`MkBlockParser.parse(md, isStreaming = true)` marks the last block with
`isStreaming = true`. `MkRenderPanel` appends a blinking `▌` cursor via
`InfiniteTransition` alpha animation to that block's `AnnotatedString`.

### Inline spans

`MkBlockParser` builds `AnnotatedString` with `SpanStyle` pushes for:

| Syntax | Style |
|---|---|
| `**bold**` | `FontWeight.Bold` |
| `*italic*` | `FontStyle.Italic` |
| `~~strike~~` | `TextDecoration.LineThrough` |
| `` `code` `` | `FontFamily.Monospace` + code colours |
| `[link](url)` | underline + `URL` annotation |

### Task list items

The C parser emits `LIST_ITEM → TASK_LIST_ITEM (leaf) → PARAGRAPH`. The JNI
layer maps `mk_node_task_list_item_checked` to `taskState` (1 = unchecked, 2 = checked).
`MkBlockParser` stores this in `pendingTaskState` and attaches it to the
`BulletItem` block on `LIST_ITEM` close. `MkRenderPanel` renders ☐ / ☑ / • accordingly.

## Colour palette

All colours use **Catppuccin Mocha**:

| Role | Colour |
|---|---|
| Background | `#1E1E2E` |
| Code background | `#181825` |
| Body text | `#CDD6F4` |
| Headings / blockquote | `#CBA6F7` (Mauve) |
| Code text | `#A6E3A1` (Green) |
| Bullet marker | `#FAB387` (Peach) |
| Ordered number | `#F9E2AF` (Yellow) |
| Links | `#89DCEB` (Sky) |

## LLM integration

### Mock (no API key needed)

Select **Mock (offline)** in the LLM tab — uses `MockProvider` which emits a
pre-built Markdown response character-by-character.

### OpenAI

1. Select **OpenAI-compatible**
2. Set Base URL: `https://api.openai.com/v1`
3. Set API Key: `sk-…`
4. Set Model: `gpt-4o-mini`

### Ollama (local model on host machine)

1. Install Ollama and pull a model: `ollama pull llama3.2`
2. Select **OpenAI-compatible**
3. Set Base URL: `http://10.0.2.2:11434/v1`  ← emulator loopback to host
4. Set API Key: `ollama`
5. Set Model: `llama3.2`

### Custom provider

Implement the `LlmProvider` interface:

```kotlin
class MyProvider : LlmProvider {
    override val label = "My backend"
    override fun stream(prompt: String): Flow<String> = flow {
        // emit tokens here
    }
}
```

Then inject it in `MkDemoViewModel.startLlmStream()`.

## How the JNI build works

`app/src/main/cpp/CMakeLists.txt` receives `MK_ROOT` from Gradle and compiles:

```
$MK_ROOT/src/arena.c
$MK_ROOT/src/ast.c
$MK_ROOT/src/parser.c
$MK_ROOT/src/block.c
$MK_ROOT/src/inline_parser.c
$MK_ROOT/src/plugin.c
$MK_ROOT/src/getters.c
app/src/main/cpp/mk_jni.c
```

into `libmk_parser_jni.so` for `arm64-v8a` and `x86_64`.

The demo ships its own `mk_jni.c` (in `app/src/main/cpp/`) rather than the one
in `bindings/android/` so that demo-specific fixes (UTF-8 conversion, task list
state mapping) stay local to the demo project.
