package com.dfdx047.dragonrage.data

import java.io.File

/**
 * The engine's settings file (port/src/plat_settings.c): `name=number` lines, names up to 23 characters, at most
 * 200 of them. The app edits the same file the engine reads at start, and keeps every line it does not know (the
 * controller bindings the engine writes, for example).
 */
class EngineSettings(private val file: File) {
    private val values = linkedMapOf<String, Int>()

    init {
        load()
    }

    @Synchronized
    fun load() {
        values.clear()
        if (!file.exists()) return
        file.forEachLine { line ->
            val eq = line.indexOf('=')
            if (eq <= 0) return@forEachLine
            val name = line.substring(0, eq).trim()
            val v = line.substring(eq + 1).trim().toIntOrNull() ?: return@forEachLine
            if (name.isNotEmpty() && name.length < 24) values[name] = v
        }
    }

    @Synchronized
    fun get(name: String, default: Int): Int = values[name] ?: default

    @Synchronized
    fun snapshot(): Map<String, Int> = LinkedHashMap(values)

    @Synchronized
    fun set(name: String, value: Int) {
        require(name.length < 24) { "setting name too long: $name" }
        if (values[name] == value) return
        values[name] = value
        write()
    }

    private fun write() {
        file.parentFile?.mkdirs()
        val tmp = File(file.parentFile, file.name + ".tmp")
        tmp.writeText(values.entries.joinToString(separator = "\n", postfix = "\n") { "${it.key}=${it.value}" })
        if (!tmp.renameTo(file)) {
            file.writeText(tmp.readText())
            tmp.delete()
        }
    }

    companion object Keys {
        // The engine's own (defaults as in port/src/gs/gs_draw.c)
        const val SCALE = "scale"                 // internal resolution 1..8, default 2 (whole; kept for older engines)
        const val SCALE4 = "scale4"               // the same in quarters: 4 = 1x, 6 = 1.5x .. 32 = 8x (wins over "scale")
        const val ASPECT = "aspect_milli"         // 1333, 1600, 1778, 2389, 3556
        const val FX_OFF = "fx_off"               // bits: 1 outline, 2 see-through tint, 4 depth tint, 8 glow, 16 blur
        const val GLOW = "glow"                   // 0..200 %, default 60
        const val MUSIC = "music"                 // 0..200 %
        const val EFFECTS = "effects"             // 0..200 %, sound effects and voices
        const val METER = "meter"                 // frame-rate meter
        const val TEXTURE_PACK = "texture_pack"   // texture packs on / off, default on
        const val FILTER = "filter"               // screen filter: 0 bilinear, 1 sharp bilinear, 2 FXAA, 3 FSR 1, 4 Snapdragon GSR

        // Dragon Rage's own, read by the Android glue of the engine at start
        const val UNLOCK_ALL = "dr_unlock_all"    // 1: run Save_UnlockAll once on the next start, then 0
        const val FPS60 = "dr_fps60"              // reserved for the 60 fps mode
        const val TAG_FLAT = "tag_flat"           // cheat: no toon layer on any fighter in Tag Team (more speed, flatter look)
        const val SELECT_CLASSIC = "select_classic" // the character select: 0 the new one (default), 1 the game's own
        const val TAG_LITE = "tag_lite"           // the Tag Team's light graphics (only in that mode; also in the in-game menu)
        const val ADPF = "adpf"                   // the system's performance hints (Android 13+): 1 on (default), 0 off
        const val SUSTAINED = "dr_sustained"      // Android's sustained performance mode for the game's window: 1 on
        const val BT4_ASPECT = "bt4_aspect"       // Budokai Tenkaichi 4: 0 16:9 (the mod's own), 1 4:3, 2 the whole screen
        const val BT4_FPS60 = "bt4_fps60"         // Budokai Tenkaichi 4: 60 fps fights (BT3-Recomp's mode)
        const val BT4_FPS60_DROPPED = "bt4_fps60_dropped" // 1: they went back to 30 on this device (below 57 a second)
        const val BT4_RENDERER = "bt4_renderer"   // Budokai Tenkaichi 4: 0 Dragon Rage's GS, 1 ARMSX2's (libbt4gsx.so), 2 Nominum Render's (the same library, when it is Nominum's): the player's choice
        const val BT4_RENDERER_AUTO = "bt4_renderer_auto" // the same with no choice: 0 where the device is weak or did not hold ARMSX2's, else 1

        /** Whether this build has ARMSX2's renderer for Budokai Tenkaichi 4 (port/bt4/build_bt4.py --armsx2). */
        fun hasArmsx2Renderer(context: android.content.Context): Boolean =
            java.io.File(context.applicationInfo.nativeLibraryDir, "libbt4gsx.so").exists()
        /** The libbt4gsx.so in this APK is Nominum Render's (port/bt4/gsx/use_nominum.sh): BT4 can also use its own backend (bt4_renderer 2). */
        fun hasNominumRenderer(context: android.content.Context): Boolean =
            hasArmsx2Renderer(context) && runCatching { context.assets.open("bt4gsx/nominum.txt").close() }.isSuccess
        const val INTERP = "interp"               // smooth motion: 0 off, 1 60, 2 120, 3 240 pictures a second (the engine's)
        const val PRESENT = "present"             // present mode: 0 automatic, 1 vsync (FIFO), 2 mailbox, 3 immediate
        const val GPU_API = "gpu_api"             // the engine's renderer: 0 Vulkan, 1 OpenGL ES 3.2 (written from RENDERER)
        const val RENDERER = "dr_renderer"        // the player's choice: 0 automatic (OpenGL ES for Mali), 1 Vulkan, 2 OpenGL ES
        const val CLASH_SQUARE = "clash_square"   // clashes (beams, rushes): each press of Square counts as a direction; on by default
        const val LANG = "dr_lang"                // the in-game menu's language: 0 English, 1 Portuguese, 2 Spanish (written at start)
        const val ORIENTATION = "dr_orient"       // the game's screen: 0 landscape, 1 portrait, 2 as the device is held
        const val FRAG_DEPTH = "frag_depth"       // precise depth (the fragment shader writes depth): 1 on, 0 off (faster)
        const val ASTC = "astc"                   // texture packs kept as ASTC copies (textures_astc) where the GPU reads ASTC
        const val FRAME_SKIP = "frame_skip"       // skip pictures instead of slowing down
        const val LITE = "dr_lite"                // Lite mode for weak GPUs: 1 on (render thread, glow chains at 0 left out, render areas on Mali)
        const val TAG_TEAM = "dr_tag_team"        // the Tag Team mod: 1 on (SELECT in the main menu opens its modes), 0 off
        const val TAG_TEAM_DEFAULT = 0
        const val GLARE = "glare"                 // the sky's glare: 0..200 %, the glow's until set
        const val TAG_LITE_ASKED = "dr_tag_lite_asked" // 1: the light graphics offer (weak GPUs) was answered, not asked again
        const val GPU_TIER = "dr_gpu_tier"        // the GPU's tier the launcher's optimisations were made for (GpuProfiles)
        const val TOUCH = "dr_touch"              // on-screen controls: 0 off, 1 automatic, 2 always
        const val TOUCH_OPACITY = "dr_touch_alpha" // 10..100 %

        const val GLOW_DEFAULT = 60
        const val SCALE_DEFAULT = 2
        /** The internal resolutions offered, in quarters (the engine's settings window has the same list). */
        val SCALE_STEPS = listOf(4, 5, 6, 7, 8, 10, 12, 16, 20, 24, 32)
        const val ASPECT_DEFAULT = 0              // 0 = the screen's own shape (the engine measures it)
    }
}
