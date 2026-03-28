# mk-parser

Incremental Markdown parser designed for AI streaming scenarios. Written in pure C11 with zero global state and a two-segment arena allocator. Emits parse events character-by-character as tokens arrive — no need to buffer the full document.

```
LLM token stream  →  mk_feed()  →  on_node_open / on_text / on_node_close
```

> **New to this project?** Read the [Project Overview](docs/OVERVIEW.md) for a full walkthrough of the architecture, modules, capabilities, and design rationale.

---

## Why mk-parser?

| Problem | How mk-parser solves it |
|---|---|
| LLM streams token-by-token | `mk_feed()` accepts any chunk size, even 1 byte |
| Markdown structure needed before full doc | Push API fires callbacks the moment a block is certain |
| Multiple platforms (web, mobile, server) | WASM · Node.js N-API · Android JNI · iOS Swift |
| Zero-allocation hot paths | Two-segment arena; scratch memory rolled back between blocks |
| Custom syntax (math, alerts…) | `MkParserPlugin` + `MkTransformPlugin` vtables |

---

## Quick start — C

```c
#include "mk_parser.h"

static void on_open (void *ud, MkNode *n)               { printf("open  %s\n", mk_node_type_name(n->type)); }
static void on_close(void *ud, MkNode *n)               { printf("close %s\n", mk_node_type_name(n->type)); }
static void on_text (void *ud, MkNode *n, const char *t, size_t len) { printf("text  %.*s\n", (int)len, t); }

int main(void) {
    MkArena  *arena  = mk_arena_new();
    MkCallbacks cbs  = { .on_node_open = on_open, .on_node_close = on_close, .on_text = on_text };
    MkParser *parser = mk_parser_new(arena, &cbs);

    mk_feed(parser, "# Hello\n\nStreaming **world**.\n", 30);
    mk_finish(parser);

    mk_parser_free(parser);
    mk_arena_free(arena);
}
```

Build:

```sh
cmake -B build && cmake --build build
gcc -Iinclude example.c build/libmk_parser.a -o example
```

---

## Quick start — JavaScript (WASM)

```js
import createMkParser from './mk_parser.js';   // emscripten output

const Module = await createMkParser();

const parser = new MkParser(Module, {
  onNodeOpen (type, node) { console.log('open',  type, node); },
  onNodeClose(type)       { console.log('close', type); },
  onText     (text)       { process.stdout.write(text); },
});

// Simulate LLM streaming
for (const chunk of chunks) parser.feed(chunk);
parser.finish();
parser.destroy();
```

---

## Quick start — Node.js (native addon)

```js
const { MkParser } = require('mk-parser-node');

const p = new MkParser();
p.onNodeOpen  = (type, node) => console.log('open', type, node);
p.onText      = (text)       => process.stdout.write(text);
p.feed('# Streaming\n').finish();
```

---

## Quick start — iOS (Swift)

```swift
import MkParser

let parser = try MkParser(
    onNodeOpen:  { type, node in print("open",  type, node.level ?? "") },
    onNodeClose: { type, _    in print("close", type) },
    onText:      { text       in print("text",  text) }
)
try parser.feed("# Hello\n").finish()
```

Add via Swift Package Manager:

```swift
// Package.swift
.package(url: "…/mk_p", from: "0.1.0")
```

---

## Quick start — Android (Kotlin)

```kotlin
val parser = MkParser(
    onNodeOpen  = { type, flags, attr -> Log.d("mk", "open $type flags=$flags attr=$attr") },
    onNodeClose = { type              -> Log.d("mk", "close $type") },
    onText      = { text             -> Log.d("mk", "text $text") }
)
parser.feed("# Hello\n").finish().destroy()
```

### Android demo — native Compose renderer

The demo app (`demo/android/`) includes a fully native Markdown renderer built
on Jetpack Compose — no WebView. It converts the mk_parser push-event stream
into a `List<MkBlock>` via a state machine (`MkBlockParser`), then renders each
block in a `LazyColumn` keyed by stable IDs for efficient partial recomposition
during streaming.

Supported elements: headings (H1–H6), paragraphs, fenced code blocks with
language label, unordered / ordered lists, task lists (☐ / ☑), block quotes,
thematic breaks, and GFM tables. Inline spans: **bold**, *italic*,
~~strikethrough~~, `code`, and links.

See [`demo/android/README.md`](demo/android/README.md) for full details.

### iOS demo — native SwiftUI renderer

The demo app (`demo/ios/`) is the iOS equivalent of the Android demo: a SwiftUI
application that renders the mk_parser push-event stream in real time.

Same architecture as Android — push events flow into `MkBlockParser`, which
produces a flat `[MkBlock]` array (Swift `enum` with associated values), and
`MkRenderView` renders each block in a SwiftUI `List` keyed by stable IDs.
Streaming cursor animates via a `Timer`-driven opacity toggle.

Open in Xcode: `File → Open → demo/ios/Package.swift`

