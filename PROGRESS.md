# PROGRESS.md — Task Tracker

**Legend**：⬜ pending | 🔄 in progress | ✅ done | 🔒 blocked

---

## M1 — 项目骨架与构建系统（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| 目录结构 | ✅ | src/ include/ bindings/ demo/ tests/ cmake/ |
| CMakeLists.txt 主配置 | ✅ | 支持 native/WASM/Android/iOS 多目标 |
| cmake/options.cmake | ✅ | |
| cmake/toolchains/wasm.cmake | ✅ | Emscripten |
| cmake/toolchains/android.cmake | ✅ | arm64-v8a/armeabi-v7a/x86_64 |
| cmake/toolchains/ios.cmake | ✅ | OS64/SIMULATORARM64/SIMULATOR64 |
| 各子目录 CMakeLists.txt | ✅ | bindings/*/CMakeLists.txt 全部实现 |
| build.sh 多平台构建脚本 | ✅ | ./build.sh [native\|wasm\|android\|ios\|all] |
| include/mk_parser.h 公开头文件桩 | ✅ | 含完整 API 签名声明，M2-M7 实现 |
| src/ 各模块 .c 桩文件 | ✅ | arena/ast/parser/block/inline_parser/plugin |

## M2 — Arena 分配器（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| mk_arena_t 基础实现（bump allocator） | ✅ | 64KB block，pointer-aligned |
| stable / scratch 两段设计 | ✅ | 独立 block chain |
| 自定义 malloc/free 注入接口 | ✅ | mk_arena_new_custom |
| stable arena 按块释放 API | ✅ | scratch_commit 零拷贝转移 |
| checkpoint / rollback | ✅ | mark / rollback / commit |
| 字符串辅助函数 | ✅ | strdup_stable / strdup_scratch |
| 单元测试 | ✅ | 11 tests — M11 阶段完成 |

## M3 — AST 数据结构（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| MkNode 基础头定义 | ✅ | last_child/prev_sibling 加入，O(1) append |
| 所有 GFM 节点类型特化子结构（~25种） | ✅ | Block 13种 + Inline 12种，src/ast.h |
| 树操作 API（insert/detach/traverse） | ✅ | append/prepend/detach/traverse |
| Delta 类型定义 | ✅ | MkDelta + mk_delta_new / mk_delta_new_text |
| 节点类型到字符串（调试） | ✅ | mk_node_type_name |

## M4 — Block 解析器（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| 状态机框架 | ✅ | line-buffer + open-block stack, CRLF 处理 |
| Heading（ATX + Setext） | ✅ | ATX 含 trailing # 剥离；setext promotion via emit_modify |
| Paragraph（含 lazy continuation） | ✅ | blockquote lazy continuation 支持 |
| FencedCodeBlock | ✅ | ` ``` ` + `~~~`，含 lang tag，流式逐行 emit_text |
| IndentedCodeBlock | ✅ | 4 空格，流式逐行 |
| BlockQuote | ✅ | 嵌套支持，递归 process_line |
| UnorderedList + ListItem | ✅ | - * + marker |
| OrderedList + ListItem | ✅ | digits + . or ) |
| ThematicBreak | ✅ | --- *** ___ |
| HtmlBlock | ✅ | type 1–6，各类型 end condition |
| Table（GFM） | ✅ | paragraph → table promotion via emit_modify |
| Pending 处理（流式未完成节点） | ✅ | scratch arena + pop_frame 时 flush inline |

## M5 — Inline 解析器（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| Text | ✅ | |
| SoftBreak / HardBreak | ✅ | 两空格在 rtrim 前检测，last_line_hard_break 字段 |
| Emphasis / Strong（歧义消解） | ✅ | greedy + word-boundary for _ |
| Strikethrough（GFM） | ✅ | |
| InlineCode | ✅ | |
| Link | ✅ | |
| Image | ✅ | |
| AutoLink | ✅ | |
| HtmlInline | ✅ | |
| TaskListItem（GFM） | ✅ | |
| Pending 处理 | ✅ | smoke test passes 7-byte chunks |

## M6 — Push/Pull 双 API（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| Push API（mk_feed + 回调结构） | ✅ | MkParser wraps MkBlockParser, fires user cbs immediately |
| Delta queue 内部实现 | ✅ | malloc'd MkDelta nodes, text copied to stable arena |
| Pull API（mk_pull_delta） | ✅ | FIFO queue, mk_delta_free |
| mk_finish（流结束信号） | ✅ | |

## M7 — Plugin 系统（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| MkParserPlugin vtable 定义 | ✅ | name + inline_triggers + try_block/try_inline |
| MkTransformPlugin vtable 定义 | ✅ | name + on_node_complete |
| 插件注册 / 注销 API | ✅ | mk_register_parser/transform_plugin, 16 slots each |
| 插件调用时序 | ✅ | inline: fast-path trigger gate → try loop; block: pre-paragraph; transform: on internal_close |
| 示例插件：数学公式 $...$ | ✅ | math inline + alert block + counter transform, tests pass |

## Getter API — 公开节点特化字段访问器（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| MkAlign / MkTaskState 移入公开头 | ✅ | include/mk_parser.h |
| ~40 个 mk_node_*() getter 声明 | ✅ | heading_level, code_lang, link_href, table_col_align… |
| src/getters.c 实现 | ✅ | safe cast + NULL/wrong-type → 安全默认值 |
| CMakeLists.txt 接入 getters.c | ✅ | |
| include/module.modulemap | ✅ | Swift Package Manager C→Swift 互操 |

## M8a — WASM 绑定（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| Emscripten 编译配置 | ✅ | bindings/wasm/CMakeLists.txt，MODULARIZE + EXPORT_NAME=createMkParser |
| 事件序列化层（C → JS 回调，策略 β） | ✅ | bindings/wasm/mk_wasm.c，EM_ASM Module._wasm_on_event |
| JS 回调注册接口 | ✅ | bindings/wasm/mk_wasm_api.js，MkParser class |
| WASM 内存管理封装 | ✅ | mk_wasm_create/feed/finish/destroy |
| 产物：mk_parser.wasm + mk_parser.js | ✅ | build.sh wasm 生成 |

## M8b — Server 原生库（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| CMake 编译为 .so/.dylib | ✅ | bindings/node/CMakeLists.txt |
| Node.js napi 绑定 | ✅ | bindings/node/mk_napi.c，~350 行；NapiParserState + build_node_info |
| pkg-config 文件 | ✅ | bindings/node/mk_parser.pc.in |
| binding.gyp / package.json / index.js | ✅ | |

## M8c — Android JNI 绑定（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| NDK CMake 配置（arm64/x86_64） | ✅ | bindings/android/CMakeLists.txt，add_library(mk_parser_jni SHARED) |
| JNI 桥接层 | ✅ | bindings/android/mk_jni.c，JniState + push callback bridges |
| Kotlin 封装类 | ✅ | bindings/android/MkParser.kt，lambda callbacks + external JNI |
| .aar 打包配置 | ✅ | install(TARGETS) → jniLibs/${ANDROID_ABI}/ |

## M8d — iOS 绑定（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| Xcode CMake 工具链 | ✅ | cmake/toolchains/ios.cmake |
| xcframework 打包脚本 | ✅ | bindings/ios/build_xcframework.sh，lipo 合并 sim 切片 |
| Swift 封装 | ✅ | bindings/ios/MkParser.swift，MkParser class + MkNodeInfo struct |
| Swift Package Manager 配置 | ✅ | Package.swift，mk_parser_c + MkParser targets |
| Swift 单元测试 | ✅ | bindings/ios/Tests/MkParserTests.swift，6 tests |

## M9 — JS/TypeScript 包装层（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| TypeScript 类型定义 | ✅ | bindings/js/types.ts，NodeType/Align/TaskState const enums + 所有 Node 接口 |
| JS 侧 AST 重建 | ✅ | bindings/js/mk_parser_wasm.js，parseToAST() |
| Pull/Push 接口 TS 封装 | ✅ | WasmMkParser class，parseFull() |
| npm 包配置（ESM + CJS） | ✅ | bindings/js/package.json |

## M10 — Web Demo（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| 单页 HTML（无框架） | ✅ | demo/web/index.html，~700 行，零依赖 |
| 流式输入模拟（可调速） | ✅ | 7-byte 分块 + speed slider (1–100ms) |
| 实时 AST 树可视化 | ✅ | 彩色节点类型树，hover highlight |
| 内置 GFM 测试用例集 | ✅ | 5 个 sample：GFM Showcase / AI Streaming / Tables / Nesting / Links |
| Event stream 面板 | ✅ | open/close/text/modify 彩色事件流 |
| HTML Preview 面板 | ✅ | 内联 HTML 渲染，无外部依赖 |
| Stats bar | ✅ | Events / Nodes / Chars / Time |

## M12 — Android 原生 Compose 渲染器（Owner: Claude）

| Task | Status | Notes |
|---|---|---|
| MkBlock 密封类层次（8 种块类型） | ✅ | Heading/Paragraph/FencedCode/BulletItem/OrderedItem/BlockQuote/ThematicBreak/TableBlock |
| MkBlockParser 状态机 | ✅ | BlockBuilder 消费 push 事件 → List<MkBlock>，AnnotatedString 行内 span |
| 稳定块 ID | ✅ | type + 序号（H_0 / P_1 / CODE_0 / LI_0…），LazyColumn key 高效 diff |
| markLastStreaming 扩展函数 | ✅ | 流式时标记最后一个块，附加闪烁光标 |
| MkRenderPanel LazyColumn 渲染器 | ✅ | key = { it.id }，Catppuccin Mocha 配色 |
| 流式光标动画 | ✅ | InfiniteTransition alpha blink，▌ 附加到最后一个块 |
| 任务列表修复（leaf marker 模式） | ✅ | pendingTaskState 桥接 TASK_LIST_ITEM open → LIST_ITEM close |
| JNI UTF-8 修复（jstring_from_slice） | ✅ | 非 null 结尾切片安全封装，修复 Modified UTF-8 崩溃 |
| JNI 标准 UTF-8 输入修复（java_string_to_utf8） | ✅ | UTF-16→UTF-8 转换，正确处理 emoji 等代理对 |
| TASK_LIST_ITEM JNI attrs 映射 | ✅ | mk_node_task_list_item_checked → taskState(1=未选/2=已选) |
| MkDemoViewModel 集成 | ✅ | renderedBlocks StateFlow + parseAll/startStream/startLlmStream |
| MainScreen Preview 标签页替换 | ✅ | 原 WebView/WebPreviewPanel → MkRenderPanel |


| Task | Status | Owner | Notes |
|---|---|---|---|
| C 单元测试框架搭建 | ✅ | Claude | tests/CMakeLists.txt，mk_add_test 宏，CTest 接入 |
| Arena 单元测试 | ✅ | Claude | tests/unit/test_arena.c，11 tests |
| AST 单元测试 | ✅ | Claude | tests/unit/test_ast.c，13 tests |
| Block 解析单元测试 | ✅ | Claude | tests/unit/test_block.c，15 tests |
| Inline 解析单元测试 | ✅ | Claude | tests/unit/test_inline.c，18 tests |
| 流式集成测试 | ✅ | Claude | tests/integration/test_streaming.c，10 tests |
| GFM 合规测试 | 🔄 | — | 已接入 CTest，下一步补 CommonMark spec 数据驱动用例 |
| 性能基准测试 | 🔄 | — | tests/bench_parser.c 轻量吞吐基线 |
| 各平台绑定 smoke test | ⬜ | — | 待各平台实际构建验证 |
