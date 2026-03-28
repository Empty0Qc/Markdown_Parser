# RULES.md — Project Conventions

## Architecture Invariants（不可违反）

1. **C 核心层绝对不包含任何渲染逻辑**，只产出 AST 事件
2. **C 核心 API 必须是纯 C**（`extern "C"`，无 C++，无 OS-specific 头文件）
3. **无全局状态**，所有状态封装在 `MkParser *` 实例内，支持多实例并发
4. **所有节点内存通过 Arena 分配**，禁止在 Arena 之外 `malloc` 节点
5. **Arena 支持注入自定义 `malloc/free`**，不硬依赖系统 allocator
6. **`MkNode` 必须是所有特化节点子结构的第一个字段**，以保证安全 cast

## C 编码规范

- 文件命名：`snake_case.c` / `snake_case.h`
- 类型命名：`MkXxx`（结构体/枚举），`mk_xxx` 前缀（函数/变量）
- 公开 API 全部声明在 `include/mk_parser.h`，内部实现细节放 `src/`
- 每个 `.c` 文件对应一个私有头文件 `src/xxx_internal.h`（不对外暴露）
- 错误返回：函数返回 `int`（0 = OK，负数 = 错误码），不使用 `errno`
- 禁止 VLA（可变长数组），禁止 `alloca`

## 内存规则

- **scratch_arena**：用于 pending（流式中未完成）节点，可回退
- **stable_arena**：节点完成后迁移至此，不可回退，可按块释放
- 字符串属性（如 `lang`、`href`）也从 Arena 分配，不独立 `malloc`
- 插件分配的节点必须使用传入的 `MkArena *`，不自行管理内存

## 模块边界与文件所有权

| 模块 | 文件 | Owner |
|---|---|---|
| M1 骨架/构建 | `CMakeLists.txt`, `cmake/`, `build.sh` | Claude |
| M2 Arena | `src/arena.c`, `src/arena.h` | Claude |
| M3 AST | `src/ast.c`, `src/ast.h` | Claude |
| M4 Block 解析 | `src/block.c`, `src/block.h` | Claude |
| M5 Inline 解析 | `src/inline_parser.c`, `src/inline_parser.h` | Claude |
| M6 Push/Pull API | `src/parser.c`, `include/mk_parser.h` | Claude |
| M7 Plugin 系统 | `src/plugin.c`, `src/plugin.h` | Claude |
| M8a WASM 绑定 | `bindings/wasm/` | GPT |
| M8b Server 原生库 | `bindings/node/` | GPT |
| M8c Android JNI | `bindings/android/` | GPT |
| M8d iOS 绑定 | `bindings/ios/` | GPT |
| M9 JS/TS 包装层 | `bindings/js/` | GPT |
| M10 Web Demo | `demo/web/` | GPT |
| M11 测试 | `tests/` | 共同 |

## 分支/提交约定

- 提交信息格式：`[Mxx] 简短描述`，例如 `[M2] implement arena bump allocator`
- 每个模块完成后打 tag：`m1-done`, `m2-done` …

---

## 踩坑记录

### Android JNI：`NewStringUTF` 崩溃（Modified UTF-8 问题）

**现象**：`input is not valid Modified UTF-8: illegal start byte 0xXX`，VM abort。

**原因**：C 解析器 `on_text` 回调给出的是 `(text, len)` 切片，text 指针可能指向多字节 UTF-8 字符中间（continuation byte），而 `NewStringUTF` 要求 Modified UTF-8，遇到非法起始字节直接 abort。

**修复**：`jstring_from_slice()` 改为完整的 UTF-8→UTF-16 解码器，用 `NewString(env, jchar*, len)` 写入；无效字节替换为 U+FFFD，绝不 abort。见 `bindings/android/mk_jni.c`。

**同理**：`java_string_to_utf8()` 也不能用 `GetStringUTFChars`（返回 Modified UTF-8，emoji 会编码为代理对的两段 3 字节序列），必须走 UTF-16→UTF-8 手工转换。

---

### Android：`RecyclerView GapWorker` 预取导致 ANR

**现象**：主线程卡死，调用栈含 `GapWorker.prefetch` → `onBindViewHolder` → `MkBlockParser.parse` → `MkParser.nativeFeed`（native）。

**原因**：`GapWorker` 在主线程上提前调用 `onBindViewHolder` 做预取，而 `parse()` 是同步耗时操作，link 多的 Markdown 尤其明显。

**修复**：将 parse 移到 `Dispatchers.Default` 协程，主线程仅提交 `submitBlocks`。见 `ContentStreamInfoView`。

---

### Android：View 生命周期与 CoroutineScope

**现象**：改异步后静态/流式渲染全失效，scope 为 null。

**原因**：`HoleNormalViewHolder.removeAndCreate()` 调用 `renderBlocks()` → `refreshData()` 时，View 还未 `addView()`，`onAttachedToWindow()` 尚未触发，scope 未初始化。

**修复**：`scope` 必须在字段声明处初始化（非 null），`onDetachedFromWindow` 取消后立即重建：

```kotlin
private var scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
override fun onDetachedFromWindow() {
    super.onDetachedFromWindow()
    scope.cancel()
    scope = CoroutineScope(Dispatchers.Main + SupervisorJob())  // 立即重建！
}
```

---

### Android 库发布：iCloud 路径含空格无法用 `includeBuild`

**现象**：`includeBuild` 在 `android-assistant` 中失效，Gradle 解析 iCloud 路径 symlink 到真实路径后，URI 中空格导致 `settings.gradle.kts` 找不到。

**修复**：放弃 composite build，改为 `publishToMavenLocal` + `mavenLocal()` 依赖。每次修改 `mk_p` 代码后手动 bump 版本号并重新发布。发布命令见 `bindings/android/lib/README`（或 memory 文件）。

---

### Gradle：Kotlin source dir `".."` 触发 8.7 严格校验

**现象**：`compileReleaseKotlin uses output of configureCMake without declaring dependency`。

**原因**：`kotlin.srcDirs("..")` 把整个 `bindings/android/` 目录包含进来，与 CMake 构建输出目录重叠，Gradle 8.7 的严格任务依赖校验报错。

**修复**：将 `MkParser.kt` 复制到 `lib/src/main/kotlin/com/mkparser/MkParser.kt`，source set 只声明 `"src/main/kotlin"`，两份文件需保持同步。
