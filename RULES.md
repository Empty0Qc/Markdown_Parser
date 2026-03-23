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

## PROGRESS.md 协议

- 开始一个任务前：将状态改为 `🔄`
- 完成一个任务后：将状态改为 `✅`，并在 Notes 列填写关键产物或注意点
- 发现 blocker：将状态改为 `🔒`，在 Notes 列描述阻塞原因
- **GPT 完成任务后必须更新 PROGRESS.md**，这是两个 AI 之间的同步机制

## 分支/提交约定

- 提交信息格式：`[Mxx] 简短描述`，例如 `[M2] implement arena bump allocator`
- 每个模块完成后打 tag：`m1-done`, `m2-done` …
