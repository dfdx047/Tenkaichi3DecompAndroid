package com.dfdx047.dragonrage.touch

import android.content.Context
import android.view.View

/** What the in-game overlay and the launcher's editor need of an on-screen controller, whichever layout it is. */
interface TouchListener {
    fun onPad(buttons: Int, lx: Int, ly: Int, rx: Int, ry: Int) {}
    fun onMenu() {}
    fun onOptions() {}
    fun onConfigChanged(config: TouchConfig) {}
    fun onEditDone() {}
}

interface TouchSurface {
    var listener: TouchListener?
    var config: TouchConfig
    var editing: Boolean
    var suppressed: Boolean
    var hiddenForPad: Boolean

    /** With the controls switched off, still tell the engine where a tap was (TouchBridge.tap): BT3 only, whose
     *  menus use it; the other engines keep the touch passing through to the game. */
    var tapWhenOff: Boolean
}

/** The view of the layout the config asks for: the game-specific one (default) or the classic generic pad. */
fun createTouchSurface(context: Context, config: TouchConfig, editing: Boolean = false): TouchSurface =
    if (config.layout == TouchLayout.CLASSIC) ClassicPadView(context, config, editing) else TouchPadView(context, config, editing)

val TouchSurface.view: View get() = this as View

/**
 * The way back when the controller is switched off (the options' "Hide controls"): a small chip at the bottom of the
 * screen that switches it on again. Without it nothing was drawn and no touch was taken, so the options button was
 * gone too and the controls only came back by clearing the app's data.
 */
internal class ShowControlsChip(private val view: View) {
    private val dp = view.resources.displayMetrics.density
    private val rect = android.graphics.RectF()
    private val fill = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG).apply { color = android.graphics.Color.argb(120, 12, 18, 31) }
    private val stroke = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG).apply {
        style = android.graphics.Paint.Style.STROKE
        color = android.graphics.Color.argb(170, 255, 176, 32)
        strokeWidth = 1.5f * dp
    }
    private val text = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG).apply {
        color = android.graphics.Color.argb(215, 238, 245, 255)
        textSize = 12f * dp
        textAlign = android.graphics.Paint.Align.CENTER
        typeface = android.graphics.Typeface.DEFAULT_BOLD
    }

    fun draw(c: android.graphics.Canvas) {
        val label = view.context.getString(com.dfdx047.dragonrage.R.string.touch_show_controls)
        val w = text.measureText(label) + 28f * dp
        val h = 30f * dp
        val bottom = view.height - 10f * dp
        rect.set(view.width / 2f - w / 2, bottom - h, view.width / 2f + w / 2, bottom)
        c.drawRoundRect(rect, h / 2, h / 2, fill)
        c.drawRoundRect(rect, h / 2, h / 2, stroke)
        c.drawText(label, rect.centerX(), rect.centerY() - (text.descent() + text.ascent()) / 2, text)
    }

    /** A touch on the chip (with some room around it: a thumb is wider than the text). */
    fun hit(x: Float, y: Float): Boolean =
        rect.width() > 0f && x >= rect.left - 16f * dp && x <= rect.right + 16f * dp && y >= rect.top - 14f * dp && y <= rect.bottom + 10f * dp
}
