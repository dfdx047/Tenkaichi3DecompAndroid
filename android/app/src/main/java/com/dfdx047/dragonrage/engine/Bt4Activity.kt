package com.dfdx047.dragonrage.engine

import android.os.Bundle
import com.dfdx047.dragonrage.data.ActivityLog
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.data.GamePaths

/**
 * Budokai Tenkaichi 4: BT4decomp (NauTaro's decompilation of the BT4 mod, with Dragon Rage's
 * Android platform layer), the same game screen and engine start as Budokai Tenkaichi 3's (libmain.so, loader.c), in
 * its own folder (bt4/) and its own process (":bt4"). loader.c loads libbt4dec.so in that process instead of
 * libbt3.so. Its data tables (libbt3.so.dat/.rel/.mem, the names the loader and plat_mem.c read) and the files it
 * reads beside itself (fonts, tbo/ui, translations) are the APK's assets/bt4dec/.
 *
 * The game's data is the disc unpacked into bt4/gamedata (Bt4Installer): the image is not opened here.
 */
class Bt4Activity : GameActivity() {
    override fun gamePaths(): GamePaths = GamePaths(this, Game.BT4)

    override val game: Game get() = Game.BT4

    override val engineAssets: String get() = "bt4dec/"

    override fun onCreate(savedInstanceState: Bundle?) {
        ActivityLog.init(GamePaths(this, Game.BT4)) // (this process's lines in the same diary, for the exported log)
        super.onCreate(savedInstanceState)
    }
}
