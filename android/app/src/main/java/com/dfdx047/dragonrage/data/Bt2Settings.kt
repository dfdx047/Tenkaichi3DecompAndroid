package com.dfdx047.dragonrage.data

import java.io.File

/**
 * Budokai Tenkaichi 2's settings: the file its engine reads (<root>/bt2/user/settings, "key = value" lines;
 * bt2/runtime/host/settings.h), of which the app changes only the keys below and keeps every other line as it is
 * (the disc and memory card the engine chose, the controls). The engine refuses unknown keys, so the options that
 * exist only on Android are in a file of their own (<root>/bt2/user/android, read by
 * bt2/runtime/android/android_main.cpp).
 */
class Bt2Settings(paths: GamePaths) {
    private val file = File(paths.bt2Root, "user/settings")
    private val android = File(paths.bt2Root, "user/android")

    /** The engine's keys the app shows, with the engine's defaults. */
    data class Values(
        val superSampling: Int = 1,      // 1, 2, 4, 8, 16 samples per pixel (4 and more: twice the resolution)
        val widescreen: Boolean = false, // the game's own 16:9 patch
        val sixtyFps: Boolean = false,   // pictures between the fights' frames (heavy)
        val showFrameRate: Boolean = false,
        val integerScaling: Boolean = false, // the engine's default is on; on a phone the picture fills the height instead
        val hdTextures: Boolean = false,
        val masterVolume: Int = 100,
        val musicVolume: Int = 100,
        val effectsVolume: Int = 100,
        val blendDemote: Boolean = false, // Android: paraLLEl-GS's lighter blending (BT2_GS_HACKS)
        val noMipmaps: Boolean = false,   // Android: paraLLEl-GS without mipmaps
        val unsyncedReadbacks: Boolean = true,    // Android: the game reads drawn pictures back a frame late, without waiting for the GPU
        val backbufferPromotion: Boolean = false, // Android: paraLLEl-GS's backbuffer promotion
        val outputScale: Int = 100, // Android: the screen's picture at this percent of its size (75, 50: less work for the GPU)
        val frameSkip: Boolean = true,      // Android: a frame is not drawn when the GPU is behind (the game keeps its speed)
        val screenEffects: Boolean = true,  // Android: effects that read the screen they draw to (distortion, trails, blur)
        val fog: Boolean = true,            // Android: the GS's fog
        val presentMode: Int = 0,           // Android: 0 automatic, 1 FIFO, 2 mailbox, 3 immediate
    ) {
        /** paraLLEl-GS's speed-for-accuracy options as BT2_GS_HACKS lists them (gs/gpu/gpu_gs_backend.cpp). */
        fun hacks(): String = listOfNotNull(
            "blend_demote".takeIf { blendDemote }, "no_mipmaps".takeIf { noMipmaps },
            "late_readbacks".takeIf { unsyncedReadbacks }, "backbuffer_promotion".takeIf { backbufferPromotion },
            "frame_skip".takeIf { frameSkip }, "skip_feedback".takeIf { !screenEffects }, "no_fog".takeIf { !fog },
        ).joinToString(",")
    }

    fun load(): Values {
        val keys = read(file)
        val androidKeys = read(android)
        // (no list yet: the defaults, as applyPhoneDefaults writes them)
        val hacks = (androidKeys["gs_hacks"] ?: Values().hacks()).split(',').map { it.trim() }.toSet()
        fun flag(key: String, default: Boolean) = keys[key]?.let { it == "1" } ?: default
        fun number(key: String, default: Int) = keys[key]?.toIntOrNull() ?: default
        val d = Values()
        return Values(
            superSampling = number("super_sampling", d.superSampling),
            widescreen = flag("widescreen", d.widescreen),
            sixtyFps = flag("sixty_fps", d.sixtyFps),
            showFrameRate = flag("show_frame_rate", d.showFrameRate),
            integerScaling = flag("integer_scaling", d.integerScaling),
            hdTextures = flag("hd_textures", d.hdTextures),
            masterVolume = number("master_volume", d.masterVolume),
            musicVolume = number("music_volume", d.musicVolume),
            effectsVolume = number("effects_volume", d.effectsVolume),
            blendDemote = "blend_demote" in hacks,
            noMipmaps = "no_mipmaps" in hacks,
            unsyncedReadbacks = "late_readbacks" in hacks,
            backbufferPromotion = "backbuffer_promotion" in hacks,
            frameSkip = "frame_skip" in hacks,
            screenEffects = "skip_feedback" !in hacks,
            fog = "no_fog" !in hacks,
            presentMode = androidKeys["present_mode"]?.toIntOrNull()?.coerceIn(0, 3) ?: d.presentMode,
            outputScale = androidKeys["output_scale"]?.toIntOrNull()?.takeIf { it in OUTPUT_SCALES } ?: d.outputScale,
        )
    }

    fun save(v: Values) {
        write(file, mapOf(
            "super_sampling" to v.superSampling.toString(), "widescreen" to bit(v.widescreen), "sixty_fps" to bit(v.sixtyFps),
            "show_frame_rate" to bit(v.showFrameRate), "integer_scaling" to bit(v.integerScaling), "hd_textures" to bit(v.hdTextures),
            "master_volume" to v.masterVolume.toString(), "music_volume" to v.musicVolume.toString(), "effects_volume" to v.effectsVolume.toString(),
        ))
        write(android, mapOf("gs_hacks" to v.hacks(), "output_scale" to v.outputScale.toString(), "present_mode" to v.presentMode.toString()))
    }

    /** The phone's defaults for keys the player never set (the engine's are a PC window's): the picture fills the
     *  screen's height rather than the largest whole multiple of the PS2's, which leaves wide borders on a phone. */
    fun applyPhoneDefaults() {
        if ("integer_scaling" !in read(file)) write(file, mapOf("integer_scaling" to "0"))
        if ("gs_hacks" !in read(android)) write(android, mapOf("gs_hacks" to Values().hacks())) // frame skipping on
    }

    /** The memory card the engine uses (a folder name in bt2/saves; null: the engine chooses one). */
    fun memoryCard(): String? = read(file)["memory_card"]?.takeIf { it.isNotEmpty() }
    fun setMemoryCard(name: String?) = write(file, mapOf("memory_card" to (name ?: "")))

    /** The card in use before the unlock-everything card was chosen, to go back to. */
    fun previousCard(): String? = read(android)["previous_card"]?.takeIf { it.isNotEmpty() }
    fun setPreviousCard(name: String?) = write(android, mapOf("previous_card" to (name ?: "")))

    companion object {
        /** The screen picture's sizes offered, in percent. */
        val OUTPUT_SCALES = listOf(100, 75, 50)
    }

    private fun bit(b: Boolean) = if (b) "1" else "0"

    private fun read(f: File): Map<String, String> = runCatching {
        f.readLines().mapNotNull { line ->
            val at = line.indexOf(" = ")
            if (at < 0) null else line.substring(0, at) to line.substring(at + 3)
        }.toMap()
    }.getOrDefault(emptyMap())

    /** The file with [changes] applied: existing lines in their place, new keys added at the end. */
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
}
