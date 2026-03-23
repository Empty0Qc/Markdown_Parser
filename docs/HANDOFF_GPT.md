# GPT 协作交接文档

> 本文档是给 GPT-5.4 (Codex) 的完整上下文，包含项目背景、架构决策、你的任务范围和协作规范。

---

## 项目是什么

**mk_parser**：Markdown 在 AI 流式场景下的增量解析与渲染库。

AI 模型逐 token 输出文本（每 5–20ms 一个 token），需要：
1. 实时增量解析流式 Markdown，不等全文到达
2. 产出平台无关的 AST（抽象语法树）
3. 各平台（Web/Android/iOS/Server）基于 AST 自行实现渲染

---

## 架构决策（已定，不可更改）

| 决策 | 结论 |
|---|---|
| 解析器语言 | **C**（纯 C11，无 C++，无 OS-specific 头文件） |
| Markdown 规范 | GFM 基准 + 自定义插件扩展 |
| 解析器架构 | A+B 混合：内部 Push 状态机，对外暴露 Push API（回调）+ Pull API（delta queue） |
| AST 节点布局 | 基础头 `MkNode` + 类型特化子结构（`MkNode` 必须是第一个字段） |
| 内存管理 | Arena 分配器（stable + scratch 两段） |
| 插件机制 | vtable（函数指针结构体） |
| **跨语言绑定** | **策略 β：事件序列化推送**（C 节点完成时序列化为消息推给宿主语言，不传裸指针） |
| 解析-渲染解耦 | C 层只产出 AST 事件，渲染适配器各平台独立实现 |
| 性能目标 | 解析器 <5ms/token，渲染器按 16ms 帧预算消费 delta |

---

## 目录结构

```
mk_p/
├── CMakeLists.txt           # 根 CMake（Claude 负责）
├── cmake/
│   ├── options.cmake
│   └── toolchains/
│       ├── wasm.cmake
│       ├── android.cmake
│       └── ios.cmake
├── include/
│   └── mk_parser.h          # 公开 C API（Claude 负责）
├── src/                     # C 核心实现（Claude 负责，不要修改）
│   ├── arena.c / arena.h
│   ├── ast.c / ast.h
│   ├── parser.c
│   ├── block.c / block.h
│   ├── inline_parser.c / inline_parser.h
│   └── plugin.c / plugin.h
├── bindings/                # ← 你的主战场
│   ├── wasm/                # M8a
│   ├── android/             # M8c
│   ├── ios/                 # M8d
│   ├── node/                # M8b
│   └── js/                  # M9
├── demo/
│   └── web/                 # M10 ← 你的任务
├── tests/                   # M11 部分 ← 你的任务
│   ├── unit/
│   └── integration/
├── docs/
│   └── qa/                  # 历史决策记录（只读）
├── RULES.md                 # 必读：编码规范
└── PROGRESS.md              # 必须维护：任务状态
```

---

## 你的任务（M8a / M8b / M8c / M8d / M9 / M10 / M11 部分）

### 前置条件

**等待 Claude 完成 M1–M7 后，你才能开始 M8+**。
M7 完成后 `include/mk_parser.h` 会有完整的公开 API，你基于它实现绑定层。

### M8a — WASM 绑定（`bindings/wasm/`）

目标：将 C 核心编译为 `mk_parser.wasm`，附带 JS glue code。

关键要求：
- 使用 **Emscripten**（`emcc`）
- 采用**策略 β**：节点完成时通过 JS 回调推送序列化消息，不暴露裸 C 指针给 JS
- `mk_arena_t` 生命周期要能从 JS 侧控制（创建/释放）
- 产物：`mk_parser.wasm` + `mk_parser.js`（Emscripten glue）

### M8b — Server 原生库（`bindings/node/`）

目标：Node.js napi 绑定（`mk_parser.node`）。

关键要求：
- 使用 **node-addon-api** 或原生 **napi**
- 同样采用策略 β（异步事件推送）
- 同时提供 `.so/.dylib` CMake 编译配置

### M8c — Android JNI 绑定（`bindings/android/`）

目标：Android NDK 编译 + JNI 桥接。

关键要求：
- CMake 支持 `arm64-v8a`、`armeabi-v7a`、`x86_64` 三个 ABI
- JNI 桥接层（C ↔ Java/Kotlin），通过 JNI 回调推送 AST 事件
- Kotlin 封装类 `MkParser`（协程友好，`Flow<AstDelta>`）
- .aar 打包配置

### M8d — iOS 绑定（`bindings/ios/`）

目标：iOS xcframework + Swift 封装。

关键要求：
- 支持 device（arm64）+ simulator（arm64 + x86_64）
- `xcframework` 打包脚本
- Swift 封装 `MkParser` class（`AsyncStream<AstDelta>` 友好）
- Swift Package Manager 支持

### M9 — JS/TypeScript 包装层（`bindings/js/`）

目标：TypeScript 类型 + JS 侧 AST 重建 + npm 包。

关键要求：
- 完整 TypeScript 类型定义（AST 节点类型、Delta 类型、Plugin 接口）
- 从 M8a WASM 事件重建为 JS 原生对象树
- 同时封装 Pull API 和 Push API（TypeScript 接口）
- npm 包支持 ESM + CJS 双产物

### M10 — Web Demo（`demo/web/`）

目标：单页 HTML，无需框架，快速在浏览器验证。

要求：
- 三栏布局：左（流式输入模拟）/ 中（实时 AST 树）/ 右（渲染预览）
- 可调节 token 输入速度（5ms–200ms/token）
- 内置若干 GFM 测试用例（含 table、code block、nested list）
- 纯 HTML + vanilla JS，无需 npm install

### M11 — 测试（你负责的部分）

- `tests/` 下的 CMake 测试框架搭建（推荐 [Unity](https://github.com/ThrowTheSwitch/Unity) 或 [munit](https://nemequ.github.io/munit/)）
- GFM 合规测试（对照 commonmark spec JSON 测试集）
- 性能基准测试（大文档 / 高频 token）
- 各平台绑定 smoke test

---

## 编码规范（摘要，完整见 RULES.md）

- C 函数/变量：`mk_` 前缀，`snake_case`
- C 类型：`MkXxx`
- 所有节点内存只用传入的 `MkArena *` 分配
- 不修改 `src/` 下任何文件（那是 Claude 的领域）
- 绑定层只依赖 `include/mk_parser.h`，不 include `src/` 内部头文件

---

## 完成后如何同步

1. 更新 `PROGRESS.md`：将你完成的任务改为 `✅`，Notes 填写关键产物路径
2. 如果发现 `include/mk_parser.h` 中缺少你需要的 API，在 PROGRESS.md 的 Notes 里标注 `🔒 需要 Claude 补充 API: xxx`
3. 不要直接修改 Claude 负责的文件，通过 PROGRESS.md 传达需求

---

## 参考资料

- 完整需求和决策历史：`docs/qa/` 目录下的 01–07 文件
- GFM 规范：https://github.github.com/gfm/
- CommonMark 测试集：https://spec.commonmark.org/
- Emscripten 文档：https://emscripten.org/docs/
- node-addon-api：https://github.com/nodejs/node-addon-api
