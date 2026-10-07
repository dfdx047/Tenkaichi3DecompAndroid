package com.dfdx047.dragonrage.engine

import android.content.Context

/**
 * The link to the game engine (the Tenkaichi3Decomp port built for arm64 as libdragonrage.so).
 *
 * The engine is not part of this build yet. When it is, it ships as a native library next to SDL3
 * (libSDL3.so) and is started from an SDL activity with the app's folder (GamePaths.root) as its working
 * directory, so it finds gamedata/, textures/, stages/, songs/, saves/ and bt3_settings.txt where the PC
 * build looks for them.
 */
object EngineBridge {
    const val LIBRARY = "dragonrage"

    /** Whether the engine library is packaged in this APK. Looked up without loading it. */
    fun isAvailable(context: Context): Boolean {
        // (the class loader also finds libraries kept uncompressed inside the APK, which is the default)
        val loader = context.classLoader as? dalvik.system.BaseDexClassLoader ?: return false
        return loader.findLibrary(LIBRARY) != null
    }
}
