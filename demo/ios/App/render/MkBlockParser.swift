// MkBlockParser.swift — Push-event state machine: MkParser → [MkBlock]
//
// Logic mirrors the Android BlockBuilder in MkBlockParser.kt exactly.
// Converts mk_parser push-events into a flat list of MkBlock values
// for direct rendering by MkRenderView.

import Foundation
import SwiftUI
import MkParser   // bindings/ios/MkParser.swift (via Package.swift)

// ── Colours for inline spans (Catppuccin Mocha palette) ─────────────────────

private let colLink   = Color(red: 0x89/255.0, green: 0xDC/255.0, blue: 0xEB/255.0)
private let colCode   = Color(red: 0xCB/255.0, green: 0xA6/255.0, blue: 0xF7/255.0)
private let colCodeBg = Color(red: 0x31/255.0, green: 0x32/255.0, blue: 0x44/255.0)

// ── Public parser ─────────────────────────────────────────────────────────────

enum MkBlockParser {

    /// Parse a complete markdown string and return a flat block list.
    static func parse(_ markdown: String, isStreaming: Bool = false) -> [MkBlock] {
        guard !markdown.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return [] }
        let builder = BlockBuilder()
        let parser  = try? MkParser(
            onNodeOpen:  { type, info  in builder.onOpen(type: type, info: info)  },
            onNodeClose: { type, _     in builder.onClose(type: type) },
            onText:      { text        in builder.onText(text) },
            onModify:    { _, _        in }
        )
        try? parser?.feed(markdown)
        try? parser?.finish()
        parser?.destroy()
        let blocks = builder.build()
        return isStreaming ? blocks.markLastStreaming() : blocks
    }
}

// ── Internal state machine ────────────────────────────────────────────────────

private class BlockBuilder {

    private var blocks: [MkBlock] = []
    private var typeCounts: [String: Int] = [:]

    // Block-level stack
    private struct Frame {
        let type: MkNodeType
        let info: MkNodeInfo
        let id:   String
    }
    private var stack: [Frame] = []

    // Inline text builder (owned by the innermost content frame)
    private var ib: AttributedString?

    // Stack of inline span closures (to "pop" applied styles on close)
    private var ibRangeStarts: [(AttributedString.Index, Int)] = []
                              // (start index, push-depth) for pop

    // List context
    private struct ListCtx {
        let ordered: Bool
        var itemNum: Int = 0
    }
    private var listStack: [ListCtx] = []

    // Task state from TASK_LIST_ITEM sibling, consumed by LIST_ITEM close
    private var pendingTaskState: Int = 0

    // Table state
    private var tableHeaders:  [AttributedString] = []
    private var tableRows:     [[AttributedString]] = []
    private var currentRow:    [AttributedString] = []
    private var inTableHead    = false

    // ── ID generation ─────────────────────────────────────────────────────────

    private func newId(_ prefix: String) -> String {
        let n = typeCounts[prefix, default: 0]
        typeCounts[prefix] = n + 1
        return "\(prefix)_\(n)"
    }

    // ── Container check ───────────────────────────────────────────────────────

    private let containerTypes: Set<MkNodeType> = [
        .listItem, .taskListItem, .blockQuote, .tableCell
    ]
    private func insideContainer() -> Bool {
        stack.contains { containerTypes.contains($0.type) }
    }

    // ── Event handlers ────────────────────────────────────────────────────────

    func onOpen(type: MkNodeType, info: MkNodeInfo) {
        let id = newId(typeKey(type))
        stack.append(Frame(type: type, info: info, id: id))

        switch type {

        // Block openers that own the inline builder
        case .heading:
            ib = AttributedString()

        case .codeBlock:
            ib = AttributedString()

        case .blockQuote:
            ib = AttributedString()

        case .paragraph:
            if !insideContainer() { ib = AttributedString() }

        case .listItem:
            pendingTaskState = 0
            ib = AttributedString()

        case .taskListItem:
            // Leaf marker — store task state; LIST_ITEM owns the builder
            pendingTaskState = info.taskState == .checked ? 2 :
                               info.taskState == .unchecked ? 1 : 0
            if ib == nil { ib = AttributedString() }

        case .list:
            listStack.append(ListCtx(ordered: info.ordered ?? false))

        case .table:
            tableHeaders = []; tableRows = []

        case .tableHead:
            inTableHead = true

        case .tableRow:
            if !inTableHead { currentRow = [] }

        case .tableCell:
            ib = AttributedString()

        case .thematicBreak:
            break  // leaf — handled on close

        // ── Inline span opens ──────────────────────────────────────────────────

        case .strong:
            guard var current = ib else { ibRangeStarts.append((AttributedString.Index(utf16Offset: 0, in: AttributedString()), 0)); break }
            let start = current.endIndex
            ib = current
            ibRangeStarts.append((start, 1))

        case .emphasis:
            guard var current = ib else { ibRangeStarts.append((AttributedString.Index(utf16Offset: 0, in: AttributedString()), 0)); break }
            let start = current.endIndex
            ib = current
            ibRangeStarts.append((start, 1))

        case .strikethrough:
            guard var current = ib else { ibRangeStarts.append((AttributedString.Index(utf16Offset: 0, in: AttributedString()), 0)); break }
            let start = current.endIndex
            ib = current
            ibRangeStarts.append((start, 1))

        case .inlineCode:
            guard var current = ib else { ibRangeStarts.append((AttributedString.Index(utf16Offset: 0, in: AttributedString()), 0)); break }
            let start = current.endIndex
            ib = current
            ibRangeStarts.append((start, 1))

        case .link, .autoLink:
            guard var current = ib else { ibRangeStarts.append((AttributedString.Index(utf16Offset: 0, in: AttributedString()), 0)); break }
            let start = current.endIndex
            ib = current
            ibRangeStarts.append((start, 1))

        case .softBreak:
            ib?.append(AttributedString(" "))

        case .hardBreak:
            ib?.append(AttributedString("\n"))

        default:
            break
        }
    }

