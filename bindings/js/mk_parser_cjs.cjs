/**
 * mk_parser_cjs.cjs — CommonJS shim for mk-parser
 *
 * Allows require('mk-parser') to work in CommonJS environments
 * (Node.js without --experimental-vm-modules, Jest, etc.) by
 * bridging to the ESM WASM module via dynamic import().
 *
 * Usage:
 *   const { WasmMkParser, parseFull } = require('mk-parser');
 *   // All exports are wrapped in a Promise
 *   parseFull(module, '# Hello').then(events => console.log(events));
 */
'use strict';

// Dynamic import() is available in Node.js 12+ even in CJS context.
// We export a factory that resolves on first call.
let _modulePromise = null;

function load() {
  if (!_modulePromise) {
    _modulePromise = import('./mk_parser_wasm.js');
  }
  return _modulePromise;
}

/**
 * Async factory — resolves to the ESM module's named exports.
 *
 * @returns {Promise<{WasmMkParser, parseFull, streamParse, NodeType, Align, TaskState}>}
 */
async function getMkParser() {
  return load();
}

module.exports = getMkParser;

// Also expose named helpers so callers can do:
//   const mkParser = require('mk-parser');
//   const { parseFull } = await mkParser();
module.exports.default      = getMkParser;
module.exports.getMkParser  = getMkParser;

// Convenience async wrappers for the most common API surface.

/**
 * Parse a full markdown string and return an event array.
 *
 * @param {object} wasmModule  — resolved Emscripten module (from createMkParser())
 * @param {string} markdown
 * @returns {Promise<Array>}
 */
module.exports.parseFull = async function parseFull(wasmModule, markdown) {
  const mod = await load();
  return mod.parseFull(wasmModule, markdown);
};

/**
 * Create a streaming push-mode parser.
 * Returns a WasmMkParser instance once the module is loaded.
 *
 * @param {object} wasmModule
 * @param {object} callbacks  — { onNodeOpen, onNodeClose, onText, onNodeModify }
 * @returns {Promise<WasmMkParser>}
 */
module.exports.createParser = async function createParser(wasmModule, callbacks) {
  const mod = await load();
  return new mod.WasmMkParser(wasmModule, callbacks);
};