See [`demo/ios/README.md`](demo/ios/README.md) for full details.

---

## Build

### Prerequisites

| Target | Requirement |
|---|---|
| Native (Linux / macOS) | CMake ≥ 3.22, C11 compiler |
| WASM | Emscripten ≥ 3.1 (`emcc` in PATH) |
| Android | Android NDK r25+, `ANDROID_NDK` env set |
| iOS | Xcode 15+, CMake iOS toolchain |
| Node.js addon | Node.js ≥ 18, `node-gyp` |

### build.sh

```sh
./build.sh native    # → build/native/libmk_parser.{a,so}
./build.sh wasm      # → build/wasm/mk_parser.{js,wasm}
./build.sh android   # → build/android/jniLibs/{arm64-v8a,armeabi-v7a,x86_64}/
./build.sh ios       # → build/ios/MkParser.xcframework
./build.sh bench     # → build native + run multi-scenario throughput benchmark
./build.sh npm-pack  # → build WASM + npm pack --dry-run (validate package)
./build.sh all       # everything above
```

### CMake (manual)

```sh
cmake -B build/native \
      -DCMAKE_BUILD_TYPE=Release \
      -DMK_BUILD_TESTS=ON
cmake --build build/native
ctest --test-dir build/native   # 55 tests
```

---

## Project layout

```
mk_p/
├── include/
│   ├── mk_parser.h          Public C API (single header)
│   └── module.modulemap     Swift C-module bridge
├── src/
│   ├── arena.c / .h         Two-segment bump allocator
│   ├── ast.c / .h           Node structs (25 node types)
│   ├── block.c / .h         Line-by-line block state machine
│   ├── inline_parser.c / .h Inline tokeniser
│   ├── parser.c             mk_parser_new / mk_feed / mk_finish
│   ├── plugin.c / .h        Parser + transform plugin dispatch
│   └── getters.c            Public mk_node_*() attribute accessors
├── bindings/
│   ├── wasm/
│   │   ├── mk_wasm.c        Emscripten C glue (EM_ASM callbacks)
│   │   └── mk_wasm_api.js   MkParser JS class (MODULARIZE build)
│   ├── node/
│   │   ├── mk_napi.c        Node-API (stable ABI) native addon
│   │   ├── binding.gyp      node-gyp build
│   │   └── index.js         Entry point
│   ├── js/
│   │   ├── types.ts         TypeScript type definitions (all 25 nodes)
│   │   └── mk_parser_wasm.js WasmMkParser class + parseToAST()
│   ├── android/
│   │   ├── mk_jni.c         JNI bridge (nativeCreate/Feed/Finish/Destroy)
│   │   ├── MkParser.kt      Kotlin wrapper with lambda callbacks
│   │   └── CMakeLists.txt   NDK SHARED lib (arm64 / x86_64)
│   └── ios/
│       ├── MkParser.swift   Swift wrapper (MkNodeInfo, MkParser class)
│       ├── build_xcframework.sh  lipo + xcodebuild packaging script
│       └── Tests/           XCTest suite
├── tests/
│   ├── unit/
│   │   ├── test_arena.c     11 tests
│   │   ├── test_ast.c       13 tests
│   │   ├── test_block.c     15 tests
│   │   └── test_inline.c    18 tests
│   └── integration/
│       └── test_streaming.c 10 tests (byte-by-byte, chunk, Pull API)
├── demo/
│   ├── web/index.html       Zero-dependency live demo (open in browser)
│   └── android/             Jetpack Compose demo with native Markdown renderer
├── cmake/
│   ├── options.cmake
│   └── toolchains/          wasm / android / ios toolchain files
├── Package.swift            Swift Package Manager config
├── build.sh                 Multi-platform build script
└── PROGRESS.md              Milestone tracker
```

---

## API reference

### Arena

```c
MkArena *mk_arena_new(void);
MkArena *mk_arena_new_custom(MkAllocFn alloc, MkFreeFn free, void *ctx);
void     mk_arena_free(MkArena *);
void     mk_arena_reset_scratch(MkArena *);   // rolls back scratch segment
```

The arena has two independent allocation segments:

- **stable** — lives until `mk_arena_free`. Parser nodes, strings, committed text.
- **scratch** — rolled back between blocks. Temporary line buffers, parse state.

### Parser lifecycle

```c
MkParser *mk_parser_new(MkArena *, const MkCallbacks *);
void      mk_parser_free(MkParser *);
int       mk_feed  (MkParser *, const char *data, size_t len);  // 0 = ok
int       mk_finish(MkParser *);                                // flush pending nodes
```

### Push API — callbacks

```c
typedef struct MkCallbacks {
    void *user_data;
    void (*on_node_open)  (void *ud, MkNode *node);
    void (*on_node_close) (void *ud, MkNode *node);
    void (*on_text)       (void *ud, MkNode *node, const char *text, size_t len);
    void (*on_node_modify)(void *ud, MkNode *node);  // e.g. paragraph → table
} MkCallbacks;
```

