package com.dfdx047.dragonrage.engine

/** The on-screen controller's line to the engine (functions in libmain.so, src/main/cpp/loader.c). */
object TouchBridge {
    /** Player 1's buttons (PS2 bits, 1 = pressed) and sticks (0..255, 128 = centre). */
    @JvmStatic external fun nativeSetPad(buttons: Int, lx: Int, ly: Int, rx: Int, ry: Int)

    /** The engine's own window (settings, online) is open. */
    @JvmStatic external fun nativeMenuOpen(): Boolean

    fun setPad(buttons: Int, lx: Int, ly: Int, rx: Int, ry: Int) {
        runCatching { nativeSetPad(buttons, lx, ly, rx, ry) }
    }

    fun menuOpen(): Boolean = runCatching { nativeMenuOpen() }.getOrDefault(false)
}
