package com.dfdx047.dragonrage.data

import android.content.Context
import java.io.File

/**
 * The pictures of BT2's "Online Mode" item and screen (the app's assets, bt2/menu-item: drawn by
 * bt2/tools/menu_item/make_menu_item.py, nothing of them from the game's disc), put where the engine reads them:
 * <root>/bt2/user/menu-item (bt2/runtime/android/android_main.cpp, BT2_MAIN_MENU_ITEM). Copied again only after the
 * app was updated. Without them in the APK the engine leaves the menu as the game has it.
 */
object Bt2MenuItem {
    private const val ASSETS = "bt2/menu-item"
    private const val STAMP = ".app-update"

    fun install(context: Context, paths: GamePaths) {
        runCatching {
            val names = context.assets.list(ASSETS).orEmpty()
            if (names.isEmpty()) return
            val into = File(paths.bt2Root, "user/menu-item")
            val updated = context.packageManager.getPackageInfo(context.packageName, 0).lastUpdateTime.toString()
            if (File(into, STAMP).takeIf { it.isFile }?.readText() == updated && names.all { File(into, it).isFile }) return
            into.deleteRecursively() // (what an older version put there goes too)
            into.mkdirs()
            for (name in names) {
                context.assets.open("$ASSETS/$name").use { input -> File(into, name).outputStream().use { input.copyTo(it) } }
            }
            File(into, STAMP).writeText(updated)
        }
    }
}
