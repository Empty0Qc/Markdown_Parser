# mk-parser

专为 AI 流式输出场景设计的增量 Markdown 解析器。纯 C11 编写，无全局状态，基于双段 Arena 分配器。Token 到达即触发解析事件，无需缓冲完整文档。

```
LLM token 流  →  mk_feed()  →  on_node_open / on_text / on_node_close
```

> **第一次接触本项目？** 阅读 [架构总览](docs/ARCHITECTURE.md)，系统了解架构设计、各模块职责、对外能力与核心优势。

---

## 为什么选 mk-parser？

| 痛点 | mk-parser 的解法 |
|---|---|
| LLM 逐 token 输出 | `mk_feed()` 接受任意大小的 chunk，哪怕 1 字节 |
| 文档未结束就需要结构信息 | Push API 在确认块结构的瞬间立即触发回调 |
| 需要同时支持多个平台 | WASM · Node.js N-API · Android JNI · iOS Swift |
| 热路径零分配 | 双段 Arena；scratch 段在块间自动回滚 |
| 自定义语法（数学公式、Alert 块…） | `MkParserPlugin` + `MkTransformPlugin` 插件 vtable |

---

## 快速上手 — C

```c
#include "mk_parser.h"

static void on_open (void *ud, MkNode *n)               { printf("open  %s\n", mk_node_type_name(n->type)); }
static void on_close(void *ud, MkNode *n)               { printf("close %s\n", mk_node_type_name(n->type)); }
static void on_text (void *ud, MkNode *n, const char *t, size_t len) { printf("text  %.*s\n", (int)len, t); }

int main(void) {
    MkArena  *arena  = mk_arena_new();
    MkCallbacks cbs  = { .on_node_open = on_open, .on_node_close = on_close, .on_text = on_text };
    MkParser *parser = mk_parser_new(arena, &cbs);

    mk_feed(parser, "# 你好\n\n流式 **世界**。\n", 30);
    mk_finish(parser);

    mk_parser_free(parser);
    mk_arena_free(arena);
}
```

编译：

```sh
cmake -B build && cmake --build build
gcc -Iinclude example.c build/libmk_parser.a -o example
```

---

## 快速上手 — JavaScript（WASM）

```js
import createMkParser from './mk_parser.js';  // Emscripten 构建产物

const Module = await createMkParser();

const parser = new MkParser(Module, {
  onNodeOpen (type, node) { console.log('open',  type, node); },
  onNodeClose(type)       { console.log('close', type); },
  onText     (text)       { process.stdout.write(text); },
});

// 模拟 LLM 流式输出
for (const chunk of chunks) parser.feed(chunk);
parser.finish();
parser.destroy();
```

---

## 快速上手 — Node.js（原生插件）

```js
const { MkParser } = require('mk-parser-node');

const p = new MkParser();
p.onNodeOpen  = (type, node) => console.log('open', type, node);
p.onText      = (text)       => process.stdout.write(text);
p.feed('# 流式解析\n').finish();
```

---

## 快速上手 — iOS（Swift）

```swift
import MkParser

let parser = try MkParser(
    onNodeOpen:  { type, node in print("open",  type, node.level ?? "") },
    onNodeClose: { type, _    in print("close", type) },
    onText:      { text       in print("text",  text) }
)
try parser.feed("# 你好\n").finish()
```

通过 Swift Package Manager 引入：

```swift
// Package.swift
.package(url: "…/mk_p", from: "0.1.0")
```

---

## 快速上手 — Android（Kotlin）

```kotlin
val parser = MkParser(
    onNodeOpen  = { type, flags, attr -> Log.d("mk", "open $type flags=$flags attr=$attr") },
    onNodeClose = { type              -> Log.d("mk", "close $type") },
    onText      = { text             -> Log.d("mk", "text $text") }
)
parser.feed("# 你好\n").finish().destroy()
```

### Android Demo — 原生 Compose 渲染器

演示应用（`demo/android/`）内置了一个完全基于 Jetpack Compose 的原生 Markdown 渲染器，无需 WebView。它通过状态机（`MkBlockParser`）将 mk_parser 推送事件流转换为 `List<MkBlock>`，再用 `LazyColumn`（key 为稳定 ID）高效渲染每个块，流式过程中仅重组最后一个变化的块。

