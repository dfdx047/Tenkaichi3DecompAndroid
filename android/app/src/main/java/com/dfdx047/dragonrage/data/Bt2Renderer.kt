package com.dfdx047.dragonrage.data

import java.io.File

/**
 * Which renderer Budokai Tenkaichi 2 draws with, and the settings of BT3's renderer when it is that one.
 *
 * BT2's engine has two (bt2/runtime/gs/hw/hw_gs_mirror.h): BT3's renderer (a copy of Dragon Rage's own, drawing
 * with the GPU's rasteriser: fast on phones, the default) and paraLLEl-GS (the PS2's GS pixel by pixel on compute
 * shaders: exact, heavy). The choice is "renderer = bt3" in <root>/bt2/user/android; BT3's renderer reads its
 * settings from <root>/bt2/user/renderer with BT3's own names (bt2/runtime/gs/hw/bt3_host.c, Port_Setting), so the
 * BT2 tab and the in-game menu offer what BT3's settings offer. Nothing here is BT3's: its settings stay its own.
 */
class Bt2Renderer(paths: GamePaths) {
    private val android = File(paths.bt2Root, "user/android")
    private val file = File(paths.bt2Root, "user/renderer")

    data class Values(
        val bt3: Boolean = true,
        val scale4: Int = 4,          // internal resolution in quarters: 4 = 1x, 6 = 1.5x, 7 = 1.75x, 8 = 2x ...
        val filter: Int = 0,          // 0 bilinear, 1 sharp bilinear, 2 FXAA, 3 FSR 1, 4 Snapdragon GSR
        val frameSkip: Boolean = true,
        val preciseDepth: Boolean = false, // "frag_depth": off lets tiled GPUs drop hidden pixels early
        val smoothMotion: Int = 0,    // "interp": 0 off, 1 60, 2 120
        val present: Int = 0,         // 0 automatic, 1 FIFO, 2 mailbox, 3 immediate
        val fxOff: Int = 0,           // bits: 1 outline, 2 see-through tint, 4 depth tint, 8 glare and glow, 16 distance blur, 32 movement blur
        val glow: Int = 60,           // percent
        val smooth2d: Boolean = true,
        val openGl: Boolean = false,  // "gpu_api" 1: BT3's renderer's OpenGL ES back end (offered for Mali GPUs; Vulkan is the default)
        val renderThread: Boolean = true, // "render_thread" in <root>/bt2/user/android: the GIF packets on a thread of their own (Vulkan)
    )

    fun load(): Values {
        val k = read(file)
        fun n(key: String, def: Int) = k[key]?.toIntOrNull() ?: def
        val d = Values()
        return Values(
            bt3 = read(android)["renderer"]?.let { it == "bt3" } ?: d.bt3,
            renderThread = read(android)["render_thread"]?.let { it != "0" } ?: d.renderThread,
            scale4 = n("scale4", d.scale4), filter = n("filter", d.filter), frameSkip = n("frame_skip", 1) != 0,
            preciseDepth = n("frag_depth", 0) != 0, smoothMotion = n("interp", d.smoothMotion), present = n("present", d.present),
            fxOff = n("fx_off", d.fxOff), glow = n("glow", d.glow), smooth2d = n("smooth_2d", 1) != 0,
            openGl = n("gpu_api", 0) == 1,
        )
    }

    fun save(v: Values) {
        write(android, mapOf("renderer" to if (v.bt3) "bt3" else "pgs", "render_thread" to bit(v.renderThread)))
        write(file, mapOf(
            "scale4" to v.scale4.toString(), "filter" to v.filter.toString(), "frame_skip" to bit(v.frameSkip),
            "frag_depth" to bit(v.preciseDepth), "interp" to v.smoothMotion.toString(), "present" to v.present.toString(),
            "fx_off" to v.fxOff.toString(), "glow" to v.glow.toString(), "smooth_2d" to bit(v.smooth2d),
            "gpu_api" to bit(v.openGl),
        ))
    }

    /** The phone's defaults where nothing was chosen yet: BT3's renderer, light settings. */
    fun applyPhoneDefaults() {
        if ("renderer" !in read(android) || !file.exists()) save(load())
    }

    private fun bit(b: Boolean) = if (b) "1" else "0"

    private fun read(f: File): Map<String, String> = runCatching {
        f.readLines().mapNotNull { line ->
            val at = line.indexOf(" = ")
            if (at < 0) null else line.substring(0, at) to line.substring(at + 3)
        }.toMap()
    }.getOrDefault(emptyMap())

    private fun write(f: File, changes: Map<String, String>) {
        val lines = runCatching { f.readLines() }.getOrDefault(emptyList()).filter { it.isNotBlank() }.toMutableList()
        val left = changes.toMutableMap()
        for (i in lines.indices) {
            val key = lines[i].substringBefore(" = ", "")
            left.remove(key)?.let { lines[i] = "$key = $it" }
        }
        left.forEach { (k, v) -> lines += "$k = $v" }
        f.parentFile?.mkdirs()
        f.writeText(lines.joinToString("\n", postfix = "\n"))
    }

    companion object {
        /** The internal resolutions offered, in quarters (BT3's steps). */
        val SCALES = listOf(4, 5, 6, 7, 8, 12, 16)
        fun scaleLabel(q: Int) = if (q % 4 == 0) "${q / 4}x" else "%.2fx".format(q / 4f).trimEnd('0').replace('.', ',')
    }
}
