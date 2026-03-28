// MkRenderView.swift — SwiftUI List renderer for [MkBlock]
//
// Renders a flat block list from MkBlockParser using SwiftUI views.
// The streaming cursor (▌) animates via a Timer-driven opacity toggle.
// Colour palette: Catppuccin Mocha.

import SwiftUI

// ── Palette ───────────────────────────────────────────────────────────────────

private enum Mocha {
    static let base   = Color(red: 0x1E/255.0, green: 0x1E/255.0, blue: 0x2E/255.0)
    static let mantle = Color(red: 0x18/255.0, green: 0x18/255.0, blue: 0x2D/255.0)
    static let crust  = Color(red: 0x11/255.0, green: 0x11/255.0, blue: 0x1D/255.0)
    static let surface0 = Color(red: 0x31/255.0, green: 0x32/255.0, blue: 0x44/255.0)
    static let surface1 = Color(red: 0x45/255.0, green: 0x47/255.0, blue: 0x59/255.0)
    static let text   = Color(red: 0xCD/255.0, green: 0xD6/255.0, blue: 0xF4/255.0)
    static let subtext = Color(red: 0xA6/255.0, green: 0xAD/255.0, blue: 0xC8/255.0)
    static let mauve  = Color(red: 0xCB/255.0, green: 0xA6/255.0, blue: 0xF7/255.0)
    static let blue   = Color(red: 0x89/255.0, green: 0xB4/255.0, blue: 0xFA/255.0)
    static let sky    = Color(red: 0x89/255.0, green: 0xDC/255.0, blue: 0xEB/255.0)
    static let green  = Color(red: 0xA6/255.0, green: 0xE3/255.0, blue: 0xA1/255.0)
    static let yellow = Color(red: 0xF9/255.0, green: 0xE2/255.0, blue: 0xAF/255.0)
    static let red    = Color(red: 0xF3/255.0, green: 0x8B/255.0, blue: 0xA8/255.0)
    static let peach  = Color(red: 0xFA/255.0, green: 0xB3/255.0, blue: 0x87/255.0)
}

// ── Cursor view ───────────────────────────────────────────────────────────────

private struct BlinkCursor: View {
    @State private var visible = true
    var body: some View {
        Text("▌")
            .foregroundColor(Mocha.mauve)
            .opacity(visible ? 1 : 0)
            .onAppear {
                let timer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { _ in
                    visible.toggle()
                }
                RunLoop.main.add(timer, forMode: .common)
            }
    }
}

// ── Block row views ───────────────────────────────────────────────────────────

private struct HeadingRow: View {
    let level: Int
    let content: AttributedString
    let isStreaming: Bool

    var body: some View {
        HStack(alignment: .bottom, spacing: 0) {
            Text(content)
                .font(headingFont(level))
                .foregroundColor(headingColor(level))
            if isStreaming { BlinkCursor() }
        }
        .padding(.vertical, level <= 2 ? 8 : 4)
    }

    private func headingFont(_ l: Int) -> Font {
        switch l {
        case 1: return .title.bold()
        case 2: return .title2.bold()
        case 3: return .title3.bold()
        default: return .headline
        }
    }
    private func headingColor(_ l: Int) -> Color {
        switch l {
        case 1: return Mocha.mauve
        case 2: return Mocha.blue
        default: return Mocha.sky
        }
    }
}

private struct ParagraphRow: View {
    let content: AttributedString
    let isStreaming: Bool
    var body: some View {
        HStack(alignment: .bottom, spacing: 0) {
            Text(content).foregroundColor(Mocha.text)
            if isStreaming { BlinkCursor() }
        }
        .padding(.vertical, 2)
    }
}

private struct FencedCodeRow: View {
    let language: String
    let code: String
    let isStreaming: Bool
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            if !language.isEmpty {
                Text(language)
                    .font(.caption.monospaced())
                    .foregroundColor(Mocha.subtext)
                    .padding(.horizontal, 12)
                    .padding(.top, 8)
            }
            HStack(alignment: .bottom, spacing: 0) {
                Text(code)
                    .font(.system(.body, design: .monospaced))
                    .foregroundColor(Mocha.green)
                if isStreaming { BlinkCursor() }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, language.isEmpty ? 10 : 6)
        }
        .background(Mocha.surface0)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .padding(.vertical, 4)
    }
}

private struct BulletItemRow: View {
    let depth: Int
    let taskState: Int   // 0=none 1=unchecked 2=checked
    let content: AttributedString
    let isStreaming: Bool
    var body: some View {
        HStack(alignment: .top, spacing: 8) {
            // Indent
            Spacer().frame(width: CGFloat(depth * 16))
            // Bullet/checkbox
            Group {
                if taskState == 1 {
                    Image(systemName: "square").foregroundColor(Mocha.subtext)
                } else if taskState == 2 {
                    Image(systemName: "checkmark.square.fill").foregroundColor(Mocha.green)
                } else {
                    Text("•").foregroundColor(Mocha.mauve)
                }
            }
            .font(.body)
            .frame(width: 20, alignment: .center)
            HStack(alignment: .bottom, spacing: 0) {
                Text(content).foregroundColor(Mocha.text)
                if isStreaming { BlinkCursor() }
            }
            Spacer()
        }
        .padding(.vertical, 1)
    }
}

