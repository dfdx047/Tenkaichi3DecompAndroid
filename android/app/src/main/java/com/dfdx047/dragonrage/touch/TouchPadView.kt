package com.dfdx047.dragonrage.touch

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.util.DisplayMetrics
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.view.WindowManager
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.engine.TouchBridge
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * The on-screen controller. Draws the controls in the chosen console's look, turns touches into the PS2 pad's
 * buttons and sticks ([TouchListener.onPad]), plays macros, and in edit mode lets the controls be moved, resized and hidden
 * (with its own small toolbar at the top), handing the changed layout to [TouchListener.onConfigChanged].
 * The same view is the in-game overlay and the launcher's layout editor.
 */
@SuppressLint("ViewConstructor")
class TouchPadView(context: Context, initial: TouchConfig, editing: Boolean = false) : View(context), TouchSurface {

    override var listener: TouchListener? = null

    override var config: TouchConfig = initial
        set(v) {
            field = v
            rebuild()
        }

    override var editing: Boolean = editing
        set(v) {
            if (field == v) return
            field = v
            selected = null
            releaseAll()
            if (v) {
                Log.i("bt3", "touch roster: polling paused while layout editor is open")
                handler.removeCallbacks(teamSizePoll)
            } else if (isAttachedToWindow) {
                // Entering edit mode pauses native queries. Explicitly restart them on return to gameplay.
                Log.i("bt3", "touch roster: polling resumed after layout editor")
                handler.removeCallbacks(teamSizePoll)
                handler.post(teamSizePoll)
            }
            rebuild()
        }

    /** Hidden because a physical controller is in use ([TouchMode.AUTO]); a touch shows it again. */
    override var hiddenForPad = false
        set(v) {
            if (field != v) {
                field = v
                if (v) releaseAll()
                invalidate()
            }
        }

    /** The game's own menu is open: the controller is not drawn and touches go to the game (the menu). */
    override var suppressed = false
        set(v) {
            if (field != v) {
                field = v
                if (v) releaseAll()
                invalidate()
            }
        }

    // ---- layout

    private class El(
        val key: String,
        val ctl: Ctl?,
        val macro: Macro?,
        val kind: CtlKind,
        var cx: Float,
        var cy: Float,
        var w: Float,
        var h: Float,
        var scale: Float,
        var visible: Boolean,
        var opacity: Int = 100,
        val roundShoulder: Boolean = false, // the Dragon look draws the shoulder buttons as circles
    ) {
        val round: Boolean get() = kind == CtlKind.BUTTON || kind == CtlKind.STICK_L || kind == CtlKind.STICK_R || kind == CtlKind.SYSTEM ||
            kind == CtlKind.MACRO || (roundShoulder && kind == CtlKind.SHOULDER)
        fun hit(x: Float, y: Float, slack: Float): Boolean =
            if (round) hypot(x - cx, y - cy) <= w / 2 * slack
            else abs(x - cx) <= w / 2 * slack && abs(y - cy) <= h / 2 * slack
    }

    private val dp = resources.displayMetrics.density
    private val showChip = ShowControlsChip(this)
    private val els = mutableListOf<El>()

    // The editor and runtime overlay can span the physical display, including a camera cutout. Keep a local
    // safe rectangle in this View's coordinates; saved placements are normalized inside this rectangle, not
    // across the cutout / letterboxed area. This also works when Android has already inset the Dialog window.
    private var safeInsetLeftPx = 0f
    private var safeInsetTopPx = 0f
    private var safeInsetRightPx = 0f
    private var safeInsetBottomPx = 0f
    private var hasReceivedInsets = false

    private val safeWidth: Float
        get() = (width - safeInsetLeftPx - safeInsetRightPx).coerceAtLeast(1f)
    private val safeHeight: Float
        get() = (height - safeInsetTopPx - safeInsetBottomPx).coerceAtLeast(1f)

