package com.dfdx047.dragonrage.engine

import android.os.Bundle
import com.dfdx047.dragonrage.data.GamePaths
import java.io.File
import org.libsdl.app.SDLActivity

/**
 * The game itself: SDL's activity, which loads libSDL3.so and libmain.so (the engine's loader,
 * src/main/cpp/loader.c) and calls SDL_main with the arguments below. It runs in a process of its own (":game" in
 * the manifest): the engine needs fixed addresses below 4 GB, which a fresh process has free, and when the game ends
 * its process goes with it and the launcher stays as it was.
 */
class GameActivity : SDLActivity() {
    private lateinit var paths: GamePaths

    override fun onCreate(savedInstanceState: Bundle?) {
        paths = GamePaths(this).also { it.ensure() }
        installDataList()
        super.onCreate(savedInstanceState)
    }

    /** The list of where the game's data tables come from (made with the engine, android.py) next to the game's files. */
    private fun installDataList() {
        val out = File(paths.root, "libbt3.so.dat")
        runCatching {
            assets.open("libbt3.so.dat").use { input ->
                val bytes = input.readBytes()
                if (!out.exists() || out.length() != bytes.size.toLong() || !out.readBytes().contentEquals(bytes)) {
                    out.writeBytes(bytes)
                }
            }
        }
    }

    override fun getLibraries(): Array<String> = arrayOf("SDL3", "main")

    override fun getArguments(): Array<String> = arrayOf(paths.root.path, applicationInfo.nativeLibraryDir)
}
