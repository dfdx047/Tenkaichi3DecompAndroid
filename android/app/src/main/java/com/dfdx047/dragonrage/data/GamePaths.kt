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
 * Budokai Tenkaichi 2 (BT2-Recomp, libbt2.so) has a folder of its own under the root, in the layout its PC program
 * expects next to itself (runtime/host/game_folders.h). BT3's files stay where they always were:
 *
 *   <root>/bt2/data/BT2.iso     the disc image, whole (the BT2 runtime reads the image itself)
 *   <root>/bt2/data/textures/   its texture packs
 *   <root>/bt2/saves/           its memory cards
 *   <root>/bt2/.installed       written once the image is in place
 *
 * The root is the app's folder on shared storage (Android/data/<package>/files), so a file manager or adb can
 * reach it too.
 */
class GamePaths(context: Context, val game: Game = Game.BT3) {
    /** The app's folder; Budokai Tenkaichi 3 lives in it, Budokai Tenkaichi 4 in its bt4/ (see Game). */
    val base: File = (context.getExternalFilesDir(null) ?: context.filesDir).also { it.mkdirs() }
    val root: File = (if (game.folder.isEmpty()) base else File(base, game.folder)).also { it.mkdirs() }
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
    /** BT4 only: the disc's boot files copied out of the image (the rest is read from the image itself), and the
     *  image the player picked (its content:// address, kept with a persisted read permission). */
    val runtimeData = File(root, "runtime/data")
    val isoUri = File(root, "iso_uri.txt")

    val bt2Root = File(root, "bt2")
    val bt2Data = File(bt2Root, "data")
    val bt2Iso = File(bt2Data, "BT2.iso")
    val bt2Saves = File(bt2Root, "saves")
    val bt2InstalledMarker = File(bt2Root, ".installed")

    fun ensure() {
        listOf(gameData, textures, texturesOff, stages, songs, saves, modLibrary, temp, bt2Data, bt2Saves).forEach { it.mkdirs() }
    }
}
