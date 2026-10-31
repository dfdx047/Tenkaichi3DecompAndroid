package com.dfdx047.dragonrage.engine

import android.content.Context
import com.dfdx047.dragonrage.data.Bt2Engine
import com.dfdx047.dragonrage.data.Game

/**
 * The link to the game engines: the Tenkaichi3Decomp port built for arm64 as libbt3.so (port/tools/android.py),
 * next to SDL3 (libSDL3.so) and its loader (libmain.so), and BT2-Recomp's runtime as libbt2.so (Bt2GameActivity).
 * GameActivity starts BT3 with the app's folder (GamePaths.root) as its working directory, so it finds gamedata/,
 * textures/, stages/, songs/, saves/ and bt3_settings.txt where the PC build looks for them. A build of the app
 * without an engine still works as a launcher for the other game, or for none.
 */
object EngineBridge {
    const val LIBRARY = "bt3"

    /** Whether [game]'s engine can be started: BT2's imported by the player (Bt2Engine), or packaged in this APK. */
    fun isAvailable(context: Context, game: Game = Game.BT3): Boolean =
        game == Game.BT2 && Bt2Engine.usable(context) != null || isPackaged(context, game)

    /** The same by the engine's name (Game.engine: "bt4" is Budokai Tenkaichi 4's, libbt4.so). */
    fun isAvailable(context: Context, library: String): Boolean = isPackaged(context, library)

    /** Whether [game]'s engine library is packaged in this APK. Looked up without loading it. */
    fun isPackaged(context: Context, game: Game = Game.BT3): Boolean = isPackaged(context, game.library)

    private fun isPackaged(context: Context, library: String): Boolean {
        // (the class loader also finds libraries kept uncompressed inside the APK, which is the default)
        val loader = context.classLoader as? dalvik.system.BaseDexClassLoader ?: return false
        return loader.findLibrary(library) != null
    }
}
