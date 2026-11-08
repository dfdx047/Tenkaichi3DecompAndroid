package com.dfdx047.dragonrage.touch

import java.io.File
import org.json.JSONArray
import org.json.JSONObject

/** The look of the on-screen controller: which console's buttons it shows. The PS2 buttons behind them are the same. */
enum class PadSkin { PLAYSTATION, XBOX, NINTENDO }

/**
 * How the controls are drawn: DRAGON (0.6.0 and on, the default) dark round buttons with gold icons and coloured
 * letter badges, a Dragon Ball for the stick; GLASS the translucent look of 0.3.0 to 0.5.0.
 */
enum class PadStyle { DRAGON, GLASS }

/** Which layout of the on-screen controller: the game-specific one (0.3.0 and on) or the classic generic pad. */
enum class TouchLayout { MOBILE, CLASSIC }

/** When the on-screen controller shows: never, until a physical controller is used (and again on a touch), always. */
enum class TouchMode { OFF, AUTO, ALWAYS }

/**
 * How the left stick sits under the thumb (1.0): FIXED where it was placed; FLOATING its centre where the thumb lands
 * (anywhere in the stick's zone); DYNAMIC as floating, and the centre follows the thumb when it goes past the edge, so
 * turning back is immediate and the stick is never "lost" (the mobile ports' default).
 */
enum class StickMode { FIXED, FLOATING, DYNAMIC }

/** The vibration on a press: none, a light tick, a firm one. */
enum class Haptic { OFF, LIGHT, STRONG }

/** The PS2 pad's buttons, as the engine's pad buffer has them (port/src/gs/gs_input.c, include/sys/pad.h). */
object Ps2 {
    const val SELECT = 0x0001
    const val L3 = 0x0002
    const val R3 = 0x0004
    const val START = 0x0008
    const val UP = 0x0010
    const val RIGHT = 0x0020
    const val DOWN = 0x0040
    const val LEFT = 0x0080
    const val L2 = 0x0100
    const val R2 = 0x0200
    const val L1 = 0x0400
    const val R1 = 0x0800
    const val TRIANGLE = 0x1000
    const val CIRCLE = 0x2000
    const val CROSS = 0x4000
    const val SQUARE = 0x8000

    /** Buttons a macro step can press, in the order the editor lists them. */
    val MACRO_BUTTONS = listOf(CROSS, CIRCLE, SQUARE, TRIANGLE, L1, R1, L2, R2, UP, DOWN, LEFT, RIGHT, L3, R3, START, SELECT)
}

/** What a control on the screen is. */
enum class CtlKind { DPAD, STICK_L, STICK_R, BUTTON, PILL, SHOULDER, SYSTEM, MACRO }

/**
 * One fixed control of the on-screen controller. [size] is its diameter (or width) in dp at scale 1; [x], [y] its
 * default centre as a fraction of the screen. Face buttons are placed around a centre so the diamond keeps its shape
 * on any screen: [dx], [dy] in dp from ([x], [y]).
 */
