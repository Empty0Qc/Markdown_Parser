// OpenAIProvider.swift — Real OpenAI-compatible SSE streaming
//
// Uses URLSession + async/await bytes stream to parse Server-Sent Events
// and extract token text from the `data: {...}` JSON lines.
//
// Compatible with OpenAI, Azure OpenAI, and any OpenAI-API-compatible endpoint.

import Foundation

final class OpenAIProvider: LlmProvider {

    let label = "OpenAI"

    private let config: LlmConfig

    init(config: LlmConfig) {
        self.config = config
    }

    func stream(
        prompt:  String,
        onToken: @escaping (String) -> Void,
        onDone:  @escaping () -> Void,
        onError: @escaping (Error) -> Void
    ) -> LlmStream {

        let task = URLSession.shared.dataTask(with: makeRequest(prompt: prompt)) { _, _, _ in }
        // We use a detached task for async streaming
        let streamTask = Task.detached { [config] in
            do {
                let request = self.makeRequest(prompt: prompt)
                let (bytes, response) = try await URLSession.shared.bytes(for: request)

                if let http = response as? HTTPURLResponse, http.statusCode != 200 {
                    let body = try? await bytes.lines.reduce("") { $0 + $1 }
                    throw OpenAIError.httpError(http.statusCode, body ?? "")
                }

                for try await line in bytes.lines {
                    guard line.hasPrefix("data: ") else { continue }
                    let data = String(line.dropFirst(6))
                    if data == "[DONE]" { break }
                    guard let json = data.data(using: .utf8),
                          let obj  = try? JSONSerialization.jsonObject(with: json) as? [String: Any],
                          let choices = obj["choices"] as? [[String: Any]],
                          let delta   = choices.first?["delta"] as? [String: Any],
                          let content = delta["content"] as? String
                    else { continue }
                    await MainActor.run { onToken(content) }
                }
                await MainActor.run { onDone() }
            } catch {
                await MainActor.run { onError(error) }
            }
        }

        task.cancel() // dataTask was just for type inference, cancel it
        return LlmStream { streamTask.cancel() }
    }

    // ── Request builder ───────────────────────────────────────────────────────

    private func makeRequest(prompt: String) -> URLRequest {
        let url = URL(string: "\(config.baseURL)/chat/completions")!
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("Bearer \(config.apiKey)", forHTTPHeaderField: "Authorization")
        req.setValue("application/json",        forHTTPHeaderField: "Content-Type")

        let body: [String: Any] = [
            "model":  config.model,
            "stream": true,
            "messages": [
                ["role": "system", "content": "You are a helpful assistant. Reply in Markdown."],
                ["role": "user",   "content": prompt],
            ],
        ]
        req.httpBody = try? JSONSerialization.data(withJSONObject: body)
        return req
    }
}

// ── Errors ────────────────────────────────────────────────────────────────────

enum OpenAIError: LocalizedError {
    case httpError(Int, String)

    var errorDescription: String? {
        switch self {
        case .httpError(let code, let body):
            return "HTTP \(code): \(body.prefix(200))"
        }
    }
}
