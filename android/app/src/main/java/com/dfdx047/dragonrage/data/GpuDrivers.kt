package com.dfdx047.dragonrage.data

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import java.io.File
import java.util.zip.ZipInputStream
import org.json.JSONObject

/** A Vulkan driver imported from a zip (the AdrenoTools package: meta.json and the driver's .so). */
data class GpuDriver(val id: String, val name: String, val version: String, val library: String, val dir: File)

/**
 * Custom GPU drivers for Adreno GPUs (Turnip, newer Qualcomm drivers), loaded by the engine's loader through
 * libadrenotools. They have to live in the app's private storage (a driver cannot be loaded from shared storage), in
 * files/drivers/<id>/; files/drivers/selected holds the chosen id (none: the system's driver).
 */
class GpuDrivers(context: Context, private val resolver: ContentResolver) {
    private val root = File(context.filesDir, "drivers")
    private val selectedFile = File(root, "selected")

    fun list(): List<GpuDriver> = root.listFiles()?.filter { it.isDirectory }?.mapNotNull { read(it) }?.sortedBy { it.name.lowercase() } ?: emptyList()

    fun selected(): GpuDriver? {
        val id = runCatching { selectedFile.readText().trim() }.getOrNull() ?: return null
        return list().firstOrNull { it.id == id }
    }

    fun select(id: String?) {
        root.mkdirs()
        if (id == null) selectedFile.delete() else selectedFile.writeText(id)
    }

    fun delete(d: GpuDriver) {
        if (selected()?.id == d.id) select(null)
        d.dir.deleteRecursively()
    }

    /** Unpacks a driver zip; returns the driver, or null if the zip has no .so in it. */
    fun import(uri: Uri): GpuDriver? {
        root.mkdirs()
        val tmp = File(root, ".import").also { it.deleteRecursively(); it.mkdirs() }
        resolver.openInputStream(uri)?.use { input ->
            ZipInputStream(input.buffered()).use { zip ->
                while (true) {
                    val e = zip.nextEntry ?: break
                    if (e.isDirectory) continue
                    val name = e.name.substringAfterLast('/')
                    if (name.isEmpty() || name.startsWith(".")) continue
                    if (!name.endsWith(".so") && name != "meta.json") continue
                    File(tmp, name).outputStream().use { zip.copyTo(it) }
                }
            }
        } ?: return null
        val probe = read(tmp)
        if (probe == null) {
            tmp.deleteRecursively()
            return null
        }
        val id = safeName(probe.name + "-" + probe.version).lowercase().replace(' ', '_')
        val dest = File(root, id)
        dest.deleteRecursively()
        if (!tmp.renameTo(dest)) {
            tmp.copyRecursively(dest, overwrite = true)
            tmp.deleteRecursively()
        }
        return read(dest)
    }

    private fun read(dir: File): GpuDriver? {
        val meta = File(dir, "meta.json").takeIf { it.exists() }?.let { runCatching { JSONObject(it.readText()) }.getOrNull() }
        val lib = meta?.optString("libraryName")?.takeIf { it.isNotEmpty() && File(dir, it).exists() }
            ?: dir.listFiles()?.firstOrNull { it.name.endsWith(".so") }?.name
            ?: return null
        return GpuDriver(
            id = dir.name,
            name = meta?.optString("name")?.takeIf { it.isNotEmpty() } ?: lib.removeSuffix(".so"),
            version = meta?.optString("driverVersion") ?: "",
            library = lib,
            dir = dir,
        )
    }
}
