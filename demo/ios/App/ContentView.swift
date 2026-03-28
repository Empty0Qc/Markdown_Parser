// ContentView.swift — Root UI: markdown input + action bar + TabView
//
// Mirrors the Android MainScreen.kt structure:
//   - Top: editable markdown text field
//   - Middle: Stream / Parse All / Clear action bar
//   - Bottom: TabView with Events / Preview / LLM tabs

import SwiftUI

struct ContentView: View {
    @StateObject private var vm = DemoViewModel()
    @State private var activeTab = 0   // 0=Events 1=Preview 2=LLM

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {

                // ── Markdown input ───────────────────────────────────────────
                TextEditor(text: Binding(
                    get: { vm.markdown },
                    set: { vm.onMarkdownChanged($0) }
                ))
                .font(.system(.footnote, design: .monospaced))
                .frame(minHeight: 100, maxHeight: 180)
                .overlay(
                    RoundedRectangle(cornerRadius: 8)
                        .stroke(Color.secondary.opacity(0.3), lineWidth: 1)
                )
                .padding(.horizontal, 12)
                .padding(.top, 8)

                // ── Action bar ───────────────────────────────────────────────
                HStack(spacing: 8) {
                    Button(action: vm.startStream) {
                        Label(vm.isStreaming ? "■ Stop" : "▶ Stream",
                              systemImage: vm.isStreaming ? "stop.fill" : "play.fill")
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(vm.isStreaming ? .red : .accentColor)

                    Button("Parse All", action: { vm.parseAll() })
                        .buttonStyle(.bordered)

                    Button("Clear", action: { vm.onMarkdownChanged("") })
                        .buttonStyle(.bordered)

                    Spacer()

                    if vm.isStreaming && vm.streamTotal > 0 {
                        ProgressView(
                            value: Double(vm.streamChunk),
                            total: Double(vm.streamTotal)
                        )
                        .frame(width: 60)
                    }
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)

                Divider()

                // ── Tab bar ──────────────────────────────────────────────────
                Picker("Tab", selection: $activeTab) {
                    Text("Events").tag(0)
                    Text("Preview").tag(1)
                    Text("LLM").tag(2)
                }
                .pickerStyle(.segmented)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)

                // ── Tab content ──────────────────────────────────────────────
                Group {
                    switch activeTab {
                    case 0:
                        EventsView(vm: vm)
                    case 1:
                        MkRenderView(blocks: vm.renderedBlocks)
                    default:
                        LlmView(vm: vm)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .navigationTitle("mk-parser")
            .navigationBarTitleDisplayMode(.inline)
            .alert("Error", isPresented: Binding(
                get: { vm.errorMessage != nil },
                set: { if !$0 { vm.dismissError() } }
            )) {
                Button("OK", action: vm.dismissError)
            } message: {
                Text(vm.errorMessage ?? "")
            }
        }
    }
}

// ── LLM tab view ──────────────────────────────────────────────────────────────

struct LlmView: View {
    @ObservedObject var vm: DemoViewModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                // Provider toggle
                Toggle("Use Mock Provider", isOn: Binding(
                    get: { vm.useMock },
                    set: { _ in vm.toggleMock() }
                ))
                .padding(.horizontal)

                // API key (only visible when not using mock)
                if !vm.useMock {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("API Key").font(.caption).foregroundColor(.secondary)
                        SecureField("sk-...", text: Binding(
                            get: { vm.llmConfig.apiKey },
                            set: { vm.llmConfig.apiKey = $0 }
                        ))
                        .textFieldStyle(.roundedBorder)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)

                        Text("Model").font(.caption).foregroundColor(.secondary)
                        TextField("gpt-4o-mini", text: Binding(
                            get: { vm.llmConfig.model },
                            set: { vm.llmConfig.model = $0 }
                        ))
                        .textFieldStyle(.roundedBorder)
                        .autocorrectionDisabled()
                    }
                    .padding(.horizontal)
                }

                // Prompt
                VStack(alignment: .leading, spacing: 6) {
                    Text("Prompt").font(.caption).foregroundColor(.secondary)
                    TextEditor(text: Binding(
                        get: { vm.llmPrompt },
                        set: { vm.llmPrompt = $0 }
                    ))
                    .frame(minHeight: 80)
                    .overlay(
                        RoundedRectangle(cornerRadius: 8)
                            .stroke(Color.secondary.opacity(0.3), lineWidth: 1)
                    )
                }
                .padding(.horizontal)

                // Stream button
                Button(action: vm.startLlmStream) {
                    Label(
                        vm.isStreaming ? "■ Stop LLM Stream" : "▶ Start LLM Stream",
                        systemImage: vm.isStreaming ? "stop.fill" : "wand.and.sparkles"
                    )
                    .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .tint(vm.isStreaming ? .red : .purple)
                .padding(.horizontal)

                // Streaming render output
                if !vm.renderedBlocks.isEmpty {
                    Divider().padding(.horizontal)
                    Text("Output").font(.caption).foregroundColor(.secondary).padding(.horizontal)
                    MkRenderView(blocks: vm.renderedBlocks)
                        .frame(minHeight: 300)
                }
            }
            .padding(.vertical, 12)
        }
    }
}

// ── Preview ───────────────────────────────────────────────────────────────────

#Preview {
    ContentView()
}
