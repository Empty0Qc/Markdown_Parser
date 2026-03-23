// mk_wasm_api.js — JavaScript wrapper for mk_parser WASM (M8a)
//
// Usage (browser or Node.js with ESM/CJS wrapper):
//
//   import createMkParser from './mk_parser.js';
//
//   const Module = await createMkParser();
//   const parser = new MkParser(Module);
//
//   parser.onNodeOpen  = (nodeType, nodePtr) => { ... };
//   parser.onNodeClose = (nodeType, nodePtr) => { ... };
//   parser.onText      = (nodePtr, text)     => { ... };
//   parser.onModify    = (nodeType, nodePtr) => { ... };
//
//   parser.feed("# Hello\n");
//   parser.feed("World\n");
//   parser.finish();
//   parser.destroy();

// Node type constants (mirror MkNodeType enum in mk_parser.h)
export const NodeType = Object.freeze({
  DOCUMENT:        0,
  HEADING:         1,
  PARAGRAPH:       2,
  CODE_BLOCK:      3,
  BLOCK_QUOTE:     4,
  LIST:            5,
  LIST_ITEM:       6,
  THEMATIC_BREAK:  7,
  HTML_BLOCK:      8,
  TABLE:           9,
  TABLE_HEAD:      10,
  TABLE_ROW:       11,
  TABLE_CELL:      12,
  TEXT:            13,
  SOFT_BREAK:      14,
  HARD_BREAK:      15,
  EMPHASIS:        16,
  STRONG:          17,
  STRIKETHROUGH:   18,
  INLINE_CODE:     19,
  LINK:            20,
  IMAGE:           21,
  AUTO_LINK:       22,
  HTML_INLINE:     23,
  TASK_LIST_ITEM:  24,
});

export const Align = Object.freeze({
  NONE:   0,
  LEFT:   1,
  CENTER: 2,
  RIGHT:  3,
});

export const TaskState = Object.freeze({
  NONE:      0,
  UNCHECKED: 1,
  CHECKED:   2,
});

// ── Internal event dispatcher (called from C via EM_ASM) ─────────────────────
// This function is installed on Module before any parser is created.
function installEventDispatcher(Module, parserMap) {
  Module._wasm_on_event = function (evKind, nodePtr, arg2, arg3) {
    // Find the active parser instance (single-instance: use global g_ctx ptr = 1)
    const parser = parserMap.get(1);
    if (!parser) return;

    switch (evKind) {
      case 0: // NODE_OPEN
        parser.onNodeOpen && parser.onNodeOpen(arg2 /* nodeType */, nodePtr);
        break;
      case 1: // NODE_CLOSE
        parser.onNodeClose && parser.onNodeClose(arg2 /* nodeType */, nodePtr);
        break;
      case 2: { // TEXT
        const textPtr = arg2;
        const textLen = arg3;
        const text = Module.UTF8ToString(textPtr, textLen);
        parser.onText && parser.onText(nodePtr, text);
        break;
      }
      case 3: // NODE_MODIFY
        parser.onModify && parser.onModify(arg2 /* nodeType */, nodePtr);
        break;
    }
  };
}

// ── MkParser class ────────────────────────────────────────────────────────────
export class MkParser {
  #mod;
  #handle;
  #enc;

