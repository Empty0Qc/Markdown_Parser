// MockProvider.swift — Offline LLM simulation for testing
//
// Emits a predefined Markdown string character by character with a delay,
// allowing the full streaming pipeline to be exercised without an API key.

import Foundation

final class MockProvider: LlmProvider {

    let label = "Mock (offline)"

    private let response: String
    private let delayMs: TimeInterval

    init(response: String = MockProvider.defaultResponse, delayMs: TimeInterval = 0.03) {
        self.response = response
        self.delayMs  = delayMs
    }

    func stream(
        prompt:  String,
        onToken: @escaping (String) -> Void,
        onDone:  @escaping () -> Void,
        onError: @escaping (Error) -> Void
    ) -> LlmStream {
        var cancelled = false
        let chars = Array(response)
        var index = 0

        func emitNext() {
            guard !cancelled, index < chars.count else {
                if !cancelled { DispatchQueue.main.async { onDone() } }
                return
            }
            let token = String(chars[index])
            index += 1
            DispatchQueue.main.async { onToken(token) }
            DispatchQueue.main.asyncAfter(deadline: .now() + delayMs) { emitNext() }
        }

        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) { emitNext() }
        return LlmStream { cancelled = true }
    }

    // ── Default response ──────────────────────────────────────────────────────

    static let defaultResponse = """
    # Streaming Markdown Response

    This is a **mock** LLM response streamed token by token.

    ## Features demonstrated

    - Incremental block parsing
    - *Emphasis* and **bold** inline styles
    - `inline code` spans
    - Task lists:
      - [x] Push API works
      - [x] Pull API works
      - [ ] Your custom feature

    ## Code example

    ```swift
    let parser = try MkParser(
        onNodeOpen: { type, _ in print("open \\(type)") },
        onText:     { text    in print(text) }
    )
    try parser.feed(chunk)
    try parser.finish()
    parser.destroy()
    ```

    ## Table

    | Feature       | Status |
    |:--------------|:------:|
    | Block parser  | ✅     |
    | Inline parser | ✅     |
    | WASM binding  | ✅     |
    | iOS binding   | ✅     |

    ---

    *End of mock response.*
    """
}
