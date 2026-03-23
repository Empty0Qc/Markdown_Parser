package com.mkparser.demo.ui

import android.annotation.SuppressLint
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView

/**
 * WebView panel that loads demo/web/index.html from assets
 * and injects the current markdown via JavaScript bridge.
 *
 * The web demo's editor is updated whenever [markdown] changes so the
 * WebView always shows the same content as the native event stream.
 */
@SuppressLint("SetJavaScriptEnabled")
@Composable
fun WebPreviewPanel(
    markdown: String,
    modifier: Modifier = Modifier,
) {
    var webViewRef by remember { mutableStateOf<WebView?>(null) }

    // Inject markdown whenever it changes (after page is loaded)
    LaunchedEffect(markdown, webViewRef) {
        val wv = webViewRef ?: return@LaunchedEffect
        // Escape backticks and backslashes for JS template literal
        val escaped = markdown
            .replace("\\", "\\\\")
            .replace("`", "\\`")
            .replace("$", "\\$")
        wv.evaluateJavascript(
            "if(typeof editor !== 'undefined') { editor.value = `$escaped`; runParse(); }",
            null,
        )
    }

    AndroidView(
        modifier = modifier,
        factory  = { context ->
            WebView(context).also { wv ->
                wv.settings.javaScriptEnabled = true
                wv.settings.domStorageEnabled = true
                wv.webViewClient = object : WebViewClient() {
                    override fun onPageFinished(view: WebView, url: String) {
                        // Inject current markdown once the page finishes loading
                        webViewRef = view
                    }
                }
                // Load from assets: demo/web/index.html
                // Assets root is mapped from sourceSets["main"].assets.srcDir
                wv.loadUrl("file:///android_asset/web/index.html")
            }
        },
        update = { wv ->
            // Keep reference fresh
            webViewRef = wv
        },
    )
}
