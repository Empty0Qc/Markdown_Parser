# mk-parser — 项目全览

> 本文档面向新接手成员或外部贡献者，系统介绍 mk-parser 的整体架构、各模块职责、对外能力与核心优势。

---

## 一句话定位

**专为 LLM 流式输出场景设计的增量 Markdown 解析器。** C11 内核，零依赖，一份代码跑在 Web / Node.js / Android / iOS 四个平台。

```
LLM token 流  →  mk_feed()  →  on_node_open / on_text / on_node_close
```

---

## 整体分层架构

```
┌─────────────────────────────────────────────────────┐
│                    应用层（demo）                     │
│   demo/web/         demo/android/                   │
├─────────────────────────────────────────────────────┤
│                   语言绑定层（bindings）               │
│   WASM   Node.js/N-API   Android/JNI   iOS/Swift    │
├─────────────────────────────────────────────────────┤
│                    C 核心库（src）                    │
│   arena  ast  block  inline_parser  plugin  getters │
├─────────────────────────────────────────────────────┤
│                   公开头文件（include）                │
│              include/mk_parser.h（260 行）            │
└─────────────────────────────────────────────────────┘
```

---

## 第一层：C 核心库（src/，共 ~3000 行）

这是整个项目的心脏，7 个模块：

| 文件 | 行数 | 职责 |
|---|---|---|
| `arena.c/h` | 264 | 双段 bump 分配器。stable 段存活到销毁，scratch 段块间自动回滚，热路径零 malloc |
| `ast.c/h` | 370 | 25 种节点的结构定义（13 块级 + 12 行内），树操作 API（append/detach/traverse），Delta 类型 |
| `block.c/h` | 1106 | 逐行块状态机，最重的模块。处理标题/段落/代码块/引用/列表/表格/分隔线/HTML 块，setext 升级，表格 promote |
| `inline_parser.c/h` | 673 | 行内 token 化。strong/em 歧义消解，task list，链接，图片，autolink，HTML inline |
| `parser.c` | 227 | 对外入口。`mk_feed`/`mk_finish` 驱动块解析，Delta 队列管理，Push/Pull 双 API 统一调度 |
| `plugin.c/h` | 159 | 插件注册/调度。最多 16 个 parser plugin + 16 个 transform plugin，inline trigger 快速字符门 |
| `getters.c` | 225 | ~40 个类型安全的 `mk_node_*()` 访问器，入参为 NULL 或类型错误均返回安全默认值 |

**关键设计**：`mk_feed()` 接受任意大小的 chunk（哪怕 1 字节）。内部维护行缓冲和块栈，确定的节点立即 fire 回调，不确定的挂在 scratch arena 等待。

---

## 第二层：语言绑定（bindings/，共 ~1668 行）

四个平台的绑定，每个都是完整的、可独立使用的库：

### WASM（mk_wasm.c 167 行 + mk_wasm_api.js 186 行）

- C 侧用 `EM_ASM` 把回调 bridge 到 JS
- JS 侧 `MkParser` class 封装 feed/finish/destroy 生命周期
- 输出：`mk_parser.wasm` + `mk_parser.js`，无任何运行时依赖

```js
import createMkParser from './mk_parser.js';
const Module = await createMkParser();
const parser = new MkParser(Module, {
  onNodeOpen(type, node) { console.log('open', type); },
  onText(text) { process.stdout.write(text); },
});
for (const chunk of chunks) parser.feed(chunk);
parser.finish();
parser.destroy();
```

### Node.js N-API（mk_napi.c 417 行）

- 稳定 ABI，Node 版本升级无需重编
- `NapiParserState` 管理每个 parser 实例
- 支持 `p.onNodeOpen = fn` 赋值式回调

```js
const { MkParser } = require('mk-parser-node');
const p = new MkParser();
p.onNodeOpen = (type, node) => console.log('open', type, node);
p.onText     = (text)       => process.stdout.write(text);
p.feed('# 流式解析\n').finish();
```

### Android JNI（mk_jni.c 149 行 + MkParser.kt 118 行）

- `JniState` 结构体持有 `JavaVM*` / `jobject` / method IDs
- 处理了 Modified UTF-8 vs 标准 UTF-8 的转换问题（surrogate pair）
- Kotlin 端 lambda 回调，3 参签名 `(type, flags, attr)`，链式 API

