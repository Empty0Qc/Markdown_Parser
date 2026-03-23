# Android Demo — mk-parser

基于 Jetpack Compose 的演示应用，通过 JNI 集成 `mk_parser` C 库，并完全原生渲染 Markdown，无需 WebView。

## 功能

| 标签页 | 说明 |
|---|---|
| **Events** | 实时解析事件流（OPEN / CLOSE / TEXT / MODIFY），颜色区分事件类型 |
| **Preview** | 原生 Compose 渲染器 — `LazyColumn` 使用稳定 key，流式过程中支持局部重组 |
| **LLM** | 接入任意 OpenAI 兼容 API（或 Ollama），实时解析流式响应 |

## 环境依赖

- Android Studio Hedgehog（2023.1）或更高版本
- NDK r25 或更高版本（通过 SDK Manager → SDK Tools 安装）
- CMake 3.22+（通过 SDK Manager → SDK Tools 安装）
- Ollama 集成需要：主机上运行 Ollama 服务

## 用 Android Studio 打开

```
File → Open → demo/android/
```

Android Studio 会自动同步 Gradle、下载依赖并配置 CMake 构建。

## 命令行构建

```sh
cd demo/android
./gradlew assembleDebug
# 安装到已连接的设备或模拟器：
./gradlew installDebug
```

## 项目结构

```
demo/android/
├── app/
│   ├── src/main/
│   │   ├── cpp/
│   │   │   ├── CMakeLists.txt           原生构建（链接 mk_parser C 源码）
│   │   │   └── mk_jni.c                 JNI 桥接（nativeCreate/Feed/Finish/Destroy）
│   │   ├── kotlin/com/mkparser/
│   │   │   ├── MkParser.kt              JNI 封装类（来自 bindings/android/）
│   │   │   └── demo/
│   │   │       ├── MainActivity.kt      程序入口
│   │   │       ├── MkDemoViewModel.kt   状态管理 + 解析/流式/LLM 逻辑
│   │   │       └── ui/
│   │   │           ├── MainScreen.kt    标签页布局（Events / Preview / LLM）
│   │   │           ├── EventsPanel.kt   解析事件列表
│   │   │           ├── LlmPanel.kt      LLM 配置与生成
│   │   │           ├── render/
│   │   │           │   ├── MkBlock.kt        所有块类型的密封类层次
│   │   │           │   ├── MkBlockParser.kt  推送事件 → List<MkBlock> 状态机
│   │   │           │   └── MkRenderPanel.kt  Compose LazyColumn 渲染器
│   │   │           └── theme/Theme.kt   深色配色方案（Catppuccin Mocha）
│   │   └── res/
│   └── build.gradle.kts
├── gradle/libs.versions.toml
├── settings.gradle.kts
└── README.md
```

## 原生渲染流水线

```
MkParser（JNI）
    │  onNodeOpen / onNodeClose / onText
    ▼
MkBlockParser.BlockBuilder          ← 推送事件状态机
    │  产出 List<MkBlock>
    ▼
MkBlock 密封类                       ← Heading / Paragraph / FencedCode /
    │                                  BulletItem / OrderedItem / BlockQuote /
    │                                  ThematicBreak / TableBlock
    ▼
MkRenderPanel（LazyColumn）          ← key = { it.id }，稳定局部重组
    │  流式光标（▌ 闪烁）
    ▼
Compose UI
```

### 稳定块 ID

每个 `MkBlock` 携带基于类型 + 序号的稳定 `id`（`H_0`、`P_1`、`CODE_0`、`LI_0`……）。`LazyColumn` 以此为 key，每个新 token 到来时仅重组最后一个（正在流式输出的）块。

### 流式光标

`MkBlockParser.parse(md, isStreaming = true)` 将最后一个块标记为 `isStreaming = true`。`MkRenderPanel` 通过 `InfiniteTransition` alpha 动画向该块的 `AnnotatedString` 末尾附加闪烁的 `▌` 光标。

### 行内 Span

`MkBlockParser` 使用 `SpanStyle` push/pop 构建 `AnnotatedString`，支持：

| 语法 | 样式 |
|---|---|
| `**粗体**` | `FontWeight.Bold` |
| `*斜体*` | `FontStyle.Italic` |
| `~~删除线~~` | `TextDecoration.LineThrough` |
| `` `代码` `` | `FontFamily.Monospace` + 代码配色 |
| `[链接](url)` | 下划线 + `URL` annotation |

### 任务列表项

C 解析器的 AST 结构为 `LIST_ITEM → TASK_LIST_ITEM（叶节点）→ PARAGRAPH`。JNI 层将 `mk_node_task_list_item_checked` 映射为 `taskState`（1 = 未选中，2 = 已选中）。`MkBlockParser` 将该值存入 `pendingTaskState`，在 `LIST_ITEM` 关闭时附加到 `BulletItem` 块上。`MkRenderPanel` 据此渲染 ☐ / ☑ / •。

## 配色方案

全部使用 **Catppuccin Mocha**：

| 用途 | 颜色 |
|---|---|
| 背景 | `#1E1E2E` |
| 代码背景 | `#181825` |
| 正文 | `#CDD6F4` |
| 标题 / 引用块 | `#CBA6F7`（Mauve） |
| 代码文字 | `#A6E3A1`（Green） |
| 无序列表标记 | `#FAB387`（Peach） |
| 有序列表编号 | `#F9E2AF`（Yellow） |
| 链接 | `#89DCEB`（Sky） |

## LLM 集成

### Mock 模式（无需 API Key）

在 LLM 标签页选择 **Mock（离线）** — 使用 `MockProvider`，逐字符回放一段预置的 Markdown 响应。

### OpenAI

1. 选择 **OpenAI 兼容**
2. Base URL：`https://api.openai.com/v1`
3. API Key：`sk-…`
4. Model：`gpt-4o-mini`

### Ollama（主机本地模型）

1. 安装 Ollama 并拉取模型：`ollama pull llama3.2`
2. 选择 **OpenAI 兼容**
3. Base URL：`http://10.0.2.2:11434/v1`  ← 模拟器回环地址指向主机
4. API Key：`ollama`
5. Model：`llama3.2`

### 自定义 Provider

实现 `LlmProvider` 接口：

```kotlin
class MyProvider : LlmProvider {
    override val label = "我的后端"
    override fun stream(prompt: String): Flow<String> = flow {
        // 在此 emit token
    }
}
```

然后在 `MkDemoViewModel.startLlmStream()` 中注入即可。

## JNI 构建原理

`app/src/main/cpp/CMakeLists.txt` 从 Gradle 接收 `MK_ROOT`，编译以下源文件：

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

生成 `arm64-v8a` 和 `x86_64` 的 `libmk_parser_jni.so`。

Demo 使用自带的 `mk_jni.c`（位于 `app/src/main/cpp/`）而非 `bindings/android/` 中的版本，以便将 Demo 专属修复（UTF-8 转换、任务列表状态映射）限定在 Demo 工程内。
