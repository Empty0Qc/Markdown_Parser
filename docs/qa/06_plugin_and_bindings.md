# Q&A 06 — 插件 API 与跨语言绑定

**时间**：2026-03-22
**阶段**：Phase 2 技术方案

---

## Q3：Plugin API → vtable（函数指针结构体）✅

插件介入两个时机：

### A. 解析时介入（自定义语法）
```c
typedef struct MkParserPlugin {
    const char *name;
    // 尝试在当前位置解析，返回消耗的字节数，0 表示不匹配
    size_t (*try_block) (MkParser *, MkArena *, const char *src, size_t len, MkNode **out);
    size_t (*try_inline)(MkParser *, MkArena *, const char *src, size_t len, MkNode **out);
} MkParserPlugin;
```

用途示例：`:::tip ... :::` 自定义 block、数学公式 `$...$`

### B. AST 变换时介入（后处理）
```c
typedef struct MkTransformPlugin {
    const char *name;
    void (*on_node_complete)(MkNode *, MkArena *);
} MkTransformPlugin;
```

用途示例：URL 自动转 Link 节点、给 Heading 加 id 属性

---

## Q4：跨语言绑定 → 策略 β（事件序列化推送）✅

### 策略对比

| | α：零拷贝传指针 | β：事件序列化推送 |
|---|---|---|
| 性能 | 极致 | 轻微拷贝（节点边界） |
| JS 侧与 C 耦合 | 强（需知道 struct 布局） | 无 |
| C struct 变更影响 | JS 侧同步修改 | 仅修改序列化层 |
| 适合流式场景 | 一般 | ✅ 天然逐节点推送 |

### 工作方式

```
C Parser（WASM）
    │
    │ 节点完成时序列化为紧凑消息
    ▼
on_event(type, payload)  ← JS 回调
    │
    │ JS 侧重建为原生 JS 对象
    ▼
AST delta（JS 对象树）
```

流式场景下，拷贝发生在节点完成的边界处，不在热路径上，开销可接受。
JS 侧与 C 内存完全解耦，C 层重构不影响 JS 消费方。

---

## Phase 2 决策汇总

| 决策点 | 结论 |
|---|---|
| 解析器语言 | C |
| Markdown 规范 | GFM 基准 + 自定义扩展 |
| 解析器架构 | A+B 混合（Push 状态机内部，Push+Pull 双 API 对外） |
| AST 节点布局 | 方案 Y（基础头 + 类型特化子结构） |
| 内存管理 | Arena 分配器（stable + scratch 两段） |
| 插件机制 | vtable（函数指针结构体） |
| 跨语言绑定 | 策略 β（事件序列化推送） |
| 解析-渲染解耦 | AST delta queue（16ms 帧预算驱动渲染） |
| 性能目标 | 解析器 <5ms/token，渲染器 60fps 帧预算 |
