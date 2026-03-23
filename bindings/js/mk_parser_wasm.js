/**
 * mk-parser — WASM-backed push/pull JS wrapper (M9)
 *
 * This module wraps the low-level WASM API (bindings/wasm/mk_wasm_api.js)
 * with a typed, ergonomic interface.
 *
 * Usage (browser / Node.js with ESM):
 *
 *   import createMkParser from './mk_parser.js';   // generated WASM
 *   import { WasmMkParser, parseFull, streamParse } from './mk_parser_wasm.js';
 *
 *   const Module = await createMkParser();
 *
 *   // Push API
 *   const parser = new WasmMkParser(Module, {
 *     onNodeOpen(type, node) { console.log('open', type, node); },
 *     onText(type, text)     { console.log('text', text);      },
 *   });
 *   for await (const chunk of asyncIterable) parser.feed(chunk);
 *   parser.finish();
 *   parser.destroy();
 *
 *   // Pull / batch API
 *   const events = await parseFull(Module, '# Hello\nWorld\n');
 */

import { MkParser as LowLevelParser, NodeType, parseFull as llParseFull }
    from '../wasm/mk_wasm_api.js';

// Re-export types
export { NodeType };

export class WasmMkParser {
    #low;

    constructor(Module, callbacks = {}) {
        this.#low = new LowLevelParser(Module);

        if (callbacks.onNodeOpen) {
            this.#low.onNodeOpen = (type, ptr) => {
                const node = this.#buildNode(type, ptr);
                callbacks.onNodeOpen(type, node);
            };
        }
        if (callbacks.onNodeClose) {
            this.#low.onNodeClose = (type, ptr) => {
                const node = this.#buildNode(type, ptr);
                callbacks.onNodeClose(type, node);
            };
        }
        if (callbacks.onText) {
            this.#low.onText = (_ptr, text) => callbacks.onText(text);
        }
        if (callbacks.onModify) {
            this.#low.onModify = (type, ptr) => {
                const node = this.#buildNode(type, ptr);
                callbacks.onModify(type, node);
            };
        }
    }

    feed(text) { this.#low.feed(text); return this; }
    finish()   { this.#low.finish();   return this; }
    destroy()  { this.#low.destroy(); }

    // Build a rich node object from a C node pointer
    #buildNode(type, ptr) {
        const node = { type, flags: 0 };
        const p = this.#low;

        switch (type) {
        case NodeType.HEADING:
            node.level = p.headingLevel(ptr);
            break;
        case NodeType.CODE_BLOCK:
            node.lang   = p.codeLang(ptr);
            node.fenced = p.codeFenced(ptr);
            break;
        case NodeType.LIST:
            node.ordered = p.listOrdered(ptr);
            node.start   = p.listStart(ptr);
            break;
        case NodeType.LIST_ITEM:
            node.taskState = p.listOrdered ? 0 : 0; // MK_TASK_NONE default
            break;
        case NodeType.TABLE:
            node.colCount  = p.tableColCount(ptr);
            node.colAligns = Array.from({ length: node.colCount },
                                        (_, i) => p.tableColAlign(ptr, i));
            break;
        case NodeType.TABLE_CELL:
            node.align    = p.cellAlign(ptr);
            node.colIndex = p.cellColIndex(ptr);
            break;
        case NodeType.LINK:
            node.href  = p.linkHref(ptr);
            node.title = p.linkTitle(ptr);
            break;
        case NodeType.IMAGE:
            node.src   = p.imageSrc(ptr);
            node.alt   = p.imageAlt(ptr);
            node.title = p.imageTitle(ptr);
            break;
        case NodeType.AUTO_LINK:
            node.url     = p.autolinkUrl(ptr);
            node.isEmail = p.autolinkEmail(ptr);
            break;
        case NodeType.TEXT:
            node.text = p.textContent(ptr);
            break;
        case NodeType.INLINE_CODE:
            node.text = p.inlineCode(ptr);
            break;
        case NodeType.HTML_BLOCK:
            node.raw = p.htmlBlockRaw(ptr);
            break;
        case NodeType.HTML_INLINE:
            node.raw = p.htmlInlineRaw(ptr);
            break;
        case NodeType.TASK_LIST_ITEM:
            node.checked = p.taskChecked(ptr);
            break;
        }
        return node;
    }
}

/**
 * Parse a full Markdown string and return an array of delta events.
 * @param {object} Module  - Emscripten module from createMkParser()
 * @param {string} markdown
 * @returns {Array<{kind, type, node?, text?}>}
 */
export function parseFull(Module, markdown) {
    return llParseFull(Module, markdown);
}

/**
 * Parse a full Markdown string and reconstruct an in-memory AST.
 * Returns the root Document node.
 *
 * AST node shape: { type, ...attrs, children: ASTNode[] }
 */
export function parseToAST(Module, markdown) {
    const stack = [{ type: NodeType.DOCUMENT, children: [] }];

    const parser = new WasmMkParser(Module, {
        onNodeOpen(type, node) {
            const n = { ...node, children: [] };
            stack[stack.length - 1].children.push(n);
            stack.push(n);
        },
        onNodeClose() {
            if (stack.length > 1) stack.pop();
        },
        onText(text) {
            const parent = stack[stack.length - 1];
            if (!parent.text) parent.text = '';
            parent.text += text;
        },
        onModify(type, node) {
            // Replace top of stack's type/attrs (paragraph→table promotion etc.)
            const top = stack[stack.length - 1];
            Object.assign(top, node);
        },
    });

    parser.feed(markdown).finish();
    parser.destroy();

    return stack[0]; // Document node
}
