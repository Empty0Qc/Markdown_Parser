# Q&A 05 — AST 设计与内存管理

**时间**：2026-03-22
**阶段**：Phase 2 技术方案

---

## AST 节点类型（GFM 完整覆盖）

### Block 级
| 节点 | 说明 |
|---|---|
| Document | 根节点 |
| Heading | level 1–6 |
| Paragraph | |
| CodeBlock | fenced / indented，带 language 属性 |
| BlockQuote | |
| List | ordered / unordered |
| ListItem | |
| ThematicBreak | `---` / `***` |
| HtmlBlock | |
| Table | GFM |
| TableHead / TableRow / TableCell | align 属性 |

### Inline 级
| 节点 | 说明 |
|---|---|
| Text | |
| SoftBreak / HardBreak | |
| Emphasis | `*` or `_` |
| Strong | `**` or `__` |
| Strikethrough | GFM, `~~` |
| InlineCode | `` ` `` |
| Link | href, title |
| Image | src, alt, title |
| HtmlInline | |
| TaskListItem | GFM, `[ ]` / `[x]` |

---

## Q1：节点内存布局 → 方案 Y（基础头 + 类型特化子结构）✅

```c
// 基础节点头（所有节点共有）
typedef struct MkNode {
    MkNodeType     type;
    uint32_t       flags;
    struct MkNode *parent;
    struct MkNode *first_child;
    struct MkNode *next_sibling;
    size_t         src_begin;   // 原始文本范围，用于增量 diff
    size_t         src_end;
} MkNode;

// 类型特化子结构（MkNode 必须是第一个字段）
typedef struct MkHeadingNode {
    MkNode base;
    int    level;
} MkHeadingNode;

typedef struct MkCodeBlockNode {
    MkNode  base;
    char   *lang;
    size_t  lang_len;
} MkCodeBlockNode;

typedef struct MkLinkNode {
    MkNode  base;
    char   *href;
    char   *title;
} MkLinkNode;
```

**优势**：第三方插件可定义自己的节点类型，只要 `MkNode` 是第一个字段即可接入整个树操作 API。

---

## Q2：内存所有权 → Arena 分配器 ✅

### 三方案对比结论

| | Parser 持有 | 调用方持有 | Arena |
|---|---|---|---|
| 实现复杂度 | 低 | 中 | 中 |
| 大文档内存可控 | ❌ | ✅ | ✅ |
| 插件扩展友好 | 中 | 差 | ✅ |
| WASM/跨语言绑定 | 差 | 差 | ✅ |
| 调用方负担 | 零 | 重 | 轻 |
| 工业案例 | 小型解析器 | 少见 | tree-sitter |

### Arena 分段策略

```
mk_arena_t
  ├── stable_arena  — 已完成节点（paragraph 后跟空行、闭合 code block 等）
  │     └── 可按"已完成块"粒度整体释放，支持大文档场景
  └── scratch_arena — 当前正在构建的节点（pending，可能被回溯修改）
        └── token 到来时在此分配，节点完成时迁移到 stable_arena
```

调用方只管 `mk_arena_t` 的生命周期，不追踪任何单个节点。

---

## 参考：tree-sitter 的印证

tree-sitter 是增量解析的工业标杆，同样采用：
- 基础节点头 + 类型特化
- Arena/node pool 内存管理
- Push 状态机内部 + Pull API 外部
