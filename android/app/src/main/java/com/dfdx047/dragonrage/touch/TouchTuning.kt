package com.dfdx047.dragonrage.touch

import android.view.HapticFeedbackConstants
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.roundToInt
import kotlin.math.sin

/**
 * The touch tuning of 1.0 ("Toque Pro"), shared by both on-screen controllers (TouchPadView, ClassicPadView): how a
 * thumb's offset from the stick's centre becomes the PS2 stick's 0..255, where a floating stick may start, and the
 * vibration. Pure arithmetic, so the same numbers come out of the editor's preview and the game's overlay.
 */
object TouchTuning {
    /**
     * The stick's two bytes for a thumb at ([dx], [dy]) pixels from its centre, [r] the stick's radius in pixels:
     * nothing inside the dead zone; from its edge the tilt grows from 0 to full at [TouchConfig.stickRange] % of the
     * radius (so the dead zone does not eat the small tilts: walking stays possible); with [TouchConfig.snap8] the
     * direction is one of the 8 at full tilt.
     */
    fun stickBytes(c: TouchConfig, dx: Float, dy: Float, r: Float): Pair<Int, Int> {
        val dist = hypot(dx, dy)
        if (r <= 0f || dist <= 0f) return 128 to 128
        val dead = r * c.deadZone / 100f
        val full = (r * c.stickRange / 100f).coerceAtLeast(dead + 1f)
        if (dist <= dead) return 128 to 128
        var tilt = ((dist - dead) / (full - dead)).coerceIn(0f, 1f)
        var ang = atan2(dy, dx)
        if (c.snap8) {
            val step = (Math.PI / 4).toFloat()
            ang = (ang / step).roundToInt() * step
            tilt = 1f
        }
        val vx = (128 + cos(ang) * tilt * 127).roundToInt().coerceIn(0, 255)
        val vy = (128 + sin(ang) * tilt * 127).roundToInt().coerceIn(0, 255)
        return vx to vy
    }

    /**
     * A dynamic stick's centre when the thumb at ([x], [y]) is past the radius [r] from ([ox], [oy]): pulled along so
     * the thumb stays on the edge (a turn back is at once a tilt the other way). Null: the centre stays.
     */
    fun follow(c: TouchConfig, ox: Float, oy: Float, x: Float, y: Float, r: Float): Pair<Float, Float>? {
        if (c.stickMode != StickMode.DYNAMIC) return null
        val d = hypot(x - ox, y - oy)
        if (d <= r || d <= 0f) return null
        val k = (d - r) / d
        return (ox + (x - ox) * k) to (oy + (y - oy) * k)
    }

    /** Whether a touch at x (of a view [w] wide) and y (of [h]) may start a floating / dynamic stick. */
    fun inStickZone(c: TouchConfig, x: Float, y: Float, w: Float, h: Float): Boolean {
        if (c.stickMode == StickMode.FIXED) return false
        val zone = w * c.stickZone / 100f
        val side = if (c.leftHanded) x > w - zone else x < zone
        return side && y > h * 0.30f
    }

    /** The touch slack a button is hit with: [TouchConfig.hitArea] % of its drawn size. */
    fun slack(c: TouchConfig): Float = c.hitArea / 100f

    fun buzz(c: TouchConfig, v: View) {
        when (c.haptic) {
            Haptic.OFF -> {}
            Haptic.LIGHT -> v.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
            Haptic.STRONG -> v.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
        }
    }
}