支持的元素：标题（H1–H6）、段落、带语言标签的围栏代码块、无序/有序列表、任务列表（☐ / ☑）、引用块、分隔线、GFM 表格。行内样式：**粗体**、*斜体*、~~删除线~~、`代码`、链接。

详见 [`demo/android/README.md`](demo/android/README.md)。

### iOS Demo — 原生 SwiftUI 渲染器

演示应用（`demo/ios/`）是 Android Demo 的 iOS 对等版：一个 SwiftUI 应用，实时渲染 mk_parser 推送事件流。

架构与 Android 版相同——push 事件流入 `MkBlockParser`，产出扁平的 `[MkBlock]` 数组（Swift `enum` + 关联值），`MkRenderView` 在 SwiftUI `List`（key 为稳定 ID）中渲染每个块。流式光标通过 `Timer` 驱动的透明度切换实现动画效果。支持 OpenAI API 和内置 Mock provider。

Xcode 打开方式：`File → Open → demo/ios/Package.swift`

详见 [`demo/ios/README.md`](demo/ios/README.md)。

---

## 构建

### 环境依赖

| 目标平台 | 依赖 |
|---|---|
| 原生（Linux / macOS） | CMake ≥ 3.22、C11 编译器 |
| WASM | Emscripten ≥ 3.1（`emcc` 在 PATH 中） |
| Android | Android NDK r25+，设置 `ANDROID_NDK` 环境变量 |
| iOS | Xcode 15+，CMake iOS 工具链 |
| Node.js 插件 | Node.js ≥ 18、`node-gyp` |

### 一键构建脚本

```sh
./build.sh native    # → build/native/libmk_parser.{a,so}
./build.sh wasm      # → build/wasm/mk_parser.{js,wasm}
./build.sh android   # → build/android/jniLibs/{arm64-v8a,armeabi-v7a,x86_64}/
./build.sh ios       # → build/ios/MkParser.xcframework
./build.sh bench     # → 构建 native + 运行多场景吞吐量 benchmark
./build.sh npm-pack  # → 构建 WASM + npm pack --dry-run（验证 npm 包）
./build.sh all       # 以上全部
```

### CMake 手动构建

```sh
cmake -B build/native \
      -DCMAKE_BUILD_TYPE=Release \
      -DMK_BUILD_TESTS=ON
cmake --build build/native
ctest --test-dir build/native   # 运行全部 67 个测试
```

---

## 项目结构

```
mk_p/
├── include/
│   ├── mk_parser.h          对外公开 C API（单头文件）
│   └── module.modulemap     Swift C 模块桥接声明
├── src/
│   ├── arena.c / .h         双段 bump 分配器（stable + scratch）
│   ├── ast.c / .h           节点结构体（25 种节点类型）
│   ├── block.c / .h         逐行块状态机
│   ├── inline_parser.c / .h 行内 token 化器
│   ├── parser.c             mk_parser_new / mk_feed / mk_finish
│   ├── plugin.c / .h        解析插件与变换插件调度
│   └── getters.c            公开节点属性访问函数（mk_node_*）
├── bindings/
│   ├── wasm/
│   │   ├── mk_wasm.c        Emscripten C 胶水层（EM_ASM 回调）
│   │   └── mk_wasm_api.js   JS 封装类 MkParser（MODULARIZE 构建）
│   ├── node/
│   │   ├── mk_napi.c        Node-API 原生插件（稳定 ABI）
│   │   ├── binding.gyp      node-gyp 构建配置
│   │   └── index.js         入口文件
│   ├── js/
│   │   ├── types.d.ts       TypeScript 完整类型声明（25 种节点）
│   │   ├── mk_parser_cjs.cjs CJS 封装
│   │   └── mk_parser_wasm.js  WasmMkParser 类 + parseToAST()
│   ├── android/
│   │   ├── mk_jni.c         JNI 桥接（nativeCreate/Feed/Finish/Destroy）
│   │   ├── MkParser.kt      Kotlin 封装类（Lambda 回调）
│   │   ├── lib/             独立 Android 库工程（Gradle）
│   │   └── CMakeLists.txt   NDK SHARED 库（arm64 / x86_64）
│   └── ios/
│       ├── MkParser.swift   Swift 封装（MkNodeInfo 结构体 + MkParser 类）
│       ├── build_xcframework.sh  lipo + xcodebuild 打包脚本
│       └── Tests/           XCTest 测试套件
├── tests/
│   ├── unit/
│   │   ├── test_arena.c     11 个测试
│   │   ├── test_ast.c       13 个测试
│   │   ├── test_block.c     15 个测试
│   │   └── test_inline.c    18 个测试
│   ├── integration/
│   │   └── test_streaming.c 10 个测试（逐字节 / 分块 / Pull API）
│   └── spec/
│       ├── spec.json        CommonMark 规范测试用例
│       ├── mk_html.c / .h   规范合规性 HTML 渲染器
│       └── test_spec.c      规范驱动测试运行器
├── demo/
│   ├── web/index.html       零依赖在线 Demo（直接浏览器打开）
│   ├── android/             Jetpack Compose 演示应用（含原生 Markdown 渲染器）
│   └── ios/                 SwiftUI 演示应用（含 OpenAI / Mock 流式 provider）
├── docs/
│   ├── ARCHITECTURE.md      架构总览、模块说明、设计决策
│   └── AGENT_CONTEXT.md     AI 协作开发上下文
├── cmake/
│   ├── options.cmake        构建选项
│   └── toolchains/          wasm / android / ios 工具链文件
├── Package.swift            Swift Package Manager 配置
└── build.sh                 多平台构建脚本
```

