package com.mkparser.demo.llm

import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.sse.EventSource
import okhttp3.sse.EventSourceListener
import okhttp3.sse.EventSources
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit

/**
 * OpenAiProvider — streams tokens from any OpenAI-compatible API.
 *
 * Works with:
 *   - OpenAI (https://api.openai.com/v1)
 *   - Ollama  (http://localhost:11434/v1)
 *   - LM Studio, vLLM, llama.cpp server, etc.
 *
 * Configure via [LlmConfig]:
 *   LlmConfig(
 *       baseUrl = "http://10.0.2.2:11434/v1",  // Ollama on host from emulator
 *       apiKey  = "ollama",
 *       model   = "llama3.2",
 *   )
 */
class OpenAiProvider(private val config: LlmConfig) : LlmProvider {

    override val label = "OpenAI-compatible (${config.model})"

    private val client = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(120, TimeUnit.SECONDS)
        .build()

    override fun stream(prompt: String): Flow<String> = callbackFlow {
        val body = JSONObject().apply {
            put("model", config.model)
            put("stream", true)
            put("max_tokens", config.maxTokens)
            put("messages", JSONArray().apply {
                if (config.systemPrompt.isNotBlank()) {
                    put(JSONObject().apply {
                        put("role", "system")
                        put("content", config.systemPrompt)
                    })
                }
                put(JSONObject().apply {
                    put("role", "user")
                    put("content", prompt)
                })
            })
        }.toString()

        val request = Request.Builder()
            .url("${config.baseUrl.trimEnd('/')}/chat/completions")
            .header("Authorization", "Bearer ${config.apiKey}")
            .header("Content-Type", "application/json")
            .post(body.toRequestBody("application/json".toMediaType()))
            .build()

        val listener = object : EventSourceListener() {
            override fun onEvent(
                eventSource: EventSource,
                id: String?,
                type: String?,
                data: String,
            ) {
                if (data == "[DONE]") { close(); return }
                runCatching {
                    val delta = JSONObject(data)
                        .getJSONArray("choices")
                        .getJSONObject(0)
                        .getJSONObject("delta")
                    val content = delta.optString("content", "")
                    if (content.isNotEmpty()) trySend(content)
                }
            }

            override fun onFailure(
                eventSource: EventSource,
                t: Throwable?,
                response: Response?,
            ) {
                val msg = t?.message ?: response?.message ?: "unknown error"
                close(Exception("LLM stream error: $msg"))
            }

            override fun onClosed(eventSource: EventSource) {
                close()
            }
        }

        val source = EventSources.createFactory(client).newEventSource(request, listener)

        awaitClose { source.cancel() }
    }
}
