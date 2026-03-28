// LlmProvider.swift — Protocol for LLM streaming backends

import Foundation

/// Configuration for a real LLM endpoint.
struct LlmConfig {
    var apiKey:  String = ""
    var baseURL: String = "https://api.openai.com/v1"
    var model:   String = "gpt-4o-mini"
}

/// Any type that can stream Markdown text tokens from a prompt.
protocol LlmProvider {
    /// Human-readable display label (shown in the UI).
    var label: String { get }

    /// Begin streaming tokens for the given `prompt`.
    /// Calls `onToken` on the main actor for each received token.
    /// Calls `onDone` when the stream is complete or `onError` on failure.
    func stream(
        prompt:  String,
        onToken: @escaping (String) -> Void,
        onDone:  @escaping () -> Void,
        onError: @escaping (Error) -> Void
    ) -> LlmStream
}

/// An opaque handle that lets the caller cancel the stream.
final class LlmStream {
    private let cancelBlock: () -> Void
    init(cancel: @escaping () -> Void) { self.cancelBlock = cancel }
    func cancel() { cancelBlock() }
}