  constructor(Module) {
    this.#mod = Module;
    this.#enc = new TextEncoder();

    // Install event dispatcher if not already done
    if (!Module._parserMap) {
      Module._parserMap = new Map();
      installEventDispatcher(Module, Module._parserMap);
    }

    this.#handle = Module._mk_wasm_create();
    if (!this.#handle) throw new Error('mk_wasm_create failed');
    Module._parserMap.set(this.#handle, this);

    // User callbacks (override these)
    this.onNodeOpen  = null; // (nodeType, nodePtr) => void
    this.onNodeClose = null; // (nodeType, nodePtr) => void
    this.onText      = null; // (nodePtr, text: string) => void
    this.onModify    = null; // (nodeType, nodePtr) => void
  }

  /** Feed a chunk of markdown text. */
  feed(text) {
    const bytes = this.#enc.encode(text);
    const ptr   = this.#mod._malloc(bytes.length);
    this.#mod.HEAPU8.set(bytes, ptr);
    const rc = this.#mod._mk_wasm_feed(ptr, bytes.length);
    this.#mod._free(ptr);
    if (rc !== 0) throw new Error(`mk_wasm_feed returned ${rc}`);
    return this;
  }

  /** Signal end of stream. Must be called after all feed() calls. */
  finish() {
    const rc = this.#mod._mk_wasm_finish();
    if (rc !== 0) throw new Error(`mk_wasm_finish returned ${rc}`);
    return this;
  }

  /** Destroy parser and free all memory. */
  destroy() {
    if (this.#handle) {
      this.#mod._parserMap.delete(this.#handle);
      this.#mod._mk_wasm_destroy();
      this.#handle = 0;
    }
  }

  // ── Node attribute helpers ───────────────────────────────────────────────

  headingLevel  (ptr) { return this.#mod._mk_wasm_heading_level(ptr); }
  codeLang      (ptr) { return this._str(this.#mod._mk_wasm_code_lang(ptr)); }
  codeFenced    (ptr) { return !!this.#mod._mk_wasm_code_fenced(ptr); }
  listOrdered   (ptr) { return !!this.#mod._mk_wasm_list_ordered(ptr); }
  listStart     (ptr) { return this.#mod._mk_wasm_list_start(ptr); }
  tableColCount (ptr) { return this.#mod._mk_wasm_table_col_count(ptr); }
  tableColAlign (ptr, col) { return this.#mod._mk_wasm_table_col_align(ptr, col); }
  cellAlign     (ptr) { return this.#mod._mk_wasm_cell_align(ptr); }
  cellColIndex  (ptr) { return this.#mod._mk_wasm_cell_col_index(ptr); }
  taskChecked   (ptr) { return !!this.#mod._mk_wasm_task_checked(ptr); }
  linkHref      (ptr) { return this._str(this.#mod._mk_wasm_link_href(ptr)); }
  linkTitle     (ptr) { return this._str(this.#mod._mk_wasm_link_title(ptr)); }
  imageSrc      (ptr) { return this._str(this.#mod._mk_wasm_image_src(ptr)); }
  imageAlt      (ptr) { return this._str(this.#mod._mk_wasm_image_alt(ptr)); }
  imageTitle    (ptr) { return this._str(this.#mod._mk_wasm_image_title(ptr)); }
  autolinkUrl   (ptr) { return this._str(this.#mod._mk_wasm_autolink_url(ptr)); }
  autolinkEmail (ptr) { return !!this.#mod._mk_wasm_autolink_email(ptr); }
  htmlBlockRaw  (ptr) { return this._str(this.#mod._mk_wasm_html_raw(ptr)); }
  htmlInlineRaw (ptr) { return this._str(this.#mod._mk_wasm_html_inline_raw(ptr)); }
  textContent   (ptr) { return this._str(this.#mod._mk_wasm_text_content(ptr)); }
  inlineCode    (ptr) { return this._str(this.#mod._mk_wasm_inline_code(ptr)); }

  _str(ptr) {
    if (!ptr) return null;
    return this.#mod.UTF8ToString(ptr);
  }
}

// ── Convenience: parse full string synchronously, return array of events ─────
export function parseFull(Module, markdown) {
  const events = [];
  const parser = new MkParser(Module);
  parser.onNodeOpen  = (type, ptr) => events.push({ kind: 'open',   type, ptr });
  parser.onNodeClose = (type, ptr) => events.push({ kind: 'close',  type, ptr });
  parser.onText      = (ptr, text) => events.push({ kind: 'text',   ptr, text });
  parser.onModify    = (type, ptr) => events.push({ kind: 'modify', type, ptr });
  parser.feed(markdown).finish();
  parser.destroy();
  return events;
}
