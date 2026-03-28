# FIXES.md — Bug & Optimization Tracker

优先级：**P0** 正确性/崩溃 · **P1** 功能 bug · **P2** 性能 · **P3** API/工程

---

## P0 — 正确性 / 崩溃

### [F01] Spec 测试永远返回 0 ✅
- **文件** `tests/CMakeLists.txt:62`
- **问题** `mk_test_spec` 无论多少用例失败都 exit(0)，CI 永远绿
- **修法** 统计失败数，`exit(failures > 0 ? 1 : 0)`；在 CMake 中传最低通过率阈值
- **验证** `ctest` 跑 spec 测试预期 fail → 修后 exit 非零

### [F02] JNI 线程 attach 永不 detach ✅
- **文件** `bindings/android/mk_jni.c:184-186`
- **问题** 非 JVM 线程调用 `AttachCurrentThread` 后从不 `DetachCurrentThread`，线程池场景泄漏 JVM 线程附件
- **修法** 使用 `pthread_key_create` 注册 thread-local destructor，线程退出时自动 `DetachCurrentThread`
- **验证** Android 单元测试：多线程并发 feed，LeakCanary / JVM thread count 不增长

### [F03] 流式 Demo 每 token 全文重解析 O(n²) ✅
- **文件** `demo/ios/App/DemoViewModel.swift:116`，`bindings/android/lib/.../MkBlockParser.kt:47`
- **问题** 每次 token 到达把整个已累积文档从头 `parse()`，文档越长越慢
- **修法** 保持长生命周期 `MkParser` 实例，每次只 `feed()` 新增的 chunk，通过 delta 事件增量更新 block 列表
- **验证** 1000-token 流：修前 UI 卡顿明显，修后帧率稳定；单测验证 feed 次数 = token 次数而非 doc 长度

---

## P1 — 功能 Bug

### [F04] Hard-break 检测 — ~~恒为 false~~ 经验证实际已正确 ✅
- **结论** 不是 bug。block.c 在 `rtrim` 之前检测 trailing spaces，向 textbuf 注入 `"  \n"` sentinel（`block.c:948-949`）；inline 解析器在 `try_linebreak` 中检测 `src[pos-1/2]==' '` 时命中该 sentinel → 正确产生 `MK_NODE_HARD_BREAK`。
- **CommonMark §6.7** 规定段落末尾的 hard-break 应被忽略，单行段落不产生 break 是规范行为。
- **现有测试** `test_inline.c:test_hard_break` 已覆盖 `"line1  \nline2\n"` 场景并通过。
- **无需修改**

### [F05] 行超 8191 字节静默截断 ✅
- **文件** `src/block.h:61`，`src/block.c:1008`
- **问题** 超长行被无声截断，无错误回调，代码块内容会损坏
- **修法** `MkCallbacks` 加 `on_error(void *ud, MkErrorCode, const char *msg)` 回调 + `MkErrorCode` 枚举；`block.c` 增 `line_overflow` flag，超出时触发 `MK_ERR_LINE_TOO_LONG`（每行仅一次）；`parser.c` 内部 `internal_error` 转发给用户
- **验证** `test_block.c:test_line_too_long_fires_error` + `test_line_too_long_fires_once_per_line`，全部通过 ✅

### [F06] `mk_parser_free` 不先 `mk_finish` 丢事件 ✅
- **文件** `src/parser.c:135`
- **问题** 直接 free 时挂起的 open block 不触发 close 回调，push 用户收到不完整事件流
- **修法** `mk_parser_free` 内部先调 `mk_finish`（幂等），并在 header 注释中说明语义
- **验证** 单测：feed 内容后不调 `mk_finish` 直接 `free`，验证 push 回调仍收到所有 close 事件

---

## P2 — 性能

### [F07] Emphasis 匹配 O(n²) ✅
- **文件** `src/inline_parser.c:411`
- **问题** `try_emphasis` 逐字符扫描闭合符；对抗性输入（纯 `*` 运行，无闭合符）每个位置扫描到结尾，退化 O(N²)
- **修法** 两阶段方案：① `parse_seq` 中新增 memchr 快速检测——若后续无相同字符，直接将整个 delimiter run 作为字面量，O(1) 跳过；② `mk_inline_parse` 改为两阶段（静默构建 AST + `inline_walk` 按序触发事件），保证 push 回调顺序不变
- **验证** `bench_emphasis`：N=10000 的三种对抗输入全部 <1ms（无 closer 纯运行 0.026ms，半运行 0.031ms）✅；全部 56 个测试通过 ✅