enum class Ctl(
    val kind: CtlKind,
    val bit: Int,
    val size: Float,
    val x: Float,
    val y: Float,
    val dx: Float = 0f,
    val dy: Float = 0f,
    val shownByDefault: Boolean = true,
) {
    // Battle mode uses a virtual stick. Menu mode swaps to this D-pad because BT3's menus read digital directions.
    DPAD(CtlKind.DPAD, 0, 128f, 0.10f, 0.43f),
    STICK_L(CtlKind.STICK_L, 0, 148f, 0.145f, 0.775f),
    STICK_R(CtlKind.STICK_R, 0, 100f, 0.70f, 0.78f, shownByDefault = false),

    // Combat controls form a mobile thumb cluster rather than the classic PS2 diamond.
    // The tiny controller symbol is only a reference; the illustrated action is the primary affordance.
    SQUARE(CtlKind.BUTTON, Ps2.SQUARE, 82f, 0.845f, 0.805f),       // melee attack
    CIRCLE(CtlKind.BUTTON, Ps2.CIRCLE, 66f, 0.745f, 0.735f),      // guard
    TRIANGLE(CtlKind.BUTTON, Ps2.TRIANGLE, 66f, 0.855f, 0.605f),  // Ki blast
    CROSS(CtlKind.BUTTON, Ps2.CROSS, 60f, 0.945f, 0.735f),        // dash

    // One-touch versions of common simultaneous-input attacks. They still send the exact native PS2 bits.
    BLAST1(CtlKind.BUTTON, Ps2.L2 or Ps2.CIRCLE, 52f, 0.745f, 0.555f),
    BLAST2(CtlKind.BUTTON, Ps2.L2 or Ps2.TRIANGLE, 52f, 0.945f, 0.555f),
    ULTIMATE(CtlKind.BUTTON, Ps2.L2 or Ps2.DOWN or Ps2.TRIANGLE, 52f, 0.955f, 0.905f),

    // Utility actions stay near their screen edge and outside the primary thumb cluster.
    L1(CtlKind.SHOULDER, Ps2.L1, 78f, 0.075f, 0.155f),          // lock-on
    L2(CtlKind.SHOULDER, Ps2.L2, 78f, 0.075f, 0.275f),          // Ki charge (hold)
    R1(CtlKind.SHOULDER, Ps2.R1, 78f, 0.875f, 0.155f),          // jump / ascend
    R2(CtlKind.SHOULDER, Ps2.R2, 78f, 0.875f, 0.275f),          // descend
    L3(CtlKind.BUTTON, Ps2.L3, 50f, 0.31f, 0.58f),
    R3(CtlKind.BUTTON, Ps2.R3, 48f, 0.965f, 0.415f),             // transform / free look
    // Battle-only roster actions. The native game decides the actual team size at runtime.
    TEAM_SWAP(CtlKind.SYSTEM, 0, 54f, 0.22f, 0.325f),            // sends L3 + R3 together
    TEAM_SELECT(CtlKind.SYSTEM, 0, 48f, 0.22f, 0.435f),          // one right-stick-down input advances the roster target
    SELECT(CtlKind.PILL, Ps2.SELECT, 50f, 0.355f, 0.065f), // (shown: SELECT opens the Tag Team modes)
    START(CtlKind.PILL, Ps2.START, 50f, 0.645f, 0.065f),
    MENU(CtlKind.SYSTEM, 0, 40f, 0.465f, 0.065f),                // port pause / settings window
    OPTIONS(CtlKind.SYSTEM, 0, 40f, 0.535f, 0.065f),             // controller quick menu
    NAV_MODE(CtlKind.SYSTEM, 0, 42f, 0.50f, 0.155f),              // manually swap combat controls <-> menu D-pad
}

/** The ready-made tunings (ControlsScreen, "Perfis"): Standard, Online competitive, Relaxed, Classic (0.9's feel). */
enum class TouchPreset { STANDARD, COMPETITIVE, RELAXED, CLASSIC }

/** Buttons that can be set to toggle: the ones held in a fight. */
val TOGGLE_CANDIDATES = listOf(Ctl.L2, Ctl.CIRCLE, Ctl.R1, Ctl.R2, Ctl.L1)

/**
 * A saved profile: a whole configuration (layout, look and tuning) under a name, kept as its JSON. Loading one keeps
 * the profiles list itself (a profile does not carry the others).
 */
data class TouchProfile(val name: String, val config: JSONObject) {
    fun apply(current: TouchConfig): TouchConfig = TouchConfig.fromJson(config).copy(profiles = current.profiles, mode = current.mode)

    companion object {
        fun of(name: String, c: TouchConfig) = TouchProfile(name.take(24), c.copy(profiles = emptyList()).toJson())
    }
}

/** Where a control is: centre as a fraction of the screen, its scale, shown or not. */
data class Placement(val x: Float, val y: Float, val scale: Float = 1f, val visible: Boolean = true, val opacity: Int = 100)

