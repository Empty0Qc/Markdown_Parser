// DemoViewModel.swift — ObservableObject state + parsing logic
//
// Mirrors the Android MkDemoViewModel (demo/android/.../MkDemoViewModel.kt)
// with Swift idioms: @Published, DispatchQueue, and LlmProvider protocol.

import Foundation
import Combine
import MkParser

// ── Event model ───────────────────────────────────────────────────────────────

enum EventKind: String {
    case open    = "open"
    case close   = "close"
    case text    = "text"
    case modify  = "modify"
}

struct EventItem: Identifiable {
    let id:          Int
    let kind:        EventKind
    let typeName:    String
    let attrs:       String
    let textPreview: String
}

// ── ViewModel ─────────────────────────────────────────────────────────────────

final class DemoViewModel: ObservableObject {

    @Published var markdown:       String       = defaultMarkdown
    @Published var isStreaming:    Bool         = false
    @Published var streamChunk:    Int          = 0
    @Published var streamTotal:    Int          = 0
    @Published var renderedBlocks: [MkBlock]    = []
    @Published var events:         [EventItem]  = []
    @Published var errorMessage:   String?      = nil
    @Published var useMock:        Bool         = true
    @Published var llmConfig:      LlmConfig    = LlmConfig()
    @Published var llmPrompt:      String       = "Write a short Markdown essay about the Swift language."

    private var eventCounter = 0
    private var streamTimer:  Timer?
    private var llmStream:    LlmStream?

    init() { parseAll() }

    // ── Batch parse ──────────────────────────────────────────────────────────

    func parseAll(md: String? = nil) {
        let src = md ?? markdown
        events.removeAll(); eventCounter = 0

        let parser = buildEventParser()
        try? parser.feed(src)
        try? parser.finish()
        parser.destroy()

        renderedBlocks = MkBlockParser.parse(src)
    }

    func onMarkdownChanged(_ text: String) {
        markdown = text
        parseAll(md: text)
    }

    // ── Stream simulation ─────────────────────────────────────────────────────

    func startStream() {
        guard !isStreaming else { stopStream(); return }

        let md      = markdown
        let chunkSz = 7
        // Split by UTF-8 bytes then re-form valid String chunks
        let bytes   = Array(md.utf8)
        var chunkStrings: [String] = []
        var offset = 0
        while offset < bytes.count {
            let end   = min(offset + chunkSz, bytes.count)
            // Extend end to a valid UTF-8 boundary (drop incomplete multibyte)
            var validEnd = end
            while validEnd > offset && bytes[validEnd - 1] & 0xC0 == 0x80 { validEnd -= 1 }
            if validEnd == offset { validEnd = end } // fallback: use raw bytes
            if let s = String(bytes: bytes[offset..<validEnd], encoding: .utf8) {
                chunkStrings.append(s)
            }
            offset = validEnd
        }

        events.removeAll(); eventCounter = 0
        isStreaming = true
        streamChunk = 0
        streamTotal = md.utf8.count

        var idx = 0
        let parser = buildEventParser()

        streamTimer = Timer.scheduledTimer(withTimeInterval: 0.04, repeats: true) { [weak self] t in
            guard let self else { t.invalidate(); return }
            guard idx < chunkStrings.count else {
                try? parser.finish()
                parser.destroy()
                let finalBlocks = MkBlockParser.parse(md)
                DispatchQueue.main.async {
                    self.renderedBlocks = finalBlocks
                    self.isStreaming = false
                }
                t.invalidate()
                return
            }
            let chunk = chunkStrings[idx]; idx += 1
            try? parser.feed(chunk)
            let takenBytes = chunkStrings[..<idx].reduce(0) { $0 + $1.utf8.count }
            let taken = String(md.utf8.prefix(takenBytes)).flatMap { $0 } ?? md
            self.streamChunk = takenBytes
            self.renderedBlocks = MkBlockParser.parse(taken, isStreaming: true)
        }
    }

    func stopStream() {
        streamTimer?.invalidate(); streamTimer = nil
        isStreaming = false
    }

    // ── LLM streaming ─────────────────────────────────────────────────────────

