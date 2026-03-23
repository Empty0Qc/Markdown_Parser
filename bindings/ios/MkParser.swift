// MkParser.swift — Swift wrapper for the mk_parser C library (M8d)
//
// Wraps the C Push API with a Swift-idiomatic closure-based interface.
//
// Usage:
//
//   let parser = MkParser(
//       onNodeOpen:  { type, node in print("open", type, node) },
//       onNodeClose: { type, node in print("close", type) },
//       onText:      { text in     print("text", text)       },
//       onModify:    { type, node in print("modify", type)   }
//   )
//   try parser.feed("# Hello\n")
//   try parser.feed("World\n")
//   try parser.finish()
//   parser.destroy()

import Foundation
import mk_parser   // module map in include/module.modulemap

// ── Node type mirror ──────────────────────────────────────────────────────────

public enum MkNodeType: Int32 {
    case document       = 0
    case heading        = 1
    case paragraph      = 2
    case codeBlock      = 3
    case blockQuote     = 4
    case list           = 5
    case listItem       = 6
    case thematicBreak  = 7
    case htmlBlock      = 8
    case table          = 9
    case tableHead      = 10
    case tableRow       = 11
    case tableCell      = 12
    case text           = 13
    case softBreak      = 14
    case hardBreak      = 15
    case emphasis       = 16
    case strong         = 17
    case strikethrough  = 18
    case inlineCode     = 19
    case link           = 20
    case image          = 21
    case autoLink       = 22
    case htmlInline     = 23
    case taskListItem   = 24
}

public enum MkAlign: Int32 {
    case none   = 0
    case left   = 1
    case center = 2
    case right  = 3
}

public enum MkTaskState: Int32 {
    case none      = 0
    case unchecked = 1
    case checked   = 2
}

// ── Rich node info ────────────────────────────────────────────────────────────

public struct MkNodeInfo {
    public let type:    MkNodeType
    public let flags:   Int32

    // Heading
    public var level:       Int32?
    // CodeBlock
    public var lang:        String?
    public var fenced:      Bool?
    // List
    public var ordered:     Bool?
    public var listStart:   Int32?
    // ListItem / TaskListItem
    public var taskState:   MkTaskState?
    public var checked:     Bool?
    // Table
    public var colCount:    Int32?
    public var colAligns:   [MkAlign]?
    // TableCell
    public var align:       MkAlign?
    public var colIndex:    Int32?
    // Link / Image / AutoLink
    public var href:        String?
    public var src:         String?
    public var alt:         String?
    public var url:         String?
    public var title:       String?
    public var isEmail:     Bool?
    // Text / InlineCode / Html
    public var text:        String?
    public var raw:         String?

    init(rawNode ptr: OpaquePointer!, type: MkNodeType, flags: Int32) {
        self.type  = type
        self.flags = flags
        switch type {
        case .heading:
            level = mk_node_heading_level(ptr)
        case .codeBlock:
            lang   = mk_node_code_lang(ptr).map(String.init(cString:))
            fenced = mk_node_code_fenced(ptr) != 0
        case .list:
            ordered   = mk_node_list_ordered(ptr) != 0
            listStart = mk_node_list_start(ptr)
        case .listItem:
            taskState = MkTaskState(rawValue: mk_node_list_item_task_state(ptr).rawValue)
        case .table:
            let n = mk_node_table_col_count(ptr)
            colCount  = Int32(n)
            colAligns = (0 ..< n).map { i in
                MkAlign(rawValue: mk_node_table_col_align(ptr, i).rawValue) ?? .none
            }
        case .tableCell:
            align    = MkAlign(rawValue: mk_node_table_cell_align(ptr).rawValue)
            colIndex = Int32(mk_node_table_cell_col_index(ptr))
        case .link:
            href  = mk_node_link_href(ptr).map(String.init(cString:))
            title = mk_node_link_title(ptr).map(String.init(cString:))
        case .image:
            src   = mk_node_image_src(ptr).map(String.init(cString:))
            alt   = mk_node_image_alt(ptr).map(String.init(cString:))
            title = mk_node_image_title(ptr).map(String.init(cString:))
        case .autoLink:
            url     = mk_node_autolink_url(ptr).map(String.init(cString:))
            isEmail = mk_node_autolink_is_email(ptr) != 0
        case .text:
            text = mk_node_text_content(ptr).map(String.init(cString:))
        case .inlineCode:
            text = mk_node_inline_code(ptr).map(String.init(cString:))
        case .htmlBlock:
            raw = mk_node_html_block_raw(ptr).map(String.init(cString:))
        case .htmlInline:
            raw = mk_node_html_inline_raw(ptr).map(String.init(cString:))
        case .taskListItem:
            checked = mk_node_task_checked(ptr) != 0
        default:
            break
        }
    }
}