/** One step of a macro: these buttons held for [ms] milliseconds (0 buttons: a pause). */
data class MacroStep(val buttons: Int, val ms: Int)

/**
 * A button of the user's that plays a sequence of presses: a combo, or several buttons at once. [repeat]: it
 * plays again for as long as the button is held (turbo).
 */
data class Macro(
    val id: String,
    val name: String,
    val steps: List<MacroStep>,
    val repeat: Boolean = false,
    val place: Placement = Placement(0.74f, 0.40f, 1f, true),
)

data class TouchConfig(
    val mode: TouchMode = TouchMode.AUTO,
    val skin: PadSkin = PadSkin.XBOX,
    val style: PadStyle = PadStyle.DRAGON,
    val opacity: Int = 70,          // %
    val scale: Float = 1f,          // all controls
    val haptics: Boolean = true,
    val sticksFloat: Boolean = false, // the left stick starts where the thumb lands (within its half of the screen)
    val places: Map<Ctl, Placement> = emptyMap(), // only those moved; the rest at their defaults
    val layout: TouchLayout = TouchLayout.MOBILE,
    val autoMode: Boolean = true,   // menu buttons outside a fight, fight buttons in one (the MENU/FIGHT button still overrides)
    val classicPlaces: Map<String, Placement> = emptyMap(), // the classic layout's own moved controls (by ClassicCtl name)
    val macros: List<Macro> = emptyList(),
    // ---- 1.0: the touch tuning ("Toque Pro")
    val stickMode: StickMode = StickMode.DYNAMIC,
    val deadZone: Int = 12,         // % of the stick's radius where nothing is sent (a resting thumb drifts)
    val stickRange: Int = 75,       // % of the radius that gives full tilt: less travel for a full run
    val stickZone: Int = 45,        // % of the screen's width, from the left, where a floating / dynamic stick can start
    val snap8: Boolean = false,     // the stick sends one of 8 directions at full tilt (menus, dashes)
    val hitArea: Int = 115,         // % of each button's drawn size that a touch counts on (100: the drawing only)
    val slideAll: Boolean = true,   // a finger sliding from a button onto another presses it: shoulders and Start too
    val toggles: Set<Ctl> = emptySet(), // buttons where a tap holds and the next tap lets go (Ki charge, guard...)
    val haptic: Haptic = Haptic.LIGHT,
    val leftHanded: Boolean = false, // mirrored(): the stick's zone is the right side
    val profiles: List<TouchProfile> = emptyList(), // the player's saved tunings and layouts, by name
) {
    fun placeOf(c: Ctl): Placement? = places[c]

    /** The tuning of a ready-made profile on top of this layout (the places, macros and look stay). */
    fun withPreset(p: TouchPreset): TouchConfig = when (p) {
        TouchPreset.STANDARD -> copy(stickMode = StickMode.DYNAMIC, deadZone = 12, stickRange = 75, stickZone = 45, snap8 = false,
            hitArea = 115, slideAll = true, toggles = emptySet(), haptic = Haptic.LIGHT)
        TouchPreset.COMPETITIVE -> copy(stickMode = StickMode.DYNAMIC, deadZone = 8, stickRange = 60, stickZone = 50, snap8 = false,
            hitArea = 130, slideAll = true, toggles = setOf(Ctl.L2), haptic = Haptic.LIGHT, opacity = minOf(opacity, 55))
        TouchPreset.RELAXED -> copy(stickMode = StickMode.FLOATING, deadZone = 15, stickRange = 85, stickZone = 50, snap8 = true,
            hitArea = 135, slideAll = true, toggles = setOf(Ctl.L2, Ctl.CIRCLE), haptic = Haptic.STRONG)
        TouchPreset.CLASSIC -> copy(stickMode = StickMode.FIXED, deadZone = 10, stickRange = 100, stickZone = 45, snap8 = false,
            hitArea = 112, slideAll = false, toggles = emptySet(), haptic = Haptic.LIGHT)
    }

    /** The same layout mirrored: the stick on the right, the buttons on the left (left-handed players). */
    fun mirrored(): TouchConfig = copy(
        places = Ctl.entries.associateWith { c -> (places[c] ?: Placement(c.x, c.y)).let { it.copy(x = 1f - it.x) } },
        macros = macros.map { m -> m.copy(place = m.place.copy(x = 1f - m.place.x)) },
        leftHanded = !leftHanded,
    )

    fun toJson(): JSONObject = JSONObject().apply {
        put("version", 2)
        put("mode", mode.name)
        put("skin", skin.name)
        put("style", style.name)
        put("opacity", opacity)
        put("scale", scale.toDouble())
        put("haptics", haptics)
        put("sticksFloat", sticksFloat)
        put("places", JSONObject().apply { places.forEach { (c, p) -> put(c.name, p.json()) } })
        put("layout", layout.name)
        put("autoMode", autoMode)
        put("classicPlaces", JSONObject().apply { classicPlaces.forEach { (n, p) -> put(n, p.json()) } })
        put("stickMode", stickMode.name)
        put("deadZone", deadZone)
        put("stickRange", stickRange)
        put("stickZone", stickZone)
        put("snap8", snap8)
        put("hitArea", hitArea)
        put("slideAll", slideAll)
        put("toggles", JSONArray().apply { toggles.forEach { put(it.name) } })
        put("haptic", haptic.name)
        put("leftHanded", leftHanded)
        put("profiles", JSONArray().apply { profiles.forEach { put(JSONObject().put("name", it.name).put("config", it.config)) } })
        put("macros", JSONArray().apply {
            macros.forEach { m ->
                put(JSONObject().apply {
                    put("id", m.id)
                    put("name", m.name)
                    put("repeat", m.repeat)
                    put("place", m.place.json())
                    put("steps", JSONArray().apply { m.steps.forEach { put(JSONArray().put(it.buttons).put(it.ms)) } })
                })
            }
        })
    }

    companion object {
        fun fromJson(o: JSONObject): TouchConfig {
            val places = mutableMapOf<Ctl, Placement>()
            // Version 2 introduces the BT3-specific mobile layout. Old v1 absolute placements describe the
            // generic controller and would override the new ergonomic defaults, so migrate those once.
            if (o.optInt("version", 1) >= 2) {
                o.optJSONObject("places")?.let { p ->
                    p.keys().forEach { k ->
                        val c = Ctl.entries.firstOrNull { it.name == k } ?: return@forEach
                        p.optJSONObject(k)?.let { places[c] = placement(it) }
                    }
                }
            }
            val classic = mutableMapOf<String, Placement>()
            o.optJSONObject("classicPlaces")?.let { p ->
                p.keys().forEach { k -> p.optJSONObject(k)?.let { classic[k] = placement(it) } }
            }
            val macros = mutableListOf<Macro>()
            o.optJSONArray("macros")?.let { a ->
                for (i in 0 until a.length()) {
                    val m = a.optJSONObject(i) ?: continue
                    val steps = mutableListOf<MacroStep>()
                    m.optJSONArray("steps")?.let { s ->
                        for (j in 0 until s.length()) {
                            val st = s.optJSONArray(j) ?: continue
                            steps += MacroStep(st.optInt(0), st.optInt(1, 100).coerceIn(16, 5000))
                        }
                    }
                    macros += Macro(
                        id = m.optString("id").ifEmpty { "m$i" },
                        name = m.optString("name").ifEmpty { "M${i + 1}" },
                        steps = steps,
                        repeat = m.optBoolean("repeat"),
                        place = m.optJSONObject("place")?.let { placement(it) } ?: Placement(0.74f, 0.40f),
                    )
                }
            }
            return TouchConfig(
                mode = runCatching { TouchMode.valueOf(o.optString("mode")) }.getOrDefault(TouchMode.AUTO),
                skin = runCatching { PadSkin.valueOf(o.optString("skin")) }.getOrDefault(PadSkin.PLAYSTATION),
                style = runCatching { PadStyle.valueOf(o.optString("style")) }.getOrDefault(PadStyle.DRAGON),
                opacity = o.optInt("opacity", 70).coerceIn(10, 100),
                scale = o.optDouble("scale", 1.0).toFloat().coerceIn(0.5f, 2f),
                haptics = o.optBoolean("haptics", true),
                sticksFloat = o.optBoolean("sticksFloat", false),
                places = places,
                layout = runCatching { TouchLayout.valueOf(o.optString("layout")) }.getOrDefault(TouchLayout.MOBILE),
                autoMode = o.optBoolean("autoMode", true),
                classicPlaces = classic,
                macros = macros,
                // (no "stickMode" yet: a file from before 1.0, whose stick was fixed or floating by sticksFloat)
                stickMode = o.optString("stickMode").let { m -> StickMode.entries.firstOrNull { it.name == m } }
                    ?: if (o.optBoolean("sticksFloat", false)) StickMode.FLOATING else StickMode.DYNAMIC,
                deadZone = o.optInt("deadZone", 12).coerceIn(0, 40),
                stickRange = o.optInt("stickRange", 75).coerceIn(40, 100),
                stickZone = o.optInt("stickZone", 45).coerceIn(25, 60),
                snap8 = o.optBoolean("snap8", false),
                hitArea = o.optInt("hitArea", 115).coerceIn(90, 160),
                slideAll = o.optBoolean("slideAll", true),
                toggles = buildSet {
                    o.optJSONArray("toggles")?.let { a -> for (i in 0 until a.length()) Ctl.entries.firstOrNull { it.name == a.optString(i) }?.let { add(it) } }
                },
                haptic = o.optString("haptic").let { h -> Haptic.entries.firstOrNull { it.name == h } }
                    ?: if (o.optBoolean("haptics", true)) Haptic.LIGHT else Haptic.OFF,
                leftHanded = o.optBoolean("leftHanded", false),
                profiles = buildList {
                    o.optJSONArray("profiles")?.let { a ->
                        for (i in 0 until a.length()) {
                            val p = a.optJSONObject(i) ?: continue
                            val c = p.optJSONObject("config") ?: continue
                            add(TouchProfile(p.optString("name").ifEmpty { "${i + 1}" }.take(24), c))
                        }
                    }
                },
            )
        }

        private fun Placement.json() = JSONObject().put("x", x.toDouble()).put("y", y.toDouble()).put("s", scale.toDouble()).put("v", visible).put("o", opacity)

        private fun placement(o: JSONObject) = Placement(
            o.optDouble("x", 0.5).toFloat().coerceIn(0f, 1f),
            o.optDouble("y", 0.5).toFloat().coerceIn(0f, 1f),
            o.optDouble("s", 1.0).toFloat().coerceIn(0.4f, 2.5f),
            o.optBoolean("v", true),
            o.optInt("o", 100).coerceIn(10, 100),
        )
    }
}

/**
 * touch_controls.json in the game's folder: read by the launcher (to edit it) and by the game's process (to show
 * the controller), which run in different processes, so it is always read from the file.
 */
class TouchStore(root: File) {
    val file = File(root, "touch_controls.json")

    fun load(): TouchConfig = runCatching {
        if (file.exists()) TouchConfig.fromJson(JSONObject(file.readText())) else TouchConfig()
    }.getOrDefault(TouchConfig())

    fun save(c: TouchConfig) {
        file.parentFile?.mkdirs()
        val tmp = File(file.parentFile, file.name + ".tmp")
        tmp.writeText(c.toJson().toString(2))
        if (!tmp.renameTo(file)) {
            file.writeText(tmp.readText())
            tmp.delete()
        }
    }
}