    private fun updateSafeInsets(insets: WindowInsets? = rootWindowInsets) {
        if (insets == null) {
            if (!hasReceivedInsets) {
                safeInsetLeftPx = 0f; safeInsetTopPx = 0f
                safeInsetRightPx = 0f; safeInsetBottomPx = 0f
            }
            return
        }
        hasReceivedInsets = true
        val cutout = insets.displayCutout
        val oldLeft = safeInsetLeftPx
        val oldTop = safeInsetTopPx
        val oldRight = safeInsetRightPx
        val oldBottom = safeInsetBottomPx
        if (cutout == null || width <= 0 || height <= 0) {
            safeInsetLeftPx = 0f; safeInsetTopPx = 0f
            safeInsetRightPx = 0f; safeInsetBottomPx = 0f
        } else {
            val location = IntArray(2)
            getLocationOnScreen(location)
            val real = DisplayMetrics()
            @Suppress("DEPRECATION")
            (context.getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay.getRealMetrics(real)
            // Convert display-edge safe insets to this View's local coordinates. If Android already placed
            // the dialog inside the safe area, these values naturally resolve to zero instead of double-shifting.
            safeInsetLeftPx = (cutout.safeInsetLeft - location[0]).toFloat().coerceIn(0f, width.toFloat())
            safeInsetTopPx = (cutout.safeInsetTop - location[1]).toFloat().coerceIn(0f, height.toFloat())
            safeInsetRightPx = (location[0] + width - (real.widthPixels - cutout.safeInsetRight)).toFloat()
                .coerceIn(0f, width.toFloat())
            safeInsetBottomPx = (location[1] + height - (real.heightPixels - cutout.safeInsetBottom)).toFloat()
                .coerceIn(0f, height.toFloat())
        }
        if (abs(oldLeft - safeInsetLeftPx) >= 1f || abs(oldTop - safeInsetTopPx) >= 1f ||
            abs(oldRight - safeInsetRightPx) >= 1f || abs(oldBottom - safeInsetBottomPx) >= 1f) {
            Log.i("bt3", "touch safe-area: view=${width}x${height} insets=${safeInsetLeftPx.toInt()},${safeInsetTopPx.toInt()},${safeInsetRightPx.toInt()},${safeInsetBottomPx.toInt()}")
        }
    }

    override fun onApplyWindowInsets(insets: WindowInsets): WindowInsets {
        val applied = super.onApplyWindowInsets(insets)
        updateSafeInsets(insets)
        rebuild()
        return applied
    }

    /** BT3 has menu screens driven by digital directions. The port doesn't expose a reliable game/menu state,
     * so the player can switch layouts explicitly without a controller. */
    private var navigationMode = false

    private fun baseSize(kind: CtlKind, size: Float): Pair<Float, Float> = when (kind) {
        CtlKind.SHOULDER -> if (config.style == PadStyle.DRAGON) size * 0.74f to size * 0.74f else size to size * 0.52f
        CtlKind.PILL -> size to size * 0.44f
        else -> size to size
    }

    private fun rebuild() {
        updateSafeInsets()
        val selectedKey = selected?.key
        els.clear()
        val w = width.toFloat()
        val h = height.toFloat()
        val left = safeInsetLeftPx
        val top = safeInsetTopPx
        val right = w - safeInsetRightPx
        val bottom = h - safeInsetBottomPx
        val vw = safeWidth
        val vh = safeHeight
        if (w <= 0f || h <= 0f) return
        val all = config.scale
        for (c in Ctl.entries) {
            val p = config.placeOf(c)
            val s = p?.scale ?: 1f
            val (bw, bh) = baseSize(c.kind, c.size)
            val rawX = if (p != null) left + p.x * vw else left + c.x * vw + c.dx * dp * all
            val rawY = if (p != null) top + p.y * vh else top + c.y * vh + c.dy * dp * all
            val requestedW = bw * dp * all * s
            val requestedH = bh * dp * all * s
            // Fit the complete artwork and hit area to the real usable viewport before clamping the centre.
            val fit = min(1f, min(
                (vw - 24f * dp).coerceAtLeast(1f) / requestedW,
                (vh - 20f * dp).coerceAtLeast(1f) / requestedH,
            ))
            val ew = requestedW * fit
            val eh = requestedH * fit
            val marginX = max(ew / 2f + 12f * dp, vw * 0.04f).coerceAtMost(vw / 2f)
            val marginY = max(eh / 2f + 10f * dp, vh * 0.035f).coerceAtMost(vh / 2f)
            val cx = rawX.coerceIn(left + marginX, max(left + marginX, right - marginX))
            val cy = rawY.coerceIn(top + marginY, max(top + marginY, bottom - marginY))
            els += El(c.name, c, null, c.kind, cx, cy, ew, eh, s, c == Ctl.OPTIONS || (p?.visible ?: c.shownByDefault), p?.opacity ?: 100,
                roundShoulder = config.style == PadStyle.DRAGON)
        }
        config.macros.forEachIndexed { i, m ->
            val requestedSize = 58f * dp * all * m.place.scale
            val size = requestedSize.coerceAtMost(min(vw - 24f * dp, vh - 20f * dp).coerceAtLeast(1f))
            val (px, py) = if (m.place == Placement(0.74f, 0.40f, 1f, true)) (0.74f - 0.07f * (i % 3)) to (0.38f - 0.13f * (i / 3)) else m.place.x to m.place.y
            val marginX = max(size / 2f + 12f * dp, vw * 0.04f).coerceAtMost(vw / 2f)
            val marginY = max(size / 2f + 10f * dp, vh * 0.035f).coerceAtMost(vh / 2f)
            val cx = (left + px * vw).coerceIn(left + marginX, max(left + marginX, right - marginX))
            val cy = (top + py * vh).coerceIn(top + marginY, max(top + marginY, bottom - marginY))
            els += El("M:" + m.id, null, m, CtlKind.MACRO, cx, cy, size, size, m.place.scale, m.place.visible, m.place.opacity)
        }
        selected = selectedKey?.let { key -> els.firstOrNull { it.key == key } }
        invalidate()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        rebuild()
    }

    // ---- state

    private class Track(var el: El?, val downX: Float, val downY: Float, var originX: Float, var originY: Float, var moved: Boolean = false,
                        val downAt: Long = SystemClock.uptimeMillis(), var floated: Boolean = false)

    private val windowAt = IntArray(2)

    /** A short touch that stayed put and was not on a control (or only woke the floating stick) is a tap on the
     *  game's picture: the Tag Team menus press what is under it (TouchBridge.tap, in window pixels). */
    private fun tapEnded(t: Track, x: Float, y: Float) {
        if (t.el != null && !t.floated) return
        if (SystemClock.uptimeMillis() - t.downAt > 450) return
        if (hypot(x - t.downX, y - t.downY) > 18f * resources.displayMetrics.density) return
        getLocationInWindow(windowAt)
        TouchBridge.tap((x + windowAt[0]).toInt(), (y + windowAt[1]).toInt())
    }

    private val tracks = HashMap<Int, Track>()
    private var dpadBits = 0
    private var lx = 128
    private var ly = 128
    private var rx = 128
    private var ry = 128
    private var lastSent = longArrayOf(-1)
    private val pressedEls = HashSet<El>()
    private var macroMask = 0
    private val players = HashMap<String, MacroPlayer>()
    private val handler = Handler(Looper.getMainLooper())
    private var floatL: Pair<Float, Float>? = null // where a floating left stick was put down
    private var playerTeamSize = 0
    private var lastLoggedTeamSize = Int.MIN_VALUE
    private var oneShotMask = 0

    /** A short native button chord creates the same edge as pressing L3 and R3 together on a controller. */
    private val releaseTeamSwap = Runnable {
        oneShotMask = 0
        recompute()
    }

    /** A right-stick-down pulse maps to both BT3 target-direction bits for the physical player pad. */
    private val releaseTeamSelect = Runnable {
        if (tracks.values.none { it.el?.ctl == Ctl.STICK_R }) {
            rx = 128; ry = 128
        }
        recompute()
    }

    // Automatic menu / fight layout: 0 = no fight is running (menus), anything else is a fight (-1: unknown, which counts
    // as a fight, as the controls always did). Going to the fight layout is at once; back to the menu layout needs the
    // state twice in a row (a second), so that a blip does not flip it. The MENU/FIGHT button still works in between.
    private var autoState = 0 // 0 not known yet, 1 menu, 2 fight
    private var menuVotes = 0

    private fun autoSwitch(teamSize: Int) {
        if (teamSize != 0) {
            menuVotes = 0
            if (autoState != 2) {
                autoState = 2
                setNavigation(false)
            }
        } else if (autoState != 1) {
            if (autoState == 0 || ++menuVotes >= 2) {
                autoState = 1
                menuVotes = 0
                setNavigation(true)
            }
        }
    }

    private fun setNavigation(on: Boolean) {
        if (navigationMode != on) {
            navigationMode = on
            releaseAll()
            invalidate()
        }
    }

    private val teamSizePoll = object : Runnable {
        override fun run() {
            if (!isAttachedToWindow) return
            if (!editing) {
                val current = TouchBridge.playerTeamSize()
                if (current != lastLoggedTeamSize) {
                    Log.i("bt3", "touch roster: overlay poll teamSize=$current editing=$editing")
                    lastLoggedTeamSize = current
                }
                if (config.autoMode) autoSwitch(current)
                if (current != playerTeamSize) {
                    playerTeamSize = current
                    invalidate()
                }
            }
            // Never let a temporary editor open permanently stop discovery of the active team.
            handler.postDelayed(this, 500L)
        }
    }

    private inner class MacroPlayer(val macro: Macro) {
        var held = false
        var mask = 0
        private var index = 0
        private var running = false
        private val step = object : Runnable {
            override fun run() {
                if (macro.steps.isEmpty()) {
                    finish(); return
                }
                if (index >= macro.steps.size) {
                    if (macro.repeat && held) index = 0 else {
                        finish(); return
                    }
                }
                val s = macro.steps[index++]
                mask = s.buttons
                recompute()
                handler.postDelayed(this, s.ms.toLong().coerceAtLeast(16))
            }
        }

        fun start() {
            held = true
            if (running) return
            running = true
            index = 0
            handler.post(step)
        }

        fun release() {
            held = false
        }

        fun cancel() {
            handler.removeCallbacks(step)
            finish()
        }

        private fun finish() {
            running = false
            mask = 0
            recompute()
        }
    }

    private fun releaseAll() {
        handler.removeCallbacks(releaseTeamSwap)
        handler.removeCallbacks(releaseTeamSelect)
        oneShotMask = 0
        tracks.clear()
        pressedEls.clear()
        dpadBits = 0
        lx = 128; ly = 128; rx = 128; ry = 128
        floatL = null
        players.values.forEach { it.cancel() }
        players.clear()
        recompute()
        invalidate()
    }

    private fun recompute() {
        var b = dpadBits
        for (e in pressedEls) if (e.ctl != null) b = b or e.ctl.bit
        macroMask = 0
        for (p in players.values) macroMask = macroMask or p.mask
        b = b or macroMask or oneShotMask
        val packed = (b.toLong() shl 32) or (lx.toLong() shl 24) or (ly.toLong() shl 16) or (rx.toLong() shl 8) or ry.toLong()
        if (packed != lastSent[0]) {
            lastSent[0] = packed
            listener?.onPad(b, lx, ly, rx, ry)
        }
        invalidate()
    }

    private fun buzz() {
        if (config.haptics) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
    }

    // ---- touches

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(ev: MotionEvent): Boolean {
        if (suppressed && !editing) return false
        if (config.mode == TouchMode.OFF && !editing) {
            // switched off: only the chip that switches it on again takes a touch, the rest goes to the game
            if (ev.actionMasked == MotionEvent.ACTION_DOWN && showChip.hit(ev.x, ev.y)) {
                config = config.copy(mode = TouchMode.AUTO)
                listener?.onConfigChanged(config)
                return true
            }
            return false
        }
        if (hiddenForPad && !editing) {
            if (ev.actionMasked == MotionEvent.ACTION_DOWN) {
                hiddenForPad = false
                tracks[ev.getPointerId(0)] = Track(null, ev.x, ev.y, ev.x, ev.y) // (it may still be a tap on a menu: up)
            }
            return true
        }
        if (editing) return editTouch(ev)
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = ev.actionIndex
                down(ev.getPointerId(i), ev.getX(i), ev.getY(i))
            }
            MotionEvent.ACTION_MOVE -> for (i in 0 until ev.pointerCount) move(ev.getPointerId(i), ev.getX(i), ev.getY(i))
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val i = ev.actionIndex
                up(ev.getPointerId(i), ev.getX(i), ev.getY(i))
            }
            MotionEvent.ACTION_CANCEL -> releaseAll()
        }
        return true
    }

    private fun isShownInCurrentMode(e: El): Boolean {
        if (editing) {
            // The right stick is only an internal target-selection input; PRÓX. sends its pulse. Do not draw
            // this duplicate virtual stick in the editor. Every actual on-screen button remains editable,
            // including Pause, Options, MENU/LUTA, Start/Select and controls hidden by the user.
            return e.ctl != Ctl.STICK_R
        }
        // TEAM_SWAP is superseded by the existing L3/TROCA control at runtime, but remains editable here.
        if (e.ctl == Ctl.TEAM_SWAP) return false
        // L3/TROCA and PRÓX only appear in team battles. -1 is an unknown active state: fail open for diagnosis.
        if ((e.ctl == Ctl.L3 || e.ctl == Ctl.TEAM_SELECT) && playerTeamSize in 0..1) return false
        // This switch must remain reachable from either layout.
        if (e.ctl == Ctl.NAV_MODE) return true
        if (!e.visible) return false
        if (!navigationMode) return e.ctl != Ctl.DPAD
        return when (e.ctl) {
            Ctl.DPAD, Ctl.CROSS, Ctl.CIRCLE, Ctl.SQUARE, Ctl.TRIANGLE,
            Ctl.START, Ctl.SELECT, Ctl.MENU, Ctl.OPTIONS, Ctl.NAV_MODE -> true
            else -> false // hide movement sticks, combat macros and shoulder actions while navigating menus
        }
    }

    private fun find(x: Float, y: Float, slack: Float = 1.12f, kinds: Set<CtlKind>? = null): El? {
        var best: El? = null
        var bestD = Float.MAX_VALUE
        for (e in els) {
            if (!isShownInCurrentMode(e) || (kinds != null && e.kind !in kinds)) continue
            if (e.hit(x, y, slack)) {
                val d = hypot(x - e.cx, y - e.cy)
                if (d < bestD) {
                    bestD = d; best = e
                }
            }
        }
        return best
    }

    private fun down(id: Int, x: Float, y: Float) {
        var e = find(x, y)
        val stickL = els.firstOrNull { it.ctl == Ctl.STICK_L }
        if (e == null && config.sticksFloat && stickL != null && stickL.visible && x < width * 0.45f && y > height * 0.35f) {
            e = stickL
            floatL = x to y
        }
        val t = Track(e, x, y, e?.cx ?: x, e?.cy ?: y)
        if (e === stickL && floatL != null) {
            t.originX = x; t.originY = y
            t.floated = true
        }
        tracks[id] = t
        if (e == null) return
        when (e.kind) {
            CtlKind.BUTTON, CtlKind.SHOULDER, CtlKind.PILL -> {
                if (e.ctl == Ctl.L3) {
                    // BT3's in-battle character switch is the L3+R3 chord, not L3 by itself.
                    buzz()
                    pulseTeamSwap()
                } else {
                    pressedEls += e; buzz(); recompute()
                }
            }
            CtlKind.DPAD -> {
                dpadBits = dpadDir(e, x, y); buzz(); recompute()
            }
            CtlKind.STICK_L, CtlKind.STICK_R -> stick(e, t, x, y)
            CtlKind.MACRO -> {
                val m = e.macro ?: return
                buzz()
                players.getOrPut(m.id) { MacroPlayer(m) }.start()
                pressedEls += e
                invalidate()
            }
            CtlKind.SYSTEM -> {
                pressedEls += e; buzz(); invalidate()
            }
        }
    }

    private fun move(id: Int, x: Float, y: Float) {
        val t = tracks[id] ?: return
        val e = t.el
        if (e == null) {
            // a finger that landed between buttons and slides onto one presses it
            val n = find(x, y, 1.0f, setOf(CtlKind.BUTTON))
            if (n != null) {
                t.el = n; pressedEls += n; buzz(); recompute()
            }
            return
        }
        when (e.kind) {
            CtlKind.BUTTON -> if (!e.hit(x, y, 1.25f)) {
                // sliding across the face buttons: the one under the finger now
                pressedEls -= e
                val n = find(x, y, 1.0f, setOf(CtlKind.BUTTON))
                t.el = n
                if (n != null) {
                    pressedEls += n; buzz()
                }
                recompute()
            }
            CtlKind.DPAD -> {
                val d = dpadDir(e, x, y)
                if (d != dpadBits) {
                    dpadBits = d; recompute()
                }
            }
            CtlKind.STICK_L, CtlKind.STICK_R -> stick(e, t, x, y)
            else -> {}
        }
    }

    private fun up(id: Int, x: Float, y: Float) {
        val t = tracks.remove(id) ?: return
        tapEnded(t, x, y)
        val e = t.el ?: return
        when (e.kind) {
            CtlKind.BUTTON, CtlKind.SHOULDER, CtlKind.PILL -> {
                if (tracks.values.none { it.el === e }) pressedEls -= e
                recompute()
            }
            CtlKind.DPAD -> {
                dpadBits = 0; recompute()
            }
            CtlKind.STICK_L -> {
                lx = 128; ly = 128; floatL = null; recompute()
            }
            CtlKind.STICK_R -> {
                rx = 128; ry = 128; recompute()
            }
            CtlKind.MACRO -> {
                e.macro?.let { players[it.id]?.release() }
                pressedEls -= e; invalidate()
            }
            CtlKind.SYSTEM -> {
                pressedEls -= e; invalidate()
                if (e.hit(x, y, 1.3f)) {
                    when (e.ctl) {
                        Ctl.MENU -> listener?.onMenu()
                        Ctl.NAV_MODE -> {
                            navigationMode = !navigationMode
                            releaseAll()
                        }
                        Ctl.TEAM_SWAP -> pulseTeamSwap()
                        Ctl.TEAM_SELECT -> pulseTeamSelect()
                        Ctl.OPTIONS -> listener?.onOptions()
                        else -> listener?.onOptions()
                    }
                }
            }
        }
    }

    private fun pulseTeamSwap() {
        // 0/1 are known non-team states. -1 means the native state is not yet readable, so allow testing.
        if (playerTeamSize in 0..1) return
        handler.removeCallbacks(releaseTeamSwap)
        oneShotMask = Ps2.L3 or Ps2.R3
        recompute()
        handler.postDelayed(releaseTeamSwap, 140L)
    }

    private fun pulseTeamSelect() {
        if (playerTeamSize in 0..1 || tracks.values.any { it.el?.ctl == Ctl.STICK_R }) return
        handler.removeCallbacks(releaseTeamSelect)
        rx = 128
        ry = 255 // one short down input advances the target; the injected engine mirrors it into RS_DOWN2
        recompute()
        handler.postDelayed(releaseTeamSelect, 100L)
    }

    private fun dpadDir(e: El, x: Float, y: Float): Int {
        val dx = x - e.cx
        val dy = y - e.cy
        if (hypot(dx, dy) < e.w * 0.12f) return 0
        val a = Math.toDegrees(atan2(-dy.toDouble(), dx.toDouble())).let { if (it < 0) it + 360 else it }
        // eight sectors of 45 degrees, the diagonals press two directions
        val sector = (((a + 22.5) % 360) / 45).toInt()
        return when (sector) {
            0 -> Ps2.RIGHT
            1 -> Ps2.RIGHT or Ps2.UP
            2 -> Ps2.UP
            3 -> Ps2.UP or Ps2.LEFT
            4 -> Ps2.LEFT
            5 -> Ps2.LEFT or Ps2.DOWN
            6 -> Ps2.DOWN
            else -> Ps2.DOWN or Ps2.RIGHT
        }
    }

    private fun stick(e: El, t: Track, x: Float, y: Float) {
        val r = e.w * 0.40f
        var dx = (x - t.originX) / r
        var dy = (y - t.originY) / r
        val m = hypot(dx, dy)
        if (m > 1f) {
            dx /= m; dy /= m
        }
        val vx = (128 + dx * 127).roundToInt().coerceIn(0, 255)
        val vy = (128 + dy * 127).roundToInt().coerceIn(0, 255)
        if (e.kind == CtlKind.STICK_L) {
            lx = vx; ly = vy
        } else {
            rx = vx; ry = vy
        }
        recompute()
    }

    // ---- edit mode

    private var selected: El? = null
    private var dragging: Track? = null
    private val toolRects = Array(7) { RectF() } // size −/+, opacity −/+, show/hide, reset, done

    private fun editTouch(ev: MotionEvent): Boolean {
        val x = ev.x
        val y = ev.y
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                val ti = toolRects.indexOfFirst { it.contains(x, y) }
                if (ti >= 0) {
                    tool(ti); return true
                }
                var e: El? = null
                var bestD = Float.MAX_VALUE
                for (el in els) if (isShownInCurrentMode(el) && el.hit(x, y, 1.1f)) {
                    val d = hypot(x - el.cx, y - el.cy)
                    if (d < bestD) {
                        bestD = d; e = el
                    }
                }
                selected = e
                dragging = e?.let { Track(it, x, y, it.cx, it.cy) }
                invalidate()
            }
            MotionEvent.ACTION_MOVE -> dragging?.let { t ->
                val e = t.el ?: return true
                val minX = safeInsetLeftPx + max(e.w / 2f + 12f * dp, safeWidth * 0.04f).coerceAtMost(safeWidth / 2f)
                val maxX = width - safeInsetRightPx - max(e.w / 2f + 12f * dp, safeWidth * 0.04f).coerceAtMost(safeWidth / 2f)
                val minY = safeInsetTopPx + max(e.h / 2f + 10f * dp, safeHeight * 0.035f).coerceAtMost(safeHeight / 2f)
                val maxY = height - safeInsetBottomPx - max(e.h / 2f + 10f * dp, safeHeight * 0.035f).coerceAtMost(safeHeight / 2f)
                e.cx = (t.originX + x - t.downX).coerceIn(minX, max(minX, maxX))
                e.cy = (t.originY + y - t.downY).coerceIn(minY, max(minY, maxY))
                t.moved = true
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                val t = dragging
                dragging = null
                if (t?.moved == true) t.el?.let { store(it) }
            }
        }
        return true
    }

    private fun tool(i: Int) {
        val e = selected
        when (i) {
            0, 1 -> {
                val f = if (i == 0) 1f / 1.1f else 1.1f
                if (e == null) {
                    config = config.copy(scale = (config.scale * f).coerceIn(0.5f, 2f))
                    listener?.onConfigChanged(config)
                } else {
                    e.scale = (e.scale * f).coerceIn(0.4f, 2.5f)
                    store(e)
                }
            }
            2, 3 -> {
                val delta = if (i == 2) -10 else 10
                if (e == null) {
                    config = config.copy(opacity = (config.opacity + delta).coerceIn(10, 100))
                    listener?.onConfigChanged(config)
                    rebuild()
                } else {
                    e.opacity = (e.opacity + delta).coerceIn(10, 100)
                    store(e)
                }
            }
            4 -> if (e != null) {
                // (the options button stays: it is the only way to the editor and to "Show controls")
                if (e.ctl != Ctl.OPTIONS) e.visible = !e.visible
                store(e)
            }
            5 -> {
                config = if (e == null) {
                    config.copy(places = emptyMap(), scale = 1f, opacity = 70, macros = config.macros.map { it.copy(place = Placement(0.74f, 0.40f)) })
                } else if (e.ctl != null) {
                    config.copy(places = config.places - e.ctl)
                } else {
                    config.copy(macros = config.macros.map { if (it.id == e.macro?.id) it.copy(place = Placement(0.74f, 0.40f)) else it })
                }
                selected = null
                listener?.onConfigChanged(config)
            }
            6 -> {
                selected = null
                listener?.onEditDone()
            }
        }
        invalidate()
    }

    /** The element's position, scale and visibility into the configuration. */
    private fun store(e: El) {
        val p = Placement(
            ((e.cx - safeInsetLeftPx) / safeWidth).coerceIn(0f, 1f),
            ((e.cy - safeInsetTopPx) / safeHeight).coerceIn(0f, 1f),
            e.scale, e.visible, e.opacity,
        )
        val key = e.key
        config = if (e.ctl != null) {
            config.copy(places = config.places + (e.ctl to p))
        } else {
            config.copy(macros = config.macros.map { if (it.id == e.macro?.id) it.copy(place = p) else it })
        }
        selected = els.firstOrNull { it.key == key }
        listener?.onConfigChanged(config)
    }

    // ---- drawing

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
    }
    private val dash = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.rgb(255, 176, 32)
        pathEffect = DashPathEffect(floatArrayOf(10f, 8f), 0f)
    }
    private val path = Path()
    private val rect = RectF()

    private fun a(base: Int, alpha: Float): Int = Color.argb((Color.alpha(base) * alpha).roundToInt().coerceIn(0, 255), Color.red(base), Color.green(base), Color.blue(base))

    override fun onDraw(canvas: Canvas) {
        if (!editing && !suppressed && config.mode == TouchMode.OFF) {
            showChip.draw(canvas)
            return
        }
        if (!editing && (suppressed || hiddenForPad || config.mode == TouchMode.OFF)) return
        val alpha = config.opacity / 100f
        for (e in els) {
            if (!isShownInCurrentMode(e)) continue
            val k = if (!e.visible) 0.3f else alpha * (e.opacity / 100f)
            val pressed = e in pressedEls || (e.ctl == Ctl.L3 && oneShotMask != 0) ||
                (e.kind == CtlKind.MACRO && e.macro?.let { players[it.id]?.held } == true)
            if (config.style == PadStyle.DRAGON) drawDragon(canvas, e, k, pressed) else drawEl(canvas, e, k, pressed)
            if (editing && e === selected) {
                dash.strokeWidth = 2.5f * dp
                if (e.round) canvas.drawCircle(e.cx, e.cy, e.w / 2 + 6 * dp, dash)
                else canvas.drawRect(e.cx - e.w / 2 - 6 * dp, e.cy - e.h / 2 - 6 * dp, e.cx + e.w / 2 + 6 * dp, e.cy + e.h / 2 + 6 * dp, dash)
            }
        }
        if (editing) drawToolbar(canvas)
    }

    // DR_TOUCH_VISUAL_V42_FIXED: translucent DBZ-style touch artwork; input behavior unchanged.
    private fun drawEl(c: Canvas, e: El, k: Float, pressed: Boolean) {
        val ctl = e.ctl
        val accent = ctl?.let(::actionColor) ?: Color.rgb(80, 190, 240)
        val base = Color.rgb(12, 18, 31)
        val white = Color.rgb(238, 245, 255)
        stroke.strokeWidth = 2f * dp
        when (e.kind) {
            CtlKind.BUTTON -> {
                val button = ctl!!
                val r = e.w / 2 * (if (pressed) 0.93f else 1f)
                fill.color = a(if (pressed) Color.argb(92, Color.red(accent), Color.green(accent), Color.blue(accent)) else Color.argb(30, 10, 17, 31), k)
                c.drawCircle(e.cx, e.cy, r, fill)
                stroke.color = a(if (pressed) Color.WHITE else accent, k)
                stroke.strokeWidth = (if (pressed) 2.2f else 1.35f) * dp
                c.drawCircle(e.cx, e.cy, r, stroke)
                if (navigationMode && button in listOf(Ctl.CROSS, Ctl.CIRCLE, Ctl.SQUARE, Ctl.TRIANGLE)) {
                    // In menus the button shows only its own symbol (no captions): the cluster stays clean.
                    if (config.skin == PadSkin.PLAYSTATION) {
                        psSymbol(c, button, e.cx, e.cy, r * 0.46f, a(white, k))
                    } else {
                        label(c, faceLabel(config.skin, button), e.cx, e.cy, r * 1.1f, a(white, k))
                    }
                } else if (button in listOf(Ctl.CROSS, Ctl.CIRCLE, Ctl.SQUARE, Ctl.TRIANGLE, Ctl.L3, Ctl.R3, Ctl.BLAST1, Ctl.BLAST2, Ctl.ULTIMATE)) {
                    stroke.color = a(Color.argb(70, 255, 248, 228), k)
                    stroke.strokeWidth = 1f * dp
                    c.drawCircle(e.cx, e.cy, r * 0.82f, stroke)
                    drawActionGlyph(c, button, e.cx, e.cy, r * 0.47f, a(accent, k))
                    drawControlBadge(c, button, e.cx + r * 0.69f, e.cy - r * 0.69f, r * 0.23f, k)
                } else {
                    label(c, faceLabel(config.skin, button), e.cx, e.cy, r * 0.9f, a(white, k))
                }
            }
            CtlKind.SHOULDER -> {
                rect.set(e.cx - e.w / 2, e.cy - e.h / 2, e.cx + e.w / 2, e.cy + e.h / 2)
                val rr = e.h / 2
                fill.color = a(if (pressed) Color.argb(92, Color.red(accent), Color.green(accent), Color.blue(accent)) else Color.argb(30, 10, 17, 31), k)
                c.drawRoundRect(rect, rr, rr, fill)
                stroke.color = a(if (pressed) Color.WHITE else accent, k)
                stroke.strokeWidth = (if (pressed) 2.2f else 1.25f) * dp
                c.drawRoundRect(rect, rr, rr, stroke)
                if (ctl != null && ctl in listOf(Ctl.L1, Ctl.L2, Ctl.R1, Ctl.R2)) {
                    drawActionGlyph(c, ctl, e.cx - e.w * 0.02f, e.cy, e.h * 0.34f, a(accent, k))
                    drawControlBadge(c, ctl, e.cx + e.w * 0.39f, e.cy - e.h * 0.29f, e.h * 0.21f, k)
                } else {
                    label(c, ctl?.let { faceLabel(config.skin, it) } ?: "", e.cx, e.cy, e.h * 0.85f, a(white, k))
                }
            }
            CtlKind.PILL -> {
                rect.set(e.cx - e.w / 2, e.cy - e.h / 2, e.cx + e.w / 2, e.cy + e.h / 2)
                val rr = e.h / 2
                fill.color = a(if (pressed) Color.argb(92, Color.red(accent), Color.green(accent), Color.blue(accent)) else Color.argb(30, 10, 17, 31), k)
                c.drawRoundRect(rect, rr, rr, fill)
                stroke.color = a(if (pressed) Color.WHITE else Color.argb(170, 239, 231, 211), k)
                c.drawRoundRect(rect, rr, rr, stroke)
                drawUtilityGlyph(c, ctl ?: Ctl.START, e.cx, e.cy, e.h * 0.28f, a(white, k))
            }
            CtlKind.DPAD -> drawDpad(c, e, k)
            CtlKind.STICK_L, CtlKind.STICK_R -> {
                val (sx, sy) = if (e.kind == CtlKind.STICK_L) lx to ly else rx to ry
                val (ox, oy) = if (e.kind == CtlKind.STICK_L) (floatL ?: (e.cx to e.cy)) else (e.cx to e.cy)
                val r = e.w / 2f
                // Transparent martial-arts HUD: directional marks and a compact thumb cap.
                fill.color = a(Color.argb(30, 10, 16, 30), k)
                c.drawCircle(ox, oy, r, fill)
                stroke.color = a(Color.argb(190, 238, 229, 207), k * 0.82f)
                stroke.strokeWidth = max(1.15f * dp, r * 0.018f)
                c.drawCircle(ox, oy, r, stroke)
                stroke.color = a(Color.argb(86, 250, 245, 226), k)
                stroke.strokeWidth = max(0.8f * dp, r * 0.012f)
                c.drawCircle(ox, oy, r * 0.72f, stroke)
                // Four subtle directional arrowheads around the ring (Shadow-Fight-inspired, DBZ palette).
                fill.color = a(Color.argb(190, 239, 231, 211), k * 0.85f)
                val mark = r * 0.105f
                fun arrow(dx: Float, dy: Float) {
                    val ax = ox + dx * r * 0.80f
                    val ay = oy + dy * r * 0.80f
                    path.reset()
                    path.moveTo(ax + dx * mark, ay + dy * mark)
                    path.lineTo(ax - dx * mark * 0.72f - dy * mark * 0.72f, ay - dy * mark * 0.72f + dx * mark * 0.72f)
                    path.lineTo(ax - dx * mark * 0.72f + dy * mark * 0.72f, ay - dy * mark * 0.72f - dx * mark * 0.72f)
                    path.close()
                    c.drawPath(path, fill)
                }
                arrow(0f, -1f); arrow(1f, 0f); arrow(0f, 1f); arrow(-1f, 0f)
                val kx = ox + (sx - 128) / 127f * e.w * 0.30f
                val ky = oy + (sy - 128) / 127f * e.w * 0.30f
                val active = sx != 128 || sy != 128
                fill.color = a(Color.argb(if (active) 150 else 112, 245, 235, 213), k)
                c.drawCircle(kx, ky, e.w * 0.185f, fill)
                stroke.color = a(if (active) Color.rgb(255, 207, 116) else Color.argb(195, 255, 250, 232), k)
                stroke.strokeWidth = max(1.1f * dp, e.w * 0.012f)
                c.drawCircle(kx, ky, e.w * 0.185f, stroke)
                fill.color = a(Color.argb(110, 255, 255, 245), k)
                c.drawCircle(kx, ky, e.w * 0.075f, fill)
            }
            CtlKind.SYSTEM -> {
                val r = e.w / 2
                val accent = when (ctl) {
                    Ctl.TEAM_SELECT -> Color.rgb(255, 181, 74)
                    Ctl.TEAM_SWAP -> Color.rgb(64, 204, 170)
                    else -> Color.rgb(105, 139, 175)
                }
                fill.color = a(if (pressed) Color.argb(82, 255, 197, 105) else Color.argb(28, 10, 16, 30), k)
                c.drawCircle(e.cx, e.cy, r, fill)
                val navActive = ctl == Ctl.NAV_MODE && navigationMode
                stroke.color = a(if (navActive) Color.rgb(255, 176, 52) else accent, k)
                stroke.strokeWidth = if (navActive || pressed) 3f * dp else 2f * dp
                c.drawCircle(e.cx, e.cy, r, stroke)
                stroke.color = a(Color.argb(58, 255, 248, 229), k)
                stroke.strokeWidth = 1f * dp
                c.drawCircle(e.cx, e.cy, r * 0.82f, stroke)
                systemGlyph(c, e, r, k)
            }
            CtlKind.MACRO -> {
                val r = e.w / 2 * (if (pressed) 0.93f else 1f)
                fill.color = a(if (pressed) Color.argb(110, 255, 198, 75) else Color.argb(30, 10, 16, 30), k)
                c.drawCircle(e.cx, e.cy, r, fill)
                stroke.color = a(Color.rgb(255, 183, 70), k)
                stroke.strokeWidth = 1.6f * dp
                c.drawCircle(e.cx, e.cy, r, stroke)
                drawActionGlyph(c, Ctl.TRIANGLE, e.cx, e.cy - r * 0.12f, r * 0.34f, a(Color.rgb(255, 198, 75), k))
                if (editing) label(c, e.macro?.name?.take(5) ?: "MACRO", e.cx, e.cy + r * 0.55f, r * 0.9f, a(white, k))
            }
        }
    }

    /** The icons of the round system buttons (menu, layout switch, team), in either look. */
    private fun systemGlyph(c: Canvas, e: El, r: Float, k: Float) {
        val ctl = e.ctl
        val white = if (config.style == PadStyle.DRAGON) gold else Color.rgb(238, 245, 255)
        val accent = when (ctl) {
            Ctl.TEAM_SELECT -> Color.rgb(255, 181, 74)
            Ctl.TEAM_SWAP -> Color.rgb(64, 204, 170)
            else -> Color.rgb(105, 139, 175)
        }
        val navActive = ctl == Ctl.NAV_MODE && navigationMode
        stroke.strokeWidth = 2f * dp
        fill.color = a(white, k)
        if (ctl == Ctl.MENU) {
            val bw = r * 0.18f
            val bh = r * 0.8f
            c.drawRect(e.cx - r * 0.3f - bw / 2, e.cy - bh / 2, e.cx - r * 0.3f + bw / 2, e.cy + bh / 2, fill)
            c.drawRect(e.cx + r * 0.3f - bw / 2, e.cy - bh / 2, e.cx + r * 0.3f + bw / 2, e.cy + bh / 2, fill)
        } else if (ctl == Ctl.NAV_MODE) {
            // Four-way arrow icon; the amber ring says that the menu layout is on (no caption).
            val arrow = r * 0.62f
            stroke.color = a(if (navActive) Color.rgb(255, 190, 70) else white, k)
            stroke.strokeWidth = 2.2f * dp
            for (i in 0..3) {
                val angle = i * (Math.PI / 2.0)
                val dx = kotlin.math.cos(angle).toFloat()
                val dy = kotlin.math.sin(angle).toFloat()
                c.drawLine(e.cx - dx * arrow * 0.35f, e.cy - dy * arrow * 0.35f,
                    e.cx + dx * arrow * 0.62f, e.cy + dy * arrow * 0.62f, stroke)
                val side = arrow * 0.24f
                path.reset()
                path.moveTo(e.cx + dx * arrow * 0.62f, e.cy + dy * arrow * 0.62f)
                path.lineTo(e.cx + dx * arrow * 0.28f - dy * side, e.cy + dy * arrow * 0.28f + dx * side)
                path.lineTo(e.cx + dx * arrow * 0.28f + dy * side, e.cy + dy * arrow * 0.28f - dx * side)
                path.close()
                c.drawPath(path, fill)
            }
        } else if (ctl == Ctl.TEAM_SELECT) {
            // Three small roster cards with the centre card highlighted.
            val cw = r * 0.39f
            val ch = r * 0.57f
            for (i in -1..1) {
                val cx = e.cx + i * cw * 0.88f
                val card = RectF(cx - cw * 0.42f, e.cy - ch * 0.5f, cx + cw * 0.42f, e.cy + ch * 0.5f)
                fill.color = a(if (i == 0) accent else Color.rgb(53, 75, 97), k)
                c.drawRoundRect(card, 2f * dp, 2f * dp, fill)
                stroke.color = a(if (i == 0) accent else white, k)
                stroke.strokeWidth = 1.4f * dp
                c.drawRoundRect(card, 2f * dp, 2f * dp, stroke)
            }
            stroke.color = a(white, k)
            stroke.strokeWidth = 1.8f * dp
            c.drawLine(e.cx + r * 0.69f, e.cy - r * 0.1f, e.cx + r * 0.69f, e.cy + r * 0.3f, stroke)
            path.reset()
            path.moveTo(e.cx + r * 0.53f, e.cy + r * 0.17f)
            path.lineTo(e.cx + r * 0.69f, e.cy + r * 0.34f)
            path.lineTo(e.cx + r * 0.85f, e.cy + r * 0.17f)
            path.close()
            fill.color = a(white, k)
            c.drawPath(path, fill)
        } else if (ctl == Ctl.TEAM_SWAP) {
            // Two opposing arrows mean switch to the selected team member.
            stroke.color = a(white, k)
            stroke.strokeWidth = 2.5f * dp
            stroke.strokeCap = Paint.Cap.ROUND
            val ax1 = e.cx - r * 0.53f; val ax2 = e.cx + r * 0.44f
            val ay1 = e.cy - r * 0.2f; val ay2 = e.cy + r * 0.2f
            c.drawLine(ax1, ay1, ax2, ay1, stroke)
            c.drawLine(ax2, ay1, ax2 - r * 0.2f, ay1 - r * 0.17f, stroke)
            c.drawLine(ax2, ay1, ax2 - r * 0.2f, ay1 + r * 0.17f, stroke)
            c.drawLine(ax2, ay2, ax1, ay2, stroke)
            c.drawLine(ax1, ay2, ax1 + r * 0.2f, ay2 - r * 0.17f, stroke)
            c.drawLine(ax1, ay2, ax1 + r * 0.2f, ay2 + r * 0.17f, stroke)
            stroke.strokeCap = Paint.Cap.BUTT
        } else {
            for (i in -1..1) c.drawCircle(e.cx + i * r * 0.42f, e.cy, r * 0.12f, fill)
        }
    }

    // ---- the Dragon look (0.6.0, the default): dark round buttons lit from above, gold icons, coloured letters

    private val shade = Paint(Paint.ANTI_ALIAS_FLAG)
    private val gold = Color.rgb(255, 199, 92)
    private val goldHot = Color.rgb(255, 226, 140)
    private val rim = Color.rgb(196, 188, 214)

    /** A dark sphere-like disc: lighter towards the top left, as the buttons in the mock-up. */
    private fun orb(c: Canvas, x: Float, y: Float, r: Float, k: Float, pressed: Boolean) {
        val light = if (pressed) Color.rgb(108, 92, 70) else Color.rgb(78, 72, 102)
        val dark = if (pressed) Color.rgb(40, 30, 22) else Color.rgb(20, 17, 32)
        shade.shader = RadialGradient(x - r * 0.28f, y - r * 0.38f, r * 1.25f, a(Color.argb(238, Color.red(light), Color.green(light), Color.blue(light)), k),
            a(Color.argb(238, Color.red(dark), Color.green(dark), Color.blue(dark)), k), Shader.TileMode.CLAMP)
        c.drawCircle(x, y, r, shade)
        shade.shader = null
        stroke.color = a(if (pressed) goldHot else Color.argb(215, Color.red(rim), Color.green(rim), Color.blue(rim)), k)
        stroke.strokeWidth = max(1.6f * dp, r * (if (pressed) 0.075f else 0.05f))
        c.drawCircle(x, y, r - stroke.strokeWidth / 2, stroke)
    }

    /** The small dark disc at the top right with the button's letter (Xbox colours) or symbol. */
    private fun letterBadge(c: Canvas, ctl: Ctl, x: Float, y: Float, r: Float, k: Float) {
        val sticker = when (ctl) {
            Ctl.BLAST1 -> Ctl.CIRCLE
            Ctl.BLAST2, Ctl.ULTIMATE -> Ctl.TRIANGLE
            else -> ctl
        }
        fill.color = a(Color.argb(235, 18, 15, 26), k)
        c.drawCircle(x, y, r, fill)
        val face = sticker in listOf(Ctl.CROSS, Ctl.CIRCLE, Ctl.SQUARE, Ctl.TRIANGLE)
        val col = if (face && config.skin != PadSkin.NINTENDO) faceColor(config.skin, sticker) else Color.WHITE
        if (config.skin == PadSkin.PLAYSTATION && face) {
            psSymbol(c, sticker, x, y, r * 0.45f, a(col, k))
        } else {
            val t = faceLabel(config.skin, sticker)
            label(c, t, x, y, r * (if (t.length > 1) 1.5f else 1.75f), a(col, k))
        }
    }

    private fun star(c: Canvas, x: Float, y: Float, r: Float) {
        path.reset()
        for (i in 0 until 10) {
            val ang = -Math.PI / 2 + i * Math.PI / 5
            val rr = if (i % 2 == 0) r else r * 0.45f
            val px = x + (kotlin.math.cos(ang) * rr).toFloat()
            val py = y + (kotlin.math.sin(ang) * rr).toFloat()
            if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
        }
        path.close()
        c.drawPath(path, fill)
    }

    /** The three-star Dragon Ball the stick is held by. */
    private fun dragonBall(c: Canvas, x: Float, y: Float, r: Float, k: Float, active: Boolean) {
        shade.shader = RadialGradient(x - r * 0.3f, y - r * 0.35f, r * 1.3f, a(Color.rgb(255, 196, 96), k),
            a(if (active) Color.rgb(232, 116, 22) else Color.rgb(206, 104, 28), k), Shader.TileMode.CLAMP)
        c.drawCircle(x, y, r, shade)
        shade.shader = null
        fill.color = a(Color.argb(90, 255, 255, 235), k)
        c.drawCircle(x - r * 0.38f, y - r * 0.42f, r * 0.16f, fill)
        fill.color = a(Color.rgb(214, 38, 30), k)
        star(c, x, y - r * 0.30f, r * 0.27f)
        star(c, x - r * 0.27f, y + r * 0.18f, r * 0.27f)
        star(c, x + r * 0.27f, y + r * 0.18f, r * 0.27f)
        if (active) {
            stroke.color = a(goldHot, k)
            stroke.strokeWidth = 2f * dp
            c.drawCircle(x, y, r + 1.5f * dp, stroke)
        }
    }

    /** Icons drawn only in the Dragon look; the others come from [drawActionGlyph]. */
    private fun dragonGlyph(c: Canvas, ctl: Ctl, x: Float, y: Float, r: Float, color: Int) {
        val oldWidth = stroke.strokeWidth
        stroke.color = color
        stroke.strokeWidth = max(1.8f * dp, r * 0.16f)
        stroke.strokeCap = Paint.Cap.ROUND
        stroke.strokeJoin = Paint.Join.ROUND
        fill.color = color
        when (ctl) {
            Ctl.CROSS -> { // dash: a comet with its tail
                c.drawCircle(x + r * 0.30f, y, r * 0.30f, stroke)
                c.drawLine(x - r * 0.95f, y - r * 0.30f, x - r * 0.10f, y - r * 0.10f, stroke)
                c.drawLine(x - r * 0.95f, y + r * 0.30f, x - r * 0.10f, y + r * 0.10f, stroke)
                c.drawLine(x - r * 0.70f, y, x - r * 0.12f, y, stroke)
                c.drawPoint(x + r * 0.82f, y - r * 0.58f, stroke)
                c.drawPoint(x + r * 0.82f, y + r * 0.58f, stroke)
            }
            Ctl.CIRCLE -> { // guard: a shield with a smaller one inside
                fun shield(s: Float) {
                    path.reset()
                    path.moveTo(x, y - r * 0.82f * s)
                    path.lineTo(x + r * 0.66f * s, y - r * 0.52f * s)
                    path.quadTo(x + r * 0.62f * s, y + r * 0.42f * s, x, y + r * 0.86f * s)
                    path.quadTo(x - r * 0.62f * s, y + r * 0.42f * s, x - r * 0.66f * s, y - r * 0.52f * s)
                    path.close()
                }
                shield(1f)
                c.drawPath(path, stroke)
                shield(0.48f)
                c.drawPath(path, fill)
            }
            Ctl.TRIANGLE -> { // Ki blast: an eight-pointed burst
                path.reset()
                for (i in 0 until 16) {
                    val ang = -Math.PI / 2 + i * Math.PI / 8
                    val rr = if (i % 2 == 0) r * 0.95f else r * 0.42f
                    val px = x + (kotlin.math.cos(ang) * rr).toFloat()
                    val py = y + (kotlin.math.sin(ang) * rr).toFloat()
                    if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
                }
                path.close()
                c.drawPath(path, fill)
                fill.color = Color.argb(Color.alpha(color), 30, 24, 40)
                c.drawCircle(x, y, r * 0.2f, fill)
            }
            Ctl.L2 -> { // Ki charge: a flame
                path.reset()
                path.moveTo(x, y - r * 0.95f)
                path.cubicTo(x + r * 0.20f, y - r * 0.45f, x + r * 0.75f, y - r * 0.15f, x + r * 0.62f, y + r * 0.35f)
                path.cubicTo(x + r * 0.52f, y + r * 0.80f, x - r * 0.52f, y + r * 0.80f, x - r * 0.62f, y + r * 0.35f)
                path.cubicTo(x - r * 0.70f, y - r * 0.05f, x - r * 0.30f, y - r * 0.25f, x - r * 0.18f, y - r * 0.55f)
                path.cubicTo(x - r * 0.05f, y - r * 0.35f, x + r * 0.05f, y - r * 0.60f, x, y - r * 0.95f)
                path.close()
                c.drawPath(path, fill)
                fill.color = Color.argb(Color.alpha(color), 30, 24, 40)
                c.drawOval(x - r * 0.20f, y + r * 0.10f, x + r * 0.20f, y + r * 0.58f, fill)
            }
            Ctl.R1, Ctl.R2 -> { // flight: double chevrons up / down
                val d = if (ctl == Ctl.R1) 1f else -1f
                for (o in listOf(-0.30f, 0.22f)) {
                    path.reset()
                    path.moveTo(x - r * 0.62f, y - d * (o - 0.30f) * r)
                    path.lineTo(x, y - d * (o + 0.30f) * r)
                    path.lineTo(x + r * 0.62f, y - d * (o - 0.30f) * r)
                    c.drawPath(path, stroke)
                }
            }
            else -> drawActionGlyph(c, ctl, x, y, r, color)
        }
        stroke.strokeWidth = oldWidth
        stroke.strokeCap = Paint.Cap.BUTT
        stroke.strokeJoin = Paint.Join.MITER
    }

    private fun drawDragon(c: Canvas, e: El, k: Float, pressed: Boolean) {
        val ctl = e.ctl
        val icon = a(if (pressed) goldHot else gold, k)
        when (e.kind) {
            CtlKind.BUTTON, CtlKind.SHOULDER -> {
                val button = ctl!!
                val r = min(e.w, e.h) / 2 * (if (pressed) 0.93f else 1f)
                orb(c, e.cx, e.cy, r, k, pressed)
                if (navigationMode && button in listOf(Ctl.CROSS, Ctl.CIRCLE, Ctl.SQUARE, Ctl.TRIANGLE)) {
                    // menus: the button's own letter or symbol, large, in its colour
                    val col = a(if (config.skin == PadSkin.NINTENDO) Color.WHITE else faceColor(config.skin, button), k)
                    if (config.skin == PadSkin.PLAYSTATION) psSymbol(c, button, e.cx, e.cy, r * 0.42f, col)
                    else label(c, faceLabel(config.skin, button), e.cx, e.cy, r * 1.15f, col)
                } else if (button == Ctl.SELECT || button == Ctl.START) {
                    drawUtilityGlyph(c, button, e.cx, e.cy, r * 0.5f, icon)
                } else {
                    dragonGlyph(c, button, e.cx, e.cy, r * 0.46f, icon)
                    letterBadge(c, button, e.cx + r * 0.72f, e.cy - r * 0.72f, r * 0.27f, k)
                }
            }
            CtlKind.PILL -> {
                rect.set(e.cx - e.w / 2, e.cy - e.h / 2, e.cx + e.w / 2, e.cy + e.h / 2)
                val rr = e.h / 2
                shade.shader = RadialGradient(e.cx, e.cy - e.h * 0.4f, e.w * 0.7f, a(Color.argb(235, 78, 72, 102), k),
                    a(Color.argb(235, 20, 17, 32), k), Shader.TileMode.CLAMP)
                c.drawRoundRect(rect, rr, rr, shade)
                shade.shader = null
                stroke.color = a(if (pressed) goldHot else Color.argb(215, Color.red(rim), Color.green(rim), Color.blue(rim)), k)
                stroke.strokeWidth = 1.6f * dp
                c.drawRoundRect(rect, rr, rr, stroke)
                drawUtilityGlyph(c, ctl ?: Ctl.START, e.cx, e.cy, e.h * 0.28f, icon)
            }
            CtlKind.DPAD -> {
                val s = e.w / 2
                orb(c, e.cx, e.cy, s, k, false)
                fill.color = icon
                val t = s * 0.17f
                fun tri(dx: Float, dy: Float, on: Boolean) {
                    val x = e.cx + dx * s * 0.62f
                    val y = e.cy + dy * s * 0.62f
                    fill.color = a(if (on) goldHot else gold, k)
                    if (on) {
                        shade.color = a(Color.argb(70, 255, 200, 90), k)
                        c.drawCircle(x, y, t * 2.1f, shade)
                    }
                    path.reset()
                    path.moveTo(x + dx * t, y + dy * t)
                    path.lineTo(x - dy * t * 0.9f - dx * t * 0.6f, y + dx * t * 0.9f - dy * t * 0.6f)
                    path.lineTo(x + dy * t * 0.9f - dx * t * 0.6f, y - dx * t * 0.9f - dy * t * 0.6f)
                    path.close()
                    c.drawPath(path, fill)
                }
                tri(0f, -1f, dpadBits and Ps2.UP != 0)
                tri(0f, 1f, dpadBits and Ps2.DOWN != 0)
                tri(-1f, 0f, dpadBits and Ps2.LEFT != 0)
                tri(1f, 0f, dpadBits and Ps2.RIGHT != 0)
                fill.color = a(Color.argb(200, 36, 31, 52), k)
                c.drawCircle(e.cx, e.cy, s * 0.26f, fill)
            }
            CtlKind.STICK_L, CtlKind.STICK_R -> {
                val (sx, sy) = if (e.kind == CtlKind.STICK_L) lx to ly else rx to ry
                val (ox, oy) = if (e.kind == CtlKind.STICK_L) (floatL ?: (e.cx to e.cy)) else (e.cx to e.cy)
                val r = e.w / 2f
                shade.shader = RadialGradient(ox, oy - r * 0.25f, r * 1.1f, a(Color.argb(225, 74, 68, 98), k),
                    a(Color.argb(225, 22, 18, 34), k), Shader.TileMode.CLAMP)
                c.drawCircle(ox, oy, r, shade)
                shade.shader = null
                stroke.color = a(Color.argb(220, Color.red(rim), Color.green(rim), Color.blue(rim)), k)
                stroke.strokeWidth = max(1.8f * dp, r * 0.022f)
                c.drawCircle(ox, oy, r - stroke.strokeWidth / 2, stroke)
                fill.color = a(gold, k)
                val mark = r * 0.075f
                fun arrow(dx: Float, dy: Float) {
                    val ax = ox + dx * r * 0.80f
                    val ay = oy + dy * r * 0.80f
                    path.reset()
                    path.moveTo(ax + dx * mark, ay + dy * mark)
                    path.lineTo(ax - dx * mark * 0.72f - dy * mark * 0.85f, ay - dy * mark * 0.72f + dx * mark * 0.85f)
                    path.lineTo(ax - dx * mark * 0.72f + dy * mark * 0.85f, ay - dy * mark * 0.72f - dx * mark * 0.85f)
                    path.close()
                    c.drawPath(path, fill)
                }
                arrow(0f, -1f); arrow(1f, 0f); arrow(0f, 1f); arrow(-1f, 0f)
                val kx = ox + (sx - 128) / 127f * e.w * 0.30f
                val ky = oy + (sy - 128) / 127f * e.w * 0.30f
                dragonBall(c, kx, ky, e.w * 0.20f, k, sx != 128 || sy != 128)
            }
            CtlKind.SYSTEM, CtlKind.MACRO -> {
                // the round system buttons keep their own icons (drawn by the other look) over the Dragon disc
                val r = e.w / 2 * (if (pressed) 0.93f else 1f)
                orb(c, e.cx, e.cy, r, k, pressed || (ctl == Ctl.NAV_MODE && navigationMode))
                if (e.kind == CtlKind.MACRO) {
                    dragonGlyph(c, Ctl.TRIANGLE, e.cx, e.cy - r * 0.12f, r * 0.34f, icon)
                    if (editing) label(c, e.macro?.name?.take(5) ?: "MACRO", e.cx, e.cy + r * 0.55f, r * 0.9f, a(Color.WHITE, k))
                } else {
                    systemGlyph(c, e, r, k)
                }
            }
        }
    }

    /** Action icon and action name stay constant; only the small reference badge changes with the selected pad. */
    private fun actionName(ctl: Ctl): String = when (ctl) {
        Ctl.CROSS -> "DASH"
        Ctl.CIRCLE -> "DEF"
        Ctl.SQUARE -> "SOCO"
        Ctl.TRIANGLE -> "KI"
        Ctl.L1 -> "ALVO"
        Ctl.L2 -> "CARREGAR"
        Ctl.R1 -> "SUBIR"
        Ctl.R2 -> "DESCER"
        Ctl.L3 -> "TROCA"
        Ctl.R3 -> "FORMA"
        Ctl.BLAST1 -> "SUP.1"
        Ctl.BLAST2 -> "SUP.2"
        Ctl.ULTIMATE -> "ULT"
        else -> faceLabel(config.skin, ctl)
    }

    private fun actionColor(ctl: Ctl): Int = when (ctl) {
        Ctl.CROSS -> Color.rgb(81, 196, 255)       // dash / movement
        Ctl.CIRCLE -> Color.rgb(77, 224, 188)     // defence
        Ctl.SQUARE -> Color.rgb(255, 183, 70)     // physical attack
        Ctl.TRIANGLE -> Color.rgb(116, 160, 255)  // Ki blast
        Ctl.L1 -> Color.rgb(115, 190, 235)
        Ctl.L2 -> Color.rgb(95, 177, 255)
        Ctl.R1, Ctl.R2 -> Color.rgb(170, 190, 219)
        Ctl.L3, Ctl.R3 -> Color.rgb(255, 198, 75)
        Ctl.BLAST1 -> Color.rgb(255, 181, 74)
        Ctl.TEAM_SELECT -> Color.rgb(255, 181, 74)
        Ctl.TEAM_SWAP -> Color.rgb(64, 204, 170)
        Ctl.BLAST2 -> Color.rgb(149, 135, 255)
        Ctl.ULTIMATE -> Color.rgb(255, 105, 93)
        else -> Color.rgb(105, 139, 175)
    }

    private fun drawUtilityGlyph(c: Canvas, ctl: Ctl, x: Float, y: Float, r: Float, color: Int) {
        val oldWidth = stroke.strokeWidth
        val oldCap = stroke.strokeCap
        val oldJoin = stroke.strokeJoin
        stroke.color = color
        stroke.strokeWidth = max(1.5f * dp, r * 0.13f)
        stroke.strokeCap = Paint.Cap.ROUND
        stroke.strokeJoin = Paint.Join.ROUND
        if (ctl == Ctl.START) {
            path.reset()
            path.moveTo(x - r * 0.48f, y - r * 0.68f)
            path.lineTo(x + r * 0.65f, y)
            path.lineTo(x - r * 0.48f, y + r * 0.68f)
            path.close()
            c.drawPath(path, stroke)
        } else {
            c.drawLine(x - r * 0.62f, y - r * 0.48f, x + r * 0.62f, y - r * 0.48f, stroke)
            c.drawLine(x - r * 0.62f, y, x + r * 0.62f, y, stroke)
            c.drawLine(x - r * 0.62f, y + r * 0.48f, x + r * 0.62f, y + r * 0.48f, stroke)
            fill.color = color
            c.drawCircle(x - r * 0.18f, y - r * 0.48f, r * 0.13f, fill)
            c.drawCircle(x + r * 0.25f, y, r * 0.13f, fill)
            c.drawCircle(x - r * 0.05f, y + r * 0.48f, r * 0.13f, fill)
        }
        stroke.strokeWidth = oldWidth
        stroke.strokeCap = oldCap
        stroke.strokeJoin = oldJoin
    }

    private fun drawControlBadge(c: Canvas, ctl: Ctl, x: Float, y: Float, r: Float, k: Float) {
        val sticker = when (ctl) {
            Ctl.BLAST1 -> Ctl.CIRCLE
            Ctl.BLAST2, Ctl.ULTIMATE -> Ctl.TRIANGLE
            else -> ctl
        }
        fill.color = a(Color.argb(58, 10, 17, 31), k)
        c.drawCircle(x, y, r, fill)
        stroke.color = a(actionColor(ctl), k)
        stroke.strokeWidth = max(1.0f * dp, r * 0.10f)
        c.drawCircle(x, y, r, stroke)
        if (config.skin == PadSkin.PLAYSTATION && sticker in listOf(Ctl.CROSS, Ctl.CIRCLE, Ctl.SQUARE, Ctl.TRIANGLE)) {
            psSymbol(c, sticker, x, y, r * 0.43f, a(Color.WHITE, k))
        } else {
            label(c, faceLabel(config.skin, sticker), x, y, r * 1.35f, a(Color.WHITE, k))
        }
    }

    /** Small Canvas-drawn glyphs keep the overlay lightweight and independent of bitmap assets. */
    private fun drawActionGlyph(c: Canvas, ctl: Ctl, x: Float, y: Float, r: Float, color: Int) {
        val oldWidth = stroke.strokeWidth
        val oldCap = stroke.strokeCap
        val oldJoin = stroke.strokeJoin
        stroke.color = color
        stroke.strokeWidth = max(1.8f * dp, r * 0.16f)
        stroke.strokeCap = Paint.Cap.ROUND
        stroke.strokeJoin = Paint.Join.ROUND
        fill.color = color
        when (ctl) {
            Ctl.SQUARE -> { // stylised fist: knuckles and palm
                val nw = r * 0.36f
                val nh = r * 0.34f
                for (i in 0..2) {
                    rect.set(x - r * 0.58f + i * nw * 0.94f, y - r * 0.53f, x - r * 0.58f + i * nw * 0.94f + nw * 0.82f, y - r * 0.53f + nh)
                    c.drawRoundRect(rect, nh * 0.18f, nh * 0.18f, fill)
                }
                path.reset()
                path.moveTo(x - r * 0.62f, y - r * 0.13f)
                path.lineTo(x + r * 0.47f, y - r * 0.13f)
                path.lineTo(x + r * 0.57f, y + r * 0.37f)
                path.lineTo(x + r * 0.22f, y + r * 0.61f)
                path.lineTo(x - r * 0.43f, y + r * 0.46f)
                path.close()
                c.drawPath(path, fill)
                c.drawLine(x - r * 0.25f, y + r * 0.13f, x + r * 0.27f, y + r * 0.13f, stroke)
            }
            Ctl.CIRCLE -> { // shield
                path.reset()
                path.moveTo(x, y - r * 0.75f)
                path.lineTo(x + r * 0.62f, y - r * 0.48f)
                path.lineTo(x + r * 0.52f, y + r * 0.22f)
                path.lineTo(x, y + r * 0.76f)
                path.lineTo(x - r * 0.52f, y + r * 0.22f)
                path.lineTo(x - r * 0.62f, y - r * 0.48f)
                path.close()
                c.drawPath(path, stroke)
                c.drawLine(x, y - r * 0.4f, x, y + r * 0.35f, stroke)
            }
            Ctl.CROSS -> { // dash / speed streaks
                c.drawLine(x - r * 0.78f, y - r * 0.35f, x + r * 0.2f, y - r * 0.35f, stroke)
                c.drawLine(x - r * 0.95f, y, x + r * 0.52f, y, stroke)
                c.drawLine(x - r * 0.68f, y + r * 0.35f, x + r * 0.2f, y + r * 0.35f, stroke)
                c.drawLine(x + r * 0.06f, y - r * 0.44f, x + r * 0.67f, y, stroke)
                c.drawLine(x + r * 0.67f, y, x + r * 0.06f, y + r * 0.44f, stroke)
            }
            Ctl.TRIANGLE -> { // Ki spark
                for (i in 0..3) {
                    val a = i * (Math.PI / 2.0)
                    val dx = kotlin.math.cos(a).toFloat()
                    val dy = kotlin.math.sin(a).toFloat()
                    c.drawLine(x + dx * r * 0.22f, y + dy * r * 0.22f, x + dx * r * 0.78f, y + dy * r * 0.78f, stroke)
                }
                c.drawCircle(x, y, r * 0.23f, fill)
            }
            Ctl.L1 -> { // lock-on target
                c.drawCircle(x, y, r * 0.62f, stroke)
                c.drawCircle(x, y, r * 0.20f, stroke)
                c.drawLine(x - r, y, x - r * 0.5f, y, stroke)
                c.drawLine(x + r * 0.5f, y, x + r, y, stroke)
                c.drawLine(x, y - r, x, y - r * 0.5f, stroke)
                c.drawLine(x, y + r * 0.5f, x, y + r, stroke)
            }
            Ctl.L2, Ctl.R3 -> { // charging aura / transformation spark
                path.reset()
                path.moveTo(x + r * 0.12f, y - r * 0.82f)
                path.lineTo(x - r * 0.38f, y - r * 0.05f)
                path.lineTo(x - r * 0.02f, y - r * 0.05f)
                path.lineTo(x - r * 0.18f, y + r * 0.76f)
                path.lineTo(x + r * 0.43f, y - r * 0.12f)
                path.lineTo(x + r * 0.08f, y - r * 0.12f)
                path.close()
                c.drawPath(path, fill)
            }
            Ctl.R1, Ctl.R2 -> { // flight height
                val d = if (ctl == Ctl.R1) -1f else 1f
                c.drawLine(x, y - d * r * 0.75f, x, y + d * r * 0.55f, stroke)
                c.drawLine(x, y + d * r * 0.55f, x - r * 0.48f, y + d * r * 0.10f, stroke)
                c.drawLine(x, y + d * r * 0.55f, x + r * 0.48f, y + d * r * 0.10f, stroke)
            }
            Ctl.L3 -> { // character switch
                c.drawLine(x - r * 0.75f, y - r * 0.25f, x + r * 0.50f, y - r * 0.25f, stroke)
                c.drawLine(x + r * 0.50f, y - r * 0.25f, x + r * 0.20f, y - r * 0.52f, stroke)
                c.drawLine(x + r * 0.50f, y - r * 0.25f, x + r * 0.20f, y + r * 0.02f, stroke)
                c.drawLine(x + r * 0.75f, y + r * 0.32f, x - r * 0.50f, y + r * 0.32f, stroke)
                c.drawLine(x - r * 0.50f, y + r * 0.32f, x - r * 0.20f, y + r * 0.05f, stroke)
                c.drawLine(x - r * 0.50f, y + r * 0.32f, x - r * 0.20f, y + r * 0.59f, stroke)
            }
            Ctl.BLAST1 -> { // compressed energy wave
                c.drawCircle(x, y, r * 0.31f, stroke)
                c.drawCircle(x, y, r * 0.62f, stroke)
                for (i in 0..3) {
                    val a = i * (Math.PI / 2.0) + Math.PI / 4.0
                    val dx = kotlin.math.cos(a).toFloat()
                    val dy = kotlin.math.sin(a).toFloat()
                    c.drawLine(x + dx * r * 0.72f, y + dy * r * 0.72f, x + dx * r * 0.98f, y + dy * r * 0.98f, stroke)
                }
            }
            Ctl.BLAST2 -> { // stronger energy sphere
                c.drawCircle(x, y, r * 0.36f, fill)
                c.drawCircle(x, y, r * 0.64f, stroke)
                c.drawOval(x - r * 0.92f, y - r * 0.42f, x + r * 0.92f, y + r * 0.42f, stroke)
                c.drawOval(x - r * 0.42f, y - r * 0.92f, x + r * 0.42f, y + r * 0.92f, stroke)
            }
            Ctl.ULTIMATE -> { // explosive finishing move
                for (i in 0..7) {
                    val a = i * (Math.PI / 4.0)
                    val dx = kotlin.math.cos(a).toFloat()
                    val dy = kotlin.math.sin(a).toFloat()
                    c.drawLine(x + dx * r * 0.22f, y + dy * r * 0.22f, x + dx * r * 0.90f, y + dy * r * 0.90f, stroke)
                }
                path.reset()
                path.moveTo(x, y - r * 0.34f)
                path.lineTo(x + r * 0.28f, y)
                path.lineTo(x, y + r * 0.34f)
                path.lineTo(x - r * 0.28f, y)
                path.close()
                c.drawPath(path, fill)
            }
            else -> {
                c.drawCircle(x, y, r * 0.58f, stroke)
                c.drawLine(x - r * 0.6f, y + r * 0.6f, x + r * 0.6f, y - r * 0.6f, stroke)
            }
        }
        stroke.strokeWidth = oldWidth
        stroke.strokeCap = oldCap
        stroke.strokeJoin = oldJoin
    }

    private fun drawDpad(c: Canvas, e: El, k: Float) {
        val s = e.w / 2
        val arm = s * 0.36f
        stroke.strokeWidth = 1.15f * dp
        fun seg(bit: Int, l: Float, t: Float, r: Float, b: Float) {
            rect.set(e.cx + l, e.cy + t, e.cx + r, e.cy + b)
            fill.color = a(if (dpadBits and bit != 0) Color.argb(115, 255, 194, 99) else Color.argb(28, 10, 16, 30), k)
            c.drawRoundRect(rect, arm * 0.35f, arm * 0.35f, fill)
            stroke.color = a(Color.argb(160, 239, 231, 211), k)
            c.drawRoundRect(rect, arm * 0.35f, arm * 0.35f, stroke)
        }
        seg(Ps2.UP, -arm, -s, arm, -arm * 0.6f)
        seg(Ps2.DOWN, -arm, arm * 0.6f, arm, s)
        seg(Ps2.LEFT, -s, -arm, -arm * 0.6f, arm)
        seg(Ps2.RIGHT, arm * 0.6f, -arm, s, arm)
        // arrows
        fill.color = a(Color.argb(220, 246, 239, 220), k)
        val t = arm * 0.45f
        fun tri(x: Float, y: Float, dx: Float, dy: Float) {
            path.reset()
            path.moveTo(x + dx * t, y + dy * t)
            path.lineTo(x - dy * t * 0.8f - dx * t * 0.5f, y + dx * t * 0.8f - dy * t * 0.5f)
            path.lineTo(x + dy * t * 0.8f - dx * t * 0.5f, y - dx * t * 0.8f - dy * t * 0.5f)
            path.close()
            c.drawPath(path, fill)
        }
        val m = (s + arm * 0.6f) / 2
        tri(e.cx, e.cy - m, 0f, -1f)
        tri(e.cx, e.cy + m, 0f, 1f)
        tri(e.cx - m, e.cy, -1f, 0f)
        tri(e.cx + m, e.cy, 1f, 0f)
    }

    private fun label(c: Canvas, s: String, x: Float, y: Float, maxW: Float, color: Int) {
        text.color = color
        text.textSize = maxW * 0.62f
        if (s.length > 2) text.textSize = min(text.textSize, maxW * 1.6f / s.length)
        val fm = text.fontMetrics
        c.drawText(s, x, y - (fm.ascent + fm.descent) / 2, text)
    }

    private fun psSymbol(c: Canvas, ctl: Ctl, x: Float, y: Float, r: Float, color: Int) {
        stroke.color = color
        val old = stroke.strokeWidth
        stroke.strokeWidth = max(2.5f * dp, r * 0.22f)
        when (ctl) {
            Ctl.CROSS -> {
                c.drawLine(x - r, y - r, x + r, y + r, stroke)
                c.drawLine(x - r, y + r, x + r, y - r, stroke)
            }
            Ctl.CIRCLE -> c.drawCircle(x, y, r, stroke)
            Ctl.SQUARE -> c.drawRect(x - r * 0.9f, y - r * 0.9f, x + r * 0.9f, y + r * 0.9f, stroke)
            else -> {
                path.reset()
                path.moveTo(x, y - r * 1.05f)
                path.lineTo(x + r * 1.05f, y + r * 0.75f)
                path.lineTo(x - r * 1.05f, y + r * 0.75f)
                path.close()
                c.drawPath(path, stroke)
            }
        }
        stroke.strokeWidth = old
    }

    private fun lighten(col: Int): Int = Color.rgb(min(255, Color.red(col) + 70), min(255, Color.green(col) + 70), min(255, Color.blue(col) + 70))

    private fun drawToolbar(c: Canvas) {
        val labels = listOf(
            "−", "+", "α−", "α+",
            context.getString(if (selected?.visible == false) R.string.touch_show else R.string.touch_hide),
            context.getString(R.string.touch_reset), context.getString(R.string.touch_done),
        )
        val baseH = 22f * dp
        val baseGap = 2.5f * dp
        text.textSize = 10f * dp
        val baseWidths = labels.mapIndexed { i, label -> if (i < 4) baseH else text.measureText(label) + 12f * dp }
        val baseTotal = baseWidths.sum() + baseGap * (labels.size - 1)
        val left = safeInsetLeftPx + 8f * dp
        val safeRight = width - safeInsetRightPx - 8f * dp
        val available = (safeRight - left).coerceAtLeast(1f)
        val fit = min(1f, available / baseTotal)
        val h = baseH * fit
        val gap = baseGap * fit
        val widths = baseWidths.map { it * fit }
        val total = widths.sum() + gap * (labels.size - 1)
        // Pin the editor toolbar to the upper-left safe corner, never the horizontal centre over MENU/PAUSE.
        val x0 = left
        val y = safeInsetTopPx + 1f * dp

        rect.set(x0 - 4f * dp, y - 1f * dp, x0 + total + 4f * dp, y + h + 14f * dp)
        fill.color = Color.argb(232, 11, 16, 32)
        c.drawRoundRect(rect, 9f * dp, 9f * dp, fill)

        var x = x0
        labels.forEachIndexed { i, label ->
            toolRects[i].set(x, y, x + widths[i], y + h)
            fill.color = when {
                i == 4 && selected != null -> Color.rgb(255, 138, 31)
                i == 4 -> Color.argb(135, 60, 65, 78)
                i == 6 -> Color.rgb(35, 154, 110)
                else -> Color.argb(238, 28, 31, 42)
            }
            c.drawRoundRect(toolRects[i], h / 2f, h / 2f, fill)
            stroke.color = Color.argb(95, 255, 255, 255)
            stroke.strokeWidth = 1f * dp
            c.drawRoundRect(toolRects[i], h / 2f, h / 2f, stroke)
            text.color = if (i == 4 && selected != null) Color.rgb(30, 20, 10) else Color.WHITE
            text.textSize = (if (i < 4) 14f else 10f) * dp * fit
            text.textAlign = Paint.Align.CENTER
            val fm = text.fontMetrics
            c.drawText(label, toolRects[i].centerX(), toolRects[i].centerY() - (fm.ascent + fm.descent) / 2, text)
            x += widths[i] + gap
        }

        // Short, left-aligned help text stays below the toolbar and fits inside the same safe rectangle.
        var status = if (selected == null) {
            "Todos • ${config.opacity}% | arraste: mover • toque: selecionar"
        } else {
            "${nameOf(selected!!).take(16)} • ${selected!!.opacity}% | arraste: mover"
        }
        text.textAlign = Paint.Align.LEFT
        text.color = Color.rgb(224, 232, 245)
        text.textSize = 8.5f * dp * fit
        val maxStatusWidth = (safeRight - x0).coerceAtLeast(1f)
        val measured = text.measureText(status)
        if (measured > maxStatusWidth && measured > 0f) {
            text.textSize *= maxStatusWidth / measured
        }
        c.drawText(status, x0, y + h + 10.5f * dp, text)
        text.textAlign = Paint.Align.CENTER
    }

    private fun nameOf(e: El): String = e.macro?.name ?: when (e.ctl) {
        Ctl.DPAD -> "D-pad"
        Ctl.STICK_L -> "Analógico de movimento"
        Ctl.STICK_R -> "Analógico direito"
        Ctl.MENU -> context.getString(R.string.touch_ctl_menu)
        Ctl.OPTIONS -> context.getString(R.string.touch_ctl_options)
        Ctl.NAV_MODE -> "Alternar modo menu/luta"
        Ctl.TEAM_SELECT -> "Selecionar próximo personagem da equipe"
        Ctl.TEAM_SWAP -> "Trocar para o personagem selecionado"
        Ctl.CROSS, Ctl.CIRCLE, Ctl.SQUARE, Ctl.TRIANGLE, Ctl.L1, Ctl.L2, Ctl.R1, Ctl.R2, Ctl.L3, Ctl.R3,
        Ctl.BLAST1, Ctl.BLAST2, Ctl.ULTIMATE -> e.ctl?.let(::actionName) ?: ""
        else -> e.ctl?.let { faceLabel(config.skin, it) } ?: ""
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        updateSafeInsets()
        rebuild()
        handler.removeCallbacks(teamSizePoll)
        if (!editing) {
            Log.i("bt3", "touch roster: runtime overlay attached; starting team poll")
            handler.post(teamSizePoll)
        }
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        players.values.forEach { it.cancel() }
        handler.removeCallbacksAndMessages(null)
        oneShotMask = 0
    }

    companion object {
        /** The label a control shows in each console's look. Positions stay the PS2's: south is cross. */
        fun faceLabel(skin: PadSkin, c: Ctl): String = when (skin) {
            PadSkin.PLAYSTATION -> when (c) {
                Ctl.CROSS -> "✕"; Ctl.CIRCLE -> "○"; Ctl.SQUARE -> "□"; Ctl.TRIANGLE -> "△"
                Ctl.L1 -> "L1"; Ctl.L2 -> "L2"; Ctl.R1 -> "R1"; Ctl.R2 -> "R2"; Ctl.L3 -> "L3"; Ctl.R3 -> "R3"
                Ctl.START -> "START"; Ctl.SELECT -> "SELECT"
                else -> ""
            }
            PadSkin.XBOX -> when (c) {
                Ctl.CROSS -> "A"; Ctl.CIRCLE -> "B"; Ctl.SQUARE -> "X"; Ctl.TRIANGLE -> "Y"
                Ctl.L1 -> "LB"; Ctl.L2 -> "LT"; Ctl.R1 -> "RB"; Ctl.R2 -> "RT"; Ctl.L3 -> "L3"; Ctl.R3 -> "R3"
                Ctl.START -> "MENU"; Ctl.SELECT -> "VIEW"
                else -> ""
            }
            PadSkin.NINTENDO -> when (c) {
                Ctl.CROSS -> "B"; Ctl.CIRCLE -> "A"; Ctl.SQUARE -> "Y"; Ctl.TRIANGLE -> "X"
                Ctl.L1 -> "L"; Ctl.L2 -> "ZL"; Ctl.R1 -> "R"; Ctl.R2 -> "ZR"; Ctl.L3 -> "LS"; Ctl.R3 -> "RS"
                Ctl.START -> "+"; Ctl.SELECT -> "−"
                else -> ""
            }
        }

        fun faceColor(skin: PadSkin, c: Ctl): Int = when (skin) {
            PadSkin.PLAYSTATION -> when (c) {
                Ctl.CROSS -> Color.rgb(124, 170, 230)
                Ctl.CIRCLE -> Color.rgb(240, 104, 110)
                Ctl.SQUARE -> Color.rgb(226, 140, 196)
                Ctl.TRIANGLE -> Color.rgb(64, 204, 170)
                else -> Color.WHITE
            }
            PadSkin.XBOX -> when (c) {
                Ctl.CROSS -> Color.rgb(96, 186, 70)
                Ctl.CIRCLE -> Color.rgb(226, 70, 64)
                Ctl.SQUARE -> Color.rgb(56, 132, 222)
                Ctl.TRIANGLE -> Color.rgb(244, 196, 48)
                else -> Color.argb(150, 20, 20, 26)
            }
            PadSkin.NINTENDO -> Color.argb(170, 46, 46, 52)
        }
    }
}