### [F08] Delta 每节点单独 malloc/free ✅
- **文件** `src/parser.c:50`
- **问题** 每个解析事件走系统分配器，绕开 arena，高频流式场景产生大量碎片
- **修法** `delta_alloc` 改为从 stable arena 分配；`mk_delta_free` 改为 no-op（内存随 arena 释放）；新增公开 API `mk_drain_deltas(parser)` 用于批量丢弃未消费 delta
- **验证** 所有现有测试通过；`mk_delta_free` 和 `mk_drain_deltas` API 兼容 ✅

### [F09] `mk_plugin_is_inline_trigger` 每字符调用 ✅
- **文件** `src/inline_parser.c:581`，`src/plugin.c`，`src/parser.c`
- **问题** 内层循环每字符调用一次，函数内再遍历所有插件做 `strchr`
- **修法** `MkParser` 添加 `uint8_t trigger_map[256]`；`mk_register_parser_plugin` 注册时更新 bitmap；`mk_plugin_is_inline_trigger` 改为 O(1) bitmap 查找；新增 `mk_parser_trigger_map_test` shim
- **验证** `test_m7.c:test_trigger_bitmap`：`$` 命中、`a`/`!` 未命中；全部 M7 测试通过 ✅

### [F10] JNI hot path 每 text 事件 malloc ✅
- **文件** `bindings/android/mk_jni.c:37`
- **问题** 每个 text 事件 malloc jchar 缓冲区，text 事件是最高频事件
- **修法** 线程局部 512-byte 栈缓冲，超出时才堆分配
- **验证** Android systrace：text callback 中 malloc 调用从 N 降为 ~0（ASCII 内容）

---

## P3 — API / 工程

### [F11] `MkParser.kt` 未实现 `AutoCloseable` ✅
- **文件** `bindings/android/MkParser.kt:22`
- **修法** 实现 `AutoCloseable`，`close()` 调 `destroy()`
- **验证** 用 `use { }` 块的单测，验证 native 内存释放

### [F12] iOS `BlinkCursor` Timer 泄漏 ✅
- **文件** `demo/ios/App/render/MkRenderView.swift:37`
- **问题** `onAppear` 起 Timer 无 `onDisappear` 取消，View 反复进出屏幕积累多个 Timer
- **修法** 用 `@State var timer: Timer?` 保存引用，`onDisappear` 调 `timer?.invalidate()`
- **验证** 模拟 View 10 次进出屏幕，验证只有 1 个 Timer 活跃

### [F13] Kotlin / JS 枚举常量与 C 手动同步脆弱 ✅
- **文件** `bindings/android/MkParser.kt:92`，`bindings/js/types.d.ts`
- **问题** `NodeType` 整数与 C enum 手动对齐，C 端改动静默出错
- **修法** 在 CMake build 中生成 `mk_constants.h`（C enum 值），JNI 层用 `#include` 验证；Kotlin 侧加编译期断言测试
- **验证** 故意修改 C enum 顺序，构建报错而非静默错误

### [F14] CMake 无 install target ✅
- **文件** `CMakeLists.txt`
- **修法** 添加 `install(TARGETS mk_parser_core ...)` + `install(FILES include/mk_parser.h ...)`
- **验证** `cmake --install build/native --prefix /tmp/mk_test`，验证头文件和库文件正确落地

### [F15] 链接目标括号转义未处理 ✅
- **文件** `src/inline_parser.c:130-136`
- **问题** `parse_link_dest` 不跳过 `\(`/`\)`，含转义括号的链接解析错误
- **修法** depth 计数循环中检测反斜杠转义，跳过下一字符
- **验证** 新增单测：`[link](\(foo\))` 解析，href = `\(foo\)`；已有链接测试全部通过

---

## 进度总览

| ID | 描述 | 状态 |
|---|---|---|
| F01 | Spec 测试返回值 | ✅ done |
| F02 | JNI thread detach | ⬜ pending |
| F03 | 流式 Demo O(n²) | ⬜ pending |
| F04 | Hard-break 恒 false | ✅ 经验证已正确，无需修改 |
| F05 | 行截断无通知 | ✅ done |
| F06 | free 前不 finish | ✅ done |
| F07 | Emphasis O(n²) | ✅ done |
| F08 | Delta malloc | ✅ done |
| F09 | trigger per-char | ✅ done |
| F10 | JNI text malloc | ⬜ pending |
| F11 | AutoCloseable | ✅ done |
| F12 | Timer 泄漏 | ✅ done |
| F13 | 枚举同步脆弱 | ⬜ pending |
| F14 | CMake install | ✅ done |
| F15 | 括号转义 | ✅ done |
