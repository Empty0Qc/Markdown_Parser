# Agent Context — mk-parser

> 本文档面向参与本项目的 AI Agent（Claude / GPT / Gemini 等），提供上手所需的完整上下文。
> 架构细节见 `docs/ARCHITECTURE.md`，编码规范与踩坑见 `RULES.md`。

---

## 项目定位

**专为 LLM 流式输出场景设计的增量 Markdown 解析器。** C11 内核，零依赖，四平台绑定（Web / Node.js / Android / iOS）。

```
LLM token 流  →  mk_feed()  →  on_node_open / on_text / on_node_close
```

---

## 当前状态（截至 2026-03-29）

所有里程碑均已完成（M1–M12 + iOS Demo），持续修 bug + 提升 spec 通过率：

| 层 | 状态 |
|---|---|
| C 核心（arena / ast / block / inline / plugin / getters） | ✅ 完成 |
| WASM 绑定（M8a） | ✅ 完成 |
| Node.js N-API 绑定（M8b） | ✅ 完成 |
| Android JNI 绑定（M8c） | ✅ 完成 |
| iOS Swift 绑定（M8d） | ✅ 完成 |
| JS/TS 包装层（M9） | ✅ 完成 |
| Web Demo（M10） | ✅ 完成 |
| C 单元测试 67 个（M11） | ✅ 完成 |
| Android View 渲染器（M12）| ✅ 完成 |
| iOS SwiftUI Demo | ✅ 完成 |
| Bug fixes F01–F20 | ✅ 全部修复 |
| CommonMark 0.31 spec 通过率 | **40.5%**（264/652，初始 31.4%） |

### 最近改动（本会话 2026-03-29）

| 模块 | 改动 | 详情 |
|---|---|---|
| `src/ast.c` | 新增 36 个节点属性 getter | `mk_node_heading_level` 等，类型安全，NULL 防御 |
| `src/block.c` | 修复 F16：setext 标题双 close | 删除 `pop_to` 后多余的 `emit_close` |
| `src/inline_parser.c` | 修复 F19：pos+new_pos 双加 | 标准构造返回绝对位置，插件返回相对偏移，分开处理 |
| `src/inline_parser.c` | 修复 F20：Emphasis punctuation 规则 | 实现 CommonMark §6.2 left/right-flanking 标点条件 |
| `tests/spec/mk_html.c` | 修复 F17/F18：on_modify + table | setext → `<h{n}>`，table → `<table>`，TABLE_HEAD 不再双写 `<tr>` |
| `demo/android` + `demo/ios` | 修复 F03：O(n²) 全文重解析 | 新增 `MkStreamingParser`，每 token 只 feed 新增部分 |

---

## 架构决策（不可更改）

| 决策 | 结论 |
|---|---|
| 解析器语言 | 纯 C11，无 C++，无 OS-specific 头文件 |
| Markdown 规范 | GFM 基准 + 插件扩展 |
| 解析器架构 | 内部 Push 状态机；对外暴露 Push（回调）+ Pull（delta queue）双 API |
| AST 节点布局 | 基础头 `MkNode` + 类型特化子结构（`MkNode` 必须是第一个字段） |
| 内存管理 | Arena（stable + scratch 两段） |
| 跨语言绑定策略 | 策略 β：事件序列化推送，不暴露裸 C 指针给宿主语言 |
| 渲染解耦 | C 层只产出 AST 事件，渲染适配器各平台独立实现 |

---

## 关键文件导航

```
mk_p/
├── include/mk_parser.h              # 公开 C API（所有绑定只 include 这个）
├── src/                             # C 核心（尽量不动，有 bug 才改）
├── bindings/android/lib/            # Android AAR 库（发布到 mavenLocal）
│   ├── src/main/kotlin/com/mkparser/
│   │   ├── MkParser.kt              # JNI 封装（与 bindings/android/MkParser.kt 同步）
│   │   ├── MkBlock.kt               # 渲染数据模型
│   │   ├── MkBlockParser.kt         # push 事件 → List<MkBlock>
│   │   └── MkBlockAdapter.kt        # RecyclerView Adapter
│   └── build.gradle.kts             # version 字段在第 8 行
├── bindings/android/mk_jni.c        # JNI 桥接（已修复 UTF-8 崩溃）
├── docs/ARCHITECTURE.md             # 完整架构/模块/API 说明
└── RULES.md                         # 编码规范 + 踩坑记录
```

---

## Android 库发布流程

每次修改 `mk_p` C 代码或 Kotlin 绑定后：

1. 在 `bindings/android/lib/build.gradle.kts` 第 8 行 bump version（patch +1）
2. 运行：
   ```bash
   ~/.gradle/wrapper/dists/gradle-8.7-all/64d3p1hmdgljtz5yubyd7rcdx/gradle-8.7/bin/gradle \
     publishToMavenLocal \
     -p "/Users/didi/Library/Mobile Documents/com~apple~CloudDocs/Desktop/personal_p/mk_p/bindings/android/lib"
   ```
3. 提醒用户在 `android-assistant/assistant/build.gradle` 更新版本号

> **原因**：iCloud 路径含空格，Gradle 解析 symlink 后 URI 报错，无法使用 `includeBuild`，只能用 mavenLocal。

---

## 已知遗留事项

| 事项 | 说明 |
|---|---|
| `android-assistant` D3 | `TravelCityView` + `HistoryOrderListDialog` 仍依赖旧 markdown 库，待这两个视图迁移后再删旧依赖 |
| WASM 绑定单实例 | `g_ctx` 为全局静态变量，实际只支持一个 parser 实例，如需多实例需重构为 handle map |
| `memmem` 在 block.c | HTML block 结束判定用了非 C11 标准的 `memmem`，非 GNU 环境需替换为手动扫描 |
| Spec 通过率 40.5% | 剩余失败：Links(64)、Emphasis(61)、List items(43)、Block quotes(23)、Link reference defs(22)；主要缺失：链接引用定义、GFM 列表规则细节、blockquote lazy continuation |

---

## 给新接手 Agent 的建议

- **先读** `docs/ARCHITECTURE.md`：完整模块结构、API 速查、各平台绑定示例
- **再读** `RULES.md`：编码规范 + 四个关键踩坑（JNI UTF-8、ANR、scope 生命周期、Gradle iCloud）
- `src/` 下的 C 代码尽量不动，通过 `include/mk_parser.h` 的 getter/callback 接口消费
- 绑定层改动后必须 bump 版本并重新发布（见上方发布流程）
- 与用户确认任何破坏性操作（删文件、改公开 API、重构 JNI 签名）