    func startLlmStream() {
        guard !isStreaming else { stopLlmStream(); return }

        let provider: LlmProvider = useMock ? MockProvider() : OpenAIProvider(config: llmConfig)
        events.removeAll(); eventCounter = 0
        var accumulated = ""
        isStreaming = true
        markdown    = ""
        renderedBlocks = []

        let parser = buildEventParser()

        llmStream = provider.stream(
            prompt: llmPrompt,
            onToken: { [weak self] token in
                guard let self else { return }
                accumulated += token
                try? parser.feed(token)
                self.markdown      = accumulated
                self.renderedBlocks = MkBlockParser.parse(accumulated, isStreaming: true)
            },
            onDone: { [weak self] in
                guard let self else { return }
                try? parser.finish()
                parser.destroy()
                self.renderedBlocks = MkBlockParser.parse(accumulated)
                self.isStreaming = false
            },
            onError: { [weak self] error in
                guard let self else { return }
                self.errorMessage = error.localizedDescription
                self.isStreaming  = false
                parser.destroy()
            }
        )
    }

    func stopLlmStream() {
        llmStream?.cancel(); llmStream = nil
        isStreaming = false
    }

    func dismissError() { errorMessage = nil }
    func toggleMock()   { useMock.toggle() }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private func buildEventParser() -> MkParser {
        // MkParser init only fails if the C arena allocation fails, which is
        // extremely unlikely in practice. Force-unwrap is intentional here.
        return try! MkParser(
            onNodeOpen:  { [weak self] type, _ in
                guard let self else { return }
                let name = nodeTypeName(type)
                self.eventCounter += 1
                self.events.append(EventItem(id: self.eventCounter, kind: .open,  typeName: name, attrs: "", textPreview: ""))
            },
            onNodeClose: { [weak self] type, _ in
                guard let self else { return }
                let name = nodeTypeName(type)
                self.eventCounter += 1
                self.events.append(EventItem(id: self.eventCounter, kind: .close, typeName: name, attrs: "", textPreview: ""))
            },
            onText: { [weak self] text in
                guard let self else { return }
                let preview = String(text.prefix(60)).replacingOccurrences(of: "\n", with: "↵")
                self.eventCounter += 1
                self.events.append(EventItem(id: self.eventCounter, kind: .text, typeName: "TEXT", attrs: "",
                    textPreview: "\"\(preview)\(text.count > 60 ? "…" : "")\""))
            },
            onModify: { [weak self] type, _ in
                guard let self else { return }
                let name = nodeTypeName(type)
                self.eventCounter += 1
                self.events.append(EventItem(id: self.eventCounter, kind: .modify, typeName: name, attrs: "", textPreview: ""))
            }
        )
    }
}

// ── Node type name ─────────────────────────────────────────────────────────────

private func nodeTypeName(_ type: MkNodeType) -> String {
    switch type {
    case .document:      return "DOCUMENT"
    case .heading:       return "HEADING"
    case .paragraph:     return "PARAGRAPH"
    case .codeBlock:     return "CODE_BLOCK"
    case .blockQuote:    return "BLOCK_QUOTE"
    case .list:          return "LIST"
    case .listItem:      return "LIST_ITEM"
    case .thematicBreak: return "THEMATIC_BREAK"
    case .htmlBlock:     return "HTML_BLOCK"
    case .table:         return "TABLE"
    case .tableHead:     return "TABLE_HEAD"
    case .tableRow:      return "TABLE_ROW"
    case .tableCell:     return "TABLE_CELL"
    case .text:          return "TEXT"
    case .softBreak:     return "SOFT_BREAK"
    case .hardBreak:     return "HARD_BREAK"
    case .emphasis:      return "EMPHASIS"
    case .strong:        return "STRONG"
    case .strikethrough: return "STRIKETHROUGH"
    case .inlineCode:    return "INLINE_CODE"
    case .link:          return "LINK"
    case .image:         return "IMAGE"
    case .autoLink:      return "AUTO_LINK"
    case .htmlInline:    return "HTML_INLINE"
    case .taskListItem:  return "TASK_LIST_ITEM"
    @unknown default:    return "CUSTOM(\(type.rawValue))"
    }
}

// ── Default markdown ──────────────────────────────────────────────────────────

let defaultMarkdown = """
# mk-parser Demo

Streaming Markdown parser for AI output.

## Features

- Incremental parsing — works char by char
- **Bold**, *italic*, ~~strikethrough~~, `code`
- [Links](https://github.com) and images
- Fenced code blocks:

```swift
let parser = try MkParser(onText: { print($0) })
try parser.feed("# Hello\\n")
try parser.finish()
```

## Task list

- [x] Push API
- [x] iOS Swift binding
- [ ] Your next feature

## Table

| Layer    | Status |
|----------|:------:|
| C core   | ✅     |
| iOS      | ✅     |
| Android  | ✅     |
"""
