package com.dfdx047.dragonrage.data

import android.content.Context
import java.io.File

/**
 * BT2's "unlock everything": a 100% save, packaged in the app's assets (bt2/unlock), on a memory card of its own
 * that the engine uses from the next start; turning it off goes back to the card in use before. Shared by the BT2
 * tab (Bt2ViewModel) and the in-game menu (engine/Bt2Menu).
 */
object Bt2Unlock {
    const val CARD = "Tudo desbloqueado"
    const val ASSETS = "bt2/unlock"

    fun available(context: Context): Boolean = runCatching { context.assets.list(ASSETS).orEmpty().isNotEmpty() }.getOrDefault(false)

    fun isOn(paths: GamePaths): Boolean = Bt2Settings(paths).memoryCard() == CARD

    /** Turns it on or off; false when the card could not be made. */
    fun set(context: Context, paths: GamePaths, on: Boolean): Boolean {
        val settings = Bt2Settings(paths)
        if (on) {
            val card = File(paths.bt2Saves, CARD)
            if (!card.isDirectory && runCatching { copyAssets(context, ASSETS, card) }.isFailure) return false
            val current = settings.memoryCard()
            if (current != CARD) settings.setPreviousCard(current)
            settings.setMemoryCard(CARD)
        } else {
            settings.setMemoryCard(settings.previousCard())
        }
        return true
    }

    /**
     * The card of an online match: a fresh copy of the 100% save from the APK's assets, in the user folder (never the player's own card, which
     * their play may have changed). Both players start the match from the same bytes; the engine takes it when a match is on (android_main.cpp).
     */
    fun makeMatchCard(context: Context, paths: GamePaths): Boolean {
        val card = File(paths.bt2Root, "user/online-card")
        return runCatching { card.deleteRecursively(); copyAssets(context, ASSETS, card) }.isSuccess
    }

    private fun copyAssets(context: Context, path: String, into: File) {
        val names = context.assets.list(path).orEmpty()
        if (names.isEmpty()) { // a file
            into.parentFile?.mkdirs()
            context.assets.open(path).use { input -> into.outputStream().use { input.copyTo(it) } }
            return
        }
        into.mkdirs()
        names.forEach { copyAssets(context, "$path/$it", File(into, it)) }
    }
}
