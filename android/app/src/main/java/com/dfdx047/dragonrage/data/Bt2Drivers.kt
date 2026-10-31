package com.dfdx047.dragonrage.data

import android.content.Context
import java.io.File

/**
 * Budokai Tenkaichi 2's own GPU driver choice. The installed drivers are the app's (GpuDrivers: files/drivers),
 * but which one BT2 uses is kept apart from BT3's (<root>/bt2/user/driver), so choosing one on the BT2 tab never
 * changes BT3's.
 */
object Bt2Drivers {
    private fun file(paths: GamePaths) = File(paths.bt2Root, "user/driver")

    fun selectedId(paths: GamePaths): String? = runCatching { file(paths).readText().trim() }.getOrNull()?.takeIf { it.isNotEmpty() }

    fun selected(context: Context, paths: GamePaths): GpuDriver? {
        val id = selectedId(paths) ?: return null
        return GpuDrivers(context, context.contentResolver).list().firstOrNull { it.id == id }
    }

    fun select(paths: GamePaths, id: String?) {
        val f = file(paths)
        if (id == null) f.delete() else { f.parentFile?.mkdirs(); f.writeText(id) }
    }
}