```kotlin
val parser = MkParser(
    onNodeOpen  = { type, flags, attr -> Log.d("mk", "open $type") },
    onNodeClose = { type              -> Log.d("mk", "close $type") },
    onText      = { text             -> Log.d("mk", "text $text") }
)
parser.feed("# 你好\n").finish().destroy()
```

### iOS Swift（MkParser.swift 248 行）

- `MkNodeInfo` struct 封装节点属性
- Swift Package Manager 支持，直接 `.package(url:)` 引入
- 附带 6 个 XCTest 测试

```swift
let parser = try MkParser(
    onNodeOpen:  { type, node in print("open", type) },
    onNodeClose: { type, _   in print("close", type) },
    onText:      { text      in print("text",  text) }
)
try parser.feed("# 你好\n").finish()
```

---

## 第三层：Demo 应用（demo/）

### Web Demo（demo/web/index.html，零依赖）

直接浏览器打开，无需任何构建步骤：

- **Events 面板**：实时 open/close/text/modify 事件流，颜色区分类型
- **AST 面板**：可交互彩色节点树，显示 level/lang/href 等属性
- **Preview 面板**：流式 HTML 渲染，打字机动效 + 闪烁光标
- Speed 滑块控制 chunk 延迟（1–100 ms），慢速下可直观感受增量解析
- 5 个内置样本（All 25 Node Types / AI Streaming Sim / Tables & Alignment / Nested Structures / Links, Images & Auto-links）
- 内置 **LLM 配置区**：Mock/offline + OpenAI-compatible 流式输出
- 支持 **A/B 对比模式**：双栏预览、分栏性能指标、可选同步滚动
- 支持 **故障注入**：确定性模式 + 概率模式（延迟/断流/异常 token）
- 支持 **Fuzz 回归链路**：基线对比、JSON 导出/导入、按 run 回放
- 点击事件行或 AST 节点可高亮定位左侧编辑器中的对应源码区间
- 内置性能指标：tokens、TTFT、tokens/s、平均 tick 延迟、总耗时
- 支持界面 **中英文切换（EN / 中文）**，事件负载与 AST 节点类型保持原始内容

### Android Demo（demo/android/，Jetpack Compose）

包含完全原生的 Markdown 渲染器，无 WebView：

**原生渲染流水线：**

```
MkParser（JNI）
    │  onNodeOpen / onNodeClose / onText
    ▼
MkBlockParser.BlockBuilder     ← push 事件状态机
    │  产出 List<MkBlock>
    ▼
MkBlock 密封类                  ← Heading / Paragraph / FencedCode /
    │                             BulletItem / OrderedItem / BlockQuote /
    │                             ThematicBreak / TableBlock
    ▼
MkRenderPanel（LazyColumn）    ← key = { it.id }，稳定局部重组
    │  流式光标（▌ 闪烁）
    ▼
Compose UI
```

**支持的元素：**
- 块级：标题 H1–H6、段落、围栏代码块（含语言标签）、无序/有序列表、任务列表（☐ / ☑）、引用块、分隔线、GFM 表格
- 行内：**粗体**、*斜体*、~~删除线~~、`代码`、链接
- 配色：Catppuccin Mocha 深色主题

**LLM 接入：** Mock 离线 / OpenAI-compatible / Ollama 三种模式

---

## 测试覆盖（tests/，共 67 个用例）

| 套件 | 文件 | 用例数 | 覆盖重点 |
|---|---|---|---|
| Arena | test_arena.c | 11 | bump 分配、stable/scratch 独立回滚、自定义 allocator |
| AST | test_ast.c | 13 | 节点创建、树操作、Delta 类型 |
| Block | test_block.c | 15 | 各块类型解析，setext/promote 场景 |
| Inline | test_inline.c | 18 | strong/em 歧义、task list、链接、autolink |
| 流式集成 | test_streaming.c | 10 | 逐字节喂入、随机分块、Pull API |
| **合计** | | **67** | |

```sh
cmake -B build -DMK_BUILD_TESTS=ON && cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 对外能力总结

### 能力一：任意 chunk 大小的流式解析

```c
// 每次只喂 1 字节也能正确工作
for (size_t i = 0; i < len; i++)
    mk_feed(parser, &data[i], 1);
