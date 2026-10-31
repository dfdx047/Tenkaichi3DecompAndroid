package com.dfdx047.dragonrage.data

/**
 * The games the launcher opens. Each has its own folder, disc image, saves and settings, and its own engine:
 *  - BT3: Budokai Tenkaichi 3 (USA, SLUS-21678), the Tenkaichi3Decomp port (libbt3.so), as always: its folder is the
 *    app's folder itself, so nothing of an existing install moves.
 *  - BT4: Budokai Tenkaichi 4, Team BT4's mod of BT3 (SLUS-219.78, Beta 14 Rev2), recompiled with PS2Recomp
 *    (recomp-code/dbz-bt4-recomp) and drawn by Dragon Rage's own GS (libbt4.so). In the folder bt4/.
 *  - BT2: Budokai Tenkaichi 2 (USA, SLUS-21441), BT2-Recomp (libbt2.so, imported by the player or packaged). It keeps
 *    to bt2/ under the app's folder (GamePaths.bt2Root) and its own pages, view model and process (Bt2GameActivity):
 *    nothing set there reaches BT3 or BT4. [folder] is empty because that bt2/ is not a root of the AppViewModel kind.
 *
 * [serial] is the disc's code; [library] the engine library's name (libNAME.so).
 */
enum class Game(val id: String, val folder: String, val engine: String, val serial: String, val library: String) {
    BT3("bt3", "", "bt3", "SLUS-21678", "bt3"),
    BT4("bt4", "bt4", "bt4", "SLUS-219.78", "bt4"),
    BT2("bt2", "", "bt2", "SLUS-21441", "bt2");

    companion object {
        fun of(id: String?): Game = entries.firstOrNull { it.id == id } ?: BT3
    }
}