    func onClose(type: MkNodeType) {
        guard let frame = stack.last else { return }
        stack.removeLast()

        switch type {

        // ── Inline span close — apply accumulated style to the range ──────────

        case .strong:
            applyInlineStyle(frame: frame, type: .strong)

        case .emphasis:
            applyInlineStyle(frame: frame, type: .emphasis)

        case .strikethrough:
            applyInlineStyle(frame: frame, type: .strikethrough)

        case .inlineCode:
            applyInlineStyle(frame: frame, type: .inlineCode)

        case .link, .autoLink:
            applyInlineStyle(frame: frame, type: .link, href: frame.info.href ?? frame.info.url ?? "")

        // ── Block emitters ────────────────────────────────────────────────────

        case .heading:
            let content = ib ?? AttributedString(); ib = nil
            blocks.append(.heading(id: frame.id, level: Int(frame.info.level ?? 1), content: content))

        case .paragraph:
            if !insideContainer() {
                let content = ib ?? AttributedString(); ib = nil
                blocks.append(.paragraph(id: frame.id, content: content))
            }

        case .codeBlock:
            var text = ib.map { String($0.characters) } ?? ""; ib = nil
            if text.hasSuffix("\n") { text = String(text.dropLast()) }
            blocks.append(.fencedCode(id: frame.id, language: frame.info.lang ?? "", code: text))

        case .blockQuote:
            let content = ib ?? AttributedString(); ib = nil
            blocks.append(.blockQuote(id: frame.id, content: content))

        case .listItem:
            let content = ib ?? AttributedString(); ib = nil
            if let ctx = listStack.last, ctx.ordered {
                listStack[listStack.count - 1].itemNum += 1
                let num = listStack[listStack.count - 1].itemNum
                blocks.append(.orderedItem(id: frame.id, depth: listStack.count - 1, number: num, content: content))
            } else {
                let ts = pendingTaskState != 0 ? pendingTaskState : (frame.info.taskState == .checked ? 2 : frame.info.taskState == .unchecked ? 1 : 0)
                pendingTaskState = 0
                blocks.append(.bulletItem(id: frame.id, depth: listStack.count - 1, taskState: ts, content: content))
            }

        case .taskListItem:
            let hasListItemParent = stack.contains { $0.type == .listItem }
            if !hasListItemParent {
                let content = ib ?? AttributedString(); ib = nil
                let ts = pendingTaskState != 0 ? pendingTaskState : (frame.info.taskState == .checked ? 2 : 1)
                blocks.append(.bulletItem(id: frame.id, depth: listStack.count - 1, taskState: ts, content: content))
            }

        case .list:
            listStack.removeLast()

        case .thematicBreak:
            blocks.append(.thematicBreak(id: frame.id))

        case .tableCell:
            let content = ib ?? AttributedString(); ib = nil
            if inTableHead { tableHeaders.append(content) } else { currentRow.append(content) }

        case .tableRow:
            if !inTableHead { tableRows.append(currentRow); currentRow = [] }

        case .tableHead:
            inTableHead = false

        case .table:
            blocks.append(.tableBlock(id: frame.id, headers: tableHeaders, rows: tableRows))

        default:
            break
        }
    }

    func onText(_ text: String) {
        ib?.append(AttributedString(text))
    }

    func build() -> [MkBlock] { blocks }

    // ── Style application ─────────────────────────────────────────────────────

    private func applyInlineStyle(frame: Frame, type: MkNodeType, href: String = "") {
        guard var current = ib else { _ = ibRangeStarts.popLast(); return }
        guard let (startIndex, _) = ibRangeStarts.popLast() else { return }

        // Clamp start to valid range
        let safeStart = startIndex <= current.endIndex ? startIndex : current.startIndex
        let range = safeStart..<current.endIndex

        switch type {
        case .strong:
            current[range].font = .body.bold()
        case .emphasis:
            current[range].font = .body.italic()
        case .strikethrough:
            current[range].strikethroughStyle = Text.LineStyle(pattern: .solid)
        case .inlineCode:
            current[range].font = .system(.body, design: .monospaced)
            current[range].foregroundColor = UIColor(colCode)
            current[range].backgroundColor = UIColor(colCodeBg)
        case .link, .autoLink:
            current[range].foregroundColor = UIColor(colLink)
            current[range].underlineStyle  = .single
            if !href.isEmpty, let url = URL(string: href) {
                current[range].link = url
            }
        default:
            break
        }

        ib = current
    }

    // ── Type key ──────────────────────────────────────────────────────────────

    private func typeKey(_ type: MkNodeType) -> String {
        switch type {
        case .heading:      return "H"
        case .paragraph:    return "P"
        case .codeBlock:    return "CODE"
        case .blockQuote:   return "BQ"
        case .listItem, .taskListItem: return "LI"
        case .thematicBreak: return "HR"
        case .table:        return "TBL"
        case .tableCell:    return "TD"
        default:            return "X"
        }
    }
}
