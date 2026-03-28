package com.mkparser

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.graphics.drawable.GradientDrawable
import android.text.Spannable
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.method.LinkMovementMethod
import android.text.style.ImageSpan
import android.text.style.URLSpan
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.HorizontalScrollView
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TableLayout
import android.widget.TableRow
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

/**
 * RecyclerView adapter that renders a [List<MkBlock>] into Android Views.
 *
 * Usage:
 * ```kotlin
 * val adapter = MkBlockAdapter(style = MkBlockStyle.content())
 * recyclerView.adapter = adapter
 *
 * // Optional: wire up syntax highlighting (e.g. via Prism4j)
 * adapter.codeHighlighter = { code, language -> myHighlighter.highlight(code, language) }
 *
 * // Optional: wire up image loading (e.g. via Glide)
 * adapter.imageLoader = { url, imageView -> Glide.with(imageView).load(url).into(imageView) }
 *
 * adapter.submitBlocks(blocks)
 * ```
 */
class MkBlockAdapter(
    private val style: MkBlockStyle = MkBlockStyle.content(),
) : RecyclerView.Adapter<MkBlockAdapter.BlockViewHolder>() {

    /**
     * Optional syntax highlighter for fenced code blocks.
     * Called on the main thread during [onBindViewHolder].
     * Return a [CharSequence] (plain or [android.text.Spanned]) for the code text.
     * If null, code is shown as plain monospace text.
     *
     * Example using Prism4j from the consuming app:
     * ```kotlin
     * adapter.codeHighlighter = { code, lang ->
     *     val grammar = prism.grammar(lang) ?: return@codeHighlighter code
     *     PrismSyntaxHighlight.highlight(code, grammar, theme)
     * }
     * ```
     */
    var codeHighlighter: ((code: String, language: String) -> CharSequence)? = null

    /**
     * Optional image loader for inline images.
     * Receives the image URL and a callback to set the loaded [Drawable].
     * The callback updates the ImageSpan inside the text and invalidates the TextView.
     * If null, image placeholders remain as link-style text.
     *
     * Example using Glide:
     * ```kotlin
     * adapter.imageLoader = { url, onLoaded ->
     *     Glide.with(context).load(url).into(object : CustomTarget<Drawable>() {
     *         override fun onResourceReady(r: Drawable, t: Transition<in Drawable>?) = onLoaded(r)
     *         override fun onLoadCleared(p: Drawable?) = Unit
     *     })
     * }
     * ```
     */
    var imageLoader: ((url: String, onLoaded: (Drawable) -> Unit) -> Unit)? = null

    private var blocks: List<MkBlock> = emptyList()

    fun submitBlocks(newBlocks: List<MkBlock>) {
        blocks = newBlocks
        notifyDataSetChanged()
    }

    // ── View type constants ───────────────────────────────────────────────────

    companion object {
        private const val TYPE_HEADING    = 0
        private const val TYPE_PARAGRAPH  = 1
        private const val TYPE_CODE       = 2
        private const val TYPE_BULLET     = 3
        private const val TYPE_ORDERED    = 4
        private const val TYPE_BLOCKQUOTE = 5
        private const val TYPE_THEMATIC   = 6
        private const val TYPE_TABLE      = 7
    }

    override fun getItemCount(): Int = blocks.size

    override fun getItemViewType(position: Int): Int = when (blocks[position]) {
        is MkBlock.Heading       -> TYPE_HEADING
        is MkBlock.Paragraph     -> TYPE_PARAGRAPH
        is MkBlock.FencedCode    -> TYPE_CODE
        is MkBlock.BulletItem    -> TYPE_BULLET
        is MkBlock.OrderedItem   -> TYPE_ORDERED
        is MkBlock.BlockQuote    -> TYPE_BLOCKQUOTE
        is MkBlock.ThematicBreak -> TYPE_THEMATIC
        is MkBlock.TableBlock    -> TYPE_TABLE
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BlockViewHolder {
        val ctx = parent.context
        return when (viewType) {
            TYPE_HEADING    -> HeadingVH(ctx, style)
            TYPE_PARAGRAPH  -> ParagraphVH(ctx, style)
            TYPE_CODE       -> CodeVH(ctx, style)
            TYPE_BULLET     -> BulletVH(ctx, style)
            TYPE_ORDERED    -> OrderedVH(ctx, style)
            TYPE_BLOCKQUOTE -> QuoteVH(ctx, style)
            TYPE_THEMATIC   -> ThematicVH(ctx, style)
            TYPE_TABLE      -> TableVH(ctx, style)
            else            -> ParagraphVH(ctx, style)
        }
    }

    override fun onBindViewHolder(holder: BlockViewHolder, position: Int) {
        holder.bind(blocks[position])
    }

    // ── Base ViewHolder ───────────────────────────────────────────────────────

    abstract class BlockViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        abstract fun bind(block: MkBlock)
    }

    // ── dp helper ─────────────────────────────────────────────────────────────

    private fun Context.dp(value: Float): Int =
        (value * resources.displayMetrics.density + 0.5f).toInt()

    private fun Context.dp(value: Int): Int = dp(value.toFloat())

    // ── Heading ───────────────────────────────────────────────────────────────

    private inner class HeadingVH(ctx: Context, private val s: MkBlockStyle) :
        BlockViewHolder(makeTextView(ctx)) {

        private val tv = itemView as TextView

        override fun bind(block: MkBlock) {
            block as MkBlock.Heading
            val ctx = tv.context
            val level = block.level.coerceIn(1, 6) - 1
            tv.textSize = s.headingTextSizesDp[level]
            tv.setTextColor(s.headingTextColor)
            tv.setTypeface(null, s.headingTypefaceStyle)
            val content = SpannableStringBuilder(block.content)
            tv.text = withCursor(content, block.isStreaming, s)
            tv.movementMethod = LinkMovementMethod.getInstance()
            val topDp = if (level == 0) 18 else 12
            tv.setPadding(0, ctx.dp(topDp), 0, ctx.dp(4))
            applyImages(tv, content)
        }
    }

    // ── Paragraph ─────────────────────────────────────────────────────────────

    private inner class ParagraphVH(ctx: Context, private val s: MkBlockStyle) :
        BlockViewHolder(makeTextView(ctx)) {

        private val tv = itemView as TextView

        override fun bind(block: MkBlock) {
            block as MkBlock.Paragraph
            val ctx = tv.context
            tv.textSize = s.paragraphTextSizeDp
            tv.setTextColor(s.paragraphTextColor)
            tv.setTypeface(null, Typeface.NORMAL)
            tv.setLineSpacing(ctx.dp(s.paragraphLineSpacingDp - s.paragraphTextSizeDp * 1.2f).toFloat(), 1f)
            val content = SpannableStringBuilder(block.content)
            tv.text = withCursor(content, block.isStreaming, s)
            tv.movementMethod = LinkMovementMethod.getInstance()
            tv.setPadding(0, ctx.dp(4), 0, ctx.dp(4))
            applyImages(tv, content)
        }
    }

    // ── Fenced code block ─────────────────────────────────────────────────────

    private inner class CodeVH(ctx: Context, private val s: MkBlockStyle) :
        BlockViewHolder(makeCodeContainer(ctx, s)) {

        private val langLabel = (itemView as LinearLayout).getChildAt(0) as TextView
        private val codeText  = ((itemView as LinearLayout).getChildAt(1) as HorizontalScrollView)
            .getChildAt(0) as TextView

        override fun bind(block: MkBlock) {
            block as MkBlock.FencedCode
            val ctx = itemView.context
            if (block.language.isNotEmpty()) {
                langLabel.text = block.language
                langLabel.visibility = View.VISIBLE
            } else {
                langLabel.visibility = View.GONE
            }
            val rawCode = if (block.isStreaming && s.showStreamingCursor)
                block.code + s.streamingCursorChar
            else block.code

            // Use injected highlighter if available, otherwise plain text
            val highlighted = codeHighlighter?.invoke(block.code, block.language)
            codeText.text = if (highlighted != null) {
                if (block.isStreaming && s.showStreamingCursor)
                    SpannableStringBuilder(highlighted).append(s.streamingCursorChar)
                else highlighted
            } else {
                rawCode
            }
            codeText.textSize = s.codeBlockTextSizeDp
            codeText.setTextColor(s.codeBlockTextColor)
        }
    }

    private fun makeCodeContainer(ctx: Context, s: MkBlockStyle): LinearLayout {
        val pad = ctx.dp(s.codeBlockPaddingDp)
        val marginV = ctx.dp(s.codeBlockMarginVertDp)

        // Language label
        val langLabel = TextView(ctx).apply {
            textSize = s.codeLangTextSizeDp
            setTextColor(s.codeBlockTextColor)
            setPadding(pad, ctx.dp(4), pad, ctx.dp(4))
        }

        // Code content
        val codeText = TextView(ctx).apply {
            typeface = Typeface.MONOSPACE
            textSize = s.codeBlockTextSizeDp
            setTextColor(s.codeBlockTextColor)
            setPadding(pad, ctx.dp(4), pad, ctx.dp(4))
        }
        val scroll = HorizontalScrollView(ctx).apply {
            isHorizontalScrollBarEnabled = false
            addView(codeText, ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ))
        }

        // Background container
        val bg = GradientDrawable().apply {
            setColor(s.codeBlockBgColor)
            cornerRadius = s.codeBlockCornerRadiusDp * ctx.resources.displayMetrics.density
        }

        return LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            background = bg
            val lp = RecyclerView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ).also { it.setMargins(0, marginV, 0, marginV) }
            layoutParams = lp
            addView(langLabel, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            addView(scroll, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        }
    }

    // ── Bullet item ───────────────────────────────────────────────────────────

    private inner class BulletVH(ctx: Context, private val s: MkBlockStyle) :
        BlockViewHolder(makeBulletContainer(ctx, s)) {

        private val container = itemView as LinearLayout
        private val marker    = container.getChildAt(0) as BulletView
        private val tv        = container.getChildAt(1) as TextView

        override fun bind(block: MkBlock) {
            block as MkBlock.BulletItem
            val ctx = itemView.context
            val indent = ctx.dp(s.listDepthIndentDp) * block.depth
            container.setPadding(indent, ctx.dp(s.listItemSpacingDp), 0, ctx.dp(s.listItemSpacingDp))

            // Task state: 0=normal bullet, 1=unchecked □, 2=checked ☑
            val content = SpannableStringBuilder(block.content)
            when (block.taskState) {
                1 -> { marker.visibility = View.GONE; tv.text = SpannableStringBuilder("☐  ").append(content) }
                2 -> { marker.visibility = View.GONE; tv.text = SpannableStringBuilder("☑  ").append(content) }
                else -> {
                    marker.visibility = View.VISIBLE
                    tv.text = withCursor(content, block.isStreaming, s)
                }
            }
            tv.textSize = s.listTextSizeDp
            tv.setTextColor(s.listTextColor)
            tv.movementMethod = LinkMovementMethod.getInstance()
            applyImages(tv, content)
        }
    }

    private fun makeBulletContainer(ctx: Context, s: MkBlockStyle): LinearLayout {
        val bulletSize = ctx.dp(s.listBulletSizeDp)
        val gap        = ctx.dp(s.listBulletGapDp)

        val bullet = BulletView(ctx, s)
        val bulletLp = LinearLayout.LayoutParams(bulletSize, bulletSize).also {
            it.gravity = Gravity.CENTER_VERTICAL
            it.marginEnd = gap
        }

        val tv = makeTextView(ctx)

        return LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            layoutParams = RecyclerView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            addView(bullet, bulletLp)
            addView(tv, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }
    }

    // ── Ordered item ──────────────────────────────────────────────────────────

    private inner class OrderedVH(ctx: Context, private val s: MkBlockStyle) :
        BlockViewHolder(makeOrderedContainer(ctx, s)) {

        private val container = itemView as LinearLayout
        private val numTv     = container.getChildAt(0) as TextView
        private val tv        = container.getChildAt(1) as TextView

        override fun bind(block: MkBlock) {
            block as MkBlock.OrderedItem
            val ctx = itemView.context
            val indent = ctx.dp(s.listDepthIndentDp) * block.depth
            container.setPadding(indent, ctx.dp(s.listItemSpacingDp), 0, ctx.dp(s.listItemSpacingDp))
            numTv.text = "${block.number}."
            numTv.textSize = s.listTextSizeDp
            numTv.setTextColor(s.listTextColor)
            val content = SpannableStringBuilder(block.content)
            tv.text = withCursor(content, block.isStreaming, s)
            tv.textSize = s.listTextSizeDp
            tv.setTextColor(s.listTextColor)
            tv.movementMethod = LinkMovementMethod.getInstance()
            applyImages(tv, content)
        }
    }

    private fun makeOrderedContainer(ctx: Context, s: MkBlockStyle): LinearLayout {
        val gap   = ctx.dp(s.listBulletGapDp)
        val numTv = TextView(ctx).apply {
            minWidth = ctx.dp(20)
            gravity = Gravity.END or Gravity.TOP
            setPadding(0, 0, gap, 0)
        }
        val tv = makeTextView(ctx)

        return LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.TOP
            layoutParams = RecyclerView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            addView(numTv, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            addView(tv,    LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }
    }

    // ── Block quote ───────────────────────────────────────────────────────────

    private inner class QuoteVH(ctx: Context, private val s: MkBlockStyle) :
        BlockViewHolder(makeQuoteContainer(ctx, s)) {

        private val container = itemView as LinearLayout
        private val tv        = container.getChildAt(1) as TextView

        override fun bind(block: MkBlock) {
            block as MkBlock.BlockQuote
            val content = SpannableStringBuilder(block.content)
            tv.text = withCursor(content, block.isStreaming, s)
            tv.textSize = s.quoteTextSizeDp
            tv.setTextColor(s.quoteTextColor)
            tv.movementMethod = LinkMovementMethod.getInstance()
            applyImages(tv, content)
        }
    }

    private fun makeQuoteContainer(ctx: Context, s: MkBlockStyle): LinearLayout {
        val pad       = ctx.dp(s.quotePaddingDp)
        val marginV   = ctx.dp(s.quoteMarginVertDp)
        val indWidth  = ctx.dp(s.quoteIndicatorWidthDp)

        val indicator = View(ctx).apply {
            setBackgroundColor(s.quoteIndicatorColor)
        }
        val tv = makeTextView(ctx).apply {
            setPadding(pad, ctx.dp(4), pad, ctx.dp(4))
        }

        val bg = GradientDrawable().apply {
            setColor(s.quoteBgColor)
            cornerRadius = s.quoteCornerRadiusDp * ctx.resources.displayMetrics.density
        }

        return LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            background = bg
            val lp = RecyclerView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            lp.setMargins(0, marginV, 0, marginV)
            layoutParams = lp
            addView(indicator, LinearLayout.LayoutParams(indWidth, ViewGroup.LayoutParams.MATCH_PARENT))
            addView(tv,        LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }
    }

    // ── Thematic break ────────────────────────────────────────────────────────

    private inner class ThematicVH(ctx: Context, private val s: MkBlockStyle) :
        BlockViewHolder(makeDivider(ctx, s)) {

        override fun bind(block: MkBlock) { /* static, nothing to bind */ }
    }

    private fun makeDivider(ctx: Context, s: MkBlockStyle): View {
        val color = Color.argb(
            (s.dividerAlpha * 255).toInt(),
            Color.red(s.dividerColor),
            Color.green(s.dividerColor),
            Color.blue(s.dividerColor)
        )
        return View(ctx).apply {
            setBackgroundColor(color)
            val h  = ctx.dp(s.dividerHeightDp)
            val mh = ctx.dp(s.dividerMarginHorzDp)
            val mv = ctx.dp(s.dividerMarginVertDp)
            val lp = RecyclerView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, h)
            lp.setMargins(mh, mv, mh, mv)
            layoutParams = lp
        }
    }

    // ── Table ─────────────────────────────────────────────────────────────────

    private inner class TableVH(ctx: Context, private val s: MkBlockStyle) :
        BlockViewHolder(makeTableContainer(ctx)) {

        private val scroll = itemView as HorizontalScrollView
        private val table  = scroll.getChildAt(0) as TableLayout

        override fun bind(block: MkBlock) {
            block as MkBlock.TableBlock
            table.removeAllViews()
            val ctx = itemView.context

            // Header row
            if (block.headers.isNotEmpty()) {
                val row = TableRow(ctx)
                block.headers.forEach { cell ->
                    row.addView(makeTableCell(ctx, cell, isHeader = true, s))
                }
                table.addView(row, TableLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            }
            // Data rows
            block.rows.forEach { rowData ->
                val row = TableRow(ctx)
                rowData.forEach { cell ->
                    row.addView(makeTableCell(ctx, cell, isHeader = false, s))
                }
                table.addView(row, TableLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            }
        }
    }

    private fun makeTableContainer(ctx: Context): HorizontalScrollView {
        val table = TableLayout(ctx).apply {
            isStretchAllColumns = true
        }
        return HorizontalScrollView(ctx).apply {
            isHorizontalScrollBarEnabled = false
            addView(table, ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            layoutParams = RecyclerView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        }
    }

    private fun makeTableCell(
        ctx: Context,
        content: SpannableStringBuilder,
        isHeader: Boolean,
        s: MkBlockStyle,
    ): TextView {
        val pH = ctx.dp(s.tableCellPaddingHorzDp)
        val pV = ctx.dp(s.tableCellPaddingVertDp)
        return TextView(ctx).apply {
            text = content
            textSize = if (isHeader) s.tableHeaderTextSizeDp else s.tableCellTextSizeDp
            setTextColor(if (isHeader) s.tableHeaderTextColor else s.tableCellTextColor)
            if (isHeader) setTypeface(null, s.tableHeaderTypefaceStyle)
            setPadding(pH, pV, pH, pV)
            maxWidth = ctx.dp(s.tableMaxCellWidthDp)
            movementMethod = LinkMovementMethod.getInstance()
            // Simple bottom/end border via background
            val bg = GradientDrawable().apply {
                setStroke(1, s.tableBorderColor)
                setColor(Color.TRANSPARENT)
            }
            background = bg
        }
    }

    // ── Shared helpers ────────────────────────────────────────────────────────

    private fun makeTextView(ctx: Context): TextView = TextView(ctx).apply {
        layoutParams = RecyclerView.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
    }

    private fun withCursor(
        content: SpannableStringBuilder,
        isStreaming: Boolean,
        s: MkBlockStyle,
    ): CharSequence {
        if (!isStreaming || !s.showStreamingCursor) return content
        return SpannableStringBuilder(content).append(s.streamingCursorChar)
    }

    /**
     * Scans [text] for [URLSpan]s that look like image sources (heuristic: URL ends in
     * common image extension or was tagged as IMAGE by MkBlockParser).
     * For each one, calls [imageLoader] and on completion replaces the URLSpan with an
     * [ImageSpan], then re-sets [tv]'s text to trigger a redraw.
     */
    private fun applyImages(tv: TextView, text: SpannableStringBuilder) {
        val loader = imageLoader ?: return
        val spans = text.getSpans(0, text.length, URLSpan::class.java)
        spans.filter { url -> url.url.looksLikeImage() }.forEach { urlSpan ->
            val start = text.getSpanStart(urlSpan)
            val end   = text.getSpanEnd(urlSpan)
            if (start < 0 || end <= start) return@forEach
            loader(urlSpan.url) { drawable ->
                drawable.setBounds(0, 0, drawable.intrinsicWidth, drawable.intrinsicHeight)
                text.removeSpan(urlSpan)
                text.setSpan(
                    ImageSpan(drawable, urlSpan.url),
                    start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
                )
                tv.text = text
            }
        }
    }

    private fun String.looksLikeImage(): Boolean {
        val lower = lowercase()
        return lower.endsWith(".png") || lower.endsWith(".jpg") || lower.endsWith(".jpeg") ||
               lower.endsWith(".gif") || lower.endsWith(".webp") || lower.endsWith(".svg")
    }
}

// ── BulletView: a small filled circle drawn on canvas ─────────────────────────

private class BulletView(ctx: Context, s: MkBlockStyle) : View(ctx) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = s.listBulletColor
        style = Paint.Style.FILL
    }

    override fun onDraw(canvas: Canvas) {
        val cx = width / 2f
        val cy = height / 2f
        canvas.drawCircle(cx, cy, minOf(cx, cy), paint)
    }
}
