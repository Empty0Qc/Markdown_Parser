// index.js — mk-parser-native entry point
//
// Loads the native addon and re-exports it with a friendly API.

'use strict';

let native;
try {
  native = require('./build/Release/mk_parser_native');
} catch (_) {
  try {
    native = require('./build/Debug/mk_parser_native');
  } catch (e) {
    throw new Error(
      'mk-parser-native: native addon not built. Run `npm run build` first.\n' + e.message
    );
  }
}

const { MkParser, NodeType } = native;

module.exports = { MkParser, NodeType };
