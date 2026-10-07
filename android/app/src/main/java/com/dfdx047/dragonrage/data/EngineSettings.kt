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
        require(name.length < 24) { "nome de ajuste longo demais: $name" }
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
        const val SCALE = "scale"                 // internal resolution 1..8, default 2
        const val ASPECT = "aspect_milli"         // 1333, 1600, 1778, 2389, 3556
        const val FX_OFF = "fx_off"               // bits: 1 outline, 2 see-through tint, 4 depth tint, 8 glow, 16 blur
        const val GLOW = "glow"                   // 0..200 %, default 60
        const val MUSIC = "music"                 // 0..200 %
        const val EFFECTS = "effects"             // 0..200 %, sound effects and voices
        const val METER = "meter"                 // frame-rate meter
        const val TEXTURE_PACK = "texture_pack"   // texture packs on / off, default on

        // Dragon Rage's own, read by the Android glue of the engine at start
        const val UNLOCK_ALL = "dr_unlock_all"    // 1: run Save_UnlockAll once on the next start, then 0
        const val FPS60 = "dr_fps60"              // reserved for the 60 fps mode
        const val TOUCH = "dr_touch"              // on-screen controls: 0 off, 1 automatic, 2 always
        const val TOUCH_OPACITY = "dr_touch_alpha" // 10..100 %

        const val GLOW_DEFAULT = 60
        const val SCALE_DEFAULT = 2
        const val ASPECT_DEFAULT = 1778
    }
}