// ── Errors ────────────────────────────────────────────────────────────────────

public enum MkParserError: Error {
    case initFailed
    case feedFailed(Int32)
    case finishFailed(Int32)
    case destroyed
}

// ── Parser ────────────────────────────────────────────────────────────────────

/// Swift wrapper around the mk_parser C Push API.
public final class MkParser {

    public typealias NodeOpenCallback  = (MkNodeType, MkNodeInfo) -> Void
    public typealias NodeCloseCallback = (MkNodeType, MkNodeInfo) -> Void
    public typealias TextCallback      = (String) -> Void
    public typealias ModifyCallback    = (MkNodeType, MkNodeInfo) -> Void

    private var arena:  OpaquePointer?
    private var parser: OpaquePointer?

    // Callbacks
    public var onNodeOpen:  NodeOpenCallback?
    public var onNodeClose: NodeCloseCallback?
    public var onText:      TextCallback?
    public var onModify:    ModifyCallback?

    public init(
        onNodeOpen:  NodeOpenCallback? = nil,
        onNodeClose: NodeCloseCallback? = nil,
        onText:      TextCallback?     = nil,
        onModify:    ModifyCallback?   = nil
    ) throws {
        self.onNodeOpen  = onNodeOpen
        self.onNodeClose = onNodeClose
        self.onText      = onText
        self.onModify    = onModify

        arena = mk_arena_new()
        guard arena != nil else { throw MkParserError.initFailed }

        var cbs = MkCallbacks()
        cbs.user_data      = Unmanaged.passUnretained(self).toOpaque()
        cbs.on_node_open   = { ud, node in
            guard let ud = ud, let node = node else { return }
            let me = Unmanaged<MkParser>.fromOpaque(ud).takeUnretainedValue()
            guard let cb = me.onNodeOpen else { return }
            let t = MkNodeType(rawValue: node.pointee.type.rawValue) ?? .document
            cb(t, MkNodeInfo(rawNode: node, type: t, flags: node.pointee.flags))
        }
        cbs.on_node_close  = { ud, node in
            guard let ud = ud, let node = node else { return }
            let me = Unmanaged<MkParser>.fromOpaque(ud).takeUnretainedValue()
            guard let cb = me.onNodeClose else { return }
            let t = MkNodeType(rawValue: node.pointee.type.rawValue) ?? .document
            cb(t, MkNodeInfo(rawNode: node, type: t, flags: node.pointee.flags))
        }
        cbs.on_text        = { ud, node, text, len in
            guard let ud = ud, let text = text else { return }
            let me = Unmanaged<MkParser>.fromOpaque(ud).takeUnretainedValue()
            guard let cb = me.onText else { return }
            cb(String(bytes: UnsafeBufferPointer(start: text, count: len), encoding: .utf8) ?? "")
        }
        cbs.on_node_modify = { ud, node in
            guard let ud = ud, let node = node else { return }
            let me = Unmanaged<MkParser>.fromOpaque(ud).takeUnretainedValue()
            guard let cb = me.onModify else { return }
            let t = MkNodeType(rawValue: node.pointee.type.rawValue) ?? .document
            cb(t, MkNodeInfo(rawNode: node, type: t, flags: node.pointee.flags))
        }

        parser = mk_parser_new(arena, &cbs)
        guard parser != nil else {
            mk_arena_free(arena)
            throw MkParserError.initFailed
        }
    }

    @discardableResult
    public func feed(_ text: String) throws -> MkParser {
        guard parser != nil else { throw MkParserError.destroyed }
        let rc = text.withCString { mk_feed(parser, $0, text.utf8.count) }
        guard rc == 0 else { throw MkParserError.feedFailed(rc) }
        return self
    }

    @discardableResult
    public func finish() throws -> MkParser {
        guard parser != nil else { throw MkParserError.destroyed }
        let rc = mk_finish(parser)
        guard rc == 0 else { throw MkParserError.finishFailed(rc) }
        return self
    }

    public func destroy() {
        if let p = parser { mk_parser_free(p); parser = nil }
        if let a = arena  { mk_arena_free(a);  arena  = nil }
    }

    deinit { destroy() }
}
