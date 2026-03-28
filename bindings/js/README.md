# mk-parser

> Incremental Markdown parser for AI streaming scenarios — WebAssembly powered, zero dependencies.

Parses GFM (GitHub Flavored Markdown) in real time as tokens arrive, making it ideal for rendering LLM output chunk-by-chunk. Built on a C11 parser core compiled to WebAssembly.

## Features

- **Streaming-first**: feed input in arbitrary byte chunks (even 1 byte at a time)
- **Push + Pull API**: register callbacks or poll a delta queue
- **GFM-complete**: headings, paragraphs, lists, tables, code blocks, task lists, blockquotes, inline emphasis/code/links/images
- **Zero dependencies**: only the compiled `.wasm` file is needed at runtime
- **TypeScript types** included

## Installation

```bash
npm install mk-parser
```

> **Note:** The package ships pre-built WASM. No native compilation required.

## Quick start (ESM)

```js
import createMkParser from 'mk-parser/wasm-api';   // Emscripten factory
import { parseFull } from 'mk-parser';

const Module = await createMkParser();

const events = parseFull(Module, '# Hello\n\nWorld **wide** web.\n');
for (const ev of events) {
  console.log(ev.kind, ev.type, ev.text ?? ev.node);
}
```

## Quick start (CommonJS)

```js
const getMkParser = require('mk-parser');

async function main() {
  const { parseFull } = await getMkParser();
  // same as ESM from here
}
main();
```

## Streaming / Push API

```js
import createMkParser from 'mk-parser/wasm-api';
import { WasmMkParser } from 'mk-parser';

const Module = await createMkParser();

const parser = new WasmMkParser(Module, {
  onNodeOpen(type, node) {
    console.log('open', type, node);
  },
  onText(type, text) {
    process.stdout.write(text);
  },
  onNodeClose(type) {
    console.log('close', type);
  },
});

// Feed data in chunks (simulating an LLM token stream)
for (const chunk of chunks) {
  parser.feed(chunk);
}
parser.finish();
parser.destroy();
```

## API Reference

### `parseFull(wasmModule, markdown) → Delta[]`

Parse a complete markdown string synchronously and return all delta events.

| Parameter    | Type     | Description                        |
|--------------|----------|------------------------------------|
| `wasmModule` | `object` | Resolved Emscripten module         |
| `markdown`   | `string` | Input markdown text                |

Returns an array of `Delta` objects (see types).

### `new WasmMkParser(wasmModule, callbacks?)`

Create a streaming push-mode parser.

| Method              | Description                                        |
|---------------------|----------------------------------------------------|
| `.feed(text)`       | Feed a chunk of markdown text                      |
| `.finish()`         | Signal end of stream (required, triggers flush)    |
| `.destroy()`        | Release WASM memory                                |

### `streamParse(wasmModule, markdown, chunkSize?) → AsyncGenerator<Delta>`

Async generator that yields events as the markdown is fed in chunks.

### Delta types

| Kind       | Fields                          |
|------------|---------------------------------|
| `'open'`   | `type`, `node`                  |
| `'close'`  | `type`, `node`                  |
| `'text'`   | `type`, `text`                  |
| `'modify'` | `type`, `node` (attribute update)|

### Node types

`NodeType` enum (values 0–24): `DOCUMENT`, `HEADING`, `PARAGRAPH`, `CODE_BLOCK`, `BLOCK_QUOTE`, `LIST`, `LIST_ITEM`, `THEMATIC_BREAK`, `HTML_BLOCK`, `TABLE`, `TABLE_HEAD`, `TABLE_ROW`, `TABLE_CELL`, `TEXT`, `SOFT_BREAK`, `HARD_BREAK`, `EMPHASIS`, `STRONG`, `STRIKETHROUGH`, `INLINE_CODE`, `LINK`, `IMAGE`, `AUTO_LINK`, `HTML_INLINE`, `TASK_LIST_ITEM`.

## Building from source

```bash
# Install Emscripten (https://emscripten.org/docs/getting_started/)
source /path/to/emsdk/emsdk_env.sh

# Build WASM and copy artifacts
npm run build:wasm
npm run copy:wasm
```

## License

MIT
