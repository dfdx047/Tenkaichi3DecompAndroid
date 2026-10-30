package com.dfdx047.dragonrage.engine

/**
 * The touch menu's line to the engine (functions in libmain.so, src/main/cpp/loader.c, which call the engine's
 * Ui_Nm*: port/src/gs/ui_android.inc; Budokai Tenkaichi 4: port/bt4/host/bt4_main.cpp). Keys are the names of
 * bt3_settings.txt. Every call is safe without an engine: it answers the default.
 */
object MenuBridge {
    @JvmStatic external fun nativeOpen(open: Boolean): Boolean
    @JvmStatic external fun nativeGen(): Int
    @JvmStatic external fun nativeGet(key: String, def: Int): Int
    @JvmStatic external fun nativeSet(key: String, value: Int)
    @JvmStatic external fun nativeText(key: String): String
    @JvmStatic external fun nativeAction(action: Int)

    const val ACTION_RESUME = 1
    const val ACTION_QUIT = 2
    const val ACTION_PAD_BUTTONS = 3
    // (Budokai Tenkaichi 3: a lobby's match and a watched match, ui_android.inc's nm_frame)
    const val ACTION_LOBBY_BACK = 4
    const val ACTION_LOBBY_LEAVE = 5
    const val ACTION_SPEC_VIEW = 6
    const val ACTION_SPEC_STOP = 7
    const val ACTION_PAD_AUTO = 8 // the controller's automatic set-up: the PS2 layout on the wireless one

    fun open(open: Boolean): Boolean = runCatching { nativeOpen(open) }.getOrDefault(false)
    fun gen(): Int = runCatching { nativeGen() }.getOrDefault(-1)
    fun get(key: String, def: Int): Int = runCatching { nativeGet(key, def) }.getOrDefault(def)
    fun set(key: String, value: Int) { runCatching { nativeSet(key, value) } }
    fun text(key: String): String = runCatching { nativeText(key) }.getOrDefault("")
    fun action(action: Int) { runCatching { nativeAction(action) } }
}
