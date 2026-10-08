package com.dfdx047.dragonrage.engine

import android.content.Context

/**
 * The link to the game engine: the Tenkaichi3Decomp port built for arm64 as libbt3.so (port/tools/android.py),
 * next to SDL3 (libSDL3.so) and its loader (libmain.so). GameActivity starts it with the app's folder
 * (GamePaths.root) as its working directory, so it finds gamedata/, textures/, stages/, songs/, saves/ and
 * bt3_settings.txt where the PC build looks for them. A build of the app without the engine still works as a
 * launcher.
 */
object EngineBridge {
    const val LIBRARY = "bt3"

    /** Whether the engine library is packaged in this APK. Looked up without loading it. */
    fun isAvailable(context: Context): Boolean {
        // (the class loader also finds libraries kept uncompressed inside the APK, which is the default)
        val loader = context.classLoader as? dalvik.system.BaseDexClassLoader ?: return false
        return loader.findLibrary(LIBRARY) != null
    }
}
