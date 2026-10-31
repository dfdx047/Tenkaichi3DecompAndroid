package com.dfdx047.dragonrage.touch

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.engine.TouchBridge
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * One fixed control of the on-screen controller. [size] is its diameter (or width) in dp at scale 1; [x], [y] its
 * default centre as a fraction of the screen. Face buttons are placed around a centre so the diamond keeps its shape
 * on any screen: [dx], [dy] in dp from ([x], [y]).
 */
enum class ClassicCtl(
    val kind: CtlKind,
    val bit: Int,
    val size: Float,
    val x: Float,
    val y: Float,
    val dx: Float = 0f,
    val dy: Float = 0f,
    val shownByDefault: Boolean = true,
) {
    DPAD(CtlKind.DPAD, 0, 128f, 0.10f, 0.40f),
    STICK_L(CtlKind.STICK_L, 0, 150f, 0.19f, 0.74f),
    STICK_R(CtlKind.STICK_R, 0, 110f, 0.66f, 0.80f, shownByDefault = false),
    CROSS(CtlKind.BUTTON, Ps2.CROSS, 66f, 0.86f, 0.64f, 0f, 62f),
    CIRCLE(CtlKind.BUTTON, Ps2.CIRCLE, 66f, 0.86f, 0.64f, 62f, 0f),
    SQUARE(CtlKind.BUTTON, Ps2.SQUARE, 66f, 0.86f, 0.64f, -62f, 0f),
    TRIANGLE(CtlKind.BUTTON, Ps2.TRIANGLE, 66f, 0.86f, 0.64f, 0f, -62f),
    L1(CtlKind.SHOULDER, Ps2.L1, 84f, 0.17f, 0.12f),
    L2(CtlKind.SHOULDER, Ps2.L2, 84f, 0.07f, 0.12f),
    R1(CtlKind.SHOULDER, Ps2.R1, 84f, 0.83f, 0.12f),
    R2(CtlKind.SHOULDER, Ps2.R2, 84f, 0.93f, 0.12f),
    L3(CtlKind.BUTTON, Ps2.L3, 50f, 0.30f, 0.60f, shownByDefault = false),
    R3(CtlKind.BUTTON, Ps2.R3, 50f, 0.56f, 0.70f, shownByDefault = false),
    SELECT(CtlKind.PILL, Ps2.SELECT, 70f, 0.42f, 0.92f),
    START(CtlKind.PILL, Ps2.START, 70f, 0.58f, 0.92f),
    MENU(CtlKind.SYSTEM, 0, 40f, 0.47f, 0.07f),    // the game's pause / settings window (the back key)
    OPTIONS(CtlKind.SYSTEM, 0, 40f, 0.53f, 0.07f), // this controller's own quick menu
}

/**
 * The CLASSIC on-screen controller (the layout before 0.3.0: a generic console pad, sticks, d-pad, face diamond,
 * shoulders, Start/Select), kept as an option next to the game-specific one ([TouchPadView]).
 * Draws the controls in the chosen console's look, turns touches into the PS2 pad's
 * buttons and sticks ([TouchListener.onPad]), plays macros, and in edit mode lets the controls be moved, resized and hidden
 * (with its own small toolbar at the top), handing the changed layout to [TouchListener.onConfigChanged].
 * The same view is the in-game overlay and the launcher's layout editor.
 */
@SuppressLint("ViewConstructor")
class ClassicPadView(context: Context, initial: TouchConfig, editing: Boolean = false) : View(context), TouchSurface {

    override var listener: TouchListener? = null

    override var config: TouchConfig = initial
        set(v) {
            field = v
            rebuild()
        }

    override var editing: Boolean = editing
        set(v) {
            field = v
            selected = null
            releaseAll()
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
        val ctl: ClassicCtl?,
        val macro: Macro?,
        val kind: CtlKind,
        var cx: Float,
        var cy: Float,
        var w: Float,
        var h: Float,
        var scale: Float,
        var visible: Boolean,
    ) {
        val round: Boolean get() = kind == CtlKind.BUTTON || kind == CtlKind.STICK_L || kind == CtlKind.STICK_R || kind == CtlKind.SYSTEM || kind == CtlKind.MACRO
        fun hit(x: Float, y: Float, slack: Float): Boolean =
            if (round) hypot(x - cx, y - cy) <= w / 2 * slack
            else abs(x - cx) <= w / 2 * slack && abs(y - cy) <= h / 2 * slack
    }

    private val dp = resources.displayMetrics.density
    private val showChip = ShowControlsChip(this)
    private val els = mutableListOf<El>()