mk_finish(parser);
```

### 能力二：Push / Pull 双 API

- **Push**：注册回调，解析器主动推送 open/close/text/modify 事件，适合同步渲染
- **Pull**：调用 `mk_pull_delta()` 主动拉取 Delta 队列，适合协程/异步场景

### 能力三：完整 GFM 支持

标题（ATX + setext）、段落、围栏/缩进代码块、引用块、无序/有序列表、任务列表、分隔线、HTML 块/行内、表格、链接、图片、autolink、strong/em/strike/code 行内样式

### 能力四：插件扩展

- **Parser plugin**：注入自定义块/行内语法（数学公式 `$...$`、Alert 块等）
- **Transform plugin**：节点完成后的后处理钩子（统计、转换、二次加工）

### 能力五：四平台原生绑定

| 平台 | 引入方式 |
|---|---|
| Web / WASM | `import createMkParser from './mk_parser.js'` |
| Node.js | `require('mk-parser-node')` |
| Android | JNI + Kotlin lambda |
| iOS | Swift Package Manager |

---

## 核心优势

### 1. 真正的零缓冲增量解析

市面上大多数 Markdown 解析器需要完整文档才能开始工作。mk-parser 在 C 层就是增量的——block 状态机逐行推进，inline 解析器逐字符推进，确定的结构立即 emit。这是架构层面的差异，不是上层做的 hack。

### 2. 内存极其克制

双段 Arena 设计：stable 段存已确定的节点和字符串，scratch 段存"正在解析但未确定"的临时状态，每次块完成自动回滚。长文档解析内存占用可预测，不会随文档长度线性增长。

### 3. 零全局状态

所有状态都在 `MkParser*` 实例内，可以并发创建任意多个 parser，实例间完全隔离，线程安全。

### 4. 单头文件公开 API，极易嵌入

`include/mk_parser.h` 260 行，签名清晰，getter 全部类型安全，任何 C 项目 `#include` 即可使用，无需修改构建系统。

### 5. 绑定质量高，不是草草包装

每个平台的绑定都处理了该平台的特有问题：
- WASM：`EM_ASM` 事件序列化，JS class 封装生命周期
- Android：Modified UTF-8 vs 标准 UTF-8，surrogate pair 正确转换，task list leaf marker 模式
- iOS：Swift-native `MkNodeInfo` struct 封装，SPM 集成，`module.modulemap` C-Swift 桥接
- Node.js：N-API 稳定 ABI，多实例状态隔离

---

## API 速查

### 解析器生命周期

```c
MkArena  *mk_arena_new(void);
MkParser *mk_parser_new(MkArena *, const MkCallbacks *);
int       mk_feed  (MkParser *, const char *data, size_t len);
int       mk_finish(MkParser *);
void      mk_parser_free(MkParser *);
void      mk_arena_free(MkArena *);
```

### 回调结构

```c
typedef struct MkCallbacks {
    void *user_data;
    void (*on_node_open)  (void *ud, MkNode *node);
    void (*on_node_close) (void *ud, MkNode *node);
    void (*on_text)       (void *ud, MkNode *node, const char *text, size_t len);
    void (*on_node_modify)(void *ud, MkNode *node);  // 节点晋升（段落→表格等）
} MkCallbacks;
```

### Pull API

```c
MkDelta *mk_pull_delta(MkParser *);  // 队列为空返回 NULL
void     mk_delta_free(MkDelta *);
```

### 常用 Getter

```c
int         mk_node_heading_level(const MkNode *);       // 1–6
const char *mk_node_code_lang    (const MkNode *);       // 无则 NULL
int         mk_node_list_ordered (const MkNode *);
MkTaskState mk_node_list_item_task_state(const MkNode *);
const char *mk_node_link_href    (const MkNode *);
size_t      mk_node_table_col_count(const MkNode *);
MkAlign     mk_node_table_col_align(const MkNode *, size_t col);
```

### 插件注册

```c
mk_register_parser_plugin   (parser, &my_parser_plugin);
mk_register_transform_plugin(parser, &my_transform_plugin);
```

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
| Getter API | ✅ |
| M8a WASM 绑定 | ✅ |
| M8b Node.js N-API 绑定 | ✅ |
| M8c Android JNI 绑定 | ✅ |
| M8d iOS Swift 绑定 | ✅ |
| M9 JS / TypeScript 包装层 | ✅ |
| M10 Web Demo | ✅ |
| M11 C 单元测试（67 个） | ✅ |
| M12 Android 原生 Compose 渲染器 | ✅ |
