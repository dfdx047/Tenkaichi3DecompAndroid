package com.dfdx047.dragonrage.engine

/** Budokai Tenkaichi 2's in-game menu's line to its engine (libbt2.so: bt2/runtime/android/android_main.cpp). */
object Bt2Bridge {
    /** paraLLEl-GS's speed-for-accuracy options (Bt2Settings.Values.hacks), from the renderers' next frame. */
    @JvmStatic external fun nativeSetGsHacks(list: String)

    /** The game stands still at its next field while [paused] (the in-game menu is open). */
    @JvmStatic external fun nativeSetPaused(paused: Boolean)

    /** The engine reads its settings file again (volumes, scaling) from its next field. */
    @JvmStatic external fun nativeReloadSettings()

    /** How pictures reach the display (Bt2Settings.Values.presentMode), from the next picture. */
    @JvmStatic external fun nativeSetPresentMode(mode: Int)

    /** The performance panel's running counts (Bt2PerfOverlay): pictures shown, fields run, the game thread's id. */
    @JvmStatic external fun nativeStats(): LongArray

    fun setPresentMode(mode: Int) {
        runCatching { nativeSetPresentMode(mode) }
    }

    fun setPaused(paused: Boolean) {
        runCatching { nativeSetPaused(paused) }
    }

    fun reloadSettings() {
        runCatching { nativeReloadSettings() }
    }

    fun setGsHacks(list: String) {
        runCatching { nativeSetGsHacks(list) }
    }
}
