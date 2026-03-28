// EventsView.swift — Displays the stream of parser events
//
// Shows a scrollable list of OPEN / CLOSE / TEXT / MODIFY events with
// colour coding. Mirrors the Android EventsPanel in EventsPanel.kt.

import SwiftUI

struct EventsView: View {
    @ObservedObject var vm: DemoViewModel

    var body: some View {
        List(vm.events) { item in
            HStack(alignment: .top, spacing: 8) {
                // Kind badge
                Text(item.kind.rawValue.uppercased())
                    .font(.caption.monospaced().bold())
                    .foregroundColor(kindColor(item.kind))
                    .frame(width: 56, alignment: .leading)

                // Type name
                Text(item.typeName)
                    .font(.caption.monospaced())
                    .foregroundColor(.primary)
                    .frame(width: 120, alignment: .leading)

                // Text preview (for TEXT events)
                if !item.textPreview.isEmpty {
                    Text(item.textPreview)
                        .font(.caption.monospaced())
                        .foregroundColor(.secondary)
                        .lineLimit(1)
                }
                Spacer()

                // Index
                Text("#\(item.id)")
                    .font(.caption2.monospaced())
                    .foregroundColor(.tertiary)
            }
            .padding(.vertical, 1)
            .listRowBackground(rowBackground(item.kind))
        }
        .listStyle(.plain)
        .overlay(
            Group {
                if vm.events.isEmpty {
                    VStack {
                        Text("No events yet")
                            .foregroundColor(.secondary)
                        Text("Parse or stream some Markdown above")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
            }
        )
    }

    private func kindColor(_ kind: EventKind) -> Color {
        switch kind {
        case .open:   return .green
        case .close:  return .red
        case .text:   return .orange
        case .modify: return .purple
        }
    }

    private func rowBackground(_ kind: EventKind) -> Color {
        switch kind {
        case .open:   return Color.green.opacity(0.05)
        case .close:  return Color.red.opacity(0.05)
        case .text:   return Color.orange.opacity(0.05)
        case .modify: return Color.purple.opacity(0.05)
        }
    }
}