private struct OrderedItemRow: View {
    let depth: Int
    let number: Int
    let content: AttributedString
    let isStreaming: Bool
    var body: some View {
        HStack(alignment: .top, spacing: 8) {
            Spacer().frame(width: CGFloat(depth * 16))
            Text("\(number).")
                .foregroundColor(Mocha.mauve)
                .frame(width: 28, alignment: .trailing)
            HStack(alignment: .bottom, spacing: 0) {
                Text(content).foregroundColor(Mocha.text)
                if isStreaming { BlinkCursor() }
            }
            Spacer()
        }
        .padding(.vertical, 1)
    }
}

private struct BlockQuoteRow: View {
    let content: AttributedString
    let isStreaming: Bool
    var body: some View {
        HStack(alignment: .top, spacing: 0) {
            Rectangle()
                .fill(Mocha.mauve.opacity(0.6))
                .frame(width: 4)
                .clipShape(Capsule())
                .padding(.vertical, 2)
            HStack(alignment: .bottom, spacing: 0) {
                Text(content)
                    .foregroundColor(Mocha.subtext)
                    .padding(.leading, 12)
                if isStreaming { BlinkCursor() }
            }
            Spacer()
        }
        .padding(.vertical, 4)
    }
}

private struct ThematicBreakRow: View {
    var body: some View {
        Divider()
            .background(Mocha.surface1)
            .padding(.vertical, 8)
    }
}

private struct TableBlockRow: View {
    let headers: [AttributedString]
    let rows: [[AttributedString]]
    let isStreaming: Bool
    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            VStack(alignment: .leading, spacing: 0) {
                // Header
                HStack(spacing: 0) {
                    ForEach(headers.indices, id: \.self) { i in
                        Text(headers[i])
                            .font(.headline)
                            .foregroundColor(Mocha.mauve)
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                            .frame(minWidth: 80, alignment: .leading)
                    }
                    if isStreaming { BlinkCursor() }
                }
                .background(Mocha.surface0)
                Divider().background(Mocha.surface1)
                // Body
                ForEach(rows.indices, id: \.self) { r in
                    HStack(spacing: 0) {
                        ForEach(rows[r].indices, id: \.self) { c in
                            Text(rows[r][c])
                                .foregroundColor(Mocha.text)
                                .padding(.horizontal, 12)
                                .padding(.vertical, 6)
                                .frame(minWidth: 80, alignment: .leading)
                        }
                    }
                    .background(r.isMultiple(of: 2) ? Color.clear : Mocha.surface0.opacity(0.4))
                    Divider().background(Mocha.surface1.opacity(0.5))
                }
            }
        }
        .background(Mocha.mantle)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .padding(.vertical, 4)
    }
}

// ── Main render panel ─────────────────────────────────────────────────────────

struct MkRenderView: View {
    let blocks: [MkBlock]

    var body: some View {
        List(blocks) { block in
            blockRow(block)
                .listRowBackground(Color.clear)
                .listRowSeparator(.hidden)
                .listRowInsets(EdgeInsets(top: 1, leading: 12, bottom: 1, trailing: 12))
        }
        .listStyle(.plain)
        .background(Mocha.base)
        .scrollContentBackground(.hidden)
    }

    @ViewBuilder
    private func blockRow(_ block: MkBlock) -> some View {
        switch block {
        case .heading(_, let l, let c, let s):
            HeadingRow(level: l, content: c, isStreaming: s)
        case .paragraph(_, let c, let s):
            ParagraphRow(content: c, isStreaming: s)
        case .fencedCode(_, let lang, let code, let s):
            FencedCodeRow(language: lang, code: code, isStreaming: s)
        case .bulletItem(_, let d, let ts, let c, let s):
            BulletItemRow(depth: d, taskState: ts, content: c, isStreaming: s)
        case .orderedItem(_, let d, let n, let c, let s):
            OrderedItemRow(depth: d, number: n, content: c, isStreaming: s)
        case .blockQuote(_, let c, let s):
            BlockQuoteRow(content: c, isStreaming: s)
        case .thematicBreak:
            ThematicBreakRow()
        case .tableBlock(_, let h, let r, let s):
            TableBlockRow(headers: h, rows: r, isStreaming: s)
        }
    }
}

// ── Preview ───────────────────────────────────────────────────────────────────

#Preview {
    MkRenderView(blocks: MkBlockParser.parse("""
    # Hello World

    Paragraph with **bold** and *italic* and `code`.

    - item one
    - item two

    > A blockquote

    | A | B |
    |---|---|
    | 1 | 2 |
    """))
}
