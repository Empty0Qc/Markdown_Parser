package com.mkparser.demo.llm

import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow

/**
 * MockProvider — simulates an LLM streaming response locally.
 *
 * Useful for testing the parser pipeline without a real API key.
 * Emits the predefined [response] string character-by-character with [delayMs]
 * between each emission.
 */
class MockProvider(
    private val response: String = DEFAULT_RESPONSE,
    private val delayMs: Long = 30L,
) : LlmProvider {

    override val label = "Mock (offline)"

    override fun stream(prompt: String): Flow<String> = flow {
        for (ch in response) {
            emit(ch.toString())
            delay(delayMs)
        }
    }

    companion object {
        val DEFAULT_RESPONSE = """
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

```kotlin
val parser = MkParser(
    onNodeOpen  = { type, _ -> println("open ${'$'}type") },
    onText      = { text    -> print(text) },
)
parser.feed(chunk).finish()
```

## Table

| Feature      | Status |
|:-------------|:------:|
| Block parser | ✅     |
| Inline parser| ✅     |
| WASM binding | ✅     |
| Android JNI  | ✅     |

---

*End of mock response.*
        """.trimIndent()
    }
}
