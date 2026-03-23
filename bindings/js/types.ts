/**
 * mk-parser — TypeScript type definitions
 *
 * These types mirror the C API (include/mk_parser.h) exactly.
 * Works with both the WASM backend (browser) and NAPI backend (Node.js).
 */

// ── Enumerations ──────────────────────────────────────────────────────────────

export const enum NodeType {
  DOCUMENT        = 0,
  HEADING         = 1,
  PARAGRAPH       = 2,
  CODE_BLOCK      = 3,
  BLOCK_QUOTE     = 4,
  LIST            = 5,
  LIST_ITEM       = 6,
  THEMATIC_BREAK  = 7,
  HTML_BLOCK      = 8,
  TABLE           = 9,
  TABLE_HEAD      = 10,
  TABLE_ROW       = 11,
  TABLE_CELL      = 12,
  TEXT            = 13,
  SOFT_BREAK      = 14,
  HARD_BREAK      = 15,
  EMPHASIS        = 16,
  STRONG          = 17,
  STRIKETHROUGH   = 18,
  INLINE_CODE     = 19,
  LINK            = 20,
  IMAGE           = 21,
  AUTO_LINK       = 22,
  HTML_INLINE     = 23,
  TASK_LIST_ITEM  = 24,
  CUSTOM          = 0x1000,
}

export const enum DeltaType {
  NODE_OPEN   = 0,
  NODE_CLOSE  = 1,
  TEXT        = 2,
  NODE_MODIFY = 3,
}

export const enum Align {
  NONE   = 0,
  LEFT   = 1,
  CENTER = 2,
  RIGHT  = 3,
}

export const enum TaskState {
  NONE      = 0,
  UNCHECKED = 1,
  CHECKED   = 2,
}

// ── Node attribute data (populated by the binding layer) ─────────────────────

export interface BaseNode {
  type: NodeType;
  flags: number;
}

export interface HeadingNode extends BaseNode {
  type: NodeType.HEADING;
  level: 1 | 2 | 3 | 4 | 5 | 6;
}

export interface CodeBlockNode extends BaseNode {
  type: NodeType.CODE_BLOCK;
  lang: string | null;
  fenced: boolean;
}

export interface ListNode extends BaseNode {
  type: NodeType.LIST;
  ordered: boolean;
  start: number;
}

export interface ListItemNode extends BaseNode {
  type: NodeType.LIST_ITEM;
  taskState: TaskState;
}

export interface HtmlBlockNode extends BaseNode {
  type: NodeType.HTML_BLOCK;
  htmlType: number;
  raw: string;
}

export interface TableNode extends BaseNode {
  type: NodeType.TABLE;
  colCount: number;
  colAligns: Align[];
}

export interface TableCellNode extends BaseNode {
  type: NodeType.TABLE_CELL;
  align: Align;
  colIndex: number;
}

export interface TextNode extends BaseNode {
  type: NodeType.TEXT;
  text: string;
}

export interface InlineCodeNode extends BaseNode {
  type: NodeType.INLINE_CODE;
  text: string;
}

export interface LinkNode extends BaseNode {
  type: NodeType.LINK;
  href: string;
  title: string | null;
}

export interface ImageNode extends BaseNode {
  type: NodeType.IMAGE;
  src: string;
  alt: string;
  title: string | null;
}

export interface AutoLinkNode extends BaseNode {
  type: NodeType.AUTO_LINK;
  url: string;
  isEmail: boolean;
}

export interface HtmlInlineNode extends BaseNode {
  type: NodeType.HTML_INLINE;
  raw: string;
}

export interface TaskListItemNode extends BaseNode {
  type: NodeType.TASK_LIST_ITEM;
  checked: boolean;
}

// Node union type
export type AnyNode =
  | HeadingNode
  | CodeBlockNode
  | ListNode
  | ListItemNode
  | HtmlBlockNode
  | TableNode
  | TableCellNode
  | TextNode
  | InlineCodeNode
  | LinkNode
  | ImageNode
  | AutoLinkNode
  | HtmlInlineNode
  | TaskListItemNode
  | BaseNode;

// ── Delta events ──────────────────────────────────────────────────────────────

export interface NodeOpenDelta {
  kind: 'open';
  type: NodeType;
  node: AnyNode;
}

export interface NodeCloseDelta {
  kind: 'close';
  type: NodeType;
  node: AnyNode;
}

export interface TextDelta {
  kind: 'text';
  type: NodeType;
  text: string;
}

export interface NodeModifyDelta {
  kind: 'modify';
  type: NodeType;
  node: AnyNode;
}

export type Delta = NodeOpenDelta | NodeCloseDelta | TextDelta | NodeModifyDelta;

// ── Parser callbacks (Push API) ───────────────────────────────────────────────

export interface ParserCallbacks {
  onNodeOpen?  (type: NodeType, node: AnyNode): void;
  onNodeClose? (type: NodeType, node: AnyNode): void;
  onText?      (type: NodeType, text: string):  void;
  onNodeModify?(type: NodeType, node: AnyNode): void;
}

// ── Parser interface ──────────────────────────────────────────────────────────

export interface IMkParser {
  /** Feed a chunk of Markdown text (streaming). */
  feed(text: string): this;

  /** Signal end of stream. Must be called once after all feed() calls. */
  finish(): this;

  /** Release all resources. The parser must not be used after destroy(). */
  destroy(): void;
}
