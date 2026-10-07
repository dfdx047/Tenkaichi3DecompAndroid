package com.dfdx047.dragonrage.data

import android.content.Context
import java.io.File

/**
 * Where everything lives. The layout is the one the PC port expects next to its program, so the engine can be
 * started with this folder as its working directory and needs no Android-specific paths:
 *
 *   <root>/gamedata/            the disc unpacked (disc/, pzs3us0/ ..., .installed marker)
 *   <root>/gamedata/mods/       file overrides (same path as in gamedata/) — rebuilt from modlib/
 *   <root>/textures/            texture packs in PCSX2 naming, any depth (one folder per pack)
 *   <root>/textures_off/        packs switched off in the app (the engine never looks here)
 *   <root>/stages/  songs/      extra stages (.unk) and songs (.adx), with maps.txt / songs.txt for names
 *   <root>/saves/               memory card
 *   <root>/bt3_settings.txt     the engine's settings (name=number)
 *   <root>/modlib/<mod>/        the app's library of file-override mods, each one switchable
 *
 * The root is the app's folder on shared storage (Android/data/<package>/files), so a file manager or adb can
 * reach it too.
 */
class GamePaths(context: Context) {
    val root: File = (context.getExternalFilesDir(null) ?: context.filesDir).also { it.mkdirs() }
    val gameData = File(root, "gamedata")
    val installedMarker = File(gameData, ".installed")
    val mods = File(gameData, "mods")
    val textures = File(root, "textures")
    val texturesOff = File(root, "textures_off")
    val stages = File(root, "stages")
    val songs = File(root, "songs")
    val saves = File(root, "saves")
    val settings = File(root, "bt3_settings.txt")
    val modLibrary = File(root, "modlib")
    val temp = File(context.cacheDir, "import")

    fun ensure() {
        listOf(gameData, textures, texturesOff, stages, songs, saves, modLibrary, temp).forEach { it.mkdirs() }
    }
}
