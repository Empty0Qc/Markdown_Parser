package com.mkparser.demo.llm

import kotlinx.coroutines.flow.Flow

/**
 * Abstraction for any text-generation backend.
 * Implement this interface to plug in a remote API (OpenAI, Ollama, …)
 * or a local on-device model.
 *
 * Usage:
 *   val provider: LlmProvider = OpenAiProvider(config)
 *   provider.stream("Write a short Markdown essay about cats").collect { token ->
 *       parser.feed(token)
 *   }
 */
interface LlmProvider {
    /** Stream tokens for the given prompt. Each emission is a raw text chunk. */
    fun stream(prompt: String): Flow<String>

    /** Human-readable label shown in the UI. */
    val label: String
}

/** Configuration for an OpenAI-compatible REST endpoint. */
data class LlmConfig(
    val baseUrl: String = "https://api.openai.com/v1",
    val apiKey:  String = "",
    val model:   String = "gpt-4o-mini",
    /** Max tokens to request. */
    val maxTokens: Int = 512,
    /** System prompt prepended to every request. */
    val systemPrompt: String = "You are a helpful assistant. Reply using Markdown formatting.",
)
