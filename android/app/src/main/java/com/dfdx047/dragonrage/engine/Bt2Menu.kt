package com.dfdx047.dragonrage.engine

import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.Switch
import android.widget.TextView
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.Bt2Renderer
import com.dfdx047.dragonrage.data.Renderer
import com.dfdx047.dragonrage.data.Bt2Settings
import com.dfdx047.dragonrage.data.Bt2Unlock
import com.dfdx047.dragonrage.data.GamePaths

/**
 * Budokai Tenkaichi 2's in-game menu, laid out as BT3's (its engine draws that one: the pages on the left, the
 * page's settings on the right, "leave" and "continue" at the top). BT2's engine has no menu of its own on Android,
 * so this one is the app's, over the game, which stands still while it is open (Bt2Bridge.setPaused).
 *
 * What it changes is BT2's alone (Bt2Settings: <root>/bt2/user). The screen's resolution, the renderer's speed
 * options, the volumes and the scaling apply at once; quality, widescreen, 60 fps and the unlock card from the
 * next start, as their notes say.
 */
class Bt2Menu(
    private val activity: Bt2GameActivity,
    private val paths: GamePaths,
    private val onControls: () -> Unit,
    private val onClose: () -> Unit,
) {
    private val store = Bt2Settings(paths)
    private var v = store.load()
    private val rstore = Bt2Renderer(paths)
    private val gpu = runCatching { Renderer.gpuName(paths) }.getOrDefault("")
    private var r = rstore.load()
    private val dp = activity.resources.displayMetrics.density
    private val root = FrameLayout(activity)
    private val content = LinearLayout(activity).apply { orientation = LinearLayout.VERTICAL }
    private val pageButtons = mutableListOf<TextView>()
    private var page = 0

    private enum class Page(val label: Int) { VIDEO(R.string.video), PERFORMANCE(R.string.performance), EFFECTS(R.string.bt2_page_effects), AUDIO(R.string.audio), CONTROLS(R.string.nav_controls), CHEATS(R.string.nav_cheats) }

    val isShown get() = root.parent != null

    fun show(into: ViewGroup) {
        if (isShown) return
        v = store.load()
        r = rstore.load()
        root.removeAllViews()
        root.setBackgroundColor(Color.argb(240, 12, 16, 30))
        root.isClickable = true // nothing under it is touched while it is open
        val column = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(px(28), px(18), px(28), px(12))
        }
        column.addView(topBar())
        val middle = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL }
        val nav = LinearLayout(activity).apply { orientation = LinearLayout.VERTICAL }
        pageButtons.clear()
        Page.entries.forEachIndexed { i, p ->
            val b = text(activity.getString(p.label), 16f, TEXT).apply {
                setPadding(px(20), px(12), px(20), px(12))
                setOnClickListener { select(i) }
            }
            pageButtons += b
            nav.addView(b, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { bottomMargin = px(8) })
        }
        middle.addView(ScrollView(activity).apply { addView(nav) }, LinearLayout.LayoutParams(px(200), ViewGroup.LayoutParams.MATCH_PARENT).apply { marginEnd = px(16) })
        (content.parent as? ViewGroup)?.removeView(content) // still in the last opening's panel
        val panel = ScrollView(activity).apply {
            background = rounded(PANEL, 14f)
            setPadding(px(22), px(14), px(22), px(14))
            addView(content)
        }
        middle.addView(panel, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f))
        column.addView(middle, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        column.addView(text(activity.getString(R.string.bt2_menu_hint), 13f, SUBTLE).apply { setPadding(0, px(8), 0, 0) })
        root.addView(column, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        into.addView(root, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        select(page)
        Bt2Bridge.setPaused(true)
    }

    fun close() {
        if (!isShown) return
        (root.parent as? ViewGroup)?.removeView(root)
        Bt2Bridge.setPaused(false)
        onClose()
    }

    private fun topBar(): View = LinearLayout(activity).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        setPadding(0, 0, 0, px(14))
        addView(text("DRAGON RAGE", 26f, GOLD))
        addView(text(activity.getString(R.string.bt2_menu_paused), 16f, SUBTLE).apply { setPadding(px(18), px(6), 0, 0) })
        addView(View(activity), LinearLayout.LayoutParams(0, 1, 1f))
        addView(pill(activity.getString(R.string.bt2_menu_leave), selected = false) { activity.quitToApp() })
        addView(View(activity), LinearLayout.LayoutParams(px(12), 1))
        addView(pill(activity.getString(R.string.bt2_menu_continue), selected = true) { close() })
    }

    private fun select(i: Int) {
        page = i
        pageButtons.forEachIndexed { n, b ->
            b.background = rounded(if (n == i) NAV_SELECTED else NAV, 12f)
            b.setTextColor(if (n == i) GOLD else TEXT)
        }
        content.removeAllViews()
        when (Page.entries[i]) {
            Page.VIDEO -> video()
            Page.PERFORMANCE -> performance()
            Page.EFFECTS -> effects()
            Page.AUDIO -> audio()
            Page.CONTROLS -> controls()
            Page.CHEATS -> cheats()
        }
    }

    // ---- pages

    private fun video() {
        section(activity.getString(R.string.bt2_set_picture))
        chips(activity.getString(R.string.bt2_renderer), listOf(activity.getString(R.string.bt2_renderer_fast), activity.getString(R.string.bt2_renderer_exact)),
            if (r.bt3) 0 else 1, later(activity.getString(R.string.bt2_renderer_text))) { n -> changeR(r.copy(bt3 = n == 0)) }
        if (r.bt3) {
            // OpenGL ES: an option for Mali GPUs only (Vulkan stays the default everywhere: BT3 measured a Mali-G52's
            // fights at 2 to 3 pictures a second in OpenGL ES against about 40 in Vulkan; OpenGL ES is for a driver
            // whose Vulkan fails). From the next start: the window is made for it (bt2/runtime/android/android_main.cpp).
            if (Renderer.isMali(gpu)) {
                chips(activity.getString(R.string.bt2_gpu_api), listOf("Vulkan", "OpenGL ES"), if (r.openGl) 1 else 0,
                    later(activity.getString(R.string.bt2_gpu_api_text))) { n -> changeR(r.copy(openGl = n == 1)) }
            }
            chips(activity.getString(R.string.resolution), Bt2Renderer.SCALES.map { Bt2Renderer.scaleLabel(it) }, Bt2Renderer.SCALES.indexOf(r.scale4),
                activity.getString(R.string.resolution_hint)) { n -> changeR(r.copy(scale4 = Bt2Renderer.SCALES[n])) }
            chips(activity.getString(R.string.screen_filter), listOf(activity.getString(R.string.filter_bilinear), activity.getString(R.string.filter_sharp), "FXAA", "AMD FSR 1", "Snapdragon GSR"),
                r.filter, activity.getString(R.string.screen_filter_hint)) { n -> changeR(r.copy(filter = n)) }
            toggle(activity.getString(R.string.bt2_patch_widescreen), later(activity.getString(R.string.bt2_patch_widescreen_text)), v.widescreen) { change(v.copy(widescreen = it)) }
            toggle(activity.getString(R.string.bt2_set_fps), activity.getString(R.string.bt2_set_fps_text), v.showFrameRate) { change(v.copy(showFrameRate = it)); Bt2Bridge.reloadSettings(); activity.refreshPerfOverlay() }
            return
        }
        chips(activity.getString(R.string.bt2_set_output), Bt2Settings.OUTPUT_SCALES.map { "$it%" }, Bt2Settings.OUTPUT_SCALES.indexOf(v.outputScale),
            activity.getString(R.string.bt2_set_output_text)) { n ->
            change(v.copy(outputScale = Bt2Settings.OUTPUT_SCALES[n]))
            activity.applyOutputScale(v.outputScale)
        }
        val steps = listOf(1 to R.string.bt2_ss_1, 2 to R.string.bt2_ss_2, 4 to R.string.bt2_ss_4, 8 to R.string.bt2_ss_8, 16 to R.string.bt2_ss_16)
        chips(activity.getString(R.string.bt2_set_ss), steps.map { activity.getString(it.second) }, steps.indexOfFirst { it.first == v.superSampling },
            later(activity.getString(R.string.bt2_set_ss_text))) { n -> change(v.copy(superSampling = steps[n].first)) }
        toggle(activity.getString(R.string.bt2_patch_widescreen), later(activity.getString(R.string.bt2_patch_widescreen_text)), v.widescreen) { change(v.copy(widescreen = it)) }
        toggle(activity.getString(R.string.bt2_set_integer), activity.getString(R.string.bt2_set_integer_text), v.integerScaling) { change(v.copy(integerScaling = it)); Bt2Bridge.reloadSettings() }
        toggle(activity.getString(R.string.bt2_set_fps), activity.getString(R.string.bt2_set_fps_text), v.showFrameRate) { change(v.copy(showFrameRate = it)); Bt2Bridge.reloadSettings(); activity.refreshPerfOverlay() }
    }

    private fun performance() {
        section(activity.getString(R.string.bt2_set_speed))
        toggle(activity.getString(R.string.bt2_set_fps), activity.getString(R.string.bt2_set_fps_text), v.showFrameRate) { change(v.copy(showFrameRate = it)); Bt2Bridge.reloadSettings(); activity.refreshPerfOverlay() }
        if (r.bt3) {
            toggle(activity.getString(R.string.bt2_render_thread), later(activity.getString(R.string.bt2_render_thread_text)), r.renderThread) {
                changeR(r.copy(renderThread = it))
            }
            toggle(activity.getString(R.string.frame_skip), activity.getString(R.string.frame_skip_text), r.frameSkip) { changeR(r.copy(frameSkip = it)) }
            toggle(activity.getString(R.string.frag_depth), activity.getString(R.string.frag_depth_text), r.preciseDepth) { changeR(r.copy(preciseDepth = it)) }
            chips(activity.getString(R.string.fps60), listOf(activity.getString(R.string.off), "60", "120"), r.smoothMotion.coerceIn(0, 2),
                activity.getString(R.string.fps60_text)) { n -> changeR(r.copy(smoothMotion = n)) }
            chips(activity.getString(R.string.bt2_set_present), listOf(activity.getString(R.string.bt2_present_auto), "FIFO (VSync)", "Mailbox", "Immediate"),
                r.present, activity.getString(R.string.bt2_set_present_text)) { n -> changeR(r.copy(present = n)) }
            return
        }
        toggle(activity.getString(R.string.bt2_set_frameskip), activity.getString(R.string.bt2_set_frameskip_text), v.frameSkip) {
            change(v.copy(frameSkip = it)); Bt2Bridge.setGsHacks(v.hacks())
        }
        chips(activity.getString(R.string.bt2_set_present), listOf(activity.getString(R.string.bt2_present_auto), "FIFO (VSync)", "Mailbox", "Immediate"),
            v.presentMode, activity.getString(R.string.bt2_set_present_text)) { n -> change(v.copy(presentMode = n)); Bt2Bridge.setPresentMode(n) }
        fun hack(title: Int, text: Int, on: Boolean, copy: (Boolean) -> Bt2Settings.Values) =
            toggle(activity.getString(title), activity.getString(text), on) { change(copy(it)); Bt2Bridge.setGsHacks(v.hacks()) }
        hack(R.string.bt2_set_light, R.string.bt2_set_light_text, v.blendDemote) { v.copy(blendDemote = it) }
        hack(R.string.bt2_set_unsynced, R.string.bt2_set_unsynced_text, v.unsyncedReadbacks) { v.copy(unsyncedReadbacks = it) }
        hack(R.string.bt2_set_promote, R.string.bt2_set_promote_text, v.backbufferPromotion) { v.copy(backbufferPromotion = it) }
        toggle(activity.getString(R.string.bt2_patch_60), later(activity.getString(R.string.bt2_patch_60_text)), v.sixtyFps) { change(v.copy(sixtyFps = it)) }
    }

    private fun effects() {
        section(activity.getString(R.string.bt2_fx_title))
        if (r.bt3) {
            listOf(
                Triple(1, R.string.fx_outline, R.string.fx_outline_text), Triple(2, R.string.fx_see, R.string.fx_see_text),
                Triple(4, R.string.fx_depth, R.string.fx_depth_text), Triple(8, R.string.fx_glow, R.string.fx_glow_text),
                Triple(16, R.string.fx_blur, R.string.fx_blur_text),
            ).forEach { (bit, title, text) ->
                toggle(activity.getString(title), activity.getString(text), r.fxOff and bit == 0) { on ->
                    changeR(r.copy(fxOff = if (on) r.fxOff and bit.inv() else r.fxOff or bit))
                }
            }
            volume(activity.getString(R.string.glow_strength), r.glow.coerceIn(0, 100)) { changeR(r.copy(glow = it)) }
            return
        }
        toggle(activity.getString(R.string.bt2_fx_screen), activity.getString(R.string.bt2_fx_screen_text), v.screenEffects) {
            change(v.copy(screenEffects = it)); Bt2Bridge.setGsHacks(v.hacks())
        }
        toggle(activity.getString(R.string.bt2_fx_fog), activity.getString(R.string.bt2_fx_fog_text), v.fog) {
            change(v.copy(fog = it)); Bt2Bridge.setGsHacks(v.hacks())
        }
        toggle(activity.getString(R.string.bt2_fx_mipmaps), activity.getString(R.string.bt2_set_nomip_text), !v.noMipmaps) {
            change(v.copy(noMipmaps = !it)); Bt2Bridge.setGsHacks(v.hacks())
        }
        content.addView(text(activity.getString(R.string.bt2_fx_note), 14f, SUBTLE))
    }

    private fun audio() {
        section(activity.getString(R.string.bt2_set_sound))
        volume(activity.getString(R.string.bt2_set_master), v.masterVolume) { change(v.copy(masterVolume = it)) }
        volume(activity.getString(R.string.bt2_set_music), v.musicVolume) { change(v.copy(musicVolume = it)) }
        volume(activity.getString(R.string.bt2_set_effects), v.effectsVolume) { change(v.copy(effectsVolume = it)) }
    }

    private fun controls() {
        section(activity.getString(R.string.nav_controls))
        row(activity.getString(R.string.bt2_menu_touch), activity.getString(R.string.bt2_menu_touch_text),
            pill(activity.getString(R.string.touch_controls), selected = true) { onControls() })
    }

    private fun cheats() {
        section(activity.getString(R.string.bt2_unlock_title))
        if (!Bt2Unlock.available(activity)) {
            content.addView(text(activity.getString(R.string.bt2_unlock_missing), 14f, SUBTLE))
            return
        }
        toggle(activity.getString(R.string.bt2_unlock_title), later(activity.getString(R.string.bt2_unlock_text)), Bt2Unlock.isOn(paths)) { on ->
            Bt2Unlock.set(activity, paths, on)
            v = store.load() // the card is in the settings file too
        }
    }

    // ---- pieces, in BT3's menu's look

    /** BT3's renderer's settings: saved, and given to it between two frames (bt2_host.c: Bt2Hw_ApplySettingsIfAsked). */
    private fun changeR(new: Bt2Renderer.Values) {
        r = new
        rstore.save(r)
        Bt2Bridge.reloadSettings()
    }

    private fun change(new: Bt2Settings.Values) {
        v = new
        store.save(v)
    }

    private fun later(text: String) = text + " " + activity.getString(R.string.bt2_menu_next)

    private fun section(title: String) {
        content.addView(text(title, 18f, GOLD))
        content.addView(View(activity).apply { background = rounded(ACCENT, 2f) }, LinearLayout.LayoutParams(px(64), px(3)).apply { topMargin = px(6); bottomMargin = px(14) })
    }

    private fun row(title: String, note: String?, control: View) {
        val line = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        val words = LinearLayout(activity).apply { orientation = LinearLayout.VERTICAL }
        words.addView(text(title, 16f, TEXT))
        if (note != null) words.addView(text(note, 14f, SUBTLE).apply { setPadding(0, px(4), 0, 0) })
        line.addView(words, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        line.addView(control, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { marginStart = px(16) })
        content.addView(line, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { bottomMargin = px(18) })
    }

    private fun toggle(title: String, note: String?, on: Boolean, change: (Boolean) -> Unit) {
        val states = arrayOf(intArrayOf(android.R.attr.state_checked), intArrayOf())
        val switch = Switch(activity).apply {
            isChecked = on
            thumbTintList = ColorStateList(states, intArrayOf(Color.rgb(255, 244, 228), Color.rgb(170, 176, 200)))
            trackTintList = ColorStateList(states, intArrayOf(ACCENT, Color.rgb(52, 58, 84)))
            setOnCheckedChangeListener { _, checked -> change(checked) }
        }
        row(title, note, switch)
    }

    private fun chips(title: String, labels: List<String>, selected: Int, note: String?, choose: (Int) -> Unit) {
        content.addView(text(title, 16f, TEXT))
        val line = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL }
        val pills = mutableListOf<TextView>()
        labels.forEachIndexed { n, label ->
            val p = pill(label, n == selected) {
                pills.forEachIndexed { m, other -> style(other, m == n) }
                choose(n)
            }
            pills += p
            line.addView(p, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { marginEnd = px(10) })
        }
        content.addView(android.widget.HorizontalScrollView(activity).apply { addView(line) },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = px(8) })
        if (note != null) content.addView(text(note, 14f, SUBTLE).apply { setPadding(0, px(8), 0, 0) })
        content.addView(View(activity), LinearLayout.LayoutParams(1, px(18)))
    }

    private fun volume(title: String, value: Int, change: (Int) -> Unit) {
        val label = text("$title: $value%", 16f, TEXT)
        content.addView(label)
        content.addView(SeekBar(activity).apply {
            max = 20
            progress = value / 5
            progressTintList = ColorStateList.valueOf(ACCENT)
            thumbTintList = ColorStateList.valueOf(ACCENT)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                    label.text = "$title: ${p * 5}%"
                }

                override fun onStartTrackingTouch(sb: SeekBar) {}
                override fun onStopTrackingTouch(sb: SeekBar) {
                    change(sb.progress * 5)
                    Bt2Bridge.reloadSettings()
                }
            })
        }, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { bottomMargin = px(14) })
    }

    private fun pill(label: String, selected: Boolean, click: () -> Unit) = text(label, 16f, TEXT).apply {
        setPadding(px(22), px(10), px(22), px(10))
        style(this, selected)
        setOnClickListener { click() }
    }

    private fun style(view: TextView, selected: Boolean) {
        view.background = rounded(if (selected) ACCENT else NAV, 22f)
        view.setTextColor(if (selected) Color.rgb(40, 22, 6) else TEXT)
    }

    private fun text(value: String, sp: Float, color: Int) = TextView(activity).apply {
        text = value
        setTextColor(color)
        setTextSize(TypedValue.COMPLEX_UNIT_SP, sp)
        if (sp >= 18f) typeface = Typeface.DEFAULT_BOLD
    }

    private fun rounded(color: Int, radius: Float) = GradientDrawable().apply {
        setColor(color)
        cornerRadius = radius * dp
    }

    private fun px(value: Int) = (value * dp).toInt()

    private companion object {
        val GOLD = Color.rgb(255, 196, 77)
        val ACCENT = Color.rgb(255, 140, 26)
        val TEXT = Color.rgb(237, 239, 247)
        val SUBTLE = Color.rgb(158, 166, 192)
        val PANEL = Color.rgb(28, 34, 56)
        val NAV = Color.rgb(30, 36, 58)
        val NAV_SELECTED = Color.rgb(62, 69, 102)
    }
}
