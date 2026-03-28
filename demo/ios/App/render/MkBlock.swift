// MkBlock.swift — Block-level Markdown model for SwiftUI rendering
//
// Swift enum with associated values mirrors the Kotlin sealed class in the
// Android demo (demo/android/.../render/MkBlock.kt).
//
// Each block has a stable `id` (type + ordinal) so SwiftUI `List` can use
// it as a `Identifiable` key and avoid unnecessary re-renders during streaming.

import Foundation
import SwiftUI

// ── Block model ───────────────────────────────────────────────────────────────

/// A block-level Markdown node ready for direct rendering.
enum MkBlock: Identifiable {

    case heading(id: String, level: Int, content: AttributedString, isStreaming: Bool = false)
    case paragraph(id: String, content: AttributedString, isStreaming: Bool = false)
    case fencedCode(id: String, language: String, code: String, isStreaming: Bool = false)
    case bulletItem(id: String, depth: Int, taskState: Int, content: AttributedString, isStreaming: Bool = false)
    case orderedItem(id: String, depth: Int, number: Int, content: AttributedString, isStreaming: Bool = false)
    case blockQuote(id: String, content: AttributedString, isStreaming: Bool = false)
    case thematicBreak(id: String)
    case tableBlock(id: String, headers: [AttributedString], rows: [[AttributedString]], isStreaming: Bool = false)

    var id: String {
        switch self {
        case .heading(let id, _, _, _):           return id
        case .paragraph(let id, _, _):            return id
        case .fencedCode(let id, _, _, _):        return id
        case .bulletItem(let id, _, _, _, _):     return id
        case .orderedItem(let id, _, _, _, _):    return id
        case .blockQuote(let id, _, _):           return id
        case .thematicBreak(let id):              return id
        case .tableBlock(let id, _, _, _):        return id
        }
    }

    var isStreaming: Bool {
        switch self {
        case .heading(_, _, _, let s):        return s
        case .paragraph(_, _, let s):         return s
        case .fencedCode(_, _, _, let s):     return s
        case .bulletItem(_, _, _, _, let s):  return s
        case .orderedItem(_, _, _, _, let s): return s
        case .blockQuote(_, _, let s):        return s
        case .thematicBreak:                  return false
        case .tableBlock(_, _, _, let s):     return s
        }
    }

    /// Return a copy with `isStreaming` set to `true`.
    func markStreaming() -> MkBlock {
        switch self {
        case .heading(let id, let l, let c, _):       return .heading(id: id, level: l, content: c, isStreaming: true)
        case .paragraph(let id, let c, _):            return .paragraph(id: id, content: c, isStreaming: true)
        case .fencedCode(let id, let lang, let c, _): return .fencedCode(id: id, language: lang, code: c, isStreaming: true)
        case .bulletItem(let id, let d, let ts, let c, _):  return .bulletItem(id: id, depth: d, taskState: ts, content: c, isStreaming: true)
        case .orderedItem(let id, let d, let n, let c, _):  return .orderedItem(id: id, depth: d, number: n, content: c, isStreaming: true)
        case .blockQuote(let id, let c, _):           return .blockQuote(id: id, content: c, isStreaming: true)
        case .thematicBreak:                          return self
        case .tableBlock(let id, let h, let r, _):   return .tableBlock(id: id, headers: h, rows: r, isStreaming: true)
        }
    }
}

extension Array where Element == MkBlock {
    /// Mark the last block as streaming (for blink-cursor rendering).
    func markLastStreaming() -> [MkBlock] {
        guard !isEmpty else { return self }
        var copy = self
        copy[copy.count - 1] = copy.last!.markStreaming()
        return copy
    }
}