---

## API 速查

### Arena 内存管理

```c
MkArena *mk_arena_new(void);
MkArena *mk_arena_new_custom(MkAllocFn alloc, MkFreeFn free, void *ctx);
void     mk_arena_free(MkArena *);
void     mk_arena_reset_scratch(MkArena *);   // 回滚 scratch 段
```

Arena 包含两段独立的分配区：

- **stable 段**：生命周期到 `mk_arena_free`。存放节点、字符串、已提交文本。
- **scratch 段**：块间自动回滚。存放行缓冲、临时解析状态。

### 解析器生命周期

```c
MkParser *mk_parser_new(MkArena *, const MkCallbacks *);
void      mk_parser_free(MkParser *);
int       mk_feed  (MkParser *, const char *data, size_t len);  // 返回 0 表示成功
int       mk_finish(MkParser *);                                // 刷新所有待定节点
```

### Push API — 回调

```c
typedef struct MkCallbacks {
    void *user_data;
    void (*on_node_open)  (void *ud, MkNode *node);
    void (*on_node_close) (void *ud, MkNode *node);
    void (*on_text)       (void *ud, MkNode *node, const char *text, size_t len);
    void (*on_node_modify)(void *ud, MkNode *node);  // 节点晋升，如段落→表格
} MkCallbacks;
```

`on_node_modify` 在**节点晋升**时触发：例如，解析到分隔行后段落升级为表格，或识别到 setext 标记后提升为标题。

### Pull API — Delta 队列

```c
MkDelta *mk_pull_delta(MkParser *);   // 队列为空时返回 NULL
void     mk_delta_free(MkDelta *);

// MkDelta 字段：
//   .type     MK_DELTA_NODE_OPEN | CLOSE | TEXT | MODIFY
//   .node     受影响的节点
//   .text     （仅 TEXT 类型）Arena 拥有的文本切片
//   .text_len
```

### 节点类型

| 分类 | 类型 |
|---|---|
| 块级（13 种） | DOCUMENT · HEADING · PARAGRAPH · CODE_BLOCK · BLOCK_QUOTE · LIST · LIST_ITEM · THEMATIC_BREAK · HTML_BLOCK · TABLE · TABLE_HEAD · TABLE_ROW · TABLE_CELL |
| 行内（12 种） | TEXT · SOFT_BREAK · HARD_BREAK · EMPHASIS · STRONG · STRIKETHROUGH · INLINE_CODE · LINK · IMAGE · AUTO_LINK · HTML_INLINE · TASK_LIST_ITEM |
| 自定义 | MK_NODE_CUSTOM（0x1000 起）— 插件专用 |

### 节点属性访问函数

所有 getter 在传入 `NULL` 或类型不匹配时均返回安全默认值：

```c
int         mk_node_heading_level(const MkNode *);         // 1–6
const char *mk_node_code_lang    (const MkNode *);         // 无 info string 时返回 NULL
int         mk_node_code_fenced  (const MkNode *);         // 1=围栏式，0=缩进式
int         mk_node_list_ordered (const MkNode *);
MkTaskState mk_node_list_item_task_state(const MkNode *);  // NONE / UNCHECKED / CHECKED
size_t      mk_node_table_col_count(const MkNode *);
MkAlign     mk_node_table_col_align(const MkNode *, size_t col);
const char *mk_node_link_href    (const MkNode *);
const char *mk_node_image_src    (const MkNode *);
int         mk_node_autolink_is_email(const MkNode *);
// … 完整列表见 include/mk_parser.h
```