`on_node_modify` fires when a block is *promoted* — a paragraph that turns into
a table once the separator row arrives, or an ATX heading upgraded to setext.

### Pull API — delta queue

```c
MkDelta *mk_pull_delta(MkParser *);   // NULL when queue is empty
void     mk_delta_free(MkDelta *);

// MkDelta fields:
//   .type     MK_DELTA_NODE_OPEN | CLOSE | TEXT | MODIFY
//   .node     affected node
//   .text     (TEXT only) arena-owned slice
//   .text_len
```

### Node types

| Category | Types |
|---|---|
| Block (13) | DOCUMENT · HEADING · PARAGRAPH · CODE_BLOCK · BLOCK_QUOTE · LIST · LIST_ITEM · THEMATIC_BREAK · HTML_BLOCK · TABLE · TABLE_HEAD · TABLE_ROW · TABLE_CELL |
| Inline (12) | TEXT · SOFT_BREAK · HARD_BREAK · EMPHASIS · STRONG · STRIKETHROUGH · INLINE_CODE · LINK · IMAGE · AUTO_LINK · HTML_INLINE · TASK_LIST_ITEM |
| Custom | MK_NODE_CUSTOM (0x1000+) — reserved for plugins |

### Attribute getters

Every node attribute has a typed getter that returns a safe default when
the node is `NULL` or the wrong type:

```c
int         mk_node_heading_level(const MkNode *);          // 1–6
const char *mk_node_code_lang    (const MkNode *);          // NULL if none
int         mk_node_code_fenced  (const MkNode *);
int         mk_node_list_ordered (const MkNode *);
MkTaskState mk_node_list_item_task_state(const MkNode *);
size_t      mk_node_table_col_count(const MkNode *);
MkAlign     mk_node_table_col_align(const MkNode *, size_t col);
const char *mk_node_link_href    (const MkNode *);
const char *mk_node_image_src    (const MkNode *);
int         mk_node_autolink_is_email(const MkNode *);
// … (full list in include/mk_parser.h)
```

### Plugin system

```c
// Parser plugin — intercepts block and inline parsing
typedef struct MkParserPlugin {
    const char *name;
    const char *inline_triggers;          // fast-path character gate, e.g. "$"
    size_t (*try_block) (MkParser *, MkArena *, const char *src, size_t, MkNode **out);
    size_t (*try_inline)(MkParser *, MkArena *, const char *src, size_t, MkNode **out);
} MkParserPlugin;

// Transform plugin — post-close hook on any completed node
typedef struct MkTransformPlugin {
    const char *name;
    void (*on_node_complete)(MkNode *, MkArena *);
} MkTransformPlugin;

mk_register_parser_plugin   (parser, &my_plugin);
mk_register_transform_plugin(parser, &my_transform);
```

---

## Node tree

Even in streaming mode the parser builds a live in-memory AST:

```c
MkNode *root = mk_get_root(parser);  // always valid after mk_parser_new

// Walk the tree
void walk(MkNode *n, int depth) {
    printf("%*s%s\n", depth*2, "", mk_node_type_name(n->type));
    for (MkNode *c = n->first_child; c; c = c->next_sibling)
        walk(c, depth + 1);
}
```

---

## Web demo

Open `demo/web/index.html` in any modern browser — no build step required.

- **Events tab** — live open / close / text / modify event stream
- **AST tab** — interactive colour-coded node tree
- **Preview tab** — rendered HTML with streaming typewriter effect
- Speed slider controls chunk delay (1–100 ms)
- Five built-in GFM samples; edit the left pane to parse arbitrary Markdown

---

## Tests

```sh
cmake -B build -DMK_BUILD_TESTS=ON && cmake --build build
ctest --test-dir build --output-on-failure
```

| Suite | File | Tests |
|---|---|---|
| Arena | tests/unit/test_arena.c | 11 |
| AST | tests/unit/test_ast.c | 13 |
| Block | tests/unit/test_block.c | 15 |
| Inline | tests/unit/test_inline.c | 18 |
| Streaming | tests/integration/test_streaming.c | 10 |
| **Total** | | **67** |

---

## Milestone status

| Milestone | Status |
|---|---|
| M1 Project skeleton & build system | ✅ |
| M2 Arena allocator | ✅ |
| M3 AST data structures | ✅ |
| M4 Block parser | ✅ |
| M5 Inline parser | ✅ |
| M6 Push / Pull dual API | ✅ |
| M7 Plugin system | ✅ |
| Getter API | ✅ |
| M8a WASM binding | ✅ |
| M8b Node.js N-API binding | ✅ |
| M8c Android JNI binding | ✅ |
| M8d iOS Swift binding | ✅ |
| M9 JS/TypeScript wrapper | ✅ |
| M10 Web demo | ✅ |
| M11 C unit tests | ✅ |
| M12 Android native Compose renderer | ✅ |

---

## License

TBD
