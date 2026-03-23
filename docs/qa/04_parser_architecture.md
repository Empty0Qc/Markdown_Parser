# Q&A 04 — 解析器架构选型

**时间**：2026-03-22
**阶段**：Phase 2 技术方案

---

## 讨论背景

流式 Markdown 解析器需要是一个可暂停/可恢复的状态机。

---

## 三种方案对比

### 方案 A：Push Parser（推式，回调驱动）
```c
void mk_feed(Parser *p, const char *chunk, size_t len);
p->on_node_open  = my_callback;
p->on_node_close = my_callback;
```
- 优点：天然流式，内存占用低，业内最成熟
- 缺点：调用方被动，插件介入需要回调链设计

### 方案 B：Pull Parser + 内部缓冲
```c
void mk_feed(Parser *p, const char *chunk, size_t len);
AstDelta *mk_pull_delta(Parser *p);  // NULL 表示暂无变化
```
- 优点：调用方控制节奏，易于组合，插件可在 pull 时介入
- 缺点：需要维护内部 delta queue

### 方案 C：Coroutine/Continuation 风格

C 里的具体实现路线及致命缺陷：

| 路线 | 问题 |
|---|---|
| Protothreads / Duff's Device | 只能顶层 yield，Markdown 递归文法下直接致命 |
| `ucontext` 完整栈切换 | WASM 不支持，Emscripten Asyncify 有 10–30% 性能损耗 |
| 手写状态表 | 代码量是递归的 3–5 倍，调试极难 |

**结论**：C + WASM 目标下，方案 C 无实际优势。

---

## 业内参考

| 项目 | 方案 | 备注 |
|---|---|---|
| expat (XML) | Push 回调 | 最经典流式解析器 |
| llhttp (Node.js HTTP) | Push 回调 | 取代 http_parser |
| md4c (Qt) | Push 回调 | 目前最快 C Markdown 解析器 |
| micromark (JS) | Pull 状态机 | 专为流式设计 |
| tree-sitter | Pull + 增量 | 增量解析工业标杆 |

C 生态流式解析器，业内绝大多数选 Push 回调方案。

---

## 决策：A+B 混合方案 ✅

```
内部：Push 状态机（维护解析状态，逐字节推进）
外部 API 暴露两层：
  ├── Push API：on_node_open / on_node_close 回调（低延迟调用方）
  └── Pull API：mk_pull_delta()（需要控制节奏的调用方）
```

内部统一用 Push 状态机实现，Pull API 是在其上加一层 delta queue 的薄封装。

---

## 待设计（进入 Phase 2）

1. AST 节点类型定义与内存布局
2. delta 的数据结构（Open/Close/Text/Modify）
3. 插件 API（函数指针 / vtable）
4. 跨语言绑定策略（C → JS/其他语言）
5. 内存所有权策略