### 插件系统

```c
/* 解析插件 — 拦截块或行内解析 */
typedef struct MkParserPlugin {
    const char *name;
    const char *inline_triggers;    // 触发字符快速门，如 "$" 用于数学公式
    size_t (*try_block) (MkParser *, MkArena *, const char *src, size_t, MkNode **out);
    size_t (*try_inline)(MkParser *, MkArena *, const char *src, size_t, MkNode **out);
} MkParserPlugin;

/* 变换插件 — 节点关闭后的后处理钩子 */
typedef struct MkTransformPlugin {
    const char *name;
    void (*on_node_complete)(MkNode *, MkArena *);
} MkTransformPlugin;

mk_register_parser_plugin   (parser, &my_plugin);
mk_register_transform_plugin(parser, &my_transform);
```

---

## 节点树遍历

即使在流式模式下，解析器也会实时构建内存中的 AST：

```c
MkNode *root = mk_get_root(parser);   // mk_parser_new 后始终有效

void walk(MkNode *n, int depth) {
    printf("%*s%s\n", depth * 2, "", mk_node_type_name(n->type));
    for (MkNode *c = n->first_child; c; c = c->next_sibling)
        walk(c, depth + 1);
}

walk(root, 0);
```

---

## Web Demo

直接用浏览器打开 `demo/web/index.html`，无需任何构建步骤。

- **Events 面板** — 实时显示 open / close / text / modify 事件流，颜色区分事件类型
- **AST 面板** — 可交互的彩色节点树，显示 level / lang / href 等属性
- **Preview 面板** — 渲染后的 HTML，流式模式下有打字机动效和闪烁光标
- Speed 滑块控制每个 chunk 的延迟（1–100 ms），慢速下可直观感受增量解析
- 内置 5 个 GFM 样本；左侧编辑区支持实时输入任意 Markdown
- 内置 **LLM 配置区**：支持 mock/offline 与 OpenAI-compatible 流式输出
- 支持 **A/B 对比模式**：双栏预览、分栏性能指标、可选同步滚动
- 支持 **故障注入**（确定性 + 概率模式），用于验证流式链路鲁棒性
- 支持 **Fuzz 回归链路**：基线对比、JSON 导出/导入、按 run 回放
- 点击事件行或 AST 节点可高亮定位左侧编辑器中的对应源码区间
- 性能指标包含：tokens、TTFT、tokens/s、平均 tick 延迟、总耗时
- 支持界面 **中英文切换（EN / 中文）**；事件负载与 AST 节点类型保持原始内容

---

## 测试

```sh
cmake -B build -DMK_BUILD_TESTS=ON && cmake --build build
ctest --test-dir build --output-on-failure
```

| 测试套件 | 文件 | 数量 |
|---|---|---|
| Arena | tests/unit/test_arena.c | 11 |
| AST | tests/unit/test_ast.c | 13 |
| Block | tests/unit/test_block.c | 15 |
| Inline | tests/unit/test_inline.c | 18 |
| 流式集成 | tests/integration/test_streaming.c | 10 |
| CommonMark 规范 | tests/spec/test_spec.c | spec.json |
| **合计（单元+集成）** | | **67** |

---

## 里程碑状态

| 里程碑 | 状态 |
|---|---|
| M1 项目骨架与构建系统 | ✅ |
| M2 Arena 分配器 | ✅ |
| M3 AST 数据结构 | ✅ |
| M4 Block 解析器 | ✅ |
| M5 Inline 解析器 | ✅ |
| M6 Push / Pull 双 API | ✅ |
| M7 插件系统 | ✅ |
| Getter API（节点属性访问层） | ✅ |
| M8a WASM 绑定 | ✅ |
| M8b Node.js N-API 绑定 | ✅ |
| M8c Android JNI 绑定 | ✅ |
| M8d iOS Swift 绑定 | ✅ |
| M9 JS / TypeScript 包装层 | ✅ |
| M10 Web Demo | ✅ |
| M11 C 单元测试（67 个） | ✅ |
| M12 Android 原生 Compose 渲染器 | ✅ |
| M12 iOS 原生 SwiftUI 渲染器 | ✅ |

---

## License

待定