    private fun baseSize(kind: CtlKind, size: Float): Pair<Float, Float> = when (kind) {
        CtlKind.SHOULDER -> size to size * 0.52f
        CtlKind.PILL -> size to size * 0.44f
        else -> size to size
    }

    private fun rebuild() {
        els.clear()
        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return
        val all = config.scale
        // On a narrow screen (the RG Cube's 720 x 720) the defaults would run the shoulder pills into each other and the
        // face diamond and the d-pad past the edges: the outer pills keep to the edges, the inner ones beside them, the
        // diamond and the d-pad come in until they fit. On a 16:9 screen none of this moves anything.
        val pill = baseSize(CtlKind.SHOULDER, 84f).first * dp * all
        val l2 = maxOf(ClassicCtl.L2.x * w, pill / 2)
        val r2 = minOf(ClassicCtl.R2.x * w, w - pill / 2)
        val faceX = minOf(ClassicCtl.CIRCLE.x * w, w - (62f + 66f / 2 + 6f) * dp * all)
        fun defaultX(c: ClassicCtl): Float = when (c) {
            ClassicCtl.L2 -> l2
            ClassicCtl.L1 -> maxOf(c.x * w, l2 + pill)
            ClassicCtl.R2 -> r2
            ClassicCtl.R1 -> minOf(c.x * w, r2 - pill)
            ClassicCtl.CROSS, ClassicCtl.CIRCLE, ClassicCtl.SQUARE, ClassicCtl.TRIANGLE -> faceX
            ClassicCtl.DPAD -> maxOf(c.x * w, c.size / 2 * dp * all)
            else -> c.x * w
        } + c.dx * dp * all
        for (c in ClassicCtl.entries) {
            val p = config.classicPlaces[c.name]
            val s = p?.scale ?: 1f
            val (bw, bh) = baseSize(c.kind, c.size)
            val cx = p?.x?.times(w) ?: defaultX(c)
            val cy = p?.y?.times(h) ?: (c.y * h + c.dy * dp * all)
            els += El(c.name, c, null, c.kind, cx, cy, bw * dp * all * s, bh * dp * all * s, s, c == ClassicCtl.OPTIONS || (p?.visible ?: c.shownByDefault))
        }
        config.macros.forEachIndexed { i, m ->
            val size = 58f * dp * all * m.place.scale
            val (px, py) = if (m.place == Placement(0.74f, 0.40f, 1f, true)) (0.74f - 0.07f * (i % 3)) to (0.38f - 0.13f * (i / 3)) else m.place.x to m.place.y
            els += El("M:" + m.id, null, m, CtlKind.MACRO, px * w, py * h, size, size, m.place.scale, m.place.visible)
        }
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
        b = b or macroMask
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

    private fun find(x: Float, y: Float, slack: Float = 1.12f, kinds: Set<CtlKind>? = null): El? {
        var best: El? = null
        var bestD = Float.MAX_VALUE
        for (e in els) {
            if (!e.visible || (kinds != null && e.kind !in kinds)) continue
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
        val stickL = els.firstOrNull { it.ctl == ClassicCtl.STICK_L }
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
                pressedEls += e; buzz(); recompute()
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
                    if (e.ctl == ClassicCtl.MENU) listener?.onMenu() else listener?.onOptions()
                }
            }
        }
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
    private val toolRects = Array(5) { RectF() } // smaller, larger, show/hide, reset, done

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
                for (el in els) if (el.hit(x, y, 1.1f)) {
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
                e.cx = (t.originX + x - t.downX).coerceIn(0f, width.toFloat())
                e.cy = (t.originY + y - t.downY).coerceIn(0f, height.toFloat())
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
            2 -> if (e != null) {
                // (the options button stays: it is the only way to the editor and to "Show controls")
                if (e.ctl != ClassicCtl.OPTIONS) e.visible = !e.visible
                store(e)
            }
            3 -> {
                config = if (e == null) {
                    config.copy(classicPlaces = emptyMap(), scale = 1f, macros = config.macros.map { it.copy(place = Placement(0.74f, 0.40f)) })
                } else if (e.ctl != null) {
                    config.copy(classicPlaces = config.classicPlaces - e.ctl.name)
                } else {
                    config.copy(macros = config.macros.map { if (it.id == e.macro?.id) it.copy(place = Placement(0.74f, 0.40f)) else it })
                }
                selected = null
                listener?.onConfigChanged(config)
            }
            4 -> {
                selected = null
                listener?.onEditDone()
            }
        }
        invalidate()
    }

    /** The element's position, scale and visibility into the configuration. */
    private fun store(e: El) {
        val p = Placement((e.cx / width).coerceIn(0f, 1f), (e.cy / height).coerceIn(0f, 1f), e.scale, e.visible)
        val key = e.key
        config = if (e.ctl != null) {
            config.copy(classicPlaces = config.classicPlaces + (e.ctl.name to p))
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
            if (!e.visible && !editing) continue
            val k = if (!e.visible) 0.3f else alpha
            val pressed = e in pressedEls || (e.kind == CtlKind.MACRO && e.macro?.let { players[it.id]?.held } == true)
            drawEl(canvas, e, k, pressed)
            if (editing && e === selected) {
                dash.strokeWidth = 2.5f * dp
                if (e.round) canvas.drawCircle(e.cx, e.cy, e.w / 2 + 6 * dp, dash)
                else canvas.drawRect(e.cx - e.w / 2 - 6 * dp, e.cy - e.h / 2 - 6 * dp, e.cx + e.w / 2 + 6 * dp, e.cy + e.h / 2 + 6 * dp, dash)
            }
        }
        if (editing) drawToolbar(canvas)
    }

    private fun drawEl(c: Canvas, e: El, k: Float, pressed: Boolean) {
        val skin = config.skin
        val base = if (pressed) Color.argb(200, 255, 255, 255) else Color.argb(150, 20, 20, 26)
        val ring = Color.argb(200, 255, 255, 255)
        stroke.strokeWidth = 2f * dp
        when (e.kind) {
            CtlKind.BUTTON -> {
                val ctl = e.ctl!!
                val r = e.w / 2 * (if (pressed) 0.93f else 1f)
                val face = faceColor(skin, ctl)
                fill.color = a(if (pressed) lighten(face) else if (skin == PadSkin.XBOX) face else base, k)
                c.drawCircle(e.cx, e.cy, r, fill)
                stroke.color = a(if (skin == PadSkin.XBOX) Color.argb(120, 255, 255, 255) else ring, k)
                c.drawCircle(e.cx, e.cy, r, stroke)
                if (skin == PadSkin.PLAYSTATION && ctl in listOf(ClassicCtl.CROSS, ClassicCtl.CIRCLE, ClassicCtl.SQUARE, ClassicCtl.TRIANGLE)) {
                    psSymbol(c, ctl, e.cx, e.cy, r * 0.42f, a(if (pressed) Color.rgb(30, 30, 30) else face, k))
                } else {
                    label(c, faceLabel(skin, ctl), e.cx, e.cy, r * 0.9f, a(if (skin == PadSkin.XBOX && ctl == ClassicCtl.TRIANGLE) Color.rgb(40, 34, 10) else if (pressed) Color.rgb(30, 30, 30) else Color.WHITE, k))
                }
            }
            CtlKind.SHOULDER, CtlKind.PILL -> {
                rect.set(e.cx - e.w / 2, e.cy - e.h / 2, e.cx + e.w / 2, e.cy + e.h / 2)
                val rr = e.h / 2
                fill.color = a(base, k)
                c.drawRoundRect(rect, rr, rr, fill)
                stroke.color = a(ring, k)
                c.drawRoundRect(rect, rr, rr, stroke)
                label(c, faceLabel(skin, e.ctl!!), e.cx, e.cy, e.h * 0.85f, a(if (pressed) Color.rgb(30, 30, 30) else Color.WHITE, k))
            }
            CtlKind.DPAD -> drawDpad(c, e, k)
            CtlKind.STICK_L, CtlKind.STICK_R -> {
                val (sx, sy) = if (e.kind == CtlKind.STICK_L) lx to ly else rx to ry
                val (ox, oy) = if (e.kind == CtlKind.STICK_L) (floatL ?: (e.cx to e.cy)) else (e.cx to e.cy)
                fill.color = a(Color.argb(110, 20, 20, 26), k)
                c.drawCircle(ox, oy, e.w / 2, fill)
                stroke.color = a(ring, k)
                c.drawCircle(ox, oy, e.w / 2, stroke)
                val kx = ox + (sx - 128) / 127f * e.w * 0.40f
                val ky = oy + (sy - 128) / 127f * e.w * 0.40f
                fill.color = a(if (sx != 128 || sy != 128) Color.argb(230, 255, 255, 255) else Color.argb(190, 70, 70, 80), k)
                c.drawCircle(kx, ky, e.w * 0.24f, fill)
                stroke.color = a(ring, k)
                c.drawCircle(kx, ky, e.w * 0.24f, stroke)
            }
            CtlKind.SYSTEM -> {
                val r = e.w / 2
                fill.color = a(base, k)
                c.drawCircle(e.cx, e.cy, r, fill)
                stroke.color = a(ring, k)
                c.drawCircle(e.cx, e.cy, r, stroke)
                fill.color = a(if (pressed) Color.rgb(30, 30, 30) else Color.WHITE, k)
                if (e.ctl == ClassicCtl.MENU) { // pause bars
                    val bw = r * 0.18f
                    val bh = r * 0.8f
                    c.drawRect(e.cx - r * 0.3f - bw / 2, e.cy - bh / 2, e.cx - r * 0.3f + bw / 2, e.cy + bh / 2, fill)
                    c.drawRect(e.cx + r * 0.3f - bw / 2, e.cy - bh / 2, e.cx + r * 0.3f + bw / 2, e.cy + bh / 2, fill)
                } else { // three dots
                    for (i in -1..1) c.drawCircle(e.cx + i * r * 0.42f, e.cy, r * 0.12f, fill)
                }
            }
            CtlKind.MACRO -> {
                val r = e.w / 2 * (if (pressed) 0.93f else 1f)
                fill.color = a(if (pressed) Color.rgb(255, 176, 32) else Color.argb(150, 20, 20, 26), k)
                c.drawCircle(e.cx, e.cy, r, fill)
                stroke.color = a(Color.rgb(255, 138, 31), k)
                stroke.strokeWidth = 3f * dp
                c.drawCircle(e.cx, e.cy, r, stroke)
                label(c, e.macro?.name?.take(5) ?: "M", e.cx, e.cy, r * 0.7f, a(if (pressed) Color.rgb(30, 30, 30) else Color.WHITE, k))
            }
        }
    }

    private fun drawDpad(c: Canvas, e: El, k: Float) {
        val s = e.w / 2
        val arm = s * 0.36f
        fun seg(bit: Int, l: Float, t: Float, r: Float, b: Float) {
            rect.set(e.cx + l, e.cy + t, e.cx + r, e.cy + b)
            fill.color = a(if (dpadBits and bit != 0) Color.argb(220, 255, 255, 255) else Color.argb(150, 20, 20, 26), k)
            c.drawRoundRect(rect, arm * 0.35f, arm * 0.35f, fill)
            stroke.color = a(Color.argb(200, 255, 255, 255), k)
            c.drawRoundRect(rect, arm * 0.35f, arm * 0.35f, stroke)
        }
        seg(Ps2.UP, -arm, -s, arm, -arm * 0.6f)
        seg(Ps2.DOWN, -arm, arm * 0.6f, arm, s)
        seg(Ps2.LEFT, -s, -arm, -arm * 0.6f, arm)
        seg(Ps2.RIGHT, arm * 0.6f, -arm, s, arm)
        // arrows
        fill.color = a(Color.WHITE, k)
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

    private fun psSymbol(c: Canvas, ctl: ClassicCtl, x: Float, y: Float, r: Float, color: Int) {
        stroke.color = color
        val old = stroke.strokeWidth
        stroke.strokeWidth = max(2.5f * dp, r * 0.22f)
        when (ctl) {
            ClassicCtl.CROSS -> {
                c.drawLine(x - r, y - r, x + r, y + r, stroke)
                c.drawLine(x - r, y + r, x + r, y - r, stroke)
            }
            ClassicCtl.CIRCLE -> c.drawCircle(x, y, r, stroke)
            ClassicCtl.SQUARE -> c.drawRect(x - r * 0.9f, y - r * 0.9f, x + r * 0.9f, y + r * 0.9f, stroke)
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
        val labels = listOf("−", "+", context.getString(if (selected?.visible == false) R.string.touch_show else R.string.touch_hide),
            context.getString(R.string.touch_reset), context.getString(R.string.touch_done))
        val h = 44f * dp
        val gap = 8f * dp
        text.textSize = 16f * dp
        val widths = labels.mapIndexed { i, s -> if (i < 2) h else text.measureText(s) + 28f * dp }
        var x = (width - (widths.sum() + gap * (labels.size - 1))) / 2
        val y = 14f * dp
        labels.forEachIndexed { i, s ->
            toolRects[i].set(x, y, x + widths[i], y + h)
            fill.color = if (i == 4) Color.rgb(255, 138, 31) else Color.argb(225, 28, 28, 36)
            if (i == 2 && selected == null) fill.color = Color.argb(120, 28, 28, 36)
            c.drawRoundRect(toolRects[i], h / 2, h / 2, fill)
            text.color = if (i == 4) Color.rgb(30, 20, 10) else Color.WHITE
            text.textSize = if (i < 2) 24f * dp else 16f * dp
            val fm = text.fontMetrics
            c.drawText(s, toolRects[i].centerX(), toolRects[i].centerY() - (fm.ascent + fm.descent) / 2, text)
            x += widths[i] + gap
        }
        // what the buttons act on
        text.textSize = 13f * dp
        text.color = Color.WHITE
        val what = selected?.let { nameOf(it) } ?: context.getString(R.string.touch_all_controls)
        c.drawText(what, width / 2f, y + h + 22f * dp, text)
    }

    private fun nameOf(e: El): String = e.macro?.name ?: when (e.ctl) {
        ClassicCtl.DPAD -> "D-pad"
        ClassicCtl.STICK_L -> "L stick"
        ClassicCtl.STICK_R -> "R stick"
        ClassicCtl.MENU -> context.getString(R.string.touch_ctl_menu)
        ClassicCtl.OPTIONS -> context.getString(R.string.touch_ctl_options)
        else -> e.ctl?.let { faceLabel(config.skin, it) } ?: ""
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        players.values.forEach { it.cancel() }
        handler.removeCallbacksAndMessages(null)
    }

    companion object {
        /** The label a control shows in each console's look. Positions stay the PS2's: south is cross. */
        fun faceLabel(skin: PadSkin, c: ClassicCtl): String = when (skin) {
            PadSkin.PLAYSTATION -> when (c) {
                ClassicCtl.CROSS -> "✕"; ClassicCtl.CIRCLE -> "○"; ClassicCtl.SQUARE -> "□"; ClassicCtl.TRIANGLE -> "△"
                ClassicCtl.L1 -> "L1"; ClassicCtl.L2 -> "L2"; ClassicCtl.R1 -> "R1"; ClassicCtl.R2 -> "R2"; ClassicCtl.L3 -> "L3"; ClassicCtl.R3 -> "R3"
                ClassicCtl.START -> "START"; ClassicCtl.SELECT -> "SELECT"
                else -> ""
            }
            PadSkin.XBOX -> when (c) {
                ClassicCtl.CROSS -> "A"; ClassicCtl.CIRCLE -> "B"; ClassicCtl.SQUARE -> "X"; ClassicCtl.TRIANGLE -> "Y"
                ClassicCtl.L1 -> "LB"; ClassicCtl.L2 -> "LT"; ClassicCtl.R1 -> "RB"; ClassicCtl.R2 -> "RT"; ClassicCtl.L3 -> "LS"; ClassicCtl.R3 -> "RS"
                ClassicCtl.START -> "MENU"; ClassicCtl.SELECT -> "VIEW"
                else -> ""
            }
            PadSkin.NINTENDO -> when (c) {
                ClassicCtl.CROSS -> "B"; ClassicCtl.CIRCLE -> "A"; ClassicCtl.SQUARE -> "Y"; ClassicCtl.TRIANGLE -> "X"
                ClassicCtl.L1 -> "L"; ClassicCtl.L2 -> "ZL"; ClassicCtl.R1 -> "R"; ClassicCtl.R2 -> "ZR"; ClassicCtl.L3 -> "LS"; ClassicCtl.R3 -> "RS"
                ClassicCtl.START -> "+"; ClassicCtl.SELECT -> "−"
                else -> ""
            }
        }

        fun faceColor(skin: PadSkin, c: ClassicCtl): Int = when (skin) {
            PadSkin.PLAYSTATION -> when (c) {
                ClassicCtl.CROSS -> Color.rgb(124, 170, 230)
                ClassicCtl.CIRCLE -> Color.rgb(240, 104, 110)
                ClassicCtl.SQUARE -> Color.rgb(226, 140, 196)
                ClassicCtl.TRIANGLE -> Color.rgb(64, 204, 170)
                else -> Color.WHITE
            }
            PadSkin.XBOX -> when (c) {
                ClassicCtl.CROSS -> Color.rgb(96, 186, 70)
                ClassicCtl.CIRCLE -> Color.rgb(226, 70, 64)
                ClassicCtl.SQUARE -> Color.rgb(56, 132, 222)
                ClassicCtl.TRIANGLE -> Color.rgb(244, 196, 48)
                else -> Color.argb(150, 20, 20, 26)
            }
            PadSkin.NINTENDO -> Color.argb(170, 46, 46, 52)
        }
    }
}
